#pragma once 

#include <algorithm>

#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Core/CANKernelTypes.hpp"

namespace Candy {
    namespace utils {
        inline auto time_range_filter(CANTime start, CANTime end) {
            return [start, end](const CANMessage& msg) {
                return msg.sample.first >= start && msg.sample.first <= end;
            };
        }

        // Helper for signal value filtering
        inline auto signal_value_filter(const std::string& signal_name, 
                                    double min_val, double max_val) {
            return [signal_name, min_val, max_val](const CANMessage& msg) {
                auto it = msg.decoded_signals.find(signal_name);
                if (it == msg.decoded_signals.end()) return false;
                return it->second >= min_val && it->second <= max_val;
            };
        }

        // Helper for message name filtering
        inline auto message_name_filter(const std::string& name) {
            return [name](const CANMessage& msg) {
                return msg.message_name == name;
            };
        }

        // Helper for CAN ID filtering across multiple IDs
        inline auto can_id_filter(const std::vector<canid_t>& ids) {
            return [ids](const CANMessage& msg) {
                return std::ranges::find(ids, msg.sample.second.can_id) != ids.end();
            };
        }

        // Helper for multiplexed signal filtering
        inline auto mux_filter(uint64_t mux_value) {
            return [mux_value](const CANMessage& msg) {
                return msg.mux_value && msg.mux_value.value() == mux_value;
            };
        }
    }
}