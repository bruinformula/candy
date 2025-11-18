#pragma once 

#include "Candy/IO/CANIOConcepts.hpp"

namespace Candy {
    template<typename Derived>
    struct CANWriteable { // Impliticly Queued
        
        void write_message(const CANMessage& message) {
            static_assert(HasWriteMessage<Derived>, "Derived must implement write_message_vrtl");
            static_cast<Derived&>(*this).write_message_vrtl(message);
        }
        
        void write_metadata(const CANDataStreamMetadata& metadata) {
            static_assert(HasWriteMetadata<Derived>, "Derived must implement write_metadata_vrtl");
            static_cast<Derived&>(*this).write_metadata_vrtl(metadata);
        }
        
        void flush() {
            static_assert(HasFlush<Derived>, "Derived must implement flush_vrtl");
            static_cast<Derived&>(*this).flush_vrtl();
        }
        
        void flush_sync() {
            static_assert(HasFlushSync<Derived>, "Derived must implement flush_sync_vrtl");
            static_cast<Derived&>(*this).flush_sync_vrtl();
        }
        
        std::future<void> flush_async() {
            static_assert(HasFlushAsync<Derived>, "Derived must implement flush_async_vrtl");
            return static_cast<Derived&>(*this).flush_async_vrtl();
        }
    };

    template<typename Derived>
    struct CANReadable { //should maybe be async
        std::vector<CANMessage> read_messages(canid_t can_id) {
            static_assert(HasReadMessages<Derived>, "Derived must implement read_messages_vrtl");
            return static_cast<Derived&>(*this).read_messages_vrtl(can_id);
        }
        
        std::vector<CANMessage> read_messages_in_range(canid_t can_id, CANTime start, CANTime end) {
            static_assert(HasReadMessagesInRange<Derived>, "Derived must implement read_messages_in_range_vrtl");
            return static_cast<Derived&>(*this).read_messages_in_range_vrtl(can_id, start, end);
        }
        
        CANDataStreamMetadata read_metadata() {
            static_assert(HasReadMetadata<Derived>, "Derived must implement read_metadata_vrtl");
            return static_cast<Derived&>(*this).read_metadata_vrtl();
        }
    };

}