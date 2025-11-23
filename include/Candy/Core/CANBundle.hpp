#pragma once

#include "Candy/Core/DBC/DBCParser.hpp"
#include "Candy/Core/CANIOConcepts.hpp"


namespace Candy {
    //Forward Declarations
    template<typename Derived>
    struct CANReceivable;

    template<size_t BatchSize, typename Derived>
    struct CANBatchReceivable;

    template<typename Derived>
    struct CANTransmittable;

    template<size_t BatchSize, typename Derived>
    struct CANBatchTransmittable;

    //Parse
    template <typename... Items>
    requires (IsDBCParsable<Items> && ...)
    struct DBCBundle : 
        public DBCParser<DBCBundle<Items&...>> 
    {
        std::tuple<Items&...> item;

        DBCBundle(Items&... xs)
            : item(xs...)
        {}
        
        template <typename Func>
        inline void for_each_item(Func f) {
            std::apply([&](Items&... args) { (f(args), ...); }, item);
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
    template <typename Transmitter, typename... Receivers>
    requires IsCANTransmittable<Transmitter> && (IsCANReceivable<Receivers> && ...)
    struct CANManyReceiverBundle : 
        public CANTransmittable<CANManyReceiverBundle<Transmitter&, Receivers&...>>,
        public CANReceivable<CANManyReceiverBundle<Transmitter&, Receivers&...>> 
    {
    public:
        Transmitter& transmitter;
        std::tuple<Receivers&...> receivers;

        CANManyReceiverBundle(Transmitter& transmitter, Receivers&... receivers) :
            transmitter(transmitter),
            receivers(receivers...)
        {}

        template <typename Func>
        inline void for_each_item(Func f) {
            std::apply([&](Receivers&... args) { (f(args), ...); }, receivers);
        }

        const CANMessage& transmit_message() {
            return transmitter.transmit_message();
        }

        const std::pair<CANTime, CANFrame>& transmit_raw_message() {
            return transmitter.transmit_raw_message();
        }

        const std::tuple<std::string, TableType>& transmit_table_message() {
            return transmitter.transmit_table_message();
        }
        const CANDataStreamMetadata& transmit_metadata() {
            return transmitter.transmit_metadata();
        }

        //CANWriteable
        void receive_message(const CANMessage& message) {
            for_each_item([&](auto& item) {
                item.receive_message(message);
            });
        }
        
        void receive_metadata(const CANDataStreamMetadata& metadata) {
            for_each_item([&](auto& item) {
                item.receive_metadata(metadata);
            });
        }

        void receive_raw_message(std::pair<CANTime, CANFrame> sample) {
            for_each_item([&](auto& item) {
                item.receive_raw_message(sample);
            });
        }

        void receive_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            for_each_item([&](auto& item) {
                item.receive_table_message(table, data);
            });
        }

    };

    // One Read / One Write
    template <typename Transmitter, typename Receiver>
    requires IsCANTransmittable<Transmitter> && IsCANReceivable<Receiver>
    struct CANPairBundle : 
        public CANTransmittable<CANPairBundle<Transmitter&, Receiver&>>, 
        public CANReceivable<CANPairBundle<Transmitter&, Receiver&>> 
    {
        Transmitter& transmitter;
        Receiver& receiver; 

        CANPairBundle(Transmitter& transmitter, Receiver& receiver) :
            transmitter(transmitter),
            receiver(receiver)
        {}

        //CANTransmittable 
        const CANMessage& transmit_message() {
            return transmitter.transmit_message();
        }

        const std::pair<CANTime, CANFrame>& transmit_raw_message() {
            return transmitter.transmit_raw_message();
        }

        const std::tuple<std::string, TableType>& transmit_table_message() {
            return transmitter.transmit_table_message();
        }
        const CANDataStreamMetadata& transmit_metadata() {
            return transmitter.transmit_metadata();
        }

        //CANWriteable
        void receive_message(const CANMessage& message) {
            receiver.receive_message(message);
        }
        
        void receive_metadata(const CANDataStreamMetadata& metadata) {
            receiver.receive_metadata(metadata);
        }

        void receive_raw_message(std::pair<CANTime, CANFrame> sample) {
            receiver.receive_raw_message(sample);
        }

        void receive_table_message(const std::string& table, const std::vector<std::pair<std::string, std::string>>& data) {
            receiver.receive_table_message(table, data);  
        }

    };
    // Batch Read / One Write
    template <size_t BatchSize, typename Receiver, typename BatchTransmitter>
    requires IsCANBatchTransmittable<BatchSize, BatchTransmitter> && IsCANReceivable<Receiver>
    struct CANBatchTransmitterBundle : 
        public CANBatchTransmittable<BatchSize, CANBatchTransmitterBundle<BatchSize, Receiver&, BatchTransmitter&>>, 
        public CANReceivable<CANBatchTransmitterBundle<BatchSize, Receiver&, BatchTransmitter&>> 
    {
        BatchTransmitter& batch_transmitter; 
        Receiver& receiver;

        CANBatchTransmitterBundle(BatchTransmitter& batch_transmitter, Receiver& receiver) : 
            batch_transmitter(batch_transmitter), 
            receiver(receiver)
        {}
        //CANBatchTransmittable
        const std::array<CANMessage, BatchSize>& transmit_message_batch() {
            return batch_transmitter.transmit_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& transmit_raw_message_batch() {
            return batch_transmitter.transmit_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& transmit_table_message_batch() {
            return batch_transmitter.transmit_table_message_batch();
        }
        //CANReceivable
        void receive_message(const CANMessage& message) {
            receiver.receive_message(message);
        }
        
        void receive_metadata(const CANDataStreamMetadata& metadata) {
            receiver.receive_metadata(metadata);
        }

        void receive_raw_message(const std::pair<CANTime, CANFrame>& sample) {
            receiver.receive_raw_message(sample);
        }

        void receive_table_message(const std::string& table, const TableType& data) {
            receiver.receive_table_message(table, data);
        }

    };
    // One Read / Batch Write
    template <size_t BatchSize, typename Transmitter, typename BatchReceiver>
    requires IsCANTransmittable<Transmitter> && IsCANBatchReceivable<BatchSize, BatchReceiver>
    struct CANBatchReceiverBundle : 
        public CANTransmittable<CANBatchReceiverBundle<BatchSize, Transmitter&, BatchReceiver&>>, 
        public CANBatchReceivable<BatchSize, CANBatchReceiverBundle<BatchSize, Transmitter&, BatchReceiver&>> 
    {
        Transmitter& transmitter; 
        BatchReceiver& batch_receiver;

        CANBatchReceiverBundle(Transmitter& transmitter, BatchReceiver& batch_receiver) : 
            transmitter(transmitter), 
            batch_receiver(batch_receiver)
        {}

        //CANTransmittable
        const CANMessage& transmit_message() {
            return transmitter.transmit_message();
        }

        const std::pair<CANTime, CANFrame>& transmit_raw_message() {
            return transmitter.transmit_raw_message();
        }

        const std::tuple<std::string, TableType>& transmit_table_message() {
            return transmitter.transmit_table_message();
        }

        const CANDataStreamMetadata& transmit_metadata() {
            return transmitter.transmit_metadata();
        }
        //CANBatchReceivable
        void receive_message_batch(const std::array<CANMessage, BatchSize>& message) {
            batch_receiver.receive_message_batch(message);
        }

        void receive_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            batch_receiver.receive_raw_message_batch(samples);
        }

        void receive_table_message_batch(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            batch_receiver.receive_table_message_batch(table, data);
        }

    };

    // Batch Read / Batch Write
    template <size_t BatchSize, typename BatchTransmitter, typename BatchReceiver>
    requires IsCANBatchTransmittable<BatchSize, BatchTransmitter> && IsCANBatchReceivable<BatchSize, BatchReceiver>
    struct CANBatchBundle : 
        public CANBatchTransmittable<BatchSize, CANBatchBundle<BatchSize, BatchTransmitter&, BatchReceiver&>>, 
        public CANBatchReceivable<BatchSize, CANBatchBundle<BatchSize, BatchTransmitter&, BatchReceiver&>> 
    {
        BatchTransmitter& batch_transmitter; 
        BatchReceiver& batch_receiver;

        CANBatchBundle(BatchTransmitter& batch_transmitter, BatchReceiver& batch_receiver) : 
            batch_transmitter(batch_transmitter), 
            batch_receiver(batch_receiver)
        {}

        //CANBatchTransmittable
        const std::array<CANMessage, BatchSize>& transmit_message_batch() {
            return batch_transmitter.transmit_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& transmit_raw_message_batch() {
            return batch_transmitter.transmit_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchSize>& transmit_table_message_batch() {
            return batch_transmitter.transmit_table_message_batch();
        }

        //CANBatchReceivable
        void receive_message_batch(const std::array<CANMessage, BatchSize>& message) {
            batch_receiver.receive_message_batch(message);
        }

        void receive_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchSize>& samples) {
            batch_receiver.receive_raw_message_batch(samples);
        }

        void receive_table_message_batch(const std::array<std::string, BatchSize>& table, const std::array<TableType, BatchSize>& data) {
            batch_receiver.receive_table_message_batch(table, data);
        }

    };
    
    template <size_t BatchTransmitterSize, size_t BatchReceiverSize, typename BatchTransmitter, typename BatchReceiver>
    requires IsCANBatchTransmittable<BatchTransmitterSize, BatchTransmitter> && IsCANBatchReceivable<BatchReceiverSize, BatchReceiver>
    struct CANDualBatchBundle : 
        public CANBatchTransmittable<BatchTransmitterSize, CANDualBatchBundle<BatchTransmitterSize, BatchReceiverSize, BatchTransmitter&, BatchReceiver&>>, 
        public CANBatchReceivable<BatchReceiverSize, CANDualBatchBundle<BatchTransmitterSize, BatchReceiverSize, BatchTransmitter&, BatchReceiver&>> 
    {
        BatchTransmitter& batch_transmitter; 
        BatchReceiver& batch_receiver;

        CANDualBatchBundle(BatchTransmitter& batch_transmitter, BatchReceiver& batch_receiver) : 
            batch_transmitter(batch_transmitter), 
            batch_receiver(batch_receiver)
        {}

        // CANBatchTransmittable (BatchTransmitterSize)
        const std::array<CANMessage, BatchTransmitterSize>& transmit_message_batch() {
            return batch_transmitter.transmit_message_batch();
        }

        const std::array<std::pair<CANTime, CANFrame>, BatchTransmitterSize>& transmit_raw_message_batch() {
            return batch_transmitter.transmit_raw_message_batch();
        }

        const std::array<std::tuple<std::string, TableType>, BatchTransmitterSize>& transmit_table_message_batch() {
            return batch_transmitter.transmit_table_message_batch();
        }

        // CANBatchReceivable (BatchReceiverSize)
        void receive_message_batch(const std::array<CANMessage, BatchReceiverSize>& message) {
            batch_receiver.receive_message_batch(message);
        }

        void receive_raw_message_batch(const std::array<std::pair<CANTime, CANFrame>, BatchReceiverSize>& samples) {
            batch_receiver.receive_raw_message_batch(samples);
        }

        void receive_table_message_batch(const std::array<std::string, BatchReceiverSize>& table, const std::array<TableType, BatchReceiverSize>& data) {
            batch_receiver.receive_table_message_batch(table, data);
        }
    };

    // Batch Read // errrr 
    template <size_t BatchSize = 0, typename Transmitter = void>
    requires IsCANTransmittable<Transmitter> && IsCANBatchTransmittable<BatchSize, Transmitter>
    struct NullTransmitter :
        public CANTransmittable<NullTransmitter<BatchSize, Transmitter>>,
        public CANBatchTransmittable<BatchSize, NullTransmitter<BatchSize, Transmitter>>
    {
        //CANTransmittable
        const CANMessage& transmit_message() {
            throw std::runtime_error("NullTransmitter: no data");
        }
        const std::pair<CANTime, CANFrame>& transmit_raw_message() {
            throw std::runtime_error("NullTransmitter: no data");
        }
        const std::tuple<std::string, TableType>& transmit_table_message() {
            throw std::runtime_error("NullTransmitter: no data");
        }
        const CANDataStreamMetadata& transmit_metadata() = delete;

        //CANBatchTransmittable
        const std::array<CANMessage, BatchSize>& transmit_message_batch() {
            throw std::runtime_error("NullTransmitter: no data");
        }
        const std::array<std::pair<CANTime, CANFrame>, BatchSize>& transmit_raw_message_batch() {
            throw std::runtime_error("NullTransmitter: no data");
        }
        const std::array<std::tuple<std::string, TableType>, BatchSize>& transmit_table_message_batch() {
            throw std::runtime_error("NullTransmitter: no data");
        }
    };


}