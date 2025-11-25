#pragma once 

#include <string>
#include <vector>

#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {

    using TableType = std::vector<std::pair<std::string, std::string>>;

    template<typename T>
    concept IsCANReceivable = requires(T t, const CANMessage& msg, const CANDataStreamMetadata& md, const std::pair<CANTime, CANFrame>& sample, const std::string& table, const TableType& data) {
        { t.receive_message(msg) } -> std::same_as<void>;
        { t.receive_raw_message(sample) } -> std::same_as<void>;
        { t.receive_metadata(md) } -> std::same_as<void>;
    };

    template<size_t BatchSize, typename T>
    concept IsCANBatchReceivable = requires(T t, const std::array<CANMessage, BatchSize>& msg, const std::array<std::tuple<CANTime,CANFrame>, BatchSize>& samples, const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
        { t.receive_message_batch(msg) } -> std::same_as<void>;
        { t.receive_raw_message_batch(samples) } -> std::same_as<void>;
    };

    template<typename T>
    concept IsCANTransmittable = requires(T t) {
        { t.transmit_message() } -> std::same_as<const CANMessage&>;
        { t.transmit_raw_message() } -> std::same_as<const std::pair<CANTime, CANFrame>&>;
        { t.transmit_metadata() } -> std::same_as<const CANDataStreamMetadata&>;
    };

    template< size_t BatchSize, typename T>
    concept IsCANBatchTransmittable = requires(T t) {
        { t.transmit_messages_batch() } -> std::same_as<std::array<CANMessage, BatchSize>>;
        { t.transmit_raw_message_batch() } -> std::same_as<std::array<std::pair<CANTime, CANFrame>, BatchSize>>;
    };
}