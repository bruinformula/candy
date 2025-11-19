#pragma once

#include <vector>
#include <span>
#include <cstring>

#include "Candy/Core/CANKernelTypes.hpp"

namespace Candy {
    //CANFrame nonmutex helper function

    inline void use_non_muxed(CANFrame& cf, bool use) {
        if (use) cf.__res0 |= 0x1;
        else cf.__res0 &= ~0x1;
    }

    inline bool use_non_muxed(const CANFrame& cf) {
        return cf.__res0 & 0x1;
    }

    class FramePacket {
        using base = std::vector<uint8_t>;
        base _buff;

    public:
        FramePacket();
        FramePacket(base buff);

        FramePacket(FramePacket&&) = default;
        FramePacket& operator=(FramePacket&&) = default;
        FramePacket(const FramePacket&) = delete;
        FramePacket& operator=(const FramePacket&) = delete;

        void prepare(uint32_t utc);
        uint32_t utc() const;

        bool is_empty() const;
        size_t byte_size() const;

        const uint8_t* begin() const;
        const uint8_t* end() const;
        
        // Type-safe span-based access
        std::span<const uint8_t> data() const;
        std::span<const uint8_t> payload() const; // Data after header (UTC + version)

        void append(CANFrame frame);

        template <typename IntType>
        void append(IntType val);
        
        // Type-safe data access
        template <typename T>
        T read_at(size_t offset) const;

        std::vector<uint8_t> release();
    };
}