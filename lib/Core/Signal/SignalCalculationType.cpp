#include <vector>
#include <cstring>

#include "Candy/Core/Signal/SignalCalculationType.hpp"

namespace Candy {

    template <ValidSignalType T>
    SignalCalculationType<T>::SignalCalculationType(uint64_t raw_value) {
        value = *reinterpret_cast<const T*>(&raw_value);
    }

    template <ValidSignalType T>
    uint64_t SignalCalculationType<T>::get_raw_value() const {
        return *reinterpret_cast<const uint64_t*>(&value);
    }

    template <ValidSignalType T>
    SignalCalculationType<T> SignalCalculationType<T>::operator+(const SignalCalculationType& rhs) const {
        return from_value(value + rhs.value);
    }

    template <ValidSignalType T>
    SignalCalculationType<T> SignalCalculationType<T>::idivround(int64_t divisor) const {
        if constexpr (std::is_integral_v<T>)
            return from_value((value < 0) ? (value - divisor / 2) / divisor : (value + divisor / 2) / divisor);
        else
            return from_value(value / divisor);
    }

    template <ValidSignalType T>
    SignalCalculationType<T> SignalCalculationType<T>::from_value(T val) const {
        return SignalCalculationType<T>(*reinterpret_cast<const uint64_t*>(&val));
    }

    // Explicit template instantiation
    template class SignalCalculationType<uint64_t>;
    template class SignalCalculationType<int64_t>;
    template class SignalCalculationType<float>;
    template class SignalCalculationType<double>;

}