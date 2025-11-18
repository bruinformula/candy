#include "Candy/Buffer/CANDataBuffer.hpp"
#include "Candy/Buffer/BackendInterface.hpp"
#include "Candy/Interpreters/SQLTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"

namespace Candy {

    // CANDataBuffer constructor
    CANDataBuffer::CANDataBuffer(const std::string& stream_name, 
                                    std::unique_ptr<BackendInterface> backend_impl,
                                    size_t max_buffer)
        : backend(std::move(backend_impl)), max_buffer_size(max_buffer) {
        metadata.stream_name = stream_name;
        metadata.creation_time = std::chrono::system_clock::now();
        metadata.last_update = metadata.creation_time;
    }

    // Factory methods
    std::unique_ptr<CANDataBuffer> CANDataBuffer::create_with_sql(
        const std::string& stream_name,
        const std::string& db_path,
        size_t max_buffer) {
        
        auto backend = std::make_unique<SQLBackend>(db_path);
        return std::make_unique<CANDataBuffer>(stream_name, std::move(backend), max_buffer);
    }

    std::unique_ptr<CANDataBuffer> CANDataBuffer::create_with_csv(
        const std::string& stream_name,
        const std::string& base_path,
        size_t max_buffer) {
        
        auto backend = std::make_unique<CSVBackend>(base_path);
        return std::make_unique<CANDataBuffer>(stream_name, std::move(backend), max_buffer);
    }

    // Core operations
    void CANDataBuffer::add_message(CANMessage message) {
        update_metadata_for_message(message);
        message_buffers[message.frame.can_id].push_back(std::move(message));
        check_auto_flush();
    }

    void CANDataBuffer::add_frame(CANTime timestamp, CANFrame frame, 
                                        const std::string& message_name,
                                        const std::unordered_map<std::string, double>& signals,
                                        const std::unordered_map<std::string, std::string>& units) {
        CANMessage message;
        message.timestamp = timestamp;
        message.frame = frame;
        message.message_name = message_name;
        message.decoded_signals = signals;
        message.signal_units = units;
        
        add_message(std::move(message));
    }

    // Query operations
    auto CANDataBuffer::get_messages(canid_t can_id) const -> const std::vector<CANMessage>& {
        static const std::vector<CANMessage> empty_vector;
        auto it = message_buffers.find(can_id);
        return (it != message_buffers.end()) ? it->second : empty_vector;
    }

    auto CANDataBuffer::get_messages_in_range(canid_t can_id, CANTime start, CANTime end) const 
        -> std::vector<CANMessage> {
        
        const auto& messages = get_messages(can_id);
        std::vector<CANMessage> result;
        for (const auto& msg : messages | std::views::filter([start, end](const CANMessage& msg) {
            return msg.timestamp >= start && msg.timestamp <= end;
        })) {
            result.push_back(msg);
        }
        return result;
    }


    auto CANDataBuffer::get_all_message_ids() const -> std::vector<canid_t> {
        std::vector<canid_t> ids;
        ids.reserve(message_buffers.size());
        
        for (const auto& [can_id, _] : message_buffers) {
            ids.push_back(can_id);
        }
        
        return ids;
    }

    // Buffer management
    void CANDataBuffer::flush_all() {
        persist_to_backend();
        backend->flush_sync();
    }

    std::future<void> CANDataBuffer::flush_async() {
        persist_to_backend();
        return backend->flush_async();
    }

    void CANDataBuffer::clear_buffers() {
        message_buffers.clear();
        metadata.total_messages = 0;
        metadata.message_counts.clear();
    }

    void CANDataBuffer::set_auto_flush(bool enabled, size_t threshold) {
        auto_flush_enabled = enabled;
        auto_flush_threshold = threshold;
    }

    // Statistics
    size_t CANDataBuffer::get_total_messages() const {
        return metadata.total_messages;
    }

    size_t CANDataBuffer::get_buffer_size(canid_t can_id) const {
        auto it = message_buffers.find(can_id);
        return (it != message_buffers.end()) ? it->second.size() : 0;
    }

    size_t CANDataBuffer::get_total_buffer_size() const {
        size_t total = 0;
        for (const auto& [_, buffer] : message_buffers) {
            total += buffer.size();
        }
        return total;
    }

    // Metadata operations
    void CANDataBuffer::update_stream_name(const std::string& name) {
        metadata.stream_name = name;
    }

    void CANDataBuffer::update_description(const std::string& desc) {
        metadata.description = desc;
    }

    // Backend operations
    void CANDataBuffer::persist_to_backend() {
        // Write all buffered messages to backend
        for (const auto& [can_id, messages] : message_buffers) {
            for (const auto& message : messages) {
                backend->write_message(message);
            }
        }
        
        // Update and write metadata
        metadata.last_update = std::chrono::system_clock::now();
        backend->write_metadata(metadata);
    }

    void CANDataBuffer::load_from_backend() {
        // Load metadata first
        metadata = backend->read_metadata();
        
        // Load all messages for each known CAN ID
        for (const auto& [can_id, _] : metadata.message_names) {
            auto messages = backend->read_messages(can_id);
            if (!messages.empty()) {
                message_buffers[can_id] = std::move(messages);
            }
        }
    }

    std::future<void> CANDataBuffer::persist_async() {
        return std::async(std::launch::async, [this]() {
            persist_to_backend();
            backend->flush_sync();
        });
    }

    // Private helper methods
    void CANDataBuffer::update_metadata_for_message(const CANMessage& message) {
        metadata.total_messages++;
        metadata.last_update = std::chrono::system_clock::now();
        metadata.message_counts[message.frame.can_id]++;
        
        if (!message.message_name.empty()) {
            metadata.message_names[message.frame.can_id] = message.message_name;
        }
    }

    void CANDataBuffer::check_auto_flush() {
        if (auto_flush_enabled && get_total_buffer_size() >= auto_flush_threshold) {
            flush_all();
            clear_buffers(); // Clear after successful persistence
        }
    }
}