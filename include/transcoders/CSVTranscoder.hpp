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

#include "CAN/CANKernelTypes.hpp"
#include "transcoders/CANTranscoderHelperTypes.hpp"
#include "transcoders/CANTranscoder.hpp"

namespace CAN {

    struct CSVTask {
        std::function<void()> operation;
        std::unique_ptr<std::promise<void>> promise;

        CSVTask(std::function<void()> op) : operation(std::move(op)), promise(nullptr) {}
        CSVTask(std::function<void()> op, std::promise<void> p) 
            : operation(std::move(op)), promise(std::make_unique<std::promise<void>>(std::move(p))) {}
    };

    class CSVTranscoder final : public CANTranscoder<CSVTranscoder, CSVTask> {
    public:
        CSVTranscoder(const std::string& base_path, size_t batch_size = 10000);
        ~CSVTranscoder();

        //transcoder methods 
        void transcode(CANTime timestamp, CANFrame frame);
        void transcode(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data);
        std::future<void> transcode_async(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data);

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
    };
}