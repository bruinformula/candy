#pragma once

#include <cstdint>
#include <memory>
#include <concepts>
#include <iostream>

#include "transcoders/V2C/translation/TranslatedSignal.hpp"
#include "transcoders/V2C/translation/TranslatedMultiplexer.hpp"
#include "CAN/signal/SignalCalculationType.hpp"
#include "CAN/signal/NumericValue.hpp"

namespace CAN {
    
    template <typename Derived>
    concept HasAssembles = requires(Derived& self, int64_t mux_val, uint64_t fd) {
        { self.assemble(mux_val, fd) } -> std::convertible_to<uint64_t>;
    };

    template <typename Derived>
    concept HasResets = requires(Derived& self) {
        { self.reset() } -> std::same_as<void>;
    };

    template <typename Derived>
    struct SignalAssembler {

        uint64_t assemble(int64_t mux_val, uint64_t fd) requires HasAssembles<Derived> {
            return static_cast<Derived*>(this)->assemble(mux_val, fd);
        }

        void reset() requires HasResets<Derived> {
            static_cast<Derived*>(this)->reset();
        }
    };

    template <typename T>
    class LastSignal : public SignalAssembler<LastSignal<T>> {
        const TranslatedSignal _sig;
        SignalCalculationType<T> _val{0};

    public:
        explicit LastSignal(const TranslatedSignal sig) : _sig(sig) {}

        uint64_t assemble(int64_t mux_val, uint64_t fd) {
            if (!_sig.is_active(mux_val))
                return 0;

            SignalCalculationType<T> val(_sig.decode(fd));
            _val = val;
            return _sig.encode(val.get_raw_value());
        }

        void reset() {
            // No-op
        }
    };

    template <typename T>
    class AverageSignal : public SignalAssembler<AverageSignal<T>> {
        const TranslatedSignal _sig;
        SignalCalculationType<T> _val{0};
        uint64_t _num_samples{0};

    public:
        explicit AverageSignal(const TranslatedSignal sig) : _sig(sig) {}

        uint64_t assemble(int64_t mux_val, uint64_t fd) {
            if (!_sig.is_active(mux_val))
                return 0;

            SignalCalculationType<T> val(_sig.decode(fd));
            _val = (_num_samples == 0) ? val : _val + val;
            ++_num_samples;

            return _sig.encode(_val.idivround(_num_samples).get_raw_value());
        }

        void reset() {
            _val = 0;
            _num_samples = 0;
        }
    };


    using SignalAssemblerVariant = std::variant<
        LastSignal<uint64_t>,
        LastSignal<int64_t>,
        LastSignal<double>,
        LastSignal<float>,
        AverageSignal<uint64_t>,
        AverageSignal<int64_t>,
        AverageSignal<double>,
        AverageSignal<float>
    >;

}