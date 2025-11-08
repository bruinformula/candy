#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <iomanip>

#include "sqlite3.h"

#include "DBC/DBCInterpreter.hpp"
#include "CAN/CANKernelTypes.hpp"
#include "interpreters/SQL/SQLHelperTypes.hpp"

namespace CAN {

    class SQLTranscoder : public DBCInterpreter<SQLTranscoder> {
    public:
        SQLTranscoder(const std::string& db_file_path, size_t batch_size = 10000);
        ~SQLTranscoder();

        void transcode(CANTime timestamp, CANFrame frame);
        void transcode(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);
        std::future<void> transcode_async(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);
        void flush();
        void flush_sync();
        void flush_async(std::function<void()> callback);

        void sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
                unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                double factor, double offset, double min_val, double max_val,
                std::string unit, std::vector<size_t> receivers);

        void sg_mux(canid_t message_id, const std::string& signal_name,
                    unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                    std::string unit, std::vector<size_t> receivers);

        void bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter);

        void sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type);

    private:
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;
        std::string db_path;
        std::unordered_map<canid_t, MessageDefinition> messages;
        size_t batch_size;
        size_t frames_batch_count;
        size_t decoded_signals_batch_count;
        sqlite3_stmt* frames_insert_stmt;
        sqlite3_stmt* decoded_signals_insert_stmt;
        mutable std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::queue<SQLTask> task_queue;
        std::thread writer_thread;
        std::atomic<bool> shutdown_requested;
        std::atomic<size_t> tasks_processed{0};
        std::atomic<bool> is_processing{false};

        void writer_loop();
        void enqueue_task(std::function<void()> task);
        void enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise);
        void prepare_statements();
        void finalize_statements();
        void batch_frame(CANTime timestamp, CANFrame frame);
        void batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        std::string build_insert_sql(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);
        void create_tables();
        void execute_sql(const std::string& sql);
        std::string escape_sql(const std::string& input);
    };
}
