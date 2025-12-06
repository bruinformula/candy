#pragma once 

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <chrono>
#include <string_view>

#include "Candy/Core/CANKernelTypes.hpp"

#include "Candy/DBCInterpreters/V2C/TransmissionGroup.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedMessage.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedSignal.hpp"
#include "Candy/DBCInterpreters/V2C/TranslatedMultiplexer.hpp"

#include "Candy/DBCInterpreters/DBC/DBCInterpreter.hpp"

namespace Candy {
    class V2CTranscoder : public DBCInterpreter<V2CTranscoder> {
        std::chrono::milliseconds publish_frequency;
        std::chrono::milliseconds update_frequency{0};

        std::unordered_map<canid_t, TranslatedMessage> _msgs;
        std::vector<std::unique_ptr<TransmissionGroup>> transmission_groups;

        FramePacket frame_packet;
        CANTime _last_update_tp;

    public:
        FramePacket transcode(std::pair<CANTime, CANFrame> sample);

        void assign_tx_group(const std::string& object_type, unsigned message_id, const std::string& tx_group);
        void add_signal(canid_t message_id, TranslatedSignal sig);
        void add_muxer(canid_t message_id, TranslatedMultiplexer mux);
        void add_message(canid_t message_id, std::string_view message_name);

        void set_env_var(const std::string& name, int64_t ev_value);
        void set_sig_val_type(canid_t message_id, const std::string& sig_name, unsigned sig_ext_val_type);
        void set_sig_agg_type(canid_t message_id, const std::string& sig_name, const std::string& agg_type);

        void setup_timers(CANTime first_stamp);
        void store_assembled(CANTime up_to);
        TranslatedMessage* find_message(canid_t message_id);

        // ---- Interpreter methods ----

        void sg(
            uint32_t message_id,
            std::optional<unsigned> sg_mux_switch_val,
            const std::string& sg_name,
            unsigned sg_start_bit,
            unsigned sg_size,
            char sg_byte_order,
            char sg_sign,
            double sg_factor,
            double sg_offset,
            double sg_min,
            double sg_max,
            const std::string& sg_unit,
            const std::vector<size_t>& rec_ords
        ) {
            SignalCodec codec{sg_start_bit, sg_size, sg_byte_order, sg_sign};
            TranslatedSignal sig{sg_name, codec, std::optional<int64_t>(sg_mux_switch_val)};
            add_signal(message_id, std::move(sig));
        }

        void sg_mux(
            uint32_t message_id,
            const std::string& sg_name,
            unsigned sg_start_bit,
            unsigned sg_size,
            char sg_byte_order,
            char sg_sign,
            const std::string& sg_unit,
            const std::vector<size_t>& rec_ords
        ) {
            SignalCodec codec{sg_start_bit, sg_size, sg_byte_order, sg_sign};
            TranslatedMultiplexer mux{codec};
            add_muxer(message_id, std::move(mux));
        }

        void bo(
            uint32_t message_id,
            const std::string& msg_name,
            size_t msg_size,
            size_t transmitter_ord
        ) {
            add_message(message_id, msg_name);
        }

        void ev(
            const std::string& name,
            unsigned type,
            double ev_min,
            double ev_max,
            const std::string& unit,
            double initial,
            unsigned ev_id,
            const std::string& access_type,
            const std::vector<size_t>& access_nodes_ords
        ) {
            set_env_var(name, static_cast<int64_t>(initial));
        }

        void ba(
            const std::string& attr_name,
            const std::string& object_type,
            const std::string& object_name,
            size_t bu_id,
            unsigned message_id,
            const std::variant<int32_t, double, std::string>& attr_val
        ) {
            if (attr_name == "AggType" && std::holds_alternative<std::string>(attr_val)) {
                set_sig_agg_type(message_id, object_name, std::get<std::string>(attr_val));
            }

            if (attr_name == "TxGroupFreq" && object_type == "BO_" && std::holds_alternative<std::string>(attr_val)) {
                assign_tx_group(object_type, message_id, std::get<std::string>(attr_val));
            }
        }

        void sig_valtype(
            unsigned message_id,
            const std::string& sig_name,
            unsigned sig_ext_val_type
        ) {
            set_sig_val_type(message_id, sig_name, sig_ext_val_type);
        }
    };
}