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

#include "Candy/CAN/CANKernelTypes.hpp"
#include "Candy/IO/IOHelperTypes.hpp"
#include "Candy/Interpreters/FileTranscoder.hpp"

namespace Candy {

    struct SQLTask {
        std::function<void()> operation;
        std::unique_ptr<std::promise<void>> promise;
        
        // For fire-and-forget operations
        SQLTask(std::function<void()> op) : 
            operation(std::move(op)), 
            promise(nullptr) 
        {}
        
        // For operations that need synchronization
        SQLTask(std::function<void()> op, std::promise<void> p) : 
            operation(std::move(op)), 
            promise(std::make_unique<std::promise<void>>(std::move(p))) 
        {}
    };

    class SQLTranscoder final : public FileTranscoder<SQLTranscoder, SQLTask> {
    public:
        SQLTranscoder(const std::string& db_file_path, size_t batch_size = 10000);
        ~SQLTranscoder();

        //transcoder methods 
        void transcode(CANTime timestamp, CANFrame frame);
        void transcode(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);
        std::future<void> transcode_async(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);

    private:
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;
        std::string db_path;
        sqlite3_stmt* decoded_signals_insert_stmt;
        sqlite3_stmt* frames_insert_stmt;

        //transcoder methods 
        void batch_frame(CANTime timestamp, CANFrame frame);
        void batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        
        //sql methods 
        void prepare_statements();
        void finalize_statements();
        std::string build_insert_sql(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data);
        void create_tables();
        void execute_sql(const std::string& sql);
        std::string escape_sql(const std::string& input);
    };
}
