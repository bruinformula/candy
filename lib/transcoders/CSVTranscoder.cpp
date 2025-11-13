#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include "interpreters/CSVTranscoder.hpp"

namespace CAN {

    CSVTranscoder::CSVTranscoder(const std::string& base_path, size_t batch_size)
        : base_path(base_path),
        shutdown_requested(false),
        batch_size(batch_size),
        frames_batch_count(0),
        decoded_signals_batch_count(0)
    {
        // Ensure the base directory exists
        std::filesystem::create_directories(base_path);
        
        // Reserve space for batches to avoid frequent reallocations
        frames_batch.reserve(batch_size);
        decoded_signals_batch.reserve(batch_size);

        writer_thread = std::thread(&CSVTranscoder::writer_loop, this);
    }

    CSVTranscoder::~CSVTranscoder() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            shutdown_requested = true;
        }
        queue_cv.notify_all();

        if (writer_thread.joinable()) {
            writer_thread.join();
        }

        // Close all CSV files
        for (auto& [filename, file] : csv_files) {
            if (file && file->is_open()) {
                file->close();
            }
        }
    }

    // public Methods
    void CSVTranscoder::transcode(CANTime timestamp, CANFrame frame) {
        enqueue_task([this, timestamp, frame]() {
            batch_frame(timestamp, frame);

            auto msg_it = messages.find(frame.can_id);
            if (msg_it != messages.end()) {
                batch_decoded_signals(timestamp, frame, msg_it->second);
            }

            if (frames_batch_count >= batch_size) flush_frames_batch();
            if (decoded_signals_batch_count >= batch_size) flush_decoded_signals_batch();
        });
    }

    void CSVTranscoder::transcode(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string csv_row = build_csv_row(data);
        enqueue_task([this, filename, csv_row, data]() {
            // Extract headers from data for first-time file creation
            std::vector<std::string> headers;
            for (const auto& [key, value] : data) {
                headers.push_back(key);
            }
            ensure_csv_file(filename, headers);
            write_to_csv(filename, csv_row);
        });
    }

    std::future<void> CSVTranscoder::transcode_async(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string csv_row = build_csv_row(data);
        std::promise<void> promise;
        auto future = promise.get_future();

        enqueue_task_with_promise([this, filename, csv_row, data]() {
            std::vector<std::string> headers;
            for (const auto& [key, value] : data) {
                headers.push_back(key);
            }
            ensure_csv_file(filename, headers);
            write_to_csv(filename, csv_row);
        }, std::move(promise));

        return future;
    }

    void CSVTranscoder::flush() {
        enqueue_task([this]() {
            flush_all_batches();
        });
    }

    void CSVTranscoder::flush_sync() {
        std::promise<void> promise;
        auto future = promise.get_future();
        enqueue_task_with_promise([this]() {
            flush_all_batches();
        }, std::move(promise));
        future.wait();
    }

    void CSVTranscoder::flush_async(std::function<void()> callback) {
        enqueue_task([this, callback]() {
            flush_all_batches();
            callback();
        });
    }

    void CSVTranscoder::sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
                        unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                        double factor, double offset, double min_val, double max_val,
                        std::string unit, std::vector<size_t> receivers)
    {
        SignalDefinition sig_def;
        sig_def.name = signal_name;
        sig_def.codec = std::make_unique<SignalCodec>(start_bit, bit_size, byte_order, sign_type);
        sig_def.numeric_value = std::make_unique<NumericValue>(factor, offset);
        sig_def.min_val = min_val;
        sig_def.max_val = max_val;
        sig_def.unit = unit;
        sig_def.mux_val = mux_val;

        messages[message_id].signals.push_back(std::move(sig_def));
    }

    void CSVTranscoder::sg_mux(canid_t message_id, const std::string& signal_name,
                            unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                            std::string unit, std::vector<size_t> receivers)
    {
        SignalDefinition mux_def;
        mux_def.name = signal_name;
        mux_def.codec = std::make_unique<SignalCodec>(start_bit, bit_size, byte_order, sign_type);
        mux_def.numeric_value = std::make_unique<NumericValue>(1.0, 0.0);
        mux_def.unit = unit;
        mux_def.is_multiplexer = true;

        messages[message_id].multiplexer = std::move(mux_def);
    }

    void CSVTranscoder::bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter) {
        messages[message_id].name = message_name;
        messages[message_id].size = message_size;
        messages[message_id].transmitter = transmitter;

        transcode("messages.csv", {
            {"message_id", std::to_string(message_id)},
            {"message_name", message_name},
            {"message_size", std::to_string(message_size)}
        });
    }

    void CSVTranscoder::sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type) {
        auto& msg = messages[message_id];
        for (auto& sig : msg.signals) {
            if (sig.name == signal_name) {
                sig.value_type = static_cast<NumericValueType>(value_type);
                break;
            }
        }
    }

    // Private Methods
    void CSVTranscoder::writer_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !task_queue.empty() || shutdown_requested;
            });

            while (!task_queue.empty()) {
                is_processing = true;
                CSVTask task = std::move(task_queue.front());
                task_queue.pop();
                lock.unlock();

                try {
                    task.operation();
                    tasks_processed++;
                    if (task.promise) task.promise->set_value();
                } catch (...) {
                    if (task.promise) task.promise->set_exception(std::current_exception());
                }

                lock.lock();
            }

            is_processing = false;

            if (shutdown_requested) {
                flush_all_batches();
                break;
            }
        }
    }

    void CSVTranscoder::enqueue_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task));
        queue_cv.notify_one();
    }

    void CSVTranscoder::enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task), std::move(promise));
        queue_cv.notify_one();
    }

    void CSVTranscoder::batch_frame(CANTime timestamp, CANFrame frame) {
        auto msg_it = messages.find(frame.can_id);
        std::string message_name = (msg_it != messages.end()) ? msg_it->second.name : "";

        std::string hex_data = format_hex_data(frame.data, frame.len);
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        std::ostringstream row;
        row << timestamp_ms << ","
            << frame.can_id << ","
            << static_cast<int>(frame.len) << ","
            << "\"" << hex_data << "\","
            << "\"" << escape_csv(message_name) << "\"";

        frames_batch.push_back(row.str());
        frames_batch_count++;
    }

    void CSVTranscoder::batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        std::optional<uint64_t> mux_value;
        if (msg_def.multiplexer.has_value()) {
            uint64_t raw_mux = (*msg_def.multiplexer->codec)(frame.data);
            mux_value = raw_mux;
        }

        for (const auto& signal : msg_def.signals) {
            if (signal.mux_val.has_value() && (!mux_value || mux_value.value() != signal.mux_val.value())) {
                continue;
            }

            try {
                uint64_t raw_value = (*signal.codec)(frame.data);
                auto converted_value = signal.numeric_value->convert(raw_value, signal.value_type);
                if (!converted_value.has_value()) continue;

                std::ostringstream row;
                row << timestamp_ms << ","
                    << frame.can_id << ","
                    << "\"" << escape_csv(msg_def.name) << "\","
                    << "\"" << escape_csv(signal.name) << "\","
                    << std::fixed << std::setprecision(6) << converted_value.value() << ","
                    << raw_value << ","
                    << "\"" << escape_csv(signal.unit) << "\","
                    << (mux_value ? std::to_string(mux_value.value()) : "");

                decoded_signals_batch.push_back(row.str());
                decoded_signals_batch_count++;
            } catch (...) {
                // Silent fail to continue processing other signals
            }
        }
    }

    void CSVTranscoder::flush_frames_batch() {
        if (frames_batch_count == 0) return;

        // Ensure frames CSV file exists with headers
        ensure_csv_file("frames.csv", {"timestamp", "can_id", "dlc", "data", "message_name"});

        // Write all batched frames
        for (const auto& row : frames_batch) {
            write_to_csv("frames.csv", row);
        }

        frames_batch.clear();
        frames_batch_count = 0;
    }

    void CSVTranscoder::flush_decoded_signals_batch() {
        if (decoded_signals_batch_count == 0) return;

        // Ensure decoded signals CSV file exists with headers
        ensure_csv_file("decoded_frames.csv", {"timestamp", "can_id", "message_name", "signal_name", "signal_value", "raw_value", "unit", "mux_value"});

        // Write all batched decoded signals
        for (const auto& row : decoded_signals_batch) {
            write_to_csv("decoded_frames.csv", row);
        }

        decoded_signals_batch.clear();
        decoded_signals_batch_count = 0;
    }

    void CSVTranscoder::flush_all_batches() {
        if (frames_batch_count > 0) flush_frames_batch();
        if (decoded_signals_batch_count > 0) flush_decoded_signals_batch();
        
        // Flush all open file streams
        for (auto& [filename, file] : csv_files) {
            if (file && file->is_open()) {
                file->flush();
            }
        }
    }

    std::string CSVTranscoder::build_csv_row(const std::vector<std::pair<std::string, std::string>>& data) {
        std::ostringstream row;
        for (size_t i = 0; i < data.size(); ++i) {
            row << "\"" << escape_csv(data[i].second) << "\"";
            if (i < data.size() - 1) {
                row << ",";
            }
        }
        return row.str();
    }

    void CSVTranscoder::ensure_csv_file(const std::string& filename, const std::vector<std::string>& headers) {
        if (csv_files.find(filename) == csv_files.end()) {
            std::string full_path = base_path + "/" + filename;
            
            // Check if file already exists and has content
            bool file_exists = std::filesystem::exists(full_path);
            bool file_has_content = false;
            
            if (file_exists) {
                std::ifstream check_file(full_path);
                std::string first_line;
                if (std::getline(check_file, first_line) && !first_line.empty()) {
                    file_has_content = true;
                }
                check_file.close();
            }
            
            // Open in append mode
            csv_files[filename] = std::make_unique<std::ofstream>(full_path, std::ios::app);
            
            if (!csv_files[filename]->is_open()) {
                throw std::runtime_error("Failed to open CSV file: " + full_path);
            }

            // Only write headers if file doesn't exist or is empty
            if (!file_has_content && headers_written.find(filename) == headers_written.end()) {
                std::ostringstream header_row;
                for (size_t i = 0; i < headers.size(); ++i) {
                    header_row << headers[i];
                    if (i < headers.size() - 1) {
                        header_row << ",";
                    }
                }
                *csv_files[filename] << header_row.str() << "\n";
                headers_written[filename] = true;
            } else {
                // Mark headers as already written for existing files
                headers_written[filename] = true;
            }
        }
    }

    void CSVTranscoder::write_to_csv(const std::string& filename, const std::string& row) {
        auto it = csv_files.find(filename);
        if (it != csv_files.end() && it->second && it->second->is_open()) {
            *it->second << row << "\n";
        } else {
            throw std::runtime_error("CSV file not open: " + filename);
        }
    }

    std::string CSVTranscoder::escape_csv(const std::string& input) {
        std::string escaped;
        for (char c : input) {
            if (c == '"') escaped += "\"\"";  // Escape quotes by doubling them
            else escaped += c;
        }
        return escaped;
    }

    std::string CSVTranscoder::format_hex_data(const uint8_t* data, size_t len) {
        std::ostringstream hex_stream;
        for (size_t i = 0; i < len; i++) {
            hex_stream << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<unsigned>(data[i]);
            if (i < len - 1) hex_stream << " ";
        }
        return hex_stream.str();
    }

} // namespace CAN