#pragma once 

#include "Candy/IO/CANIOConcepts.hpp"
#include "Candy/IO/CANIOHelperTypes.hpp"

namespace Candy {

    template<typename T>
    concept CANWriteable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md) {
        { t.write_message(msg) } -> std::same_as<void>;
        { t.write_metadata(md) } -> std::same_as<void>;
        { t.flush() } -> std::same_as<void>;
        { t.flush_sync() } -> std::same_as<void>;
        { t.flush_async([]{} ) } -> std::same_as<void>;
    };

    template<typename Derived>
    struct CANWriter { // Impliticly Queued
        
        void write_message_vtrl(const CANMessage& message) {
            static_assert(HasWriteMessage<Derived>, "Derived must implement write_message_vrtl");
            static_cast<Derived&>(*this).write_message(message);
        }
        
        void write_metadata_vtrl(const CANDataStreamMetadata& metadata) {
            static_assert(HasWriteMetadata<Derived>, "Derived must implement write_metadata_vrtl");
            static_cast<Derived&>(*this).write_metadata(metadata);
        }
        
        void flush_vtrl() {
            static_assert(HasFlush<Derived>, "Derived must implement flush_vrtl");
            static_cast<Derived&>(*this).flush();
        }

        void flush_async_vtrl(std::function<void()> callback) {
            static_assert(HasFlushAsync<Derived>, "Derived must implement flush_async_vrtl");
            static_cast<Derived&>(*this).flush_async(callback);
        }
        
        void flush_sync_vtrl() {
            static_assert(HasFlushSync<Derived>, "Derived must implement flush_sync_vrtl");
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
            static_assert(HasReadMessages<Derived>, "Derived must implement read_messages_vrtl");
            return static_cast<Derived&>(*this).read_messages(can_id);
        }
        
        std::vector<CANMessage> read_messages_in_range_vrtl(canid_t can_id, CANTime start, CANTime end) {
            static_assert(HasReadMessagesInRange<Derived>, "Derived must implement read_messages_in_range_vrtl");
            return static_cast<Derived&>(*this).read_messages_in_range(can_id, start, end);
        }
        
        CANDataStreamMetadata read_metadata_vtrl() {
            static_assert(HasReadMetadata<Derived>, "Derived must implement read_metadata_vrtl");
            return static_cast<Derived&>(*this).read_metadata();
        }
    };

}