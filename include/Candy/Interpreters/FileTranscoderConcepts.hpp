#pragma once

#include <concepts>

#include "Candy/CAN/CANKernelTypes.hpp"


namespace Candy {

    template <typename T>
    concept HasBatchFrame = requires(T t, CANTime timestamp, CANFrame frame) {
        { t.batch_frame(timestamp, frame) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasBatchDecodedSignals = requires(T t, CANTime timestamp, CANFrame frame, const MessageDefinition& msg_def) {
        { t.batch_decoded_signals(timestamp, frame, msg_def) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasFlushFramesBatch = requires(T t) {
        { t.flush_frames_batch() } -> std::same_as<void>;
    };

    template <typename T>
    concept HasFlushDecodedSignalsBatch = requires(T t) {
        { t.flush_decoded_signals_batch() } -> std::same_as<void>;
    };

    template <typename T>
    concept HasFlushAllBatches = requires(T t) {
        { t.flush_all_batches() } -> std::same_as<void>;
    };

    template <typename T>
    concept FileTranscodable = HasBatchFrame<T> &&
                                HasBatchDecodedSignals<T> &&
                                HasFlushFramesBatch<T> &&
                                HasFlushDecodedSignalsBatch<T> &&
                                HasFlushAllBatches<T>;
}