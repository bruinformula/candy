#include <cstring>

#include "Candy/Core/Frame/FramePacket.hpp"

namespace Candy {

    FramePacket::FramePacket() {}

    FramePacket::FramePacket(base buff) : _buff(std::move(buff)) {}

    void FramePacket::prepare(uint32_t utc) {
        _buff.resize(0);
        _buff.reserve(32 * 1024);

        constexpr uint16_t dbc_version = 100;
        append(dbc_version);
        append(utc);
    }

    uint32_t FramePacket::utc() const {
        return *reinterpret_cast<const uint32_t*>(_buff.data() + 2);
    }

    bool FramePacket::is_empty() const {
        return _buff.size() <= 6;
    }

    size_t FramePacket::byte_size() const {
        return _buff.size();
    }

    const uint8_t* FramePacket::begin() const {
        return _buff.data();
    }

    const uint8_t* FramePacket::end() const {
        return _buff.data() + _buff.size();
    }

    std::span<const uint8_t> FramePacket::data() const {
        return std::span<const uint8_t>(_buff.data(), _buff.size());
    }

    std::span<const uint8_t> FramePacket::payload() const {
        if (_buff.size() <= 6) {
            return {};
        }
        return std::span<const uint8_t>(_buff.data() + 6, _buff.size() - 6);
    }

    void FramePacket::append(CANFrame frame) {
		const uint8_t* b = (const uint8_t*)&frame;
		_buff.insert(_buff.end(), b, b + sizeof(CANFrame));
	}

    template <typename IntType>
    void FramePacket::append(IntType val) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&val);
        _buff.insert(_buff.end(), b, b + sizeof(IntType));
    }

    template <typename IntType>
    IntType FramePacket::transmit_at(size_t offset) const {
        if (offset + sizeof(IntType) > _buff.size()) {
            throw std::out_of_range("Read beyond buffer bounds");
        }
        IntType value;
        std::memcpy(&value, _buff.data() + offset, sizeof(IntType));
        return value;
    }
    

    std::vector<uint8_t> FramePacket::release() {
        return std::move(_buff);
    }

    template void FramePacket::append(int16_t val);
    template void FramePacket::append(int32_t val);
    template void FramePacket::append(int64_t val);

    template int16_t FramePacket::transmit_at(size_t offset) const;
    template int32_t FramePacket::transmit_at(size_t offset) const;
    template int64_t FramePacket::transmit_at(size_t offset) const;

}

