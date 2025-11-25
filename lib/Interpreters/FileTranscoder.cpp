#include "Candy/Interpreters/File/FileTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"
#include "Candy/Interpreters/SQLTranscoder.hpp"

namespace Candy {

    template<typename T, typename TaskType>
    void FileTranscoder<T, TaskType>::sg(canid_t message_id, std::optional<unsigned> mux_val, const std::string& signal_name,
                        unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                        double factor, double offset, double min_val, double max_val,
                        std::string unit, std::vector<size_t> receivers)
    {
        SignalDefinition sig_def;
        sig_def.name = signal_name;
        sig_def.codec = std::make_unique<SignalCodec>(start_bit, bit_size, byte_order, sign_type);
        sig_def.numeric_value = std::make_unique<NumericValue>(factor, offset);
        sig_def.min_val = min_val;
        sig_def.max_val = max_val;
        sig_def.unit = unit;
        sig_def.mux_val = mux_val;

        messages[message_id].signals.push_back(std::move(sig_def));
    }

    template<typename T, typename TaskType>
    void FileTranscoder<T, TaskType>::sg_mux(canid_t message_id, const std::string& signal_name,
                            unsigned start_bit, unsigned bit_size, char byte_order, char sign_type,
                            std::string unit, std::vector<size_t> receivers)
    {
        SignalDefinition mux_def;
        mux_def.name = signal_name;
        mux_def.codec = std::make_unique<SignalCodec>(start_bit, bit_size, byte_order, sign_type);
        mux_def.numeric_value = std::make_unique<NumericValue>(1.0, 0.0);
        mux_def.unit = unit;
        mux_def.is_multiplexer = true;

        messages[message_id].multiplexer = std::move(mux_def);
    }

    template<typename T, typename TaskType>
    void FileTranscoder<T, TaskType>::bo(canid_t message_id, std::string message_name, size_t message_size, size_t transmitter) {
        messages[message_id].name = message_name;
        messages[message_id].size = message_size;
        messages[message_id].transmitter = transmitter;
        
        // Store message metadata in the underlying transcoder
        this->store_message_metadata_vrtl(message_id, message_name, message_size);
    }

    template<typename T, typename TaskType>
    void FileTranscoder<T, TaskType>::sig_valtype(canid_t message_id, const std::string& signal_name, unsigned value_type) {
        auto& msg = messages[message_id];
        for (auto& sig : msg.signals) {
            if (sig.name == signal_name) {
                sig.value_type = static_cast<NumericValueType>(value_type);
                break;
            }
        }
    }

    template class FileTranscoder<CSVTranscoder, CSVTask>;
    template class FileTranscoder<SQLTranscoder, SQLTask>;

}