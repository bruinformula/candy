#pragma once    

#include <bit>

#include "CAN/CANKernelTypes.hpp"

namespace CAN {
    template<typename T>
    concept ValidSignalType = std::is_same_v<T, uint64_t> ||
                            std::is_same_v<T, int64_t>  ||
                            std::is_same_v<T, float>    ||
                            std::is_same_v<T, double>;

    template <ValidSignalType T>
    class SignalCalculationType {
        T value = 0;
    public:
        SignalCalculationType(uint64_t raw);

        uint64_t get_raw_value() const;

        SignalCalculationType<T> operator+(const SignalCalculationType<T>& rhs) const;

        SignalCalculationType<T> idivround(int64_t d) const;
    private:
        SignalCalculationType<T> from_value(T val) const;
    };
}