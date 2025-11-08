#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include "CAN/signal/NumericValue.hpp"
#include "CAN/signal/SignalCodec.hpp"

namespace CAN {

    class TranslatedSignal {
        std::string _name;
        SignalCodec _codec;

        std::string _agg_type = "LAST";
        NumericValueType _val_type = CAN::NumericValueType::i64;
        std::optional<int64_t> _mux_val; 
    public:
        TranslatedSignal(std::string name, SignalCodec codec, std::optional<int64_t> mux_val)
            : _name(std::move(name)), _codec(codec), _mux_val(mux_val)
        {}

        const std::string& name() const { return _name; }
        std::optional<int64_t> mux_val() const { return _mux_val; }

        bool is_active(uint64_t frame_mux_val) const {
            return !_mux_val || _mux_val == frame_mux_val;
        }
        
        std::string_view agg_type() const { return _agg_type; }
        void agg_type(const std::string& agg_type) { _agg_type = agg_type; }
        NumericValueType value_type() const { return _val_type; }

        void value_type(unsigned vt) {
            _val_type = NumericValueType(vt);
            if (_val_type == CAN::NumericValueType::i64 && _codec.sign_type() == '+')
                _val_type = CAN::NumericValueType::u64;
        }

        uint64_t decode(uint64_t data) const {
            return _codec((uint8_t*)&data);
        }

        uint64_t encode(uint64_t raw) const {
            uint64_t rv = 0;
            _codec(raw, (void*)&rv);
            return rv;
        }
    };

}