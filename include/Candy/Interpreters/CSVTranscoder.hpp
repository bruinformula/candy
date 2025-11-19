#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <future>
#include <functional>
#include <memory>
#include <unordered_map>

#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Interpreters/FileTranscoder.hpp"

namespace Candy {

    struct CSVTask {
        std::function<void()> operation;
        std::unique_ptr<std::promise<void>> promise;

        CSVTask(std::function<void()> op) : operation(std::move(op)), promise(nullptr) {}
        CSVTask(std::function<void()> op, std::promise<void> p) 
            : operation(std::move(op)), promise(std::make_unique<std::promise<void>>(std::move(p))) {}
    };

    class CSVTranscoder final : public FileTranscoder<CSVTranscoder, CSVTask> {
    public:
        CSVTranscoder(const std::string& base_path, size_t batch_size = 10000);
        ~CSVTranscoder();

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
        std::string base_path;
        std::unordered_map<std::string, std::unique_ptr<std::ofstream>> csv_files;
        std::unordered_map<std::string, bool> headers_written;
        std::vector<std::string> frames_batch;
        std::vector<std::string> decoded_signals_batch;

        //transcoder methods 
        void batch_frame(CANTime timestamp, CANFrame frame);
        void batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        
        //csv methods
        std::string build_csv_row(const std::vector<std::pair<std::string, std::string>>& data);
        void ensure_csv_file(const std::string& filename, const std::vector<std::string>& headers);
        void write_to_csv(const std::string& filename, const std::string& row);
        std::string escape_csv(const std::string& input);
        std::string format_hex_data(const uint8_t* data, size_t len);

        //CANIO methods

        void ensure_metadata_file();
        std::vector<std::string> parse_csv_line(const std::string& line);
        void parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len);
        void parse_serialized_data(const std::string& data_str,
                                std::unordered_map<canid_t, std::string>& names);
        void parse_serialized_counts(const std::string& counts_str,
                                    std::unordered_map<canid_t, size_t>& counts);
    };
}