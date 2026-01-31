#include "Candy/Swift/CSVTranscoderWrapper.hpp"
#include "Candy/DBCInterpreters/CSVTranscoder.hpp"

void retainCSVTranscoderWrapper(Candy::CSVTranscoderWrapper* wrapper) {
    if (wrapper) {
        wrapper->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void releaseCSVTranscoderWrapper(Candy::CSVTranscoderWrapper* wrapper) {
    if (wrapper && wrapper->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete wrapper;
    }
}


namespace Candy {

    CSVTranscoderWrapper::CSVTranscoderWrapper(const std::string& db_path) 
        : transcoder(CSVTranscoder::create(db_path, 10000)),
          ref_count(1) {}

    CSVTranscoderWrapper::CSVTranscoderWrapper(const CSVTranscoderWrapper& other)
        : transcoder(std::nullopt),
          ref_count(1) {
        // Note: We cannot copy CSVTranscoder as it's move-only
        // Swift's ARC will handle creating new instances via create()
    }

    CSVTranscoderWrapper::CSVTranscoderWrapper(CSVTranscoderWrapper&& other)
        : transcoder(std::move(other.transcoder)),
          ref_count(1) {
        // Move the transcoder from other
    }

    CSVTranscoderWrapper::~CSVTranscoderWrapper() = default;

    CSVTranscoderWrapper* CSVTranscoderWrapper::create(const std::string& db_path) {
        auto* wrapper = new CSVTranscoderWrapper(db_path);
        if (!wrapper->transcoder.has_value()) {
            delete wrapper;
            return nullptr;
        }
        return wrapper;
    }

    CSVTranscoder* CSVTranscoderWrapper::getTranscoder() {
        return transcoder.has_value() ? &(*transcoder) : nullptr;
    }

    bool CSVTranscoderWrapper::parseDBC(const std::string& dbc_content) {
        if (!transcoder.has_value()) {
            return false;
        }
        return transcoder->parse_dbc(dbc_content);
    }

    void CSVTranscoderWrapper::flush() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
        }
    }

    // CANIO methods
    void CSVTranscoderWrapper::receiveMessage(const CANMessage& message) {
        if (transcoder.has_value()) {
            transcoder->receive_message(message);
        }
    }

    void CSVTranscoderWrapper::receiveRawMessage(std::pair<CANTime, CANFrame> sample) {
        if (transcoder.has_value()) {
            transcoder->receive_raw_message(sample);
        }
    }

    void CSVTranscoderWrapper::receiveMetadata(const CANDataStreamMetadata& metadata) {
        if (transcoder.has_value()) {
            transcoder->receive_metadata(metadata);
        }
    }

    std::vector<CANMessage> CSVTranscoderWrapper::transmitMessages(canid_t can_id) {
        if (transcoder.has_value()) {
            return transcoder->transmit_messages(can_id);
        }
        return {};
    }

    std::vector<CANMessage> CSVTranscoderWrapper::transmitMessagesInRange(canid_t can_id, CANTime start, CANTime end) {
        if (transcoder.has_value()) {
            return transcoder->transmit_messages_in_range(can_id, start, end);
        }
        return {};
    }

    const CANDataStreamMetadata& CSVTranscoderWrapper::transmitMetadata() {
        static CANDataStreamMetadata empty_metadata;
        if (transcoder.has_value()) {
            return transcoder->transmit_metadata();
        }
        return empty_metadata;
    }

    // Transcoder methods
    void CSVTranscoderWrapper::batchFrame(std::pair<CANTime, CANFrame> sample) {
        if (transcoder.has_value()) {
            transcoder->batch_frame(sample);
        }
    }

    void CSVTranscoderWrapper::batchDecodedSignals(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
        if (transcoder.has_value()) {
            transcoder->batch_decoded_signals(sample, msg_def);
        }
    }

    void CSVTranscoderWrapper::flushFramesBatch() {
        if (transcoder.has_value()) {
            transcoder->flush_frames_batch();
        }
    }

    void CSVTranscoderWrapper::flushDecodedSignalsBatch() {
        if (transcoder.has_value()) {
            transcoder->flush_decoded_signals_batch();
        }
    }

    void CSVTranscoderWrapper::flushAllBatches() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
        }
    }

    void CSVTranscoderWrapper::storeMessageMetadata(canid_t message_id, const std::string& message_name, size_t message_size) {
        if (transcoder.has_value()) {
            transcoder->store_message_metadata(message_id, message_name, message_size);
        }
    }

} // namespace Candy