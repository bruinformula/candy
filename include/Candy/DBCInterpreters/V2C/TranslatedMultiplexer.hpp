#pragma once 

#include <cstdint>

#include "Candy/Core/Signal/SignalCodec.hpp"

namespace Candy {

    class TranslatedMultiplexer {
        SignalCodec _codec;
    public:
        TranslatedMultiplexer(SignalCodec codec) : _codec(codec) {}

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