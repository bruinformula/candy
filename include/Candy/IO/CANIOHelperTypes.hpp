#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>

#include "Candy/CAN/Signal/SignalCodec.hpp"
#include "Candy/CAN/Signal/NumericValue.hpp"

namespace Candy {

    struct CANMessage {
        CANTime timestamp;
        CANFrame frame;
        std::string message_name;
        std::unordered_map<std::string, double> decoded_signals;
        std::unordered_map<std::string, std::string> signal_units;
        std::optional<uint64_t> mux_value;

        // C++20 spaceship operator for comparison
        auto operator<=>(const CANMessage& other) const {
            return timestamp <=> other.timestamp;
        }

        bool operator==(const CANMessage& other) const {
            return timestamp == other.timestamp && frame.can_id == other.frame.can_id;
        }
    };

    struct CANDataStreamMetadata {
        std::string stream_name;
        std::string description;
        std::chrono::system_clock::time_point creation_time;
        std::chrono::system_clock::time_point last_update;
        size_t total_messages{0};
        std::unordered_map<canid_t, std::string> message_names;
        std::unordered_map<canid_t, size_t> message_counts;

        auto get_duration() const {
            return last_update - creation_time;
        }

        double get_message_rate(canid_t can_id) const {
            auto duration_sec = std::chrono::duration<double>(get_duration()).count();
            if (duration_sec <= 0) return 0.0;
            
            auto it = message_counts.find(can_id);
            return (it != message_counts.end()) ? it->second / duration_sec : 0.0;
        }
    };

    struct SignalDefinition {
        std::string name;
        std::unique_ptr<SignalCodec> codec;
        std::unique_ptr<NumericValue> numeric_value;
        double min_val = 0.0;
        double max_val = 0.0;
        std::string unit;
        std::optional<unsigned> mux_val;
        bool is_multiplexer = false;
        NumericValueType value_type = NumericValueType::f64;
    };
    
    struct MessageDefinition {
        std::string name;
        size_t size = 0;
        size_t transmitter = 0;
        std::vector<SignalDefinition> signals;
        std::optional<SignalDefinition> multiplexer;
    };
}