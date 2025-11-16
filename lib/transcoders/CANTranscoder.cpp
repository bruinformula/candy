#include "Candy/DBC/Transcoders/CANTranscoder.hpp"
#include "Candy/DBC/Transcoders/CSVTranscoder.hpp"
#include "Candy/DBC/Transcoders/SQLTranscoder.hpp"

namespace Candy {

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
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

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::sg_mux(canid_t message_id, const std::string& signal_name,
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

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter) {
        messages[message_id].name = message_name;
        messages[message_id].size = message_size;
        messages[message_id].transmitter = transmitter;

        transcode_vrtl("messages", {
            {"message_id", std::to_string(message_id)},
            {"message_name", message_name},
            {"message_size", std::to_string(message_size)}
        });
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type) {
        auto& msg = messages[message_id];
        for (auto& sig : msg.signals) {
            if (sig.name == signal_name) {
                sig.value_type = static_cast<NumericValueType>(value_type);
                break;
            }
        }
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::writer_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !task_queue.empty() || shutdown_requested;
            });

            while (!task_queue.empty()) {
                is_processing = true;
                TaskType task = std::move(task_queue.front());
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
                flush_all_batches_vrtl();
                break;
            }
        }
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::enqueue_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task));
        queue_cv.notify_one();
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::enqueue_task_with_promise(std::function<void()> task, std::promise<void> promise) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.emplace(std::move(task), std::move(promise));
        queue_cv.notify_one();
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::flush_async(std::function<void()> callback) {
        enqueue_task([this, callback]() {
            flush_all_batches_vrtl();
            callback();
        });
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::flush_sync() {
        std::promise<void> promise;
        auto future = promise.get_future();
        enqueue_task_with_promise([this]() {
            flush_all_batches_vrtl();
        }, std::move(promise));
        future.wait();
    }

    template<typename T, typename TaskType>
    void CANTranscoder<T, TaskType>::flush() {
        enqueue_task([this]() {
            flush_all_batches_vrtl();
        });
    }

    template class CANTranscoder<CSVTranscoder, CSVTask>;
    template class CANTranscoder<SQLTranscoder, SQLTask>;

}