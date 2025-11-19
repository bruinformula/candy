#pragma once

#include <string>
#include <vector>
#include <future>
#include <functional>
#include <memory>
#include <unordered_map>
#include "sqlite3.h"

#include "Candy/CAN/CANKernelTypes.hpp"
#include "Candy/IO/CANIOHelperTypes.hpp"
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

        //CANIO methods 
        void write_message(const CANMessage& message);
        void write_raw_message(CANTime timestamp, CANFrame frame);
        void write_table_message(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data);
        void write_metadata(const CANDataStreamMetadata& metadata);

        std::vector<CANMessage> read_messages(canid_t can_id);
        std::vector<CANMessage> read_messages_in_range(
            canid_t can_id, CANTime start, CANTime end);
        CANDataStreamMetadata read_metadata();

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

        //CANIO Methods
        void create_metadata_table();
        void load_decoded_signals_for_messages(sqlite3* db, std::vector<CANMessage>& messages);
        void parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len);
        void parse_message_names_json(const std::string& json_str, 
                                    std::unordered_map<canid_t, std::string>& names);
        void parse_message_counts_json(const std::string& json_str,
                                    std::unordered_map<canid_t, size_t>& counts);

    };
}
