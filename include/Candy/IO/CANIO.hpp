#pragma once 

#include "Candy/IO/CANIOConcepts.hpp"
#include "Candy/IO/CANIOHelperTypes.hpp"

namespace Candy {

    template<typename Derived>
    struct CANWriteable { // Impliticly Queued
        
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

    template<typename Derived>
    struct CANReadable { //should maybe be async
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