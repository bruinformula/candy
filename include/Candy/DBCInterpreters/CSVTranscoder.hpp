#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Core/CSVWriter.hpp"
#include "Candy/DBCInterpreters/File/FileTranscoder.hpp"

namespace Candy {

    class CSVTranscoder final : public FileTranscoder<CSVTranscoder> {
    public:
    ~CSVTranscoder();
        CSVTranscoder(std::string_view base_path, 
                      size_t batch_size, CSVWriter<3> messages_csv,
                      CSVWriter<5> frames_csv,
                      CSVWriter<8> decoded_frames_csv,
                      CSVWriter<7> metadata_csv);

        static std::optional<CSVTranscoder> create(std::string_view base_path, size_t batch_size = 1000);
        //CANReceivable methods 
        void receive_message(const CANMessage& message);
        void receive_raw_message(std::pair<CANTime, CANFrame> sample);
        void receive_metadata(const CANDataStreamMetadata& metadata);

        std::vector<CANMessage> transmit_messages(canid_t can_id);
        std::vector<CANMessage> transmit_messages_in_range(canid_t can_id, CANTime start, CANTime end);
        const CANDataStreamMetadata& transmit_metadata();

        //transcoder methods 
        void batch_frame(std::pair<CANTime, CANFrame> sample);
        void batch_decoded_signals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        void store_message_metadata(canid_t message_id, const std::string& message_name, size_t message_size);
        
    private:
        std::string base_path;

        CSVWriter<3> messages_csv;
        CSVWriter<5> frames_csv;
        CSVWriter<8> decoded_frames_csv;
        CSVWriter<7> metadata_csv;
        
        std::unordered_map<std::string, bool> headers_written;
        std::vector<std::pair<CANTime, CANFrame>> frames_batch;
        // Use struct instead of tuple to avoid std::string in tuple
        struct DecodedSignalBatchEntry {
            CANTime timestamp;
            canid_t can_id;
            MessageName message_name;
            SignalName signal_name;
            double signal_value;
            uint64_t raw_value;
            Unit unit;
            std::optional<uint64_t> mux_value;
        };
        std::vector<DecodedSignalBatchEntry> decoded_signals_batch;

        //csv methods
        std::string format_hex_data(const uint8_t* data, size_t len);

        //CANIO methods
        std::vector<std::string> parse_csv_line(const std::string& line);
        void parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len);
        void parse_serialized_data(const std::string& data_str, CANDataStreamMetadata& metadata);
        void parse_serialized_counts(const std::string& counts_str, CANDataStreamMetadata& metadata);
    };
    
}