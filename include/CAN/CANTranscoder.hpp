#pragma once 
#include <iostream>
#include <limits>
#include <type_traits>
#include <optional>


#include "boost/spirit/home/x3.hpp"


namespace CAN {

    // Concepts for each required function signature
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
    concept HasFlush = requires(T t) {
        { t.flush() } -> std::same_as<void>;
    };

    template <typename T>
    concept HasFlushSync = requires(T t) {
        { t.flush_sync() } -> std::same_as<void>;
    };

    template <typename T>
    concept HasFlushAsync = requires(T t, std::function<void()> callback) {
        { t.flush_async(callback) } -> std::same_as<void>;
    };

    template <typename Derived>
    struct CANTranscoder {
        void transcode_vrtl(CANTime timestamp, CANFrame frame) {
            if constexpr (HasTranscodeCAN<Derived>) {
                static_cast<Derived&>(*this).transcode(timestamp, frame);
            }
        }
        void transcode_vrtl(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            if constexpr (HasTranscodeTable<Derived>) {
                static_cast<Derived&>(*this).transcode(table, data);
            }
        }
        std::future<void> transcode_async_vrtl(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            if constexpr (HasTranscodeAsync<Derived>) {
                return static_cast<Derived&>(*this).transcode_async(table, data);
            }
            return std::future<void>{};
        }
        void flush_vrtl() {
            if constexpr (HasFlush<Derived>) {
                static_cast<Derived&>(*this).flush();
            }
        }
        void flush_sync_vrtl() {
            if constexpr (HasFlushSync<Derived>) {
                static_cast<Derived&>(*this).flush_sync();
            }
        }
        void flush_async_vrtl(std::function<void()> callback) {
            if constexpr (HasFlushAsync<Derived>) {
                static_cast<Derived&>(*this).flush_async(callback);
            }
        }
    };


}

