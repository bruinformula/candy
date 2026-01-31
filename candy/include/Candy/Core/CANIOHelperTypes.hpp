#pragma once

#include <compare>
#include <string_view>
#include <array>
#include <memory>
#include <optional>

#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/Signal/SignalCodec.hpp"
#include "Candy/Core/Signal/NumericValue.hpp"

namespace Candy {

    constexpr size_t MAX_MESSAGE_NAME_LEN = 64;
    constexpr size_t MAX_SIGNAL_NAME_LEN = 64;
    constexpr size_t MAX_UNIT_LEN = 16;
    constexpr size_t MAX_STREAM_NAME_LEN = 128;
    constexpr size_t MAX_DESCRIPTION_LEN = 256;
    constexpr size_t MAX_SIGNALS_PER_MESSAGE = 32;
    constexpr size_t MAX_MESSAGES_PER_STREAM = 256;
    constexpr size_t MAX_MESSAGE_DEFINITIONS = 256;

    using MessageName = std::array<char, MAX_MESSAGE_NAME_LEN>;
    using SignalName = std::array<char, MAX_SIGNAL_NAME_LEN>;
    using Unit = std::array<char, MAX_UNIT_LEN>;
    using StreamName = std::array<char, MAX_STREAM_NAME_LEN>;
    using Description = std::array<char, MAX_DESCRIPTION_LEN>;

    struct SignalEntry {
        SignalName name;
        double value;
        Unit unit;
        bool is_valid = false;
        
        constexpr SignalEntry() : name{}, value(0.0), unit{}, is_valid(false) {}
        
        constexpr std::string_view get_name() const {
            return std::string_view(name.data(), strnlen(name.data(), name.size()));
        }
        
        constexpr std::string_view get_unit() const {
            return std::string_view(unit.data(), strnlen(unit.data(), unit.size()));
        }
    };

    struct MessageMapEntry {
        canid_t can_id;
        MessageName name;
        size_t count = 0;
        bool is_valid = false;
        
        constexpr MessageMapEntry() : can_id(0), name{}, count(0), is_valid(false) {}
        
        constexpr std::string_view get_name() const {
            return std::string_view(name.data(), strnlen(name.data(), name.size()));
        }
    };

    struct SignalDefinition {
        SignalName name;
        std::unique_ptr<SignalCodec> codec;
        std::unique_ptr<NumericValue> numeric_value;
        double min_val = 0.0;
        double max_val = 0.0;
        Unit unit;
        std::optional<unsigned> mux_val;
        bool is_multiplexer = false;
        NumericValueType value_type = NumericValueType::f64;
        
        SignalDefinition() : name{}, codec{}, numeric_value{}, min_val(0.0), max_val(0.0), unit{}, mux_val{}, is_multiplexer(false), value_type(NumericValueType::f64) {}
        
        std::string_view get_name() const {
            return std::string_view(name.data(), strnlen(name.data(), name.size()));
        }
        
        std::string_view get_unit() const {
            return std::string_view(unit.data(), strnlen(unit.data(), unit.size()));
        }
        
        void set_name(std::string_view signal_name) {
            const size_t copy_len = std::min(signal_name.size(), name.size() - 1);
            std::copy(signal_name.begin(), signal_name.begin() + copy_len, name.begin());
            name[copy_len] = '\0';
        }
        
        void set_unit(std::string_view signal_unit) {
            const size_t copy_len = std::min(signal_unit.size(), unit.size() - 1);
            std::copy(signal_unit.begin(), signal_unit.begin() + copy_len, unit.begin());
            unit[copy_len] = '\0';
        }
    };
    
    struct MessageDefinition {
        MessageName name;
        size_t size = 0;
        size_t transmitter = 0;
        std::array<SignalDefinition, MAX_SIGNALS_PER_MESSAGE> signals;
        size_t signal_count = 0;
        std::optional<SignalDefinition> multiplexer;
        
        MessageDefinition() : name{}, size(0), transmitter(0), signals{}, signal_count(0), multiplexer{} {}
        
        std::string_view get_name() const {
            return std::string_view(name.data(), strnlen(name.data(), name.size()));
        }
        
        void set_name(std::string_view message_name) {
            const size_t copy_len = std::min(message_name.size(), name.size() - 1);
            std::copy(message_name.begin(), message_name.begin() + copy_len, name.begin());
            name[copy_len] = '\0';
        }
        
        bool add_signal(SignalDefinition&& signal) {
            if (signal_count >= signals.size()) {
                return false; // No space
            }
            signals[signal_count] = std::move(signal);
            ++signal_count;
            return true;
        }
        
        bool add_signal(std::string_view signal_name, std::string_view unit = "") {
            if (signal_count >= signals.size()) {
                return false; // No space
            }
            auto& signal = signals[signal_count];
            signal.set_name(signal_name);
            signal.set_unit(unit);
            ++signal_count;
            return true;
        }
        
        std::optional<const SignalDefinition*> get_signal(std::string_view signal_name) const {
            for (size_t i = 0; i < signal_count && i < signals.size(); ++i) {
                if (signals[i].get_name() == signal_name) {
                    return &signals[i];
                }
            }
            return std::nullopt;
        }
        
        std::optional<SignalDefinition*> get_signal_mutable(std::string_view signal_name) {
            for (size_t i = 0; i < signal_count && i < signals.size(); ++i) {
                if (signals[i].get_name() == signal_name) {
                    return &signals[i];
                }
            }
            return std::nullopt;
        }
    };

