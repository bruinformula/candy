#pragma once

#include <optional>
#include <concepts>
#include <cstdint>

namespace Candy {

    enum class NumericValueType {
        i64 = 0,
        f32 = 1,
        f64 = 2,
        u64 = 3
    };

    class NumericValue {
        double factor;
        double offset;

    public:
        NumericValue(double scale_factor, double offset_value);

        std::optional<double> convert(uint64_t raw_value, NumericValueType type) const;

    private:
        template <typename T>
        requires std::convertible_to<T, double>
        double convert_raw_to_numeric(T value) const;
    };

}