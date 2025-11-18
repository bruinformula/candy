#pragma once 

#include <concepts>
#include <type_traits>
#include <functional>

#include "Candy/IO/CANIOHelperTypes.hpp"

namespace Candy {

    template<typename Derived>
    concept HasWriteMessage = requires(Derived d, const CANMessage& message) {
        { d.write_message(message) } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasWriteMetadata = requires(Derived d, const CANDataStreamMetadata& metadata) {
        { d.write_metadata(metadata) } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlush = requires(Derived d) {
        { d.flush() } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlushSync = requires(Derived d) {
        { d.flush_sync() } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlushAsync = requires(Derived d, std::function<void()> callback) {
        { d.flush_async(callback) } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasReadMessages = requires(Derived d, canid_t can_id) {
        { d.read_messages(can_id) } -> std::same_as<std::vector<CANMessage>>;
    };

    template<typename Derived>
    concept HasReadMessagesInRange = requires(Derived d, canid_t can_id, CANTime start, CANTime end) {
        { d.read_messages_in_range(can_id, start, end) } -> std::same_as<std::vector<CANMessage>>;
    };

    template<typename Derived>
    concept HasReadMetadata = requires(Derived d) {
        { d.read_metadata() } -> std::same_as<CANDataStreamMetadata>;
    };

}