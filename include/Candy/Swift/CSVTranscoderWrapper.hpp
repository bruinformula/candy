#pragma once

#include <atomic>
#include <string>
#include <optional>

#include "Candy/DBCInterpreters/CSVTranscoder.hpp"

#if __has_include(<swift/bridging>)
#include <swift/bridging>
#else

#define SWIFT_SHARED_REFERENCE(retain, release)
#define SWIFT_RETURNS_INDEPENDENT_VALUE
#define SWIFT_RETURNS_RETAINED
#endif

namespace Candy {
    class CSVTranscoderWrapper;
}

void retainCSVTranscoderWrapper(Candy::CSVTranscoderWrapper* _Nonnull wrapper);
void releaseCSVTranscoderWrapper(Candy::CSVTranscoderWrapper* _Nonnull wrapper);

namespace Candy {

    // these are setup to be the have reference semantics the same as swift classes
    // https://www.swift.org/documentation/articles/value-and-reference-types.html
    class SWIFT_SHARED_REFERENCE(retainCSVTranscoderWrapper, releaseCSVTranscoderWrapper)
    CSVTranscoderWrapper {
    public:
        CSVTranscoderWrapper(const CSVTranscoderWrapper& other);
        CSVTranscoderWrapper(CSVTranscoderWrapper&& other);
        CSVTranscoderWrapper& operator=(const CSVTranscoderWrapper&) = delete;
        CSVTranscoderWrapper& operator=(CSVTranscoderWrapper&&) = delete;
        
        ~CSVTranscoderWrapper();

        static CSVTranscoderWrapper* _Nullable create(const std::string& base_path) SWIFT_RETURNS_RETAINED;

        CSVTranscoder* _Nullable getTranscoder() SWIFT_RETURNS_INDEPENDENT_VALUE;

        void flush();

    private:
        std::optional<CSVTranscoder> transcoder;
        std::atomic<int> ref_count;

        CSVTranscoderWrapper(const std::string& base_path);

        friend void ::retainCSVTranscoderWrapper(CSVTranscoderWrapper* _Nonnull);
        friend void ::releaseCSVTranscoderWrapper(CSVTranscoderWrapper* _Nonnull);
    };

}
