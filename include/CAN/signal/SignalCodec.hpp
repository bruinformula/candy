#pragma once

#ifdef BOOST_USE_SYSTEM
  #include <boost/endian.hpp>
#else
  #include "boost/endian.hpp"
#endif

#include "CAN/CANKernelTypes.hpp"

namespace CAN {

  class SignalCodec {
    using order = boost::endian::order;

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