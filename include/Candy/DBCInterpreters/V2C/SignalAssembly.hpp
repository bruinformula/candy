#pragma once

#include <cstdint>
#include <concepts>

#include "Candy/DBCInterpreters/V2C/TranslatedSignal.hpp"

namespace Candy {
    
    template <typename Derived>
    concept IsSignalAssembler = requires(Derived& self, int64_t mux_val, uint64_t fd) {
        { self.assemble(mux_val, fd) } -> std::convertible_to<uint64_t>;
        { self.reset() } -> std::same_as<void>;
    };
    template<typename T>
    concept IsValidSignalType = std::is_same_v<T, uint64_t> ||
                              std::is_same_v<T, int64_t>  ||
                              std::is_same_v<T, float>    ||
                              std::is_same_v<T, double>;

    template <typename Derived, typename Numeric>
    struct SignalAssembler {
        const TranslatedSignal _sig;
        Numeric _val{0};

        explicit SignalAssembler(const TranslatedSignal sig) : 
            _sig(sig) 
        {}

        uint64_t assemble_vrtl(int64_t mux_val, uint64_t fd) {
            return static_cast<Derived*>(this)->assemble(mux_val, fd);
        }

        void reset_vrtl() {
            static_cast<Derived*>(this)->reset();
        }

        SignalAssembler() {
            static_assert(IsSignalAssembler<Derived>, "Derived must satisfy IsSignalAssembler concepts");
            static_assert(IsValidSignalType<Derived>, "Derived must satisfy IsValidSignalType concepts");
        }
    };

    template <typename Numeric>
    struct LastSignal : public SignalAssembler<LastSignal<Numeric>, Numeric> {
        LastSignal(const TranslatedSignal sig) : 
            SignalAssembler<LastSignal<Numeric>, Numeric>(sig) 
        {}

        uint64_t assemble(int64_t mux_val, uint64_t fd);
        void reset();
    };

    template <typename Numeric>
    struct AverageSignal : public SignalAssembler<AverageSignal<Numeric>, Numeric> {
        uint64_t _num_samples{0};

        AverageSignal(const TranslatedSignal sig) : 
            SignalAssembler<AverageSignal<Numeric>, Numeric>(sig)
        {}

        uint64_t assemble(int64_t mux_val, uint64_t fd);
        void reset();
    };

    // Extern template declarations to prevent implicit instantiation
    extern template struct LastSignal<uint64_t>;
    extern template struct LastSignal<int64_t>;
    extern template struct LastSignal<double>;
    extern template struct LastSignal<float>;

    extern template struct AverageSignal<uint64_t>;
    extern template struct AverageSignal<int64_t>;
    extern template struct AverageSignal<double>;
    extern template struct AverageSignal<float>;

}