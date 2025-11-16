#include <cstring>

#include "Candy/CAN/CANKernelTypes.hpp"
#include "Candy/CAN/Frame/FrameIterator.hpp"

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

        int32_t millis = read_at_offset<int32_t>(current_offset);
        CANFrame frame = read_at_offset<CANFrame>(current_offset + 4);

        return { CANTime(seconds(packet_utc)) + milliseconds(millis), std::move(frame) };
    }

    FrameIterator& FrameIterator::operator++() {
        if (frame_packet.is_empty())
            return *this;

        current_offset += 4 + sizeof(CANFrame);
        return *this;
    }

    template<typename IntType>
    IntType FrameIterator::read_at_offset(size_t offset) const {
        if (offset + sizeof(IntType) > payload_data.size()) {
            throw std::out_of_range("Read beyond payload bounds");
        }
        IntType value;
        std::memcpy(&value, payload_data.data() + offset, sizeof(IntType));
        return value;
    }

    template int16_t FrameIterator::read_at_offset(size_t val) const;
    template int32_t FrameIterator::read_at_offset(size_t val) const;
    template int64_t FrameIterator::read_at_offset(size_t val) const;

}