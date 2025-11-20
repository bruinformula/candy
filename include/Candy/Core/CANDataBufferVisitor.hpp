#pragma once
#include <vector>
#include <unordered_map>

#include "Candy/Buffer/BackendInterface.hpp"
#include "Candy/Core/CANIO.hpp"
#include "Candy/Core/CANVisitor.hpp"
#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {
    template <typename... Vs>
    class CANDBufferVisitor : public CANQueueWriter<CANDBufferVisitor<Vs&...>>, 
                              public CANWriter<CANDBufferVisitor<Vs&...>>,
                              public CANVisitor<CANDBufferVisitor<Vs&...>> {
    private:
        std::unordered_map<canid_t, std::vector<CANMessage>> message_buffers;
        CANDataStreamMetadata metadata;
        std::unique_ptr<BackendInterface> backend;
        size_t max_buffer_size;
        bool auto_flush_enabled{true};
        size_t auto_flush_threshold{1000};

        void update_metadata_for_message(const CANMessage& message);
        void check_auto_flush();
    };
}