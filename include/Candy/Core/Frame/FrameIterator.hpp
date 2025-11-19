#pragma once

#include <span>
#include <cstring>
#include <utility>

#include "Candy/Core/CANKernelTypes.hpp"

#include "FramePacket.hpp"

namespace Candy {
    class FrameIteratorSentinel{};

    class FrameIterator {
        const FramePacket& frame_packet;
        uint32_t packet_utc = 0;
        std::span<const uint8_t> payload_data;
        size_t current_offset = 0;

    public:
        explicit FrameIterator(const FramePacket& fp);
        ~FrameIterator();

        bool operator==(const FrameIteratorSentinel&) const;
        std::pair<CANTime, CANFrame> operator*();
        FrameIterator& operator++();
        
    private:
        template<typename IntType>
        IntType read_at_offset(size_t offset) const;
    };

    inline FrameIterator begin(const FramePacket& fp) {
        return FrameIterator(fp);
    }

    inline FrameIteratorSentinel end(const FramePacket&) {
        return {};
    }
}