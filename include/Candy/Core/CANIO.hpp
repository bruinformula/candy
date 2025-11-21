#pragma once 

#include <functional>
#include <string>
#include <vector>

#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {

    using TableType = std::vector<std::pair<std::string, std::string>>;

    template<typename T>
    concept CANWriteable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md, const CANTime& timestamp, const CANFrame& frame, const std::string& table, const TableType& data) {
        { t.write_message(msg) } -> std::same_as<void>;
        { t.write_raw_message(timestamp, frame) } -> std::same_as<void>;
        { t.write_table_message(table, data) } -> std::same_as<void>;
        { t.write_metadata(md) } -> std::same_as<void>;
    };

    template<typename T, size_t BatchSize>
    concept CANBatchWriteable = requires(T t, const std::array<CANMessage, BatchSize>& msg, const std::array<CANTime, BatchSize>& timestamp, const std::array<CANFrame, BatchSize>& frame, const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
        { t.write_message_batch(msg) } -> std::same_as<void>;
        { t.write_raw_message_batch(timestamp, frame) } -> std::same_as<void>;
        { t.write_table_message_batch(table, data) } -> std::same_as<void>;
    };

    template<typename T>
    concept CANReadable = requires(T t) {
        { t.read_message() } -> std::same_as<CANMessage>;
        { t.read_raw_message() } -> std::same_as<std::tuple<CANTime, CANFrame>>;
        { t.read_table_message() } -> std::same_as<std::tuple<CANTime, CANFrame>>;
        { t.read_metadata() } -> std::same_as<CANDataStreamMetadata>;
    };

    template<typename T, size_t BatchSize>
    concept CANBatchReadable = requires(T t) {
        { t.read_messages_batch() } -> std::same_as<std::array<CANMessage, BatchSize>>;
        { t.read_raw_message_batch() } -> std::same_as<std::array<std::tuple<CANTime, CANFrame>, BatchSize>>;
        { t.read_table_message_batch() } -> std::same_as<std::array<std::tuple<std::string, TableType>, BatchSize>>;
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

        void write_table_message_vrtl(const std::string& table, const TableType& data) {
            static_cast<Derived&>(*this).write_table_message(table, data);
        }

        CANWriter() {
            static_assert(CANWriteable<Derived>, "Derived must satisfy CANWriteable concept");
        }
        
    };

    template<typename Derived, size_t BatchSize>
    struct CANBatchWriter {

        void write_message_batch_vtrl(const std::array<CANMessage, BatchSize>& message) {
            static_cast<Derived&>(*this).write_message_batch(message);
        }

        void write_raw_message_batch_vrtl(std::array<CANTime, BatchSize> timestamp, std::array<CANFrame, BatchSize> frame) {
            static_cast<Derived&>(*this).write_raw_message_batch(timestamp, frame);
        }

        void write_table_message_batch_vrtl(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            static_cast<Derived&>(*this).write_table_message_batch(table, data);
        }

        CANBatchWriter() {
            static_assert(CANBatchWriteable<Derived, BatchSize>, "Derived must satisfy CANBatchWriteable concept");
        }
    };

    template<typename Derived>
    struct CANReader { 
        CANMessage read_message_vtrl() {
            return static_cast<Derived&>(*this).read_message_batch();
        }

        std::tuple<CANTime, CANFrame> read_raw_message_vrtl() {
            return static_cast<Derived&>(*this).read_raw_message();
        }

        std::tuple<std::string, TableType> read_table_message_vrtl() {
            return static_cast<Derived&>(*this).read_table_message();
        }
        CANDataStreamMetadata read_metadata_vtrl() {
            return static_cast<Derived&>(*this).read_metadata();
        }

        CANReader() {
            static_assert(CANReadable<Derived>, "Derived must satisfy CANReadable concept");
        }
    };

    template<typename Derived, size_t BatchSize>
    struct CANBatchReader { 
        std::array<CANMessage, BatchSize> read_message_batch_vtrl() {
            return static_cast<Derived&>(*this).read_message_batch();
        }

        std::array<std::tuple<CANTime, CANFrame>, BatchSize> read_raw_message_batch_vrtl() {
            return static_cast<Derived&>(*this).read_raw_message_batch();
        }

        std::array<std::tuple<std::string, TableType>, BatchSize> read_table_message_batch_vrtl() {
            return static_cast<Derived&>(*this).read_table_message_batch();
        }
        CANBatchReader() {
            static_assert(CANBatchReadable<Derived, BatchSize>, "Derived must satisfy CANBatchReadable concept");
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
    concept CANStoreReadable = requires(T t, canid_t id, CANTime start, CANTime end) {
        { t.read_messages(id) } -> std::same_as<std::vector<CANMessage>>;
        { t.read_messages_in_range(id, start, end) } -> std::same_as<std::vector<CANMessage>>;
        { t.read_metadata() } -> std::same_as<CANDataStreamMetadata>;
    };

    template<typename Derived>
    struct CANStoreWriter {

        void flush_vtrl() {
            static_cast<Derived&>(*this).flush();
        }

        void flush_async_vtrl(std::function<void()> callback) {
            static_cast<Derived&>(*this).flush_async(callback);
        }
        
        void flush_sync_vtrl() {
            static_cast<Derived&>(*this).flush_sync();
        }
        CANStoreWriter() {
            static_assert(CANStoreWriteable<Derived>, "Derived must satisfy CANStoreWriteable concept");
        }
    };

    template<typename Derived>
    struct CANStoreReader {
        std::vector<CANMessage> read_messages_vrtl(canid_t can_id) {
            return static_cast<Derived&>(*this).read_messages(can_id);
        }
        
        std::vector<CANMessage> read_messages_in_range_vrtl(canid_t can_id, CANTime start, CANTime end) {
            return static_cast<Derived&>(*this).read_messages_in_range(can_id, start, end);
        }

        CANDataStreamMetadata read_metadata_vrtl() {
            return static_cast<Derived&>(*this).read_metadata();
        }

        CANStoreReader() {
            static_assert(CANStoreReadable<Derived>, "Derived must satisfy CANStoreReadable concept");
        }
    };

}