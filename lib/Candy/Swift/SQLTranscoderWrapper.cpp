#include "Candy/Swift/SQLTranscoderWrapper.hpp"
#include "Candy/DBCInterpreters/SQLTranscoder.hpp"

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

    void SQLTranscoderWrapper::flush() {
        if (transcoder.has_value()) {
            transcoder->flush_all_batches();
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
