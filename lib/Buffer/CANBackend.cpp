
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include "Candy/Buffer/BackendInterface.hpp"
#include "Candy/Buffer/CANDataBuffer.hpp"
#include "Candy/Buffer/CANDataBuffer.hpp"

#include "Candy/Interpreters/SQLTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"
#include "Candy/Interpreters/SQLTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"

namespace Candy {

    // SQLBackend Implementation

    SQLBackend::SQLBackend(const std::string& database_path, size_t batch_size) 
        : db_path(database_path) {
        transcoder = std::make_unique<SQLTranscoder>(database_path, batch_size);
        
        // Create metadata table if it doesn't exist
        create_metadata_table();
    }

    void SQLBackend::write_message(const CANMessage& message) {
        // Convert timestamp to milliseconds
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        
        // Use existing transcoder to write the raw frame
        transcoder->transcode(message.timestamp, message.frame);
        
        // Write decoded signals if available
        if (!message.decoded_signals.empty()) {
            for (const auto& [signal_name, signal_value] : message.decoded_signals) {
                std::string unit = "";
                auto unit_it = message.signal_units.find(signal_name);
                if (unit_it != message.signal_units.end()) {
                    unit = unit_it->second;
                }
                
                // Build data for decoded signals
                std::vector<std::pair<std::string, std::string>> signal_data = {
                    {"timestamp", std::to_string(timestamp_ms)},
                    {"can_id", std::to_string(message.frame.can_id)},
                    {"message_name", message.message_name},
                    {"signal_name", signal_name},
                    {"signal_value", std::to_string(signal_value)},
                    {"raw_value", "0"}, // We don't have raw value in this context
                    {"unit", unit},
                    {"mux_value", message.mux_value ? std::to_string(*message.mux_value) : ""}
                };
                
                transcoder->transcode("decoded_frames", signal_data);
            }
        }
    }

