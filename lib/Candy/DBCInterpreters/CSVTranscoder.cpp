
#include <optional>
#include <filesystem>
#include <cstdio>
#include <algorithm>
#include <string_view>

#include "Candy/DBCInterpreters/CSVTranscoder.hpp"

namespace Candy {

    CSVTranscoder::CSVTranscoder(std::string_view base_path, 
                      size_t batch_size, 
                      CSVWriter<3> messages_csv,
                      CSVWriter<5> frames_csv,
                      CSVWriter<8> decoded_frames_csv,
                      CSVWriter<7> metadata_csv) : 
        FileTranscoder<CSVTranscoder>(batch_size, 0, 0),
        base_path(base_path),
        messages_csv(std::move(messages_csv)),
        frames_csv(std::move(frames_csv)),
        decoded_frames_csv(std::move(decoded_frames_csv)),
        metadata_csv(std::move(metadata_csv))
    {
        frames_batch.reserve(batch_size);
        decoded_signals_batch.reserve(batch_size);
    }

    CSVTranscoder::~CSVTranscoder() {
        flush_all_batches();
    }

    CSVTranscoder::CSVTranscoder(CSVTranscoder&& other) noexcept
        : FileTranscoder<CSVTranscoder>(std::move(other)),
          base_path(std::move(other.base_path)),
          messages_csv(std::move(other.messages_csv)),
          frames_csv(std::move(other.frames_csv)),
          decoded_frames_csv(std::move(other.decoded_frames_csv)),
          metadata_csv(std::move(other.metadata_csv)),
          headers_written(std::move(other.headers_written)),
          frames_batch(std::move(other.frames_batch)),
          decoded_signals_batch(std::move(other.decoded_signals_batch))
    {
    }

    CSVTranscoder& CSVTranscoder::operator=(CSVTranscoder&& other) noexcept {
        if (this != &other) {
            flush_all_batches();
            
            FileTranscoder<CSVTranscoder>::operator=(std::move(other));
            base_path = std::move(other.base_path);
            messages_csv = std::move(other.messages_csv);
            frames_csv = std::move(other.frames_csv);
            decoded_frames_csv = std::move(other.decoded_frames_csv);
            metadata_csv = std::move(other.metadata_csv);
            headers_written = std::move(other.headers_written);
            frames_batch = std::move(other.frames_batch);
            decoded_signals_batch = std::move(other.decoded_signals_batch);
        }
        return *this;
    }

    std::optional<CSVTranscoder> CSVTranscoder::create(std::string_view base_path, size_t batch_size) {
        std::filesystem::create_directories(base_path);
        
        CSVHeader<3> messages_header = {
            "messages.csv",
            {"message_id", "message_name", "message_size"}
        };

        CSVHeader<5> frames_header = {
            "frames.csv",
            {"timestamp", "can_id", "dlc", "data", "message_name"}
        };

        CSVHeader<8> decoded_frames_header = {
            "decoded_frames.csv",
            {"timestamp", "can_id", "message_name", "signal_name", "signal_value", "raw_value", "unit", "mux_value"}
        };

        CSVHeader<7> metadata_header = {
            "metadata.csv",
            {"stream_name", "description", "creation_time", "last_update", "total_messages", "message_names", "message_counts"}
        };
        
        std::optional<CSVWriter<3>> messages_csv = CSVWriter<3>::create(base_path, messages_header);
        std::optional<CSVWriter<5>> frames_csv = CSVWriter<5>::create(base_path, frames_header);    
        std::optional<CSVWriter<8>> decoded_frames_csv = CSVWriter<8>::create(base_path, decoded_frames_header);
        std::optional<CSVWriter<7>> metadata_csv = CSVWriter<7>::create(base_path, metadata_header);

        if (!messages_csv.has_value() || !frames_csv.has_value() ||
            !decoded_frames_csv.has_value() || !metadata_csv.has_value()) {
            return std::nullopt;
        }

        return std::make_optional<CSVTranscoder>(base_path,
                                                 batch_size, 
                                                 std::move(messages_csv.value()), 
                                                 std::move(frames_csv.value()), 
                                                 std::move(decoded_frames_csv.value()), 
                                                 std::move(metadata_csv.value()));
    }

