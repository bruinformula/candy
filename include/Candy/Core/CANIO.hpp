#pragma once 

#include <functional>
#include <string>
#include <vector>

#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Core/CANIOConcepts.hpp"
#include "Candy/Core/CANBundle.hpp"

namespace Candy {

    template<typename Derived>
    struct CANReceivable {
        void receive_message_vrtl(const CANMessage& message) {
            static_cast<Derived&>(*this).receive_message(message);
        }
        
        void receive_metadata_vrtl(const CANDataStreamMetadata& metadata) {
            static_cast<Derived&>(*this).receive_metadata(metadata);
        }

        void receive_raw_message_vrtl(const std::pair<CANTime, CANFrame>& sample) {
            static_cast<Derived&>(*this).receive_raw_message(sample);
        }

        void receive_table_message_vrtl(const std::string& table, const TableType& data) {
            static_cast<Derived&>(*this).receive_table_message(table, data);
        }

        CANReceivable() {
            static_assert(IsCANReceivable<Derived>, "Derived must satisfy IsCANReceivable concept");
        }

    };

    template<size_t BatchSize, typename Derived>
    struct CANBatchReceivable {

        void receive_message_batch_vrtl(const std::array<CANMessage, BatchSize>& message) {
            static_cast<Derived&>(*this).receive_message_batch(message);
        }

        void receive_raw_message_batch_vrtl(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            static_cast<Derived&>(*this).receive_raw_message_batch(samples);
        }

        void receive_table_message_batch_vrtl(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            static_cast<Derived&>(*this).receive_table_message_batch(table, data);
        }

        CANBatchReceivable() {
            static_assert(IsCANBatchReceivable<BatchSize, Derived>, "Derived must satisfy IsCANBatchWriteable concept");
        }
    };

    template<typename Derived>
    struct CANTransmittable { 
        const CANMessage& transmit_message_vrtl() {
            return static_cast<Derived&>(*this).transmit_message();
        }

        const std::pair<CANTime, CANFrame>& transmit_raw_message_vrtl() {
            return static_cast<Derived&>(*this).transmit_raw_message();
        }

        const std::tuple<std::string, TableType>& transmit_table_message_vrtl() {
            return static_cast<Derived&>(*this).transmit_table_message();
        }
        const CANDataStreamMetadata& transmit_metadata_vrtl() {
            return static_cast<Derived&>(*this).transmit_metadata();
        }

        CANTransmittable() {
            static_assert(IsCANTransmittable<Derived>, "Derived must satisfy IsCANTransmittable concept");
        }

        template <typename... Receivers>
        requires IsCANTransmittable<Derived> && (IsCANReceivable<Receivers> && ...)
        CANManyReceiverBundle<Derived, Receivers...> transmit_to(Receivers&... receivers) {
            return CANManyReceiverBundle<Derived, Receivers...>(static_cast<Derived&>(*this), receivers...);
        }
        template <typename Receiver>
        requires IsCANTransmittable<Derived> && IsCANReceivable<Receiver>
        CANManyReceiverBundle<Derived, Receiver> transmit_to(Receiver& receiver) {
            return CANManyReceiverBundle<Derived, Receiver>(static_cast<Derived&>(*this), receiver);
        }

    };

    template<size_t BatchSize, typename Derived>
    struct CANBatchTransmittable { 
        const std::array<CANMessage, BatchSize>& transmit_message_batch_vrtl() {
            return static_cast<Derived&>(*this).transmit_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& transmit_raw_message_batch_vrtl() {
            return static_cast<Derived&>(*this).transmit_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& transmit_table_message_batch_vrtl() {
            return static_cast<Derived&>(*this).transmit_table_message_batch();
        }
        CANBatchTransmittable() {
            static_assert(IsCANBatchTransmittable<BatchSize, Derived>, "Derived must satisfy IsCANBatchTransmittable concept");
        }
    };









    //To Be Slimed...
    template<typename T>
    concept CANStoreWriteable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md) {
        { t.flush() } -> std::same_as<void>;
        { t.flush_sync() } -> std::same_as<void>;
        { t.flush_async([]{} ) } -> std::same_as<void>;
    };

    template<typename T>
    concept CANStoreTransmittable = requires(T t, canid_t id, CANTime start, CANTime end) {
        { t.transmit_messages(id) } -> std::same_as<std::vector<CANMessage>>;
        { t.transmit_messages_in_range(id, start, end) } -> std::same_as<std::vector<CANMessage>>;
        { t.transmit_metadata() } -> std::same_as<const CANDataStreamMetadata&>;
    };

    template<typename Derived>
    struct CANStoreReceiver {

        void flush_vrtl() {
            static_cast<Derived&>(*this).flush();
        }

        void flush_async_vrtl(std::function<void()> callback) {
            static_cast<Derived&>(*this).flush_async(callback);
        }
        
        void flush_sync_vrtl() {
            static_cast<Derived&>(*this).flush_sync();
        }
        CANStoreReceiver() {
            static_assert(CANStoreWriteable<Derived>, "Derived must satisfy CANStoreWriteable concept");
        }
    };

    template<typename Derived>
    struct CANStoreTransmitter {
        std::vector<CANMessage> transmit_messages_vrtl(canid_t can_id) {
            return static_cast<Derived&>(*this).transmit_messages(can_id);
        }
        
        std::vector<CANMessage> transmit_messages_in_range_vrtl(canid_t can_id, CANTime start, CANTime end) {
            return static_cast<Derived&>(*this).transmit_messages_in_range(can_id, start, end);
        }

        const CANDataStreamMetadata& transmit_metadata_vrtl() {
            return static_cast<Derived&>(*this).transmit_metadata();
        }

        CANStoreTransmitter() {
            static_assert(CANStoreTransmittable<Derived>, "Derived must satisfy CANStoreTransmittable concept");
        }
    };



    
}