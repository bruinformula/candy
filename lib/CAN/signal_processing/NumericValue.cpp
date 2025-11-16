    
#include "Candy/CAN/Signal/NumericValue.hpp"

namespace Candy {

    NumericValue::NumericValue(double scale_factor, double offset_value)
        : factor(scale_factor), offset(offset_value) {}

    std::optional<double> NumericValue::convert(uint64_t raw_value, NumericValueType type) const {
        switch (type) {
            case NumericValueType::i64: return convert_raw_to_numeric(*reinterpret_cast<int64_t*>(&raw_value));
            case NumericValueType::u64: return convert_raw_to_numeric(*reinterpret_cast<uint64_t*>(&raw_value));
            case NumericValueType::f32: return convert_raw_to_numeric(*reinterpret_cast<float*>(&raw_value));
            case NumericValueType::f64: return convert_raw_to_numeric(*reinterpret_cast<double*>(&raw_value));
            default: return std::nullopt;
        }
    }

    template <typename T>
    requires std::convertible_to<T, double>
    double NumericValue::convert_raw_to_numeric(T value) const {
        return static_cast<double>(value) * factor + offset;
    }

}