    void SQLBackend::write_metadata(const CANDataStreamMetadata& metadata) {
        auto creation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.creation_time.time_since_epoch()).count();
        auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.last_update.time_since_epoch()).count();
        
        // Serialize message names and counts as JSON-like strings
        std::ostringstream names_json, counts_json;
        
        names_json << "{";
        bool first = true;
        for (const auto& [can_id, name] : metadata.message_names) {
            if (!first) names_json << ",";
            names_json << "\"" << can_id << "\":\"" << name << "\"";
            first = false;
        }
        names_json << "}";
        
        counts_json << "{";
        first = true;
        for (const auto& [can_id, count] : metadata.message_counts) {
            if (!first) counts_json << ",";
            counts_json << "\"" << can_id << "\":" << count;
            first = false;
        }
        counts_json << "}";
        
        std::vector<std::pair<std::string, std::string>> meta_data = {
            {"stream_name", metadata.stream_name},
            {"description", metadata.description},
            {"creation_time", std::to_string(creation_ms)},
            {"last_update", std::to_string(update_ms)},
            {"total_messages", std::to_string(metadata.total_messages)},
            {"message_names", names_json.str()},
            {"message_counts", counts_json.str()}
        };
        
        // Clear existing metadata and insert new
        transcoder->transcode("DELETE FROM metadata", {});
        transcoder->transcode("metadata", meta_data);
    }

    void SQLBackend::flush() {
        transcoder->flush();
    }

    void SQLBackend::flush_sync() {
        transcoder->flush_sync();
    }

    std::future<void> SQLBackend::flush_async() {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        transcoder->flush_async([promise]() {
            promise->set_value();
        });
        
        return future;
    }

    std::vector<CANMessage> SQLBackend::read_messages(canid_t can_id) {
        return read_messages_in_range(can_id, 
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max());
    }

    std::vector<CANMessage> SQLBackend::read_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) {
        
        std::vector<CANMessage> messages;
        
        // Open database connection for reading
        sqlite3* db;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database for reading");
        }
        
        auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            start.time_since_epoch()).count();
        auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end.time_since_epoch()).count();
        
        // Query frames
        std::string frames_sql = 
            "SELECT timestamp, can_id, dlc, data, message_name FROM frames "
            "WHERE can_id = ? AND timestamp >= ? AND timestamp <= ? "
            "ORDER BY timestamp";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, frames_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, can_id);
            sqlite3_bind_int64(stmt, 2, start_ms);
            sqlite3_bind_int64(stmt, 3, end_ms);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                CANMessage message;
                
                // Parse timestamp
                auto timestamp_ms = sqlite3_column_int64(stmt, 0);
                message.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp_ms));
                
                // Parse frame data
                message.frame.can_id = sqlite3_column_int(stmt, 1);
                message.frame.len = sqlite3_column_int(stmt, 2);
                
                // Parse hex data
                const char* hex_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                if (hex_data) {
                    parse_hex_data(hex_data, message.frame.data, message.frame.len);
                }
                
                // Get message name
                const char* msg_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (msg_name) {
                    message.message_name = msg_name;
                }
                
                messages.push_back(std::move(message));
            }
            
            sqlite3_finalize(stmt);
        }
        
        // Query decoded signals for these messages
        load_decoded_signals_for_messages(db, messages);
        
        sqlite3_close(db);
        return messages;
    }

    CANDataStreamMetadata SQLBackend::read_metadata() {
        CANDataStreamMetadata metadata;
        
        sqlite3* db;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database for reading metadata");
        }
        
        const char* meta_sql = "SELECT * FROM metadata LIMIT 1";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, meta_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                // Parse metadata fields
                const char* stream_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                if (stream_name) metadata.stream_name = stream_name;
                if (description) metadata.description = description;
                
                auto creation_ms = sqlite3_column_int64(stmt, 3);
                auto update_ms = sqlite3_column_int64(stmt, 4);
                
                metadata.creation_time = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(creation_ms));
                metadata.last_update = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(update_ms));
                
                metadata.total_messages = sqlite3_column_int64(stmt, 5);
                
                // Parse JSON-like strings for message names and counts
                const char* names_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                const char* counts_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                
                if (names_json) parse_message_names_json(names_json, metadata.message_names);
                if (counts_json) parse_message_counts_json(counts_json, metadata.message_counts);
            }
            sqlite3_finalize(stmt);
        }
        
        sqlite3_close(db);
        return metadata;
    }

    // CSVBackend Implementation

    CSVBackend::CSVBackend(const std::string& csv_base_path, size_t batch_size) 
        : base_path(csv_base_path) {
        transcoder = std::make_unique<CSVTranscoder>(csv_base_path, batch_size);
        
        // Ensure metadata file exists
        ensure_metadata_file();
    }

    void CSVBackend::write_message(const CANMessage& message) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        
        // Write raw frame using existing transcoder
        transcoder->transcode(message.timestamp, message.frame);
        
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
                
                transcoder->transcode("decoded_frames.csv", signal_data);
            }
        }
    }

    void CSVBackend::write_metadata(const CANDataStreamMetadata& metadata) {
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
        
        transcoder->transcode("metadata.csv", meta_data);
    }

    void CSVBackend::flush() {
        transcoder->flush();
    }

    void CSVBackend::flush_sync() {
        transcoder->flush_sync();
    }

    std::future<void> CSVBackend::flush_async() {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        transcoder->flush_async([promise]() {
            promise->set_value();
        });
        
        return future;
    }

    std::vector<CANMessage> CSVBackend::read_messages(canid_t can_id) {
        return read_messages_in_range(can_id,
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max());
    }

    std::vector<CANMessage> CSVBackend::read_messages_in_range(
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

    CANDataStreamMetadata CSVBackend::read_metadata() {
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

    // Helper methods for SQLBackend

    void SQLBackend::create_metadata_table() {
        const char* create_metadata_sql = R"(
            CREATE TABLE IF NOT EXISTS metadata (
                id INTEGER PRIMARY KEY,
                stream_name TEXT,
                description TEXT,
                creation_time INTEGER,
                last_update INTEGER,
                total_messages INTEGER,
                message_names TEXT,
                message_counts TEXT
            );
        )";
        
        // Access the underlying database connection from transcoder
        // This would require exposing a method in SQLTranscoder to execute custom SQL
        // For now, we'll assume such a method exists or can be added
    }

    void SQLBackend::load_decoded_signals_for_messages(sqlite3* db, std::vector<CANMessage>& messages) {
        if (messages.empty()) return;
        
        // Build a map for quick lookup
        std::unordered_map<std::string, size_t> message_lookup;
        for (size_t i = 0; i < messages.size(); ++i) {
            auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                messages[i].timestamp.time_since_epoch()).count();
            std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(messages[i].frame.can_id);
            message_lookup[key] = i;
        }
        
        // Query decoded signals
        const char* signals_sql = 
            "SELECT timestamp, can_id, signal_name, signal_value, unit, mux_value "
            "FROM decoded_frames WHERE can_id = ? "
            "ORDER BY timestamp";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, signals_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, messages[0].frame.can_id);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto timestamp_ms = sqlite3_column_int64(stmt, 0);
                auto can_id = sqlite3_column_int(stmt, 1);
                
                std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(can_id);
                auto it = message_lookup.find(key);
                if (it != message_lookup.end()) {
                    size_t msg_idx = it->second;
                    
                    const char* signal_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    double signal_value = sqlite3_column_double(stmt, 3);
                    const char* unit = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    
                    if (signal_name) {
                        messages[msg_idx].decoded_signals[signal_name] = signal_value;
                    }
                    if (unit) {
                        messages[msg_idx].signal_units[signal_name] = unit;
                    }
                    
                    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                        messages[msg_idx].mux_value = sqlite3_column_int64(stmt, 5);
                    }
                }
            }
            
            sqlite3_finalize(stmt);
        }
    }

    void SQLBackend::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
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

    void SQLBackend::parse_message_names_json(const std::string& json_str, 
                                            std::unordered_map<canid_t, std::string>& names) {
        // Simple JSON-like parsing for "can_id":"name" pairs
        // This is a simplified parser - in production you'd want a proper JSON library
        std::string content = json_str;
        if (content.front() == '{') content = content.substr(1);
        if (content.back() == '}') content = content.substr(0, content.length()-1);
        
        std::istringstream stream(content);
        std::string pair;
        
        while (std::getline(stream, pair, ',')) {
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos) {
                try {
                    std::string id_str = pair.substr(1, colon_pos-2); // Remove quotes
                    std::string name_str = pair.substr(colon_pos+2, pair.length()-colon_pos-3); // Remove quotes
                    canid_t can_id = std::stoul(id_str);
                    names[can_id] = name_str;
                } catch (const std::exception&) {
                    continue;
                }
            }
        }
    }

    void SQLBackend::parse_message_counts_json(const std::string& json_str,
                                            std::unordered_map<canid_t, size_t>& counts) {
        // Simple JSON-like parsing for "can_id":count pairs
        std::string content = json_str;
        if (content.front() == '{') content = content.substr(1);
        if (content.back() == '}') content = content.substr(0, content.length()-1);
        
        std::istringstream stream(content);
        std::string pair;
        
        while (std::getline(stream, pair, ',')) {
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos) {
                try {
                    std::string id_str = pair.substr(1, colon_pos-2); // Remove quotes
                    std::string count_str = pair.substr(colon_pos+1);
                    canid_t can_id = std::stoul(id_str);
                    size_t count = std::stoull(count_str);
                    counts[can_id] = count;
                } catch (const std::exception&) {
                    continue;
                }
            }
        }
    }

    // Helper methods for CSVBackend

    void CSVBackend::ensure_metadata_file() {
        std::string meta_path = base_path + "/metadata.csv";
        if (!std::filesystem::exists(meta_path)) {
            std::ofstream meta_file(meta_path);
            meta_file << "stream_name,description,creation_time,last_update,total_messages,message_names,message_counts\n";
            meta_file.close();
        }
    }

    std::vector<std::string> CSVBackend::parse_csv_line(const std::string& line) {
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

    void CSVBackend::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
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

    void CSVBackend::parse_serialized_data(const std::string& data_str,
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

    void CSVBackend::parse_serialized_counts(const std::string& counts_str,
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