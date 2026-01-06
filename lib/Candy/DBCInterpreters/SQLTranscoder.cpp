
#include <iostream>


#include "Candy/DBCInterpreters/SQLTranscoder.hpp"

namespace Candy {

    SQLTranscoder::SQLTranscoder(sqlite3* raw_db, const std::string& db_file_path, size_t batch_size) : 
        FileTranscoder<SQLTranscoder>(batch_size, 0, 0),
        db_path(db_file_path),
        db(raw_db, sqlite3_close),
        decoded_signals_insert_stmt(nullptr),
        frames_insert_stmt(nullptr)
    {
    }

    std::optional<SQLTranscoder> SQLTranscoder::create(const std::string& db_file_path, size_t batch_size) {
        sqlite3* raw_db = nullptr;
        if (sqlite3_open(db_file_path.c_str(), &raw_db) != SQLITE_OK) {
            std::cerr << "Failed to open SQLite database: " + db_file_path << std::endl;
            return std::nullopt;
        }

        SQLTranscoder transcoder(raw_db, db_file_path, batch_size);

        transcoder.execute_sql("PRAGMA journal_mode=WAL");
        transcoder.execute_sql("PRAGMA synchronous=NORMAL");
        transcoder.execute_sql("PRAGMA cache_size=10000");
        transcoder.execute_sql("PRAGMA temp_store=MEMORY");

        transcoder.create_tables();
        if (!transcoder.prepare_statements()) {
            return std::nullopt;
        }

        return std::make_optional(std::move(transcoder));
    }

    SQLTranscoder::~SQLTranscoder() {
        finalize_statements();
    }

    SQLTranscoder::SQLTranscoder(SQLTranscoder&& other) noexcept
        : FileTranscoder<SQLTranscoder>(std::move(other)),
          db(std::move(other.db)),
          db_path(std::move(other.db_path)),
          decoded_signals_insert_stmt(other.decoded_signals_insert_stmt),
          frames_insert_stmt(other.frames_insert_stmt)
    {
        other.decoded_signals_insert_stmt = nullptr;
        other.frames_insert_stmt = nullptr;
    }

    SQLTranscoder& SQLTranscoder::operator=(SQLTranscoder&& other) noexcept {
        if (this != &other) {
            finalize_statements();
            
            FileTranscoder<SQLTranscoder>::operator=(std::move(other));
            db = std::move(other.db);
            db_path = std::move(other.db_path);
            decoded_signals_insert_stmt = other.decoded_signals_insert_stmt;
            frames_insert_stmt = other.frames_insert_stmt;
            
            other.decoded_signals_insert_stmt = nullptr;
            other.frames_insert_stmt = nullptr;
        }
        return *this;
    }

    //methods 

    void SQLTranscoder::store_message_metadata(canid_t message_id, const std::string& message_name, size_t message_size) {
        std::string sql = "INSERT INTO messages (message_id, message_name, message_size) VALUES (" +
            std::to_string(message_id) + ", '" + escape_sql(message_name) + "', " +
            std::to_string(message_size) + ")";
        execute_sql(sql);
    }

    // Private Methods
    bool SQLTranscoder::prepare_statements() {
        const char* frames_sql = "INSERT INTO frames (timestamp, can_id, dlc, data, message_name) VALUES (?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db.get(), frames_sql, -1, &frames_insert_stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare frames insert statement" << std::endl;
            return false;
        }

