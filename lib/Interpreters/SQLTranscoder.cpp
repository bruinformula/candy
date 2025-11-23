#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "Candy/Interpreters/SQLTranscoder.hpp"


namespace Candy {

    SQLTranscoder::SQLTranscoder(const std::string& db_file_path, size_t batch_size) : 
        FileTranscoder<SQLTranscoder, SQLTask>(false, batch_size, 0, 0),
        db_path(db_file_path),
        db(nullptr, sqlite3_close)
    {
        sqlite3* raw_db = nullptr;
        if (sqlite3_open(db_file_path.c_str(), &raw_db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite database: " + db_file_path);
        }

        db = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>(raw_db, sqlite3_close);

        execute_sql("PRAGMA journal_mode=WAL");
        execute_sql("PRAGMA synchronous=NORMAL");
        execute_sql("PRAGMA cache_size=10000");
        execute_sql("PRAGMA temp_store=MEMORY");

        create_tables();
        prepare_statements();

        receiver_thread = std::thread(&SQLTranscoder::receiver_loop, this);
    }

    SQLTranscoder::~SQLTranscoder() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            shutdown_requested = true;
        }
        queue_cv.notify_all();

        if (receiver_thread.joinable()) {
            receiver_thread.join();
        }

        finalize_statements();
    }

    //methods 
    void SQLTranscoder::receive_raw_message(std::pair<CANTime, CANFrame> sample) {
        enqueue_task([this, sample]() {
            batch_frame(sample);

            auto msg_it = messages.find(sample.second.can_id);
            if (msg_it != messages.end()) {
                batch_decoded_signals(sample, msg_it->second);
            }

            if (frames_batch_count >= batch_size) flush_frames_batch();
            if (decoded_signals_batch_count >= batch_size) flush_decoded_signals_batch();
        });
    }

    void SQLTranscoder::receive_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string sql = build_insert_sql(table, data);
        enqueue_task([this, sql]() {
            execute_sql(sql);
        });
    }

    // Private Methods
    void SQLTranscoder::prepare_statements() {
        const char* frames_sql = "INSERT INTO frames (timestamp, can_id, dlc, data, message_name) VALUES (?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db.get(), frames_sql, -1, &frames_insert_stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare frames insert statement");
        }

        const char* decoded_sql = "INSERT INTO decoded_frames (timestamp, can_id, message_name, signal_name, signal_value, raw_value, unit, mux_value) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db.get(), decoded_sql, -1, &decoded_signals_insert_stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare decoded signals insert statement");
        }
    }

    void SQLTranscoder::finalize_statements() {
        if (frames_insert_stmt) sqlite3_finalize(frames_insert_stmt);
        if (decoded_signals_insert_stmt) sqlite3_finalize(decoded_signals_insert_stmt);
    }

    void SQLTranscoder::batch_frame(std::pair<CANTime, CANFrame> sample) {
        auto msg_it = messages.find(sample.second.can_id);
        std::string message_name = (msg_it != messages.end()) ? msg_it->second.name : "";

        std::stringstream hex_data;
        for (int i = 0; i < sample.second.len; i++) {
            hex_data << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned>(sample.second.data[i]);
            if (i < sample.second.len - 1) hex_data << " ";
        }

        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sample.first.time_since_epoch()).count();

        sqlite3_bind_int64(frames_insert_stmt, 1, timestamp_ms);
        sqlite3_bind_int(frames_insert_stmt, 2, sample.second.can_id);
        sqlite3_bind_int(frames_insert_stmt, 3, sample.second.len);
        std::string hex_string = hex_data.str();
        sqlite3_bind_text(frames_insert_stmt, 4, hex_string.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(frames_insert_stmt, 5, message_name.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(frames_insert_stmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to batch frame insert");
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

        for (const auto& signal : msg_def.signals) {
            if (signal.mux_val.has_value() && (!mux_value || mux_value.value() != signal.mux_val.value())) {
                continue;
            }

            try {
                uint64_t raw_value = (*signal.codec)(sample.second.data);
                auto converted_value = signal.numeric_value->convert(raw_value, signal.value_type);
                if (!converted_value.has_value()) continue;

                sqlite3_bind_int64(decoded_signals_insert_stmt, 1, timestamp_ms);
                sqlite3_bind_int(decoded_signals_insert_stmt, 2, sample.second.can_id);
                sqlite3_bind_text(decoded_signals_insert_stmt, 3, msg_def.name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(decoded_signals_insert_stmt, 4, signal.name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_double(decoded_signals_insert_stmt, 5, converted_value.value());
                sqlite3_bind_int64(decoded_signals_insert_stmt, 6, raw_value);
                sqlite3_bind_text(decoded_signals_insert_stmt, 7, signal.unit.c_str(), -1, SQLITE_STATIC);
                if (mux_value) sqlite3_bind_int64(decoded_signals_insert_stmt, 8, mux_value.value());
                else sqlite3_bind_null(decoded_signals_insert_stmt, 8);

                if (sqlite3_step(decoded_signals_insert_stmt) != SQLITE_DONE) {
                    throw std::runtime_error("Failed to batch decoded signal insert");
                }

                sqlite3_reset(decoded_signals_insert_stmt);
                decoded_signals_batch_count++;
            } catch (...) {
                // Silent fail to continue processing other signals
            }
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
            throw std::runtime_error(error_message);
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

    void SQLTranscoder::receive_message(const CANMessage& message) {
        // Convert timestamp to milliseconds
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.sample.first.time_since_epoch()).count();
        
        // Use existing transcoder to receive the raw frame
        receive_raw_message(message.sample);
        
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
                    {"can_id", std::to_string(message.sample.second.can_id)},
                    {"message_name", message.message_name},
                    {"signal_name", signal_name},
                    {"signal_value", std::to_string(signal_value)},
                    {"raw_value", "0"}, // We don't have raw value in this context
                    {"unit", unit},
                    {"mux_value", message.mux_value ? std::to_string(*message.mux_value) : ""}
                };
                
                receive_table_message("decoded_frames", signal_data);
            }
        }
    }

    void SQLTranscoder::receive_metadata(const CANDataStreamMetadata& metadata) {
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
        receive_table_message("DELETE FROM metadata", {});
        receive_table_message("metadata", meta_data);
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
            throw std::runtime_error("Failed to open database for transmiting");
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

    const CANDataStreamMetadata& SQLTranscoder::transmit_metadata() {        
        sqlite3* db;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database for transmiting metadata");
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
        
        // Access the underlying database connection from transcoder
        // This would require exposing a method in SQLTranscoder to execute custom SQL
        // For now, we'll assume such a method exists or can be added
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

    void SQLTranscoder::parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len) {
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

    void SQLTranscoder::parse_message_names_json(const std::string& json_str, 
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

    void SQLTranscoder::parse_message_counts_json(const std::string& json_str,
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


}
