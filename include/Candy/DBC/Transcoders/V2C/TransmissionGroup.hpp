#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#include "Candy/CAN/CANKernelTypes.hpp"
#include "Candy/CAN/Frame/FramePacket.hpp"

namespace Candy {

    class TransmissionGroup {
        struct stamped_msg {
            CANTime stamp;
            canid_t message_id;
            int64_t message_mux;
            uint64_t mdata;
        };

        std::string _name;
        std::chrono::milliseconds _assemble_freq;
        CANTime _group_origin;

        std::vector<stamped_msg> _msg_clumps;

    public:
        TransmissionGroup(std::string_view name, uint32_t assemble_freq) :
            _name(name), _assemble_freq(assemble_freq)
        {}

        std::string_view name() const { return _name; }
        void time_begin(CANTime tp) { _group_origin = tp; }
        void try_publish(CANTime up_to, FramePacket& fp);
        void add_clumped(CANTime stamp, canid_t message_id, int64_t message_mux, uint64_t cval);
        bool within_interval(CANTime stamp) const;
        void assign(canid_t message_id, int64_t message_mux);


    private:
        void publish(CANTime tp, FramePacket& fp);
        bool all_collected() const;
    };

}