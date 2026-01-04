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

    void CSVTranscoderWrapper::flush() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
        }
    }

} // namespace Candy