    // public Methods
    void CSVTranscoder::receive_raw_message(std::pair<CANTime, CANFrame> sample) {
        batch_frame(sample);

        auto msg_it = messages.find(sample.second.can_id);
        if (msg_it != messages.end()) {
            batch_decoded_signals(sample, msg_it->second);
        }

        if (frames_batch_count >= batch_size) {
            flush_frames_batch();
        }
        if (decoded_signals_batch_count >= batch_size) {
            flush_decoded_signals_batch();
        }
    }

    void CSVTranscoder::store_message_metadata(canid_t message_id, const std::string& message_name, size_t message_size) {
        messages_csv.start_row();
        messages_csv.field(std::to_string(message_id));
        messages_csv.field(message_name);
        messages_csv.field(std::to_string(message_size));
        messages_csv.end_row();
        messages_csv.flush();
    }

    // protected Methods
    void CSVTranscoder::batch_frame(std::pair<CANTime, CANFrame> sample) {
        frames_batch.push_back(sample);
        frames_batch_count++;
    }

    void CSVTranscoder::batch_decoded_signals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sample.first.time_since_epoch()).count();

        std::optional<uint64_t> mux_value;
        if (msg_def.multiplexer.has_value()) {
            uint64_t raw_mux = (*msg_def.multiplexer->codec)(sample.second.data);
            mux_value = raw_mux;
        }

        for (size_t i = 0; i < msg_def.signal_count && i < msg_def.signals.size(); ++i) {
            const auto& signal = msg_def.signals[i];
            
            // Skip signals with null pointers
            if (!signal.codec || !signal.numeric_value) {
                continue;
            }
            
            if (signal.mux_val.has_value() && (!mux_value || mux_value.value() != signal.mux_val.value())) {
                continue;
            }

            uint64_t raw_value = (*signal.codec)(sample.second.data);
            auto converted_value = signal.numeric_value->convert(raw_value, signal.value_type);
            if (!converted_value.has_value()) continue;

            DecodedSignalBatchEntry entry;
            entry.timestamp = sample.first;
            entry.can_id = sample.second.can_id;
            
            entry.message_name.fill(0);
            entry.signal_name.fill(0);
            entry.unit.fill(0);
            
            auto msg_name = msg_def.get_name();
            size_t msg_len = msg_name.length() < entry.message_name.size() - 1 ? msg_name.length() : entry.message_name.size() - 1;
            std::copy_n(msg_name.begin(), msg_len, entry.message_name.begin());
            
            auto sig_name = signal.get_name();
            size_t sig_len = sig_name.length() < entry.signal_name.size() - 1 ? sig_name.length() : entry.signal_name.size() - 1;
            std::copy_n(sig_name.begin(), sig_len, entry.signal_name.begin());
            
            auto unit_name = signal.get_unit();
            size_t unit_len = unit_name.length() < entry.unit.size() - 1 ? unit_name.length() : entry.unit.size() - 1;
            std::copy_n(unit_name.begin(), unit_len, entry.unit.begin());
            
            entry.signal_value = converted_value.value();
            entry.raw_value = raw_value;
            entry.mux_value = mux_value;
            decoded_signals_batch.emplace_back(std::move(entry));
            decoded_signals_batch_count++;
        }
    }

    void CSVTranscoder::flush_frames_batch() {
        if (frames_batch_count == 0) return;

        for (const auto& [timestamp, frame] : frames_batch) {
            auto msg_it = messages.find(frame.can_id);
            std::string message_name = (msg_it != messages.end()) ? std::string(msg_it->second.get_name()) : "";
            
            auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
            std::string hex_data = format_hex_data(frame.data, frame.len);

            frames_csv.start_row();
            frames_csv.field(std::to_string(timestamp_ms));
            frames_csv.field(std::to_string(frame.can_id));
            frames_csv.field(std::to_string(static_cast<int>(frame.len)));
            frames_csv.field(hex_data);
            frames_csv.field(message_name);
            frames_csv.end_row();
        }

        frames_csv.flush();
        frames_batch.clear();
        frames_batch_count = 0;
    }

    void CSVTranscoder::flush_decoded_signals_batch() {
        if (decoded_signals_batch_count == 0) return;
        
        for (const auto& entry : decoded_signals_batch) {
            auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count();
            
            std::array<char, 32> value_buf;
            snprintf(value_buf.data(), value_buf.size(), "%.6f", entry.signal_value);

            decoded_frames_csv.start_row();
            decoded_frames_csv.field(std::to_string(timestamp_ms));
            decoded_frames_csv.field(std::to_string(entry.can_id));
            decoded_frames_csv.field(std::string_view(entry.message_name.data(), strnlen(entry.message_name.data(), entry.message_name.size())));
            decoded_frames_csv.field(std::string_view(entry.signal_name.data(), strnlen(entry.signal_name.data(), entry.signal_name.size())));
            decoded_frames_csv.field(value_buf.data());
            decoded_frames_csv.field(std::to_string(entry.raw_value));
            decoded_frames_csv.field(std::string_view(entry.unit.data(), strnlen(entry.unit.data(), entry.unit.size())));
            decoded_frames_csv.field(entry.mux_value ? std::to_string(*entry.mux_value) : "");
            decoded_frames_csv.end_row();
        }

        decoded_frames_csv.flush();
        decoded_signals_batch.clear();
        decoded_signals_batch_count = 0;
    }

    void CSVTranscoder::flush_all_batches() {
        if (frames_batch_count > 0) flush_frames_batch();
        if (decoded_signals_batch_count > 0) flush_decoded_signals_batch();
        
        messages_csv.flush();
        frames_csv.flush();
        decoded_frames_csv.flush();
        metadata_csv.flush();
    }

    std::string CSVTranscoder::format_hex_data(const uint8_t* data, size_t len) {
        std::string result;
        result.reserve(len * 3);
        for (size_t i = 0; i < len; i++) {
            std::array<char, 4> hex_buf;
            std::snprintf(hex_buf.data(), hex_buf.size(), "%02X", static_cast<unsigned>(data[i]));
            result += std::string(hex_buf.data());
            if (i < len - 1) result += " ";
        }
        return result;
    }

    //CANIO
    void CSVTranscoder::receive_message(const CANMessage& message) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.sample.first.time_since_epoch()).count();
        
        // Write raw frame using existing transcoder
        receive_raw_message(message.sample);
        
        // Write decoded signals if available
        if (!message.decoded_signals.empty()) {

            for (size_t i = 0; i < message.signal_count && i < message.decoded_signals.size(); ++i) {
                const auto& signal_entry = message.decoded_signals[i];
                if (!signal_entry.is_valid) continue;
                
                std::string signal_name = std::string(signal_entry.get_name());
                double signal_value = signal_entry.value;
                std::string unit = std::string(signal_entry.get_unit());
                
                std::array<char, 32> value_buf;
                snprintf(value_buf.data(), value_buf.size(), "%.6f", signal_value);
                
                decoded_frames_csv.start_row();
                decoded_frames_csv.field(std::to_string(timestamp_ms));
                decoded_frames_csv.field(std::to_string(message.sample.second.can_id));
                decoded_frames_csv.field(message.get_message_name());
                decoded_frames_csv.field(signal_name);
                decoded_frames_csv.field(std::string_view(value_buf.data()));
                decoded_frames_csv.field("0"); // raw_value not available
                decoded_frames_csv.field(unit);
                decoded_frames_csv.field(message.mux_value ? std::to_string(*message.mux_value) : "");
                decoded_frames_csv.end_row();
            }
            
            decoded_frames_csv.flush();
        }
    }

    void CSVTranscoder::receive_metadata(const CANDataStreamMetadata& metadata) {
        auto creation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.creation_time.time_since_epoch()).count();
        auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.last_update.time_since_epoch()).count();
        
        // Serialize message names and counts
        std::string names_str, counts_str;
        names_str.reserve(512);
        counts_str.reserve(512);
        
        for (size_t i = 0; i < metadata.message_count && i < metadata.messages.size(); ++i) {
            const auto& msg_entry = metadata.messages[i];
            if (!msg_entry.is_valid) continue;
            names_str += std::to_string(msg_entry.can_id) + ":" + std::string(msg_entry.get_name()) + ";";
            counts_str += std::to_string(msg_entry.can_id) + ":" + std::to_string(msg_entry.count) + ";";
        }
        
        metadata_csv.start_row();
        metadata_csv.field(metadata.get_stream_name());
        metadata_csv.field(metadata.get_description());
        metadata_csv.field(std::to_string(creation_ms));
        metadata_csv.field(std::to_string(update_ms));
        metadata_csv.field(std::to_string(metadata.total_messages));
        metadata_csv.field(names_str);
        metadata_csv.field(counts_str);
        metadata_csv.end_row();
        metadata_csv.flush();
    }

    std::vector<CANMessage> CSVTranscoder::transmit_messages(canid_t can_id) {
        return transmit_messages_in_range(can_id,
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max());
    }

    std::vector<CANMessage> CSVTranscoder::transmit_messages_in_range(
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
        
        FILE* frames_file = fopen(frames_path.c_str(), "r");
        if (!frames_file) {
            return messages;
        }
        
        // Skip header line
        std::array<char, 2048> line_buf;
        if (!fgets(line_buf.data(), line_buf.size(), frames_file)) {
            fclose(frames_file);
            return messages;
        }
        
        // Parse frames
        std::unordered_map<std::string, CANMessage> message_map; // timestamp+can_id -> message
        
        while (fgets(line_buf.data(), line_buf.size(), frames_file)) {
            std::string line(line_buf.data());
            // Remove trailing newline
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto fields = parse_csv_line(line);
            if (fields.size() < 5) continue;
            

            auto timestamp_ms = std::stoll(fields[0]);
            auto frame_can_id = static_cast<canid_t>(std::stoul(fields[1]));
            
            if (frame_can_id != can_id) continue;
            if (timestamp_ms < start_ms || timestamp_ms > end_ms) continue;
            
            CANMessage message;
            message.sample.first = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(timestamp_ms));
            message.sample.second.can_id = frame_can_id;
            message.sample.second.len = static_cast<uint8_t>(std::stoi(fields[2]));
            
            parse_hex_data(fields[3], message.sample.second.data, message.sample.second.len);
            
            message.set_message_name(fields[4]);
            
            std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(frame_can_id);
            message_map[key] = std::move(message);
        }

        fclose(frames_file);
        
        // Read decoded signals
        std::string decoded_path = base_path + "/decoded_frames.csv";
        if (std::filesystem::exists(decoded_path)) {
            FILE* decoded_file = fopen(decoded_path.c_str(), "r");
            if (decoded_file) {
                // Skip header
                std::array<char, 2048> line_buf;
                if (fgets(line_buf.begin(), line_buf.size(), decoded_file)) {
                while (fgets(line_buf.begin(), line_buf.size(), decoded_file)) {

                    std::string line(line_buf.data());

                    if (!line.empty() && line.back() == '\n') line.pop_back();
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    auto fields = parse_csv_line(line);
                    if (fields.size() < 8) continue;
                    
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
                        
                        it->second.add_signal(signal_name, signal_value, unit);
                        
                        if (!fields[7].empty()) {
                            it->second.mux_value = std::stoull(fields[7]);
                        }
                    }
                }
            }
                fclose(decoded_file);
            }
        }
        
        // Convert map to vector and sort by timestamp
        messages.reserve(message_map.size());
        for (auto& [_, message] : message_map) {
            messages.push_back(std::move(message));
        }
        
        std::sort(messages.begin(), messages.end(), [](const CANMessage& a, const CANMessage& b) {
            return a.sample.first < b.sample.first;
        });
        
        return messages;
    }

    const CANDataStreamMetadata& CSVTranscoder::transmit_metadata() {
        
        std::string meta_path = base_path + metadata_csv.get_header().filename;

        if (!std::filesystem::exists(meta_path)) { // set defaults
            metadata.set_stream_name("Unknown");
            metadata.creation_time = std::chrono::system_clock::now();
            metadata.last_update = metadata.creation_time;
            return metadata;
        }
        
        FILE* meta_file = fopen(meta_path.c_str(), "r");
        if (!meta_file) return metadata;
        std::array<char, 2048> line_buf;
        
        // Skip header
        if (!fgets(line_buf.data(), line_buf.size(), meta_file)) {
            fclose(meta_file);
            return metadata;
        }
        
        // Read metadata line
        if (fgets(line_buf.data(), line_buf.size(), meta_file)) {
            std::string line(line_buf.data());
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto fields = parse_csv_line(line);
            if (fields.size() >= 7) {
                metadata.set_stream_name(fields[0]);
                metadata.set_description(fields[1]);
                
                auto creation_ms = std::stoll(fields[2]);
                auto update_ms = std::stoll(fields[3]);
                
                metadata.creation_time = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(creation_ms)
                );
                metadata.last_update = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(update_ms)
                );
                
                metadata.total_messages = std::stoull(fields[4]);
                
                parse_serialized_data(fields[5], metadata);
                parse_serialized_counts(fields[6], metadata);
            }
        }
        
        fclose(meta_file);
        return metadata;
    }


    //CANIO Helpers

    std::vector<std::string> CSVTranscoder::parse_csv_line(const std::string& line) {
        
        std::vector<std::string> fields;
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

        fields.push_back(current_field);
        
        return fields;
    }

    void CSVTranscoder::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
        size_t pos = 0;
        size_t i = 0;
        
        while (pos < hex_str.length() && i < len && i < 8) {
            // Skip spaces
            while (pos < hex_str.length() && hex_str[pos] == ' ') pos++;
            if (pos >= hex_str.length()) break;
            
            // Find end of hex byte (2 chars or space/end)
            size_t end_pos = pos;
            while (end_pos < hex_str.length() && hex_str[end_pos] != ' ' && (end_pos - pos) < 2) {
                end_pos++;
            }
            
            if (end_pos > pos) {
                std::string byte_str = hex_str.substr(pos, end_pos - pos);
                data[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
                ++i;
                pos = end_pos;
            } else {
                break;
            }
        }
    }

    void CSVTranscoder::parse_serialized_data(const std::string& data_str,
                                        CANDataStreamMetadata& metadata) {
        // Parse "can_id:name;" format
        size_t pos = 0;
        while (pos < data_str.length() && metadata.message_count < metadata.messages.size()) {
            size_t semicolon_pos = data_str.find(';', pos);
            if (semicolon_pos == std::string::npos) break;
            
            std::string pair = data_str.substr(pos, semicolon_pos - pos);
            if (!pair.empty()) {
                auto colon_pos = pair.find(':');
                if (colon_pos != std::string::npos) {
                    std::string id_str = pair.substr(0, colon_pos);
                    std::string name = pair.substr(colon_pos + 1);
                    
                    char* endptr = nullptr;
                    canid_t can_id = static_cast<canid_t>(std::strtoul(id_str.c_str(), &endptr, 10));
                    if (endptr != id_str.c_str()) {
                        // Find or create message entry
                        size_t idx = metadata.message_count;
                        for (size_t i = 0; i < metadata.message_count; ++i) {
                            if (metadata.messages[i].can_id == can_id) {
                                idx = i;
                                break;
                            }
                        }
                        
                        if (idx == metadata.message_count) {
                            metadata.messages[idx].can_id = can_id;
                            size_t copy_len = name.length() < metadata.messages[idx].name.size() - 1 
                                ? name.length() 
                                : metadata.messages[idx].name.size() - 1;
                            std::copy_n(name.begin(), copy_len, metadata.messages[idx].name.begin());
                            metadata.messages[idx].name[copy_len] = '\0';
                            metadata.messages[idx].is_valid = true;
                            metadata.message_count++;
                        }
                    }
                }
            }
            pos = semicolon_pos + 1;
        }
    }

    void CSVTranscoder::parse_serialized_counts(const std::string& counts_str,
                                            CANDataStreamMetadata& metadata) {
        // Parse "can_id:count;" format
        size_t pos = 0;
        while (pos < counts_str.length()) {
            size_t semicolon_pos = counts_str.find(';', pos);
            if (semicolon_pos == std::string::npos) break;
            
            std::string pair = counts_str.substr(pos, semicolon_pos - pos);
            if (!pair.empty()) {
                auto colon_pos = pair.find(':');
                if (colon_pos != std::string::npos) {
                    std::string id_str = pair.substr(0, colon_pos);
                    std::string count_str = pair.substr(colon_pos + 1);
                    
                    char* id_endptr = nullptr;
                    char* count_endptr = nullptr;
                    canid_t can_id = static_cast<canid_t>(std::strtoul(id_str.c_str(), &id_endptr, 10));
                    size_t count = static_cast<size_t>(std::strtoull(count_str.c_str(), &count_endptr, 10));
                    
                    if (id_endptr != id_str.c_str() && count_endptr != count_str.c_str()) {
                        // Find existing message entry and update count
                        for (size_t i = 0; i < metadata.message_count; ++i) {
                            if (metadata.messages[i].can_id == can_id) {
                                metadata.messages[i].count = count;
                                break;
                            }
                        }
                    }
                }
            }
            pos = semicolon_pos + 1;
        }
    }

}