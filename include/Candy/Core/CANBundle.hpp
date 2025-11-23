#pragma once

#include "Candy/Core/CANIO.hpp"
#include "Candy/Core/DBC/DBCParser.hpp"

namespace Candy {

    //Parse
    template <typename... Items>
    requires (IsDBCParsable<Items> && ...)
    struct DBCBundle : 
        public DBCParser<DBCBundle<Items&...>> 
    {
        std::tuple<Items&...> writers;

        DBCBundle(Items&... xs)
            : writers(xs...)
        {}
        
        template <typename Func>
        inline void for_each_item(Func f) {
            std::apply([&](Items&... args) { (f(args), ...); }, writers);
        }
        
        bool parse_dbc(std::string_view dbc_src) { 
            constexpr int num_dbc_interpreters = sizeof...(Items);
            int num_successful_parsed = 0; 
            for_each_item([&](auto& item) {
                if (item.parse_dbc(dbc_src)) {
                    num_successful_parsed += 1;
                }
            });
            return num_successful_parsed == num_dbc_interpreters;
        }
        
    };

    // One Read / Many Write
    template <typename Reader, typename... Writers>
    requires (IsCANWritable<Writers> && ...)
    struct CANManyWriterBundle : 
        public CANReadable<CANManyWriterBundle<Reader&, Writers&...>>,
        public CANWritable<CANManyWriterBundle<Reader&, Writers&...>> 
    {
    public:
        Reader& reader;
        std::tuple<Writers&...> writers;

        CANManyWriterBundle(Writers&... writers, Reader reader) :
            reader(reader),
            writers(writers...)
        {}

        const CANMessage& read_message() {
            return reader.read_message();
        }

        const std::pair<CANTime, CANFrame>& read_raw_message() {
            return reader.read_raw_message();
        }

        const std::tuple<std::string, TableType>& read_table_message() {
            return reader.read_table_message();
        }
        const CANDataStreamMetadata& read_metadata() {
            return reader.read_metadata();
        }

        //CANWriteable
        void write_message(const CANMessage& message) {
            for_each_item([&](auto& item) {
                item.write_message(message);
            });
        }
        
        void write_metadata(const CANDataStreamMetadata& metadata) {
            for_each_item([&](auto& item) {
                item.write_metadata(metadata);
            });
        }

        void write_raw_message(std::pair<CANTime, CANFrame> sample) {
            for_each_item([&](auto& item) {
                item.write_raw_message(sample);
            });
        }

        void write_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            for_each_item([&](auto& item) {
                item.write_table_message(table, data);
            });
        }

    };

    // One Read / One Write
    template <typename Reader, typename Writer>
    requires IsCANReadable<Reader> && IsCANWritable<Writer>
    struct CANPairBundle : 
        public CANReadable<CANPairBundle<Reader&, Writer&>>, 
        public CANWritable<CANPairBundle<Reader&, Writer&>> 
    {
        Reader& reader;
        Writer& writer; 

        CANPairBundle(Reader& reader, Writer& writer) :
            reader(reader),
            writer(writer)
        {}

        //CANReadable 
        const CANMessage& read_message() {
            return reader.read_message();
        }

        const std::pair<CANTime, CANFrame>& read_raw_message() {
            return reader.read_raw_message();
        }

        const std::tuple<std::string, TableType>& read_table_message() {
            return reader.read_table_message();
        }
        const CANDataStreamMetadata& read_metadata() {
            return reader.read_metadata();
        }

        //CANWriteable
        void write_message(const CANMessage& message) {
            writer.write_message(message);
        }
        
        void write_metadata(const CANDataStreamMetadata& metadata) {
            writer.write_metadata(metadata);
        }

        void write_raw_message(std::pair<CANTime, CANFrame> sample) {
            writer.write_raw_message(sample);
        }

        void write_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            writer.write_table_message(table, data);  
        }

    };
    // Batch Read / One Write
    template <size_t BatchSize, typename Writer, typename BatchReader>
    requires IsCANBatchReadable<BatchSize, BatchReader> && IsCANWritable<Writer>
    struct CANBatchReaderBundle : 
        public CANBatchReadable<BatchSize, CANBatchReaderBundle<BatchSize, Writer&, BatchReader&>>, 
        public CANWritable<CANBatchReaderBundle<BatchSize, Writer&, BatchReader&>> 
    {
        BatchReader& batch_reader; 
        Writer& writer;

        CANBatchReaderBundle(BatchReader& batch_reader, Writer& writer) : 
            batch_reader(batch_reader), 
            writer(writer)
        {}
        //CANBatchReadable
        const std::array<CANMessage, BatchSize>& read_message_batch() {
            return batch_reader.read_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& read_raw_message_batch() {
            return batch_reader.read_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& read_table_message_batch() {
            return batch_reader.read_table_message_batch();
        }
        //CANWritable
        void write_message(const CANMessage& message) {
            writer.write_message(message);
        }
        
        void write_metadata(const CANDataStreamMetadata& metadata) {
            writer.write_metadata(metadata);
        }

        void write_raw_message(const std::pair<CANTime, CANFrame>& sample) {
            writer.write_raw_message(sample);
        }

        void write_table_message(const std::string& table, const TableType& data) {
            writer.write_table_message(table, data);
        }

    };
    // One Read / Batch Write
    template <size_t BatchSize, typename Reader, typename BatchWriter>
    requires IsCANReadable<Reader> && IsCANBatchWritable<BatchSize, BatchWriter>
    struct CANBatchWriterBundle : 
        public CANReadable<CANBatchWriterBundle<BatchSize, Reader&, BatchWriter&>>, 
        public CANBatchWritable<BatchSize, CANBatchWriterBundle<BatchSize, Reader&, BatchWriter&>> 
    {
        Reader& reader; 
        BatchWriter& batch_writer;

        CANBatchWriterBundle(Reader& reader, BatchWriter& batch_writer) : 
            reader(reader), 
            batch_writer(batch_writer)
        {}

        //CANReadable
        const CANMessage& read_message() {
            return reader.read_message();
        }

        const std::pair<CANTime, CANFrame>& read_raw_message() {
            return reader.read_raw_message();
        }

        const std::tuple<std::string, TableType>& read_table_message() {
            return reader.read_table_message();
        }

        const CANDataStreamMetadata& read_metadata() {
            return reader.read_metadata();
        }
        //CANBatchWritable
        void write_message_batch(const std::array<CANMessage, BatchSize>& message) {
            batch_writer.write_message_batch(message);
        }

        void write_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            batch_writer.write_raw_message_batch(samples);
        }

        void write_table_message_batch(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            batch_writer.write_table_message_batch(table, data);
        }

    };

    // Batch Read / Batch Write
    template <size_t BatchSize, typename BatchReader, typename BatchWriter>
    requires IsCANBatchReadable<BatchSize, BatchReader> && IsCANBatchWritable<BatchSize, BatchWriter>
    struct CANBatchBundle : 
        public CANBatchReadable<BatchSize, CANBatchBundle<BatchSize, BatchReader&, BatchWriter&>>, 
        public CANBatchWritable<BatchSize, CANBatchBundle<BatchSize, BatchReader&, BatchWriter&>> 
    {
        BatchReader& batch_reader; 
        BatchWriter& batch_writer;

        CANBatchBundle(BatchReader& batch_reader, BatchWriter& batch_writer) : 
            batch_reader(batch_reader), 
            batch_writer(batch_writer)
        {}

        //CANBatchReadable
        const std::array<CANMessage, BatchSize>& read_message_batch() {
            return batch_reader.read_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& read_raw_message_batch() {
            return batch_reader.read_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& read_table_message_batch() {
            return batch_reader.read_table_message_batch();
        }

        //CANBatchWritable
        void write_message_batch(const std::array<CANMessage, BatchSize>& message) {
            batch_writer.write_message_batch(message);
        }

        void write_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            batch_writer.write_raw_message_batch(samples);
        }

        void write_table_message_batch(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            batch_writer.write_table_message_batch(table, data);
        }

    };
    
    template <size_t BatchReaderSize, size_t BatchWriterSize, typename BatchReader, typename BatchWriter>
    requires IsCANBatchReadable<BatchReaderSize, BatchReader> && IsCANBatchWritable<BatchWriterSize, BatchWriter>
    struct CANDualBatchBundle : 
        public CANBatchReadable<BatchReaderSize, CANDualBatchBundle<BatchReaderSize, BatchWriterSize, BatchReader&, BatchWriter&>>, 
        public CANBatchWritable<BatchWriterSize, CANDualBatchBundle<BatchReaderSize, BatchWriterSize, BatchReader&, BatchWriter&>> 
    {
        BatchReader& batch_reader; 
        BatchWriter& batch_writer;

        CANDualBatchBundle(BatchReader& batch_reader, BatchWriter& batch_writer) : 
            batch_reader(batch_reader), 
            batch_writer(batch_writer)
        {}

        // CANBatchReadable (BatchReaderSize)
        const std::array<CANMessage, BatchReaderSize>& read_message_batch() {
            return batch_reader.read_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchReaderSize>& read_raw_message_batch() {
            return batch_reader.read_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchReaderSize>& read_table_message_batch() {
            return batch_reader.read_table_message_batch();
        }

        // CANBatchWritable (BatchWriterSize)
        void write_message_batch(const std::array<CANMessage, BatchWriterSize>& message) {
            batch_writer.write_message_batch(message);
        }

        void write_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchWriterSize>& samples) {
            batch_writer.write_raw_message_batch(samples);
        }

        void write_table_message_batch(const std::array<std::string, BatchWriterSize>& table, const std::array<TableType, BatchWriterSize>& data) {
            batch_writer.write_table_message_batch(table, data);
        }
    };

    // Batch Read // errrr 
    template <size_t BatchSize = 0, typename Reader = void>
    requires IsCANReadable<Reader> && IsCANBatchReadable<BatchSize, Reader>
    struct NullReader :
        public CANReadable<NullReader<BatchSize, Reader>>,
        public CANBatchReadable<BatchSize, NullReader<BatchSize, Reader>>
    {
        //CANReadable
        const CANMessage& read_message() {
            throw std::runtime_error("NullReader: no data");
        }
        const std::pair<CANTime, CANFrame>& read_raw_message() {
            throw std::runtime_error("NullReader: no data");
        }
        const std::tuple<std::string, TableType>& read_table_message() {
            throw std::runtime_error("NullReader: no data");
        }
        const CANDataStreamMetadata& read_metadata() = delete;

        //CANBatchReadable
        const std::array<CANMessage, BatchSize>& read_message_batch() {
            throw std::runtime_error("NullReader: no data");
        }
        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& read_raw_message_batch() {
            throw std::runtime_error("NullReader: no data");
        }
        const std::array<std::tuple<std::string, TableType>, BatchSize>& read_table_message_batch() {
            throw std::runtime_error("NullReader: no data");
        }
    };


}