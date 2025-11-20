#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Candy/Buffer/BackendInterface.hpp"
#include "Candy/Core/CANIO.hpp"
#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"

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

} // namespace Candy