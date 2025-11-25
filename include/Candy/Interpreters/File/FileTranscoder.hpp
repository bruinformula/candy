#pragma once 

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <vector>
#include <future>

#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/Interpreters/File/FileTranscoderConcepts.hpp"
#include "Candy/Core/DBC/DBCInterpreter.hpp"
#include "Candy/Core/DBC/DBCInterpreterConcepts.hpp"

#include "Candy/Core/CANIO.hpp"

namespace Candy {

    template <typename Derived, typename TaskType>
    class FileTranscoder : public DBCInterpreter<Derived>,
                           public CANStoreTransmitter<Derived>,
                           public CANReceivable<Derived>
    {
    public:
        // Constructor to initialize member variables
        FileTranscoder(size_t batch_size, size_t frames_batch_count, size_t decoded_signals_batch_count) :
            batch_size(batch_size),
            frames_batch_count(frames_batch_count),
            decoded_signals_batch_count(decoded_signals_batch_count)
        {
            static_assert(FileTranscodable<Derived>, "Derived must satisfy FileTranscodable concept");
            static_assert(HasSg<Derived>, "Derived must satisfy HasSg concept");
            static_assert(HasSgMux<Derived>, "Derived must satisfy HasSgMux concept");
            static_assert(HasBo<Derived>, "Derived must satisfy HasBo concept");
            static_assert(HasSigValType<Derived>, "Derived must satisfy HasSigValType concept");
        }

    protected:
        std::unordered_map<canid_t, MessageDefinition> messages;
        size_t batch_size;
        size_t frames_batch_count;
        size_t decoded_signals_batch_count;
        CANDataStreamMetadata metadata;

        //virtual methods

        void store_message_metadata_vrtl(canid_t message_id, std::string message_name, size_t message_size) {
            static_cast<Derived&>(*this).store_message_metadata(message_id, message_name, message_size);
        }

        void batch_frame_vrtl(std::pair<CANTime, CANFrame> sample) {
            static_cast<Derived&>(*this).batch_frame(sample);
        }

        void batch_decoded_signals_vrtl(std::pair<CANTime, CANFrame> sample, const MessageDefinition& msg_def) {
            static_cast<Derived&>(*this).batch_decoded_signals(sample, msg_def);
        }

        void flush_frames_batch_vrtl() {
            static_cast<Derived&>(*this).flush_frames_batch();
        }

        void flush_decoded_signals_batch_vrtl() {
            static_cast<Derived&>(*this).flush_decoded_signals_batch();
        }

        void flush_all_batches_vrtl() {
            static_cast<Derived&>(*this).flush_all_batches();
        }

    public:
        //DBC methods 
        void sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
            unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
            double factor, double offset, double min_val, double max_val,
            std::string unit, std::vector<size_t> receivers);

        void sg_mux(canid_t message_id, const std::string& signal_name,
                    unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                    std::string unit, std::vector<size_t> receivers);

        void bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter);

        void sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type);
    };

}

