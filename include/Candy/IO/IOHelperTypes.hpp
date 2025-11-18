#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>

#include "Candy/CAN/Signal/SignalCodec.hpp"
#include "Candy/CAN/Signal/NumericValue.hpp"

namespace Candy {
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