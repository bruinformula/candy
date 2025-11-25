#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "sqlite3.h"

#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Interpreters/File/FileTranscoder.hpp"

namespace Candy {

    class SQLTranscoder final : public FileTranscoder<SQLTranscoder> {
    public:
        SQLTranscoder(const std::string& db_file_path, size_t batch_size = 10000);
        ~SQLTranscoder();

        //CANIO methods 
        void receive_message(const CANMessage& message);
        void receive_raw_message(std::pair<CANTime, CANFrame> sample);
        void receive_metadata(const CANDataStreamMetadata& metadata);

        std::vector<CANMessage> transmit_messages(canid_t can_id);
        std::vector<CANMessage> transmit_messages_in_range(
            canid_t can_id, CANTime start, CANTime end);
        const CANDataStreamMetadata& transmit_metadata();

        //transcoder methods 
        void batch_frame(std::pair<CANTime, CANFrame> sample);
        void batch_decoded_signals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        void store_message_metadata(canid_t message_id, const std::string& message_name, size_t message_size);

    private:

        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;
        std::string db_path;
        sqlite3_stmt* decoded_signals_insert_stmt;
        sqlite3_stmt* frames_insert_stmt;

        //sql methods 
        bool prepare_statements();
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
