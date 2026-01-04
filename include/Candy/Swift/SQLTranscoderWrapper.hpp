#pragma once

#include <atomic>
#include <string>
#include <optional>

#include "Candy/DBCInterpreters/SQLTranscoder.hpp"

#if __has_include(<swift/bridging>)
#include <swift/bridging>
#else

#define SWIFT_SHARED_REFERENCE(retain, release)
#define SWIFT_RETURNS_INDEPENDENT_VALUE
#define SWIFT_RETURNS_RETAINED
#endif

namespace Candy {
    class SQLTranscoderWrapper;
}

void retainSQLTranscoderWrapper(Candy::SQLTranscoderWrapper* _Nonnull wrapper);
void releaseSQLTranscoderWrapper(Candy::SQLTranscoderWrapper* _Nonnull wrapper);

namespace Candy {

    // these are setup to be the have reference semantics the same as swift classes
    // https://www.swift.org/documentation/articles/value-and-reference-types.html
    class SWIFT_SHARED_REFERENCE(retainSQLTranscoderWrapper, releaseSQLTranscoderWrapper)
    SQLTranscoderWrapper {
    public:
        SQLTranscoderWrapper(const SQLTranscoderWrapper& other);
        SQLTranscoderWrapper(SQLTranscoderWrapper&& other);
        SQLTranscoderWrapper& operator=(const SQLTranscoderWrapper&) = delete;
        SQLTranscoderWrapper& operator=(SQLTranscoderWrapper&&) = delete;
        
        ~SQLTranscoderWrapper();

        static SQLTranscoderWrapper* _Nullable create(const std::string& db_path) SWIFT_RETURNS_RETAINED;

        SQLTranscoder* _Nullable getTranscoder() SWIFT_RETURNS_INDEPENDENT_VALUE;

        void flush();

    private:
        std::optional<SQLTranscoder> transcoder;
        std::atomic<int> ref_count;

        SQLTranscoderWrapper(const std::string& db_path);

        friend void ::retainSQLTranscoderWrapper(SQLTranscoderWrapper* _Nonnull);
        friend void ::releaseSQLTranscoderWrapper(SQLTranscoderWrapper* _Nonnull);
    };

}
