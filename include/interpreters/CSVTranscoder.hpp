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

#include "DBC/DBCInterpreter.hpp"
#include "CAN/CANKernelTypes.hpp"
#include "interpreters/SQL/SQLHelperTypes.hpp"

namespace CAN {

    struct CSVTask {
        std::function<void()> operation;
        std::unique_ptr<std::promise<void>> promise;

        CSVTask(std::function<void()> op) : operation(std::move(op)), promise(nullptr) {}
        CSVTask(std::function<void()> op, std::promise<void> p) 
            : operation(std::move(op)), promise(std::make_unique<std::promise<void>>(std::move(p))) {}
    };

    class CSVTranscoder : public DBCInterpreter<CSVTranscoder> {
    public:
        CSVTranscoder(const std::string& base_path, size_t batch_size = 10000);
        ~CSVTranscoder();

        void transcode(CANTime timestamp, CANFrame frame);
        void transcode(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data);
        std::future<void> transcode_async(const std::string& filename, const std::vector<std::pair<std::string, std::string>>& data);
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
        std::string base_path;
        std::unordered_map<canid_t, MessageDefinition> messages;
        std::unordered_map<std::string, std::unique_ptr<std::ofstream>> csv_files;
        std::unordered_map<std::string, bool> headers_written;
        size_t batch_size;
        size_t frames_batch_count;
        size_t decoded_signals_batch_count;
        
        // Batching buffers
        std::vector<std::string> frames_batch;
        std::vector<std::string> decoded_signals_batch;
        
        mutable std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::queue<CSVTask> task_queue;
        std::thread writer_thread;
        std::atomic<bool> shutdown_requested;
        std::atomic<size_t> tasks_processed{0};
        std::atomic<bool> is_processing{false};

        void writer_loop();
        void enqueue_task(std::function<void()> task);
        void enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise);
        void batch_frame(CANTime timestamp, CANFrame frame);
        void batch_decoded_signals(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def);
        void flush_frames_batch();
        void flush_decoded_signals_batch();
        void flush_all_batches();
        std::string build_csv_row(const std::vector<std::pair<std::string, std::string>>& data);
        void ensure_csv_file(const std::string& filename, const std::vector<std::string>& headers);
        void write_to_csv(const std::string& filename, const std::string& row);
        std::string escape_csv(const std::string& input);
        std::string format_hex_data(const uint8_t* data, size_t len);
    };
}