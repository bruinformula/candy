#pragma once

#include <concepts>

#include "CAN/CANKernelTypes.hpp"


namespace CAN {
    template <typename T>
    concept HasTranscodeCAN = requires(T t, CANTime timestamp, CANFrame frame) {
        { t.transcode(timestamp, frame) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasTranscodeTable = requires(T t, std::string table, std::vector<std::pair<std::string, std::string>> data) {
        { t.transcode(table, data) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasTranscodeAsync = requires(T t, std::string table, std::vector<std::pair<std::string, std::string>> data) {
        { t.transcode_async(table, data) } -> std::same_as<std::future<void>>;
    };

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
}