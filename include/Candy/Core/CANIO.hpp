#pragma once 

#include <functional>
#include <string>
#include <vector>

#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {

    using TableType = std::vector<std::pair<std::string, std::string>>;

    template<typename T>
    concept IsCANWritable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md, const std::pair<CANTime, CANFrame>& sample, const std::string& table, const TableType& data) {
        { t.write_message(msg) } -> std::same_as<void>;
        { t.write_raw_message(sample) } -> std::same_as<void>;
        { t.write_table_message(table, data) } -> std::same_as<void>;
        { t.write_metadata(md) } -> std::same_as<void>;
    };

    template<size_t BatchSize, typename T>
    concept IsCANBatchWritable = requires(T t, const std::array<CANMessage, BatchSize>& msg, const std::array<std::tuple<CANTime,CANFrame>, BatchSize>& samples, const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
        { t.write_message_batch(msg) } -> std::same_as<void>;
        { t.write_raw_message_batch(samples) } -> std::same_as<void>;
        { t.write_table_message_batch(table, data) } -> std::same_as<void>;
    };

    template<typename T>
    concept IsCANReadable = requires(T t) {
        { t.read_message() } -> std::same_as<CANMessage>;
        { t.read_raw_message() } -> std::same_as<std::pair<CANTime, CANFrame>>;
        { t.read_table_message() } -> std::same_as<std::tuple<std::string, TableType>>;
        { t.read_metadata() } -> std::same_as<CANDataStreamMetadata>;
    };

    template< size_t BatchSize, typename T>
    concept IsCANBatchReadable = requires(T t) {
        { t.read_messages_batch() } -> std::same_as<std::array<CANMessage, BatchSize>>;
        { t.read_raw_message_batch() } -> std::same_as<std::array<std::pair<CANTime, CANFrame>, BatchSize>>;
        { t.read_table_message_batch() } -> std::same_as<std::array<std::tuple<std::string, TableType>, BatchSize>>;
    };

    template<typename Derived>
    struct CANWritable {
        void write_message_vrtl(const CANMessage& message) {
            static_cast<Derived&>(*this).write_message(message);
        }
        
        void write_metadata_vrtl(const CANDataStreamMetadata& metadata) {
            static_cast<Derived&>(*this).write_metadata(metadata);
        }

        void write_raw_message_vrtl(const std::pair<CANTime, CANFrame>& sample) {
            static_cast<Derived&>(*this).write_raw_message(sample);
        }

        void write_table_message_vrtl(const std::string& table, const TableType& data) {
            static_cast<Derived&>(*this).write_table_message(table, data);
        }

        CANWritable() {
            static_assert(IsCANWritable<Derived>, "Derived must satisfy IsCANWritable concept");
        }
        
    };

    template<size_t BatchSize, typename Derived>
    struct CANBatchWritable {

        void write_message_batch_vrtl(const std::array<CANMessage, BatchSize>& message) {
            static_cast<Derived&>(*this).write_message_batch(message);
        }

        void write_raw_message_batch_vrtl(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            static_cast<Derived&>(*this).write_raw_message_batch(samples);
        }

        void write_table_message_batch_vrtl(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            static_cast<Derived&>(*this).write_table_message_batch(table, data);
        }

        CANBatchWritable() {
            static_assert(IsCANBatchWritable<BatchSize, Derived>, "Derived must satisfy IsCANBatchWriteable concept");
        }
    };

    template<typename Derived>
    struct CANReadable { 
        const CANMessage& read_message_vrtl() {
            return static_cast<Derived&>(*this).read_message();
        }

        const std::pair<CANTime, CANFrame>& read_raw_message_vrtl() {
            return static_cast<Derived&>(*this).read_raw_message();
        }

        const std::tuple<std::string, TableType>& read_table_message_vrtl() {
            return static_cast<Derived&>(*this).read_table_message();
        }
        const CANDataStreamMetadata& read_metadata_vrtl() {
            return static_cast<Derived&>(*this).read_metadata();
        }

        CANReadable() {
            static_assert(IsCANReadable<Derived>, "Derived must satisfy IsCANReadable concept");
        }
    };

    template<size_t BatchSize, typename Derived>
    struct CANBatchReadable { 
        const std::array<CANMessage, BatchSize>& read_message_batch_vrtl() {
            return static_cast<Derived&>(*this).read_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& read_raw_message_batch_vrtl() {
            return static_cast<Derived&>(*this).read_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& read_table_message_batch_vrtl() {
            return static_cast<Derived&>(*this).read_table_message_batch();
        }
        CANBatchReadable() {
            static_assert(IsCANBatchReadable<BatchSize, Derived>, "Derived must satisfy IsCANBatchReadable concept");
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
        { t.read_metadata() } -> std::same_as<const CANDataStreamMetadata&>;
    };

    template<typename Derived>
    struct CANStoreWriter {

        void flush_vrtl() {
            static_cast<Derived&>(*this).flush();
        }

        void flush_async_vrtl(std::function<void()> callback) {
            static_cast<Derived&>(*this).flush_async(callback);
        }
        
        void flush_sync_vrtl() {
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

        const CANDataStreamMetadata& read_metadata_vrtl() {
            return static_cast<Derived&>(*this).read_metadata();
        }

        CANStoreReader() {
            static_assert(CANStoreReadable<Derived>, "Derived must satisfy CANStoreReadable concept");
        }
    };

}