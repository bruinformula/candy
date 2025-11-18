#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <filesystem>

#include "Candy/Interpreters/CSVTranscoder.hpp"

namespace Candy {

    CSVTranscoder::CSVTranscoder(const std::string& base_path, size_t batch_size) : 
        FileTranscoder<CSVTranscoder, CSVTask>(false, batch_size, 0, 0),
        base_path(base_path)
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

    // protected Methods
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

    //CANIO
    void CSVTranscoder::write_message(const CANMessage& message) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        
        // Write raw frame using existing transcoder
        transcode(message.timestamp, message.frame);
        
        // Write decoded signals if available
        if (!message.decoded_signals.empty()) {
            for (const auto& [signal_name, signal_value] : message.decoded_signals) {
                std::string unit = "";
                auto unit_it = message.signal_units.find(signal_name);
                if (unit_it != message.signal_units.end()) {
                    unit = unit_it->second;
                }
                
                std::vector<std::pair<std::string, std::string>> signal_data = {
                    {"timestamp", std::to_string(timestamp_ms)},
                    {"can_id", std::to_string(message.frame.can_id)},
                    {"message_name", message.message_name},
                    {"signal_name", signal_name},
                    {"signal_value", std::to_string(signal_value)},
                    {"raw_value", "0"},
                    {"unit", unit},
                    {"mux_value", message.mux_value ? std::to_string(*message.mux_value) : ""}
                };
                
                transcode("decoded_frames.csv", signal_data);
            }
        }
    }

    void CSVTranscoder::write_metadata(const CANDataStreamMetadata& metadata) {
        auto creation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.creation_time.time_since_epoch()).count();
        auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.last_update.time_since_epoch()).count();
        
        // Serialize message names and counts
        std::ostringstream names_str, counts_str;
        
        for (const auto& [can_id, name] : metadata.message_names) {
            names_str << can_id << ":" << name << ";";
        }
        
        for (const auto& [can_id, count] : metadata.message_counts) {
            counts_str << can_id << ":" << count << ";";
        }
        
        std::vector<std::pair<std::string, std::string>> meta_data = {
            {"stream_name", metadata.stream_name},
            {"description", metadata.description},
            {"creation_time", std::to_string(creation_ms)},
            {"last_update", std::to_string(update_ms)},
            {"total_messages", std::to_string(metadata.total_messages)},
            {"message_names", names_str.str()},
            {"message_counts", counts_str.str()}
        };
        
        transcode("metadata.csv", meta_data);
    }

    std::vector<CANMessage> CSVTranscoder::read_messages(canid_t can_id) {
        return read_messages_in_range(can_id,
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max());
    }

    std::vector<CANMessage> CSVTranscoder::read_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) {
        
        std::vector<CANMessage> messages;
        
        auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            start.time_since_epoch()).count();
        auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end.time_since_epoch()).count();
        
        // Read frames from CSV
        std::string frames_path = base_path + "/frames.csv";
        if (!std::filesystem::exists(frames_path)) {
            return messages; // Return empty if file doesn't exist
        }
        
        std::ifstream frames_file(frames_path);
        std::string line;
        
        // Skip header line
        if (!std::getline(frames_file, line)) {
            return messages;
        }
        
        // Parse frames
        std::unordered_map<std::string, CANMessage> message_map; // timestamp+can_id -> message
        
        while (std::getline(frames_file, line)) {
            auto fields = parse_csv_line(line);
            if (fields.size() < 5) continue;
            
            try {
                auto timestamp_ms = std::stoll(fields[0]);
                auto frame_can_id = static_cast<canid_t>(std::stoul(fields[1]));
                
                if (frame_can_id != can_id) continue;
                if (timestamp_ms < start_ms || timestamp_ms > end_ms) continue;
                
                CANMessage message;
                message.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp_ms));
                message.frame.can_id = frame_can_id;
                message.frame.len = static_cast<uint8_t>(std::stoi(fields[2]));
                
                // Parse hex data
                parse_hex_data(fields[3], message.frame.data, message.frame.len);
                
                // Get message name
                message.message_name = fields[4];
                
                std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(frame_can_id);
                message_map[key] = std::move(message);
                
            } catch (const std::exception&) {
                continue; // Skip malformed lines
            }
        }
        frames_file.close();
        
        // Read decoded signals
        std::string decoded_path = base_path + "/decoded_frames.csv";
        if (std::filesystem::exists(decoded_path)) {
            std::ifstream decoded_file(decoded_path);
            
            // Skip header
            if (std::getline(decoded_file, line)) {
                while (std::getline(decoded_file, line)) {
                    auto fields = parse_csv_line(line);
                    if (fields.size() < 8) continue;
                    
                    try {
                        auto timestamp_ms = std::stoll(fields[0]);
                        auto frame_can_id = static_cast<canid_t>(std::stoul(fields[1]));
                        
                        if (frame_can_id != can_id) continue;
                        if (timestamp_ms < start_ms || timestamp_ms > end_ms) continue;
                        
                        std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(frame_can_id);
                        auto it = message_map.find(key);
                        if (it != message_map.end()) {
                            std::string signal_name = fields[3];
                            double signal_value = std::stod(fields[4]);
                            std::string unit = fields[6];
                            
                            it->second.decoded_signals[signal_name] = signal_value;
                            it->second.signal_units[signal_name] = unit;
                            
                            if (!fields[7].empty()) {
                                it->second.mux_value = std::stoull(fields[7]);
                            }
                        }
                    } catch (const std::exception&) {
                        continue; // Skip malformed lines
                    }
                }
            }
            decoded_file.close();
        }
        
        // Convert map to vector and sort by timestamp
        messages.reserve(message_map.size());
        for (auto& [_, message] : message_map) {
            messages.push_back(std::move(message));
        }
        
        std::ranges::sort(messages, [](const CANMessage& a, const CANMessage& b) {
            return a.timestamp < b.timestamp;
        });
        
        return messages;
    }

    CANDataStreamMetadata CSVTranscoder::read_metadata() {
        CANDataStreamMetadata metadata;
        
        std::string meta_path = base_path + "/metadata.csv";
        if (!std::filesystem::exists(meta_path)) {
            // Set defaults if metadata file doesn't exist
            metadata.stream_name = "Unknown";
            metadata.creation_time = std::chrono::system_clock::now();
            metadata.last_update = metadata.creation_time;
            return metadata;
        }
        
        std::ifstream meta_file(meta_path);
        std::string line;
        
        // Skip header
        if (!std::getline(meta_file, line)) {
            return metadata;
        }
        
        // Read metadata line
        if (std::getline(meta_file, line)) {
            auto fields = parse_csv_line(line);
            if (fields.size() >= 7) {
                try {
                    metadata.stream_name = fields[0];
                    metadata.description = fields[1];
                    
                    auto creation_ms = std::stoll(fields[2]);
                    auto update_ms = std::stoll(fields[3]);
                    
                    metadata.creation_time = std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(creation_ms));
                    metadata.last_update = std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(update_ms));
                    
                    metadata.total_messages = std::stoull(fields[4]);
                    
                    // Parse serialized message names and counts
                    parse_serialized_data(fields[5], metadata.message_names);
                    parse_serialized_counts(fields[6], metadata.message_counts);
                    
                } catch (const std::exception&) {
                    // Use defaults on parse error
                }
            }
        }
        
        meta_file.close();
        return metadata;
    }


    //CANIO Helpers
    void CSVTranscoder::ensure_metadata_file() {
        std::string meta_path = base_path + "/metadata.csv";
        if (!std::filesystem::exists(meta_path)) {
            std::ofstream meta_file(meta_path);
            meta_file << "stream_name,description,creation_time,last_update,total_messages,message_names,message_counts\n";
            meta_file.close();
        }
    }

    std::vector<std::string> CSVTranscoder::parse_csv_line(const std::string& line) {
        std::vector<std::string> fields;
        std::istringstream stream(line);
        std::string field;
        bool in_quotes = false;
        std::string current_field;
        
        for (char c : line) {
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                fields.push_back(current_field);
                current_field.clear();
            } else {
                current_field += c;
            }
        }
        fields.push_back(current_field); // Add last field
        
        return fields;
    }

    void CSVTranscoder::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
        std::istringstream hex_stream(hex_str);
        std::string byte_str;
        size_t i = 0;
        
        while (std::getline(hex_stream, byte_str, ' ') && i < len && i < 8) {
            try {
                data[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
                ++i;
            } catch (const std::exception&) {
                break;
            }
        }
    }

    void CSVTranscoder::parse_serialized_data(const std::string& data_str,
                                        std::unordered_map<canid_t, std::string>& names) {
        // Parse "can_id:name;" format
        std::istringstream stream(data_str);
        std::string pair;
        
        while (std::getline(stream, pair, ';')) {
            if (pair.empty()) continue;
            
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos) {
                try {
                    canid_t can_id = std::stoul(pair.substr(0, colon_pos));
                    std::string name = pair.substr(colon_pos + 1);
                    names[can_id] = name;
                } catch (const std::exception&) {
                    continue;
                }
            }
        }
    }

    void CSVTranscoder::parse_serialized_counts(const std::string& counts_str,
                                            std::unordered_map<canid_t, size_t>& counts) {
        // Parse "can_id:count;" format
        std::istringstream stream(counts_str);
        std::string pair;
        
        while (std::getline(stream, pair, ';')) {
            if (pair.empty()) continue;
            
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos) {
                try {
                    canid_t can_id = std::stoul(pair.substr(0, colon_pos));
                    size_t count = std::stoull(pair.substr(colon_pos + 1));
                    counts[can_id] = count;
                } catch (const std::exception&) {
                    continue;
                }
            }
        }
    }

}