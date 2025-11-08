#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>

#include "CAN/signal/SignalCodec.hpp"
#include "CAN/signal/NumericValue.hpp"

namespace CAN {
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

struct SQLTask {
    std::function<void()> operation;
    std::unique_ptr<std::promise<void>> promise;
    
    // For fire-and-forget operations
    SQLTask(std::function<void()> op) : operation(std::move(op)), promise(nullptr) {}
    
    // For operations that need synchronization
    SQLTask(std::function<void()> op, std::promise<void> p) 
    : operation(std::move(op)), promise(std::make_unique<std::promise<void>>(std::move(p))) {}
};
}