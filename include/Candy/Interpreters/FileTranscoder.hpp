#pragma once 

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <vector>
#include <future>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/spirit/home/x3.hpp>

#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Interpreters/FileTranscoderConcepts.hpp"
#include "Candy/Core/DBC/DBCInterpreter.hpp"

#include "Candy/Core/CANIO.hpp"

namespace Candy {

    template <typename Derived, typename TaskType>
    class FileTranscoder : public DBCInterpreter<Derived>,
                           public CANReader<Derived>, 
                           public CANQueueWriter<Derived> {

    public:
        // Constructor to initialize member variables
        FileTranscoder(bool shutdown_requested, size_t batch_size, size_t frames_batch_count, size_t decoded_signals_batch_count) :
            shutdown_requested(shutdown_requested),
            batch_size(batch_size),
            frames_batch_count(frames_batch_count),
            decoded_signals_batch_count(decoded_signals_batch_count)
        {}

        //CANIO Methods
        void flush_async(std::function<void()> callback);
        void flush_sync();
        void flush();

    protected:
        std::unordered_map<canid_t, MessageDefinition> messages;
        size_t batch_size;
        size_t frames_batch_count;
        size_t decoded_signals_batch_count;

        mutable std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::queue<TaskType> task_queue;
        std::thread writer_thread;
        std::atomic<bool> shutdown_requested;
        std::atomic<size_t> tasks_processed{0};
        std::atomic<bool> is_processing{false};

        void writer_loop();
        void enqueue_task(std::function<void()> task);
        void enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise);

        //virtual methods
        void batch_frame_vrtl(CANTime timestamp, CANFrame frame) {
            if constexpr (HasBatchFrame<Derived>) {
                static_cast<Derived&>(*this).batch_frame(timestamp, frame);
            }
        }

        void batch_decoded_signals_vrtl(CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def) {
            if constexpr (HasBatchDecodedSignals<Derived>) {
                static_cast<Derived&>(*this).batch_decoded_signals(timestamp, frame, msg_def);
            }
        }

        void flush_frames_batch_vrtl() {
            if constexpr (HasFlushFramesBatch<Derived>) {
                static_cast<Derived&>(*this).flush_frames_batch();
            }
        }

        void flush_decoded_signals_batch_vrtl() {
            if constexpr (HasFlushDecodedSignalsBatch<Derived>) {
                static_cast<Derived&>(*this).flush_decoded_signals_batch();
            }
        }

        void flush_all_batches_vrtl() {
            if constexpr (HasFlushAllBatches<Derived>) {
                static_cast<Derived&>(*this).flush_all_batches();
            }
        }

    public:
        //DBC/Core methods 
        void sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
            unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
            double factor, double offset, double min_val, double max_val,
            std::string unit, std::vector<size_t> receivers);

        void sg_mux(canid_t message_id, const std::string& signal_name,
                    unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                    std::string unit, std::vector<size_t> receivers);

        void bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter);

        void sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type);
    };
}

