#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <future>

#include "Candy/CAN/CANKernelTypes.hpp"
#include "Candy/Buffer/BackendInterface.hpp"
#include "Candy/IO/CANIOHelperTypes.hpp"

namespace Candy {
    
    class CANDataBuffer {
    private:
        std::unordered_map<canid_t, std::vector<CANMessage>> message_buffers;
        CANDataStreamMetadata metadata;
        std::unique_ptr<BackendInterface> backend;
        size_t max_buffer_size;
        bool auto_flush_enabled{true};
        size_t auto_flush_threshold{1000};

        void update_metadata_for_message(const CANMessage& message);
        void check_auto_flush();

    public:
        explicit CANDataBuffer(const std::string& stream_name, 
                            std::unique_ptr<BackendInterface> backend_impl,
                            size_t max_buffer = 10000);

        static std::unique_ptr<CANDataBuffer> create_with_sql(
            const std::string& stream_name,
            const std::string& db_path,
            size_t max_buffer = 10000);

        static std::unique_ptr<CANDataBuffer> create_with_csv(
            const std::string& stream_name,
            const std::string& base_path,
            size_t max_buffer = 10000);

        //data operations
        void add_message(CANMessage message);
        void add_frame(CANTime timestamp, CANFrame frame, 
                    const std::string& message_name = "",
                    const std::unordered_map<std::string, double>& can_signals = {},
                    const std::unordered_map<std::string, std::string>& units = {});

        // buffer management
        void flush_all();
        std::future<void> flush_async();
        void clear_buffers();
        void set_auto_flush(bool enabled, size_t threshold = 1000);

        auto get_messages(canid_t can_id) const -> const std::vector<CANMessage>&;
        auto get_messages_in_range(canid_t can_id, CANTime start, CANTime end) const 
            -> std::vector<CANMessage>;
        auto get_all_message_ids() const -> std::vector<canid_t>;
        
        template<typename Predicate>
        auto filter_messages(canid_t can_id, Predicate pred) const -> std::vector<CANMessage> {
            std::vector<CANMessage> result;
            auto it = message_buffers.find(can_id);
            if (it != message_buffers.end()) {
                for (const auto& msg : it->second) {
                    if (pred(msg)) {
                        result.push_back(msg);
                    }
                }
            }
            return result;
        }

        // Metadata access
        const CANDataStreamMetadata& get_metadata() const { return metadata; }
        CANDataStreamMetadata& get_metadata() { return metadata; }
        
        void update_stream_name(const std::string& name);
        void update_description(const std::string& desc);

        // Statistics
        size_t get_total_messages() const;
        size_t get_buffer_size(canid_t can_id) const;
        size_t get_total_buffer_size() const;
        
        // Backend operations
        void persist_to_backend();
        void load_from_backend();
        std::future<void> persist_async();
    };

    namespace utils {
        // Helper for time-based filtering
        inline auto time_range_filter(CANTime start, CANTime end) {
            return [start, end](const CANMessage& msg) {
                return msg.timestamp >= start && msg.timestamp <= end;
            };
        }

        // Helper for signal value filtering
        inline auto signal_value_filter(const std::string& signal_name, 
                                    double min_val, double max_val) {
            return [signal_name, min_val, max_val](const CANMessage& msg) {
                auto it = msg.decoded_signals.find(signal_name);
                if (it == msg.decoded_signals.end()) return false;
                return it->second >= min_val && it->second <= max_val;
            };
        }

        // Helper for message name filtering
        inline auto message_name_filter(const std::string& name) {
            return [name](const CANMessage& msg) {
                return msg.message_name == name;
            };
        }

        // Helper for CAN ID filtering across multiple IDs
        inline auto can_id_filter(const std::vector<canid_t>& ids) {
            return [ids](const CANMessage& msg) {
                return std::ranges::find(ids, msg.frame.can_id) != ids.end();
            };
        }

        // Helper for multiplexed signal filtering
        inline auto mux_filter(uint64_t mux_value) {
            return [mux_value](const CANMessage& msg) {
                return msg.mux_value && msg.mux_value.value() == mux_value;
            };
        }
    }

} // namespace Candy