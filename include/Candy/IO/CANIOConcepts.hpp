#pragma once 

#include <concepts>
#include <type_traits>

namespace Candy {

    template<typename Derived>
    concept HasWriteMessage = requires(Derived d, const CANMessage& message) {
        { d.write_message_vrtl(message) } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasWriteMetadata = requires(Derived d, const CANDataStreamMetadata& metadata) {
        { d.write_metadata_vrtl(metadata) } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlush = requires(Derived d) {
        { d.flush_vrtl() } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlushSync = requires(Derived d) {
        { d.flush_sync_vrtl() } -> std::same_as<void>;
    };

    template<typename Derived>
    concept HasFlushAsync = requires(Derived d) {
        { d.flush_async_vrtl() } -> std::same_as<std::future<void>>;
    };

    template<typename Derived>
    concept HasReadMessages = requires(Derived d, canid_t can_id) {
        { d.read_messages_vrtl(can_id) } -> std::same_as<std::vector<CANMessage>>;
    };

    template<typename Derived>
    concept HasReadMessagesInRange = requires(Derived d, canid_t can_id, CANTime start, CANTime end) {
        { d.read_messages_in_range_vrtl(can_id, start, end) } -> std::same_as<std::vector<CANMessage>>;
    };

    template<typename Derived>
    concept HasReadMetadata = requires(Derived d) {
        { d.read_metadata_vrtl() } -> std::same_as<CANDataStreamMetadata>;
    };

}