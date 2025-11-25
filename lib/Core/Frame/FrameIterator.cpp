#include <cstring>
#include <iostream>
#include <optional>

#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/Frame/FrameIterator.hpp"

namespace Candy {
    FrameIterator::FrameIterator(const FramePacket& fp)
        : frame_packet(fp), 
        payload_data(fp.payload()), 
        current_offset(0) {
        packet_utc = fp.utc();
    }

    FrameIterator::~FrameIterator() {}

    bool FrameIterator::operator==(const FrameIteratorSentinel&) const {
        if (frame_packet.is_empty())
            return true;

        return current_offset >= payload_data.size();
    }

    std::pair<CANTime, CANFrame> FrameIterator::operator*() {
        using namespace std::chrono;

        int32_t millis = transmit_at_offset<int32_t>(current_offset);
        CANFrame frame = transmit_at_offset<CANFrame>(current_offset + 4);

        return { CANTime(seconds(packet_utc)) + milliseconds(millis), std::move(frame) };
    }

    FrameIterator& FrameIterator::operator++() {
        if (frame_packet.is_empty())
            return *this;

        current_offset += 4 + sizeof(CANFrame);
        return *this;
    }

    template<typename IntType>
    IntType FrameIterator::transmit_at_offset(size_t offset) const {
        if (offset + sizeof(IntType) > payload_data.size()) {
            std::cerr << "Read beyond payload bounds" << std::endl;
            return {};
        }
        IntType value;
        std::memcpy(&value, payload_data.data() + offset, sizeof(IntType));
        return value;
    }

    template int16_t FrameIterator::transmit_at_offset(size_t val) const;
    template int32_t FrameIterator::transmit_at_offset(size_t val) const;
    template int64_t FrameIterator::transmit_at_offset(size_t val) const;

}