#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "interpreters/SQLTranscoder.hpp"


namespace CAN {

    SQLTranscoder::SQLTranscoder(const std::string& db_file_path, size_t batch_size)
        : db_path(db_file_path),
        shutdown_requested(false),
        db(nullptr, sqlite3_close),
        batch_size(batch_size),
        frames_batch_count(0),
        decoded_signals_batch_count(0)
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

        writer_thread = std::thread(&SQLTranscoder::writer_loop, this);
    }

    SQLTranscoder::~SQLTranscoder() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            shutdown_requested = true;
        }
        queue_cv.notify_all();

        if (writer_thread.joinable()) {
            writer_thread.join();
        }

        finalize_statements();
    }

    //methods 
    void SQLTranscoder::transcode(CANTime timestamp, CANFrame frame) {
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

    void SQLTranscoder::transcode(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string sql = build_insert_sql(table, data);
        enqueue_task([this, sql]() {
            execute_sql(sql);
        });
    }

    std::future<void> SQLTranscoder::transcode_async(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
        std::string sql = build_insert_sql(table, data);
        std::promise<void> promise;
        auto future = promise.get_future();

        enqueue_task_with_promise([this, sql]() {
            execute_sql(sql);
        }, std::move(promise));

        return future;
    }

    void SQLTranscoder::flush() {
        enqueue_task([this]() {
            flush_all_batches();
        });
    }

    void SQLTranscoder::flush_sync() {
        std::promise<void> promise;
        auto future = promise.get_future();
        enqueue_task_with_promise([this]() {
            flush_all_batches();
        }, std::move(promise));
        future.wait();
    }

    void SQLTranscoder::flush_async(std::function<void()> callback) {
        enqueue_task([this, callback]() {
            flush_all_batches();
            callback();
        });
    }

    void SQLTranscoder::sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
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

    void SQLTranscoder::sg_mux(canid_t message_id, const std::string& signal_name,
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

    void SQLTranscoder::bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter) {
        messages[message_id].name = message_name;
        messages[message_id].size = message_size;
        messages[message_id].transmitter = transmitter;

        transcode("messages", {
            {"message_id", std::to_string(message_id)},
            {"message_name", message_name},
            {"message_size", std::to_string(message_size)}
        });
    }

    void SQLTranscoder::sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type) {
        auto& msg = messages[message_id];
        for (auto& sig : msg.signals) {
            if (sig.name == signal_name) {
                sig.value_type = static_cast<NumericValueType>(value_type);
                break;
            }
        }
    }

    // Private Methods
    void SQLTranscoder::writer_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !task_queue.empty() || shutdown_requested;
            });

            while (!task_queue.empty()) {
                is_processing = true;
                SQLTask task = std::move(task_queue.front());
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

    void SQLTranscoder::enqueue_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task));
        queue_cv.notify_one();
    }

    void SQLTranscoder::enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task), std::move(promise));
        queue_cv.notify_one();
    }

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

    void SQLTranscoder::batch_frame(CANTime timestamp, CANFrame frame) {
        auto msg_it = messages.find(frame.can_id);
        std::string message_name = (msg_it != messages.end()) ? msg_it->second.name : "";

        std::stringstream hex_data;
        for (int i = 0; i < frame.len; i++) {
            hex_data << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned>(frame.data[i]);
            if (i < frame.len - 1) hex_data << " ";
        }

        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        sqlite3_bind_int64(frames_insert_stmt, 1, timestamp_ms);
        sqlite3_bind_int(frames_insert_stmt, 2, frame.can_id);
        sqlite3_bind_int(frames_insert_stmt, 3, frame.len);
        std::string hex_string = hex_data.str();
        sqlite3_bind_text(frames_insert_stmt, 4, hex_string.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(frames_insert_stmt, 5, message_name.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(frames_insert_stmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to batch frame insert");
        }

        sqlite3_reset(frames_insert_stmt);
        frames_batch_count++;
    }

    void SQLTranscoder::batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def) {
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

                sqlite3_bind_int64(decoded_signals_insert_stmt, 1, timestamp_ms);
                sqlite3_bind_int(decoded_signals_insert_stmt, 2, frame.can_id);
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
    

} // namespace CAN