    struct CANMessage {
        std::pair<CANTime,CANFrame> sample;
        MessageName message_name;
        std::array<SignalEntry, MAX_SIGNALS_PER_MESSAGE> decoded_signals;
        std::optional<uint64_t> mux_value;
        size_t signal_count = 0;

        std::strong_ordering operator<=>(const CANMessage& other) const {
            return sample.first <=> other.sample.first;
        }

        bool operator==(const CANMessage& other) const {
            return sample.first == other.sample.first && sample.second.can_id == other.sample.second.can_id;
        }
        
        CANMessage() : sample{}, message_name{}, decoded_signals{}, mux_value{}, signal_count(0) {}
        
        std::string_view get_message_name() const {
            return std::string_view(message_name.data(), strnlen(message_name.data(), message_name.size()));
        }
        
        void set_message_name(std::string_view name) {
            const size_t copy_len = std::min(name.size(), message_name.size() - 1);
            std::copy(name.begin(), name.begin() + copy_len, message_name.begin());
            message_name[copy_len] = '\0';
        }
        
        std::optional<double> get_signal_value(std::string_view signal_name) const {
            for (size_t i = 0; i < signal_count && i < decoded_signals.size(); ++i) {
                if (decoded_signals[i].is_valid && decoded_signals[i].get_name() == signal_name) {
                    return decoded_signals[i].value;
                }
            }
            return std::nullopt;
        }
        
        std::optional<std::string_view> get_signal_unit(std::string_view signal_name) const {
            for (size_t i = 0; i < signal_count && i < decoded_signals.size(); ++i) {
                if (decoded_signals[i].is_valid && decoded_signals[i].get_name() == signal_name) {
                    return decoded_signals[i].get_unit();
                }
            }
            return std::nullopt;
        }
        
        bool add_signal(std::string_view signal_name, double value, std::string_view unit = "") {
            if (signal_count >= decoded_signals.size()) {
                return false; // No space
            }
            
            auto& entry = decoded_signals[signal_count];
            const size_t name_len = std::min(signal_name.size(), entry.name.size() - 1);
            const size_t unit_len = std::min(unit.size(), entry.unit.size() - 1);
            
            std::copy(signal_name.begin(), signal_name.begin() + name_len, entry.name.begin());
            entry.name[name_len] = '\0';
            
            std::copy(unit.begin(), unit.begin() + unit_len, entry.unit.begin());
            entry.unit[unit_len] = '\0';
            
            entry.value = value;
            entry.is_valid = true;
            ++signal_count;
            return true;
        }
    };

    struct CANDataStreamMetadata {
        StreamName stream_name;
        Description description;
        std::chrono::system_clock::time_point creation_time;
        std::chrono::system_clock::time_point last_update;
        size_t total_messages{0};
        std::array<MessageMapEntry, MAX_MESSAGES_PER_STREAM> messages;
        size_t message_count = 0;

        CANDataStreamMetadata() : stream_name{}, description{}, creation_time{}, last_update{}, total_messages(0), messages{}, message_count(0) {}
        
        auto get_duration() const {
            return last_update - creation_time;
        }

        double get_message_rate(canid_t can_id) const {
            auto duration_sec = std::chrono::duration<double>(get_duration()).count();
            if (duration_sec <= 0) return 0.0;
            
            for (size_t i = 0; i < message_count && i < messages.size(); ++i) {
                if (messages[i].is_valid && messages[i].can_id == can_id) {
                    return messages[i].count / duration_sec;
                }
            }
            return 0.0;
        }
        
        std::string_view get_stream_name() const {
            return std::string_view(stream_name.data(), strnlen(stream_name.data(), stream_name.size()));
        }
        
        std::string_view get_description() const {
            return std::string_view(description.data(), strnlen(description.data(), description.size()));
        }
        
        void set_stream_name(std::string_view name) {
            const size_t copy_len = std::min(name.size(), stream_name.size() - 1);
            std::copy(name.begin(), name.begin() + copy_len, stream_name.begin());
            stream_name[copy_len] = '\0';
        }
        
        void set_description(std::string_view desc) {
            const size_t copy_len = std::min(desc.size(), description.size() - 1);
            std::copy(desc.begin(), desc.begin() + copy_len, description.begin());
            description[copy_len] = '\0';
        }
        
        std::optional<std::string_view> get_message_name(canid_t can_id) const {
            for (size_t i = 0; i < message_count && i < messages.size(); ++i) {
                if (messages[i].is_valid && messages[i].can_id == can_id) {
                    return messages[i].get_name();
                }
            }
            return std::nullopt;
        }
        
        bool add_message(canid_t can_id, std::string_view name, size_t count = 0) {
            // Check if message already exists
            for (size_t i = 0; i < message_count && i < messages.size(); ++i) {
                if (messages[i].is_valid && messages[i].can_id == can_id) {
                    messages[i].count = count;
                    return true;
                }
            }
            
            // Add new message if space available
            if (message_count >= messages.size()) {
                return false; // No space
            }
            
            auto& entry = messages[message_count];
            entry.can_id = can_id;
            entry.count = count;
            entry.is_valid = true;
            
            const size_t name_len = std::min(name.size(), entry.name.size() - 1);
            std::copy(name.begin(), name.begin() + name_len, entry.name.begin());
            entry.name[name_len] = '\0';
            
            ++message_count;
            return true;
        }
        
        bool increment_message_count(canid_t can_id) {
            for (size_t i = 0; i < message_count && i < messages.size(); ++i) {
                if (messages[i].is_valid && messages[i].can_id == can_id) {
                    ++messages[i].count;
                    ++total_messages;
                    return true;
                }
            }
            return false;
        }
    };
}