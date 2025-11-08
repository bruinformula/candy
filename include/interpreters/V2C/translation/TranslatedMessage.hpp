#pragma once

#include <vector>
#include <optional>
#include <string>
#include <ranges>
#include <cstdint>

#include "interpreters/V2C/TransmissionGroup.hpp"

#include "interpreters/V2C/translation/TranslatedSignal.hpp"
#include "interpreters/V2C/translation/TranslatedMultiplexer.hpp"
#include "interpreters/V2C/SignalAssembly.hpp"

namespace CAN {
    
    static SignalAssemblerVariant make_sig_agg(const TranslatedSignal& sig);

    class TranslatedMessage {
        std::vector<TranslatedSignal> _signals;
        std::optional<TranslatedMultiplexer> _mux;

        std::vector<SignalAssemblerVariant> _sig_asms;
        TransmissionGroup* transmission_group = nullptr;
        CANTime _last_stamp;

    public:
        void assign_group(TransmissionGroup* txg, uint32_t message_id);
        void assemble(CANTime stamp, CANFrame frame);

        void sig_agg_type(const std::string& sig_name, const std::string& agg_type);
        void sig_val_type(const std::string& sig_name, unsigned sig_ext_val_type);
        void add_signal(TranslatedSignal sig);
        void add_muxer(TranslatedMultiplexer mux);

        auto signals(uint64_t fd) const {
            uint64_t frame_mux = _mux.has_value() ? _mux->decode(fd) : -1;
            return _signals | std::ranges::views::filter([frame_mux](const auto& sig) {
                return sig.is_active(frame_mux);
            });
        }
    private:
        void make_sig_assemblers();
        void reset_sig_asms();
        TranslatedSignal* find_signal(const std::string& sig_name);
        std::vector<uint64_t> distinct_mux_vals() const;
    };


}