        const char* decoded_sql = "INSERT INTO decoded_frames (timestamp, can_id, message_name, signal_name, signal_value, raw_value, unit, mux_value) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db.get(), decoded_sql, -1, &decoded_signals_insert_stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare decoded signals insert statement" << std::endl;
            return false;
        }
        return true;
    }

    void SQLTranscoder::finalize_statements() {
        if (frames_insert_stmt) sqlite3_finalize(frames_insert_stmt);
        if (decoded_signals_insert_stmt) sqlite3_finalize(decoded_signals_insert_stmt);
    }

    void SQLTranscoder::batch_frame(std::pair<CANTime, CANFrame> sample) {
        auto msg_it = messages.find(sample.second.can_id);
        std::string message_name = (msg_it != messages.end()) ? std::string(msg_it->second.get_name()) : "";

        std::string hex_data;
        hex_data.reserve(sample.second.len * 3); // "XX " per byte
        for (int i = 0; i < sample.second.len; i++) {
            char hex_buf[4];
            snprintf(hex_buf, sizeof(hex_buf), "%02X", static_cast<unsigned>(sample.second.data[i]));
            hex_data += hex_buf;
            if (i < sample.second.len - 1) hex_data += " ";
        }

        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sample.first.time_since_epoch()).count();

        sqlite3_bind_int64(frames_insert_stmt, 1, timestamp_ms);
        sqlite3_bind_int(frames_insert_stmt, 2, sample.second.can_id);
        sqlite3_bind_int(frames_insert_stmt, 3, sample.second.len);
        // hex_data is already a string now
        sqlite3_bind_text(frames_insert_stmt, 4, hex_data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(frames_insert_stmt, 5, message_name.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(frames_insert_stmt) != SQLITE_DONE) {
            std::cerr << "Failed to batch frame insert" << std::endl;
            return;
        }

        sqlite3_reset(frames_insert_stmt);
        frames_batch_count++;
    }

    void SQLTranscoder::batch_decoded_signals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
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

            sqlite3_bind_int64(decoded_signals_insert_stmt, 1, timestamp_ms);
            sqlite3_bind_int(decoded_signals_insert_stmt, 2, sample.second.can_id);
            sqlite3_bind_text(decoded_signals_insert_stmt, 3, msg_def.get_name().data(), -1, SQLITE_STATIC);
            sqlite3_bind_text(decoded_signals_insert_stmt, 4, signal.get_name().data(), -1, SQLITE_STATIC);
            sqlite3_bind_double(decoded_signals_insert_stmt, 5, converted_value.value());
            sqlite3_bind_int64(decoded_signals_insert_stmt, 6, raw_value);
            sqlite3_bind_text(decoded_signals_insert_stmt, 7, signal.get_unit().data(), -1, SQLITE_STATIC);
            if (mux_value) sqlite3_bind_int64(decoded_signals_insert_stmt, 8, mux_value.value());
            else sqlite3_bind_null(decoded_signals_insert_stmt, 8);

            if (sqlite3_step(decoded_signals_insert_stmt) != SQLITE_DONE) {
                std::cerr << "Failed to batch decoded signal insert" << std::endl;
                return;
            }

            sqlite3_reset(decoded_signals_insert_stmt);
            decoded_signals_batch_count++;

        }
    }

    void SQLTranscoder::flush_frames_batch() {
        if (frames_batch_count == 0) return;
        execute_sql("BEGIN TRANSACTION");
        execute_sql("COMMIT");
        frames_batch_count = 0;
    }

    void SQLTranscoder::flush_decoded_signals_batch() {
        if (decoded_signals_batch_count == 0) return;
        execute_sql("BEGIN TRANSACTION");
        execute_sql("COMMIT");
        decoded_signals_batch_count = 0;
    }

    void SQLTranscoder::flush_all_batches() {
        if (frames_batch_count > 0 || decoded_signals_batch_count > 0) {
            execute_sql("BEGIN TRANSACTION");
            execute_sql("COMMIT");
            frames_batch_count = 0;
            decoded_signals_batch_count = 0;
        }
    }

    std::string SQLTranscoder::build_insert_sql(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string sql = "INSERT INTO " + table + " (";
        std::string values = "VALUES (";

        for (size_t i = 0; i < data.size(); ++i) {
            sql += data[i].first;
            values += "'" + escape_sql(data[i].second) + "'";
            if (i < data.size() - 1) {
                sql += ", ";
                values += ", ";
            }
        }

        sql += ") " + values + ");";
        return sql;
    }

    void SQLTranscoder::create_tables() {
        const char* create_signals_table = R"(
            CREATE TABLE IF NOT EXISTS signals (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                message_name TEXT,
                signal_name TEXT,
                details TEXT
            );
        )";

        const char* create_messages_table = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                message_id INTEGER,
                message_name TEXT,
                message_size INTEGER
            );
        )";

        const char* create_frames_table = R"(
            CREATE TABLE IF NOT EXISTS frames (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER,
                can_id INTEGER,
                dlc INTEGER,
                data BLOB,
                message_name TEXT
            );
        )";

        const char* create_decoded_frames_table = R"(
            CREATE TABLE IF NOT EXISTS decoded_frames (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER,
                can_id INTEGER,
                message_name TEXT,
                signal_name TEXT,
                signal_value REAL,
                raw_value INTEGER,
                unit TEXT,
                mux_value INTEGER
            );
        )";

        execute_sql("DROP TABLE IF EXISTS messages");
        execute_sql("DROP TABLE IF EXISTS signals");
        execute_sql("DROP TABLE IF EXISTS frames");
        execute_sql("DROP TABLE IF EXISTS decoded_frames");

        execute_sql(create_signals_table);
        execute_sql(create_messages_table);
        execute_sql(create_frames_table);
        execute_sql(create_decoded_frames_table);
    }

    void SQLTranscoder::execute_sql(const std::string& sql) {
        char* err_msg = nullptr;
        if (sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::string error_message = "SQLite error: " + std::string(err_msg);
            sqlite3_free(err_msg);
            std::cerr << error_message << std::endl;
        }
    }

    std::string SQLTranscoder::escape_sql(const std::string& input) {
        std::string escaped;
        for (char c : input) {
            if (c == '\'') escaped += "''";
            else escaped += c;
        }
        return escaped;
    }
    
    //CANIO Methods

    void SQLTranscoder::receive_raw_message(std::pair<CANTime, CANFrame> sample) {
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

    void SQLTranscoder::receive_message(const CANMessage& message) {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.sample.first.time_since_epoch()).count();
        
        receive_raw_message(message.sample);
        
        if (message.signal_count > 0) {
            for (size_t i = 0; i < message.signal_count && i < message.decoded_signals.size(); ++i) {
                const auto& signal_entry = message.decoded_signals[i];
                if (!signal_entry.is_valid) continue;
                
                std::string signal_name = std::string(signal_entry.get_name());
                double signal_value = signal_entry.value;
                std::string unit = std::string(signal_entry.get_unit());
                
                // Directly insert into decoded_frames table
                sqlite3_bind_int64(decoded_signals_insert_stmt, 1, timestamp_ms);
                sqlite3_bind_int(decoded_signals_insert_stmt, 2, message.sample.second.can_id);
                sqlite3_bind_text(decoded_signals_insert_stmt, 3, message.get_message_name().data(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(decoded_signals_insert_stmt, 4, signal_name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(decoded_signals_insert_stmt, 5, signal_value);
                sqlite3_bind_int64(decoded_signals_insert_stmt, 6, 0); // raw_value not available
                sqlite3_bind_text(decoded_signals_insert_stmt, 7, unit.c_str(), -1, SQLITE_TRANSIENT);
                if (message.mux_value) {
                    sqlite3_bind_int64(decoded_signals_insert_stmt, 8, *message.mux_value);
                } else {
                    sqlite3_bind_null(decoded_signals_insert_stmt, 8);
                }
                
                if (sqlite3_step(decoded_signals_insert_stmt) != SQLITE_DONE) {
                    std::cerr << "Failed to insert decoded signal" << std::endl;
                    return;
                }
                
                sqlite3_reset(decoded_signals_insert_stmt);
                decoded_signals_batch_count++;
            }
            
            if (decoded_signals_batch_count >= batch_size) {
                flush_decoded_signals_batch();
            }
        }
    }

    void SQLTranscoder::receive_metadata(const CANDataStreamMetadata& metadata) {
        auto creation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.creation_time.time_since_epoch()).count();
        auto update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.last_update.time_since_epoch()).count();
        
        // Serialize message names and counts as JSON-like strings
        std::string names_json, counts_json;
        names_json.reserve(1024);
        counts_json.reserve(1024);
        
        names_json += "{";
        bool first = true;
        for (size_t i = 0; i < metadata.message_count && i < metadata.messages.size(); ++i) {
            const auto& msg_entry = metadata.messages[i];
            if (!msg_entry.is_valid) continue;
            if (!first) names_json += ",";
            names_json += "\"" + std::to_string(msg_entry.can_id) + "\":\"" + std::string(msg_entry.get_name()) + "\"";
            first = false;
        }
        names_json += "}";
        
        counts_json += "{";
        first = true;
        for (size_t i = 0; i < metadata.message_count && i < metadata.messages.size(); ++i) {
            const auto& msg_entry = metadata.messages[i];
            if (!msg_entry.is_valid) continue;
            if (!first) counts_json += ",";
            counts_json += "\"" + std::to_string(msg_entry.can_id) + "\":" + std::to_string(msg_entry.count);
            first = false;
        }
        counts_json += "}";

        execute_sql("DELETE FROM metadata");
        
        // Insert new metadata
        std::string insert_sql = 
            "INSERT INTO metadata (stream_name, description, creation_time, last_update, total_messages, message_names, message_counts) "
            "VALUES ('" + escape_sql(std::string(metadata.get_stream_name())) + "', '" + 
            escape_sql(std::string(metadata.get_description())) + "', " + 
            std::to_string(creation_ms) + ", " + 
            std::to_string(update_ms) + ", " + 
            std::to_string(metadata.total_messages) + ", '" + 
            escape_sql(names_json) + "', '" + 
            escape_sql(counts_json) + "')";
        
        execute_sql(insert_sql);
    }

    std::vector<CANMessage> SQLTranscoder::transmit_messages(canid_t can_id) {
        return transmit_messages_in_range(can_id, 
            std::chrono::system_clock::time_point::min(),
            std::chrono::system_clock::time_point::max());
    }

    std::vector<CANMessage> SQLTranscoder::transmit_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) {
        
        std::vector<CANMessage> messages;
        
        // Open database connection for transmiting
        sqlite3* db;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Failed to open database for transmiting" << std::endl;
            return {};
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
                message.sample.first = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp_ms));
                
                // Parse frame data
                message.sample.second.can_id = sqlite3_column_int(stmt, 1);
                message.sample.second.len = sqlite3_column_int(stmt, 2);
                
                // Parse hex data
                const char* hex_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                if (hex_data) {
                    parse_hex_data(hex_data, message.sample.second.data, message.sample.second.len);
                }
                
                // Get message name
                const char* msg_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (msg_name) {
                    message.set_message_name(msg_name);
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

    const CANDataStreamMetadata& SQLTranscoder::transmit_metadata() {        
        sqlite3* db;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Failed to open database for transmiting metadata" << std::endl;
            return metadata;
        }
        
        const char* meta_sql = "SELECT * FROM metadata LIMIT 1";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, meta_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* stream_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                if (stream_name) metadata.set_stream_name(stream_name);
                if (description) metadata.set_description(description);
                
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
                
                if (names_json) {
                    parse_message_names_json(std::string(names_json), metadata);
                }
                if (counts_json) {
                    parse_message_counts_json(std::string(counts_json), metadata);
                }
            }
            sqlite3_finalize(stmt);
        }
        
        sqlite3_close(db);
        return metadata;
    }

    //CANIO Helpers
    void SQLTranscoder::create_metadata_table() {
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
    }

    void SQLTranscoder::load_decoded_signals_for_messages(sqlite3* db, std::vector<CANMessage>& messages) {
        if (messages.empty()) return;
        
        // Build a map for quick lookup
        std::unordered_map<std::string, size_t> message_lookup;
        for (size_t i = 0; i < messages.size(); ++i) {
            auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                messages[i].sample.first.time_since_epoch()).count();
            std::string key = std::to_string(timestamp_ms) + "_" + std::to_string(messages[i].sample.second.can_id);
            message_lookup[key] = i;
        }
        
        // Query decoded signals
        const char* signals_sql = 
            "SELECT timestamp, can_id, signal_name, signal_value, unit, mux_value "
            "FROM decoded_frames WHERE can_id = ? "
            "ORDER BY timestamp";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, signals_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, messages[0].sample.second.can_id);
            
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
                        const char* unit_str = unit ? unit : "";
                        messages[msg_idx].add_signal(signal_name, signal_value, unit_str);
                    }
                    
                    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                        messages[msg_idx].mux_value = sqlite3_column_int64(stmt, 5);
                    }
                }
            }
            
            sqlite3_finalize(stmt);
        }
    }

    void SQLTranscoder::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
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

    void SQLTranscoder::parse_message_names_json(const std::string& json_str, 
                                            CANDataStreamMetadata& metadata) {
        // Simple JSON-like parsing for "can_id":"name" pairs
        std::string content = json_str;
        if (!content.empty() && content.front() == '{') content = content.substr(1);
        if (!content.empty() && content.back() == '}') content = content.substr(0, content.length()-1);
        
        size_t pos = 0;
        while (pos < content.length() && metadata.message_count < metadata.messages.size()) {
            size_t comma_pos = content.find(',', pos);
            if (comma_pos == std::string::npos) comma_pos = content.length();
            
            std::string pair = content.substr(pos, comma_pos - pos);
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos && colon_pos >= 2 && pair.length() > colon_pos + 3) {
                std::string id_str = pair.substr(1, colon_pos-2); // Remove quotes
                std::string name_str = pair.substr(colon_pos+2, pair.length()-colon_pos-3); // Remove quotes
                
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
                        size_t copy_len = name_str.length() < metadata.messages[idx].name.size() - 1 
                            ? name_str.length() 
                            : metadata.messages[idx].name.size() - 1;
                        std::copy_n(name_str.begin(), copy_len, metadata.messages[idx].name.begin());
                        metadata.messages[idx].name[copy_len] = '\0';
                        metadata.messages[idx].is_valid = true;
                        metadata.message_count++;
                    }
                }
            }
            pos = comma_pos + 1;
        }
    }

    void SQLTranscoder::parse_message_counts_json(const std::string& json_str,
                                            CANDataStreamMetadata& metadata) {
        // Simple JSON-like parsing for "can_id":count pairs
        std::string content = json_str;
        if (!content.empty() && content.front() == '{') content = content.substr(1);
        if (!content.empty() && content.back() == '}') content = content.substr(0, content.length()-1);
        
        size_t pos = 0;
        while (pos < content.length()) {
            size_t comma_pos = content.find(',', pos);
            if (comma_pos == std::string::npos) comma_pos = content.length();
            
            std::string pair = content.substr(pos, comma_pos - pos);
            auto colon_pos = pair.find(':');
            if (colon_pos != std::string::npos && colon_pos >= 2) {
                std::string id_str = pair.substr(1, colon_pos-2); // Remove quotes
                std::string count_str = pair.substr(colon_pos+1);
                
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
            pos = comma_pos + 1;
        }
    }


}
