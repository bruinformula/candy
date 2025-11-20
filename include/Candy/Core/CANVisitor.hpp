#pragma once

#include "Candy/Core/CANIO.hpp"
#include "Candy/Core/DBC/DBCParser.hpp"

namespace Candy {

    template <typename... Visitors>
    class CANVisitor {
    public:
        std::tuple<Visitors&...> items;

        explicit CANVisitor(Visitors&... xs)
            : items(xs...)
        {}

        template <typename Func>
        inline void for_each_item(Func f) {
            std::apply([&](Visitors&... args) { (f(args), ...); }, items);
        }
    };
    
    
    template <typename... Visitors>
    class DBCVisitor : public DBCParser<DBCVisitor<Visitors&...>>, 
                       public CANVisitor<DBCVisitor<Visitors&...>> {
    public:
        bool parse_dbc(std::string_view dbc_src) { 
            constexpr int num_dbc_interpreters = (0 + ... + (HasParseDBC<Visitors> ? 1 : 0));
            int num_successful_parsed = 0; 
            for_each_item([&](auto& item) {
                if constexpr (HasParseDBC<decltype(item)>) {
                    if (item.parse_dbc(dbc_src)) {
                        num_successful_parsed += 1;
                    }
                }
            });
            return num_successful_parsed == num_dbc_interpreters;
        }
    };

    template <typename... Visitors>
    class CANWriterVisitor : public CANWriter<CANVisitor<Visitors&...>> {

        void write_message(const CANMessage& message) {
            for_each_item([&](auto& item) {
                if constexpr (CANWriteable<decltype(item)>) {
                    item.write_message(message);
                }
            });
        }
        
        void write_metadata(const CANDataStreamMetadata& metadata) {
            for_each_item([&](auto& item) {
                if constexpr (CANWriteable<decltype(item)>) {
                    item.write_metadata(metadata);
                }
            });
        }

        void write_raw_message(CANTime timestamp, CANFrame frame) {
            for_each_item([&](auto& item) {
                if constexpr (CANWriteable<decltype(item)>) {
                    item.write_raw_message(timestamp, frame);
                }
            });
        }

        void write_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            for_each_item([&](auto& item) {
                if constexpr (CANWriteable<decltype(item)>) {
                    item.write_table_message(table, data);
                }
            });
        }
    };

    template <typename... Visitors>
    class CANQueueWriterVisitor : public CANWriter<CANVisitor<Visitors&...>> {

        void flush_vtrl() {
            
        }

        void flush_async_vtrl(std::function<void()> callback) {

        }
        
        void flush_sync_vtrl() {
            
        }
    };


}