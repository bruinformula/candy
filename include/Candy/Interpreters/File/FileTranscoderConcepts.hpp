#pragma once

#include <concepts>

#include "Candy/Core/CANIO.hpp"
#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {

    template <typename T>
    concept HasBatchFrame = requires(T t, std::pair<CANTime, CANFrame> sample) {
        { t.batch_frame(sample) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasBatchDecodedSignals = requires(T t, std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
        { t.batch_decoded_signals(sample, msg_def) } -> std::same_as<void>;
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
    concept FileTranscodable =  HasBatchFrame<T> &&
                                HasBatchDecodedSignals<T> &&
                                HasFlushFramesBatch<T> &&
                                HasFlushDecodedSignalsBatch<T> &&
                                HasFlushAllBatches<T> &&
                                IsCANWritable<T> &&
                                CANStoreReadable<T> && 
                                CANStoreWriteable<T>;
}