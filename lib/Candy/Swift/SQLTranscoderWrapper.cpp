#include "Candy/Swift/SQLTranscoderWrapper.hpp"
#include "Candy/DBCInterpreters/SQLTranscoder.hpp"
#include "Candy/Core/CANHelpers.hpp"

namespace Candy {

    SQLTranscoderWrapper::SQLTranscoderWrapper(const std::string& db_path) 
        : transcoder(SQLTranscoder::create(db_path, 10000)),
          ref_count(1) {}

    SQLTranscoderWrapper::SQLTranscoderWrapper(const SQLTranscoderWrapper& other)
        : transcoder(std::nullopt),
          ref_count(1) {
        // Note: We cannot copy SQLTranscoder as it's move-only
        // Swift's ARC will handle creating new instances via create()
    }

    SQLTranscoderWrapper::SQLTranscoderWrapper(SQLTranscoderWrapper&& other)
        : transcoder(std::move(other.transcoder)),
          ref_count(1) {
        // Move the transcoder from other
    }

    SQLTranscoderWrapper::~SQLTranscoderWrapper() = default;

    SQLTranscoderWrapper* SQLTranscoderWrapper::create(const std::string& db_path) {
        auto* wrapper = new SQLTranscoderWrapper(db_path);
        if (!wrapper->transcoder.has_value()) {
            delete wrapper;
            return nullptr;
        }
        return wrapper;
    }

    SQLTranscoder* SQLTranscoderWrapper::getTranscoder() {
        return transcoder.has_value() ? &(*transcoder) : nullptr;
    }

    bool SQLTranscoderWrapper::parseDBC(const std::string& dbc_content) {
        if (!transcoder.has_value()) {
            return false;
        }
        return transcoder->parse_dbc(dbc_content);
    }

    void SQLTranscoderWrapper::flush() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
        }
    }

    // CANIO methods
    void SQLTranscoderWrapper::receiveMessage(const CANMessage& message) {
        if (transcoder.has_value()) {
            transcoder->receive_message(message);
        }
    }

    void SQLTranscoderWrapper::receiveRawMessage(std::pair<CANTime, CANFrame> sample) {
        if (transcoder.has_value()) {
            transcoder->receive_raw_message(sample);
        }
    }

    void SQLTranscoderWrapper::receiveMetadata(const CANDataStreamMetadata& metadata) {
        if (transcoder.has_value()) {
            transcoder->receive_metadata(metadata);
        }
    }

    std::vector<CANMessage> SQLTranscoderWrapper::transmitMessages(canid_t can_id) {
        if (transcoder.has_value()) {
            return transcoder->transmit_messages(can_id);
        }
        return {};
    }

    std::vector<CANMessage> SQLTranscoderWrapper::transmitMessagesInRange(canid_t can_id, CANTime start, CANTime end) {
        if (transcoder.has_value()) {
            return transcoder->transmit_messages_in_range(can_id, start, end);
        }
        return {};
    }

    const CANDataStreamMetadata& SQLTranscoderWrapper::transmitMetadata() {
        static CANDataStreamMetadata empty_metadata;
        if (transcoder.has_value()) {
            return transcoder->transmit_metadata();
        }
        return empty_metadata;
    }

    // Transcoder methods
    void SQLTranscoderWrapper::batchFrame(std::pair<CANTime, CANFrame> sample) {
        if (transcoder.has_value()) {
            transcoder->batch_frame(sample);
        }
    }

    void SQLTranscoderWrapper::batchDecodedSignals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
        if (transcoder.has_value()) {
            transcoder->batch_decoded_signals(sample, msg_def);
        }
    }

    void SQLTranscoderWrapper::flushFramesBatch() {
        if (transcoder.has_value()) {
            transcoder->flush_frames_batch();
        }
    }

    void SQLTranscoderWrapper::flushDecodedSignalsBatch() {
        if (transcoder.has_value()) {
            transcoder->flush_decoded_signals_batch();
        }
    }

    void SQLTranscoderWrapper::flushAllBatches() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
        }
    }

    void SQLTranscoderWrapper::storeMessageMetadata(canid_t message_id, const std::string& message_name, size_t message_size) {
        if (transcoder.has_value()) {
            transcoder->store_message_metadata(message_id, message_name, message_size);
        }
    }

} // namespace Candy

void retainSQLTranscoderWrapper(Candy::SQLTranscoderWrapper* wrapper) {
    if (wrapper) {
        wrapper->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void releaseSQLTranscoderWrapper(Candy::SQLTranscoderWrapper* wrapper) {
    if (wrapper && wrapper->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete wrapper;
    }
}
