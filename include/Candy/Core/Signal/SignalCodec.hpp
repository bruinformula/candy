#pragma once

#include <bit>
#include <cstdint>

namespace Candy {

  class SignalCodec {
    using order = std::endian;

    unsigned _start_bit, _bit_size;
    order _byte_order;
    char _sign_type;
    unsigned _byte_pos, _bit_pos, _last_bit_pos, _nbytes;

  public:
    SignalCodec(unsigned sb, unsigned bs, char bo, char st);

    uint64_t operator()(const uint8_t* data) const;
    void operator()(uint64_t raw, void* buffer) const;

    char sign_type() const;
  };
}