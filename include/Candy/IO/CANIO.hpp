#pragma once 

#include <functional>
#include <string>
#include <vector>

#include "Candy/IO/CANIOHelperTypes.hpp"

namespace Candy {

    template<typename T>
    concept CANWriteable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md, const CANTime& timestamp, const CANFrame& frame, const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
        { t.write_message(msg) } -> std::same_as<void>;
        { t.write_metadata(md) } -> std::same_as<void>;
        { t.write_raw_message(timestamp, frame) } -> std::same_as<void>;
        { t.write_table(table, data) } -> std::same_as<void>;
    };

    template<typename T>
    concept CANQueueWriteable = CANWriteable<T> && requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md) {
        { t.flush() } -> std::same_as<void>;
        { t.flush_sync() } -> std::same_as<void>;
        { t.flush_async([]{} ) } -> std::same_as<void>;
    };

    template<typename Derived>
    struct CANWriter {
        void write_message_vtrl(const CANMessage& message) {
            static_cast<Derived&>(*this).write_message(message);
        }
        
        void write_metadata_vtrl(const CANDataStreamMetadata& metadata) {
            static_cast<Derived&>(*this).write_metadata(metadata);
        }

        void write_raw_message_vrtl(CANTime timestamp, CANFrame frame) {
            static_cast<Derived&>(*this).write_raw_message(timestamp, frame);
        }

        void write_table_message_vrtl(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            static_cast<Derived&>(*this).write_table_message(table, data);
        }
        
    };

    template<typename Derived>
    struct CANQueueWriter : public CANWriter<Derived> {

        void flush_vtrl() {
            static_cast<Derived&>(*this).flush();
        }

        void flush_async_vtrl(std::function<void()> callback) {
            static_cast<Derived&>(*this).flush_async(callback);
        }
        
        void flush_sync_vtrl() {
            static_cast<Derived&>(*this).flush_sync();
        }
    };

    template<typename T>
    concept CANReadable = requires(T t, canid_t id, CANTime start, CANTime end) {
        { t.read_messages(id) } -> std::same_as<std::vector<CANMessage>>;
        { t.read_messages_in_range(id, start, end) } -> std::same_as<std::vector<CANMessage>>;
        { t.read_metadata() } -> std::same_as<CANDataStreamMetadata>;
    };

    template<typename Derived>
    struct CANReader { //should maybe be async
        std::vector<CANMessage> read_messages_vrtl(canid_t can_id) {
            return static_cast<Derived&>(*this).read_messages(can_id);
        }
        
        std::vector<CANMessage> read_messages_in_range_vrtl(canid_t can_id, CANTime start, CANTime end) {
            return static_cast<Derived&>(*this).read_messages_in_range(can_id, start, end);
        }
        
        CANDataStreamMetadata read_metadata_vtrl() {
            return static_cast<Derived&>(*this).read_metadata();
        }
    };

}