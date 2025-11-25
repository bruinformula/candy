#pragma once

#include <vector>
#include <optional>
#include <string>
#include <ranges>
#include <cstdint>

#include "Candy/Interpreters/V2C/TransmissionGroup.hpp"

#include "Candy/Interpreters/V2C/TranslatedSignal.hpp"
#include "Candy/Interpreters/V2C/TranslatedMultiplexer.hpp"
#include "Candy/Interpreters/V2C/SignalAssembly.hpp"

namespace Candy {
    
    using SignalAssemblerVariant = std::variant<
        LastSignal<uint64_t>,
        LastSignal<int64_t>,
        LastSignal<double>,
        LastSignal<float>,
        AverageSignal<uint64_t>,
        AverageSignal<int64_t>,
        AverageSignal<double>,
        AverageSignal<float>
    >;

    static SignalAssemblerVariant make_sig_agg(const TranslatedSignal& sig);

    class TranslatedMessage {
        std::vector<TranslatedSignal> _signals;
        std::optional<TranslatedMultiplexer> _mux;

        std::vector<SignalAssemblerVariant> _sig_asms;
        TransmissionGroup* transmission_group = nullptr;
        CANTime _last_stamp;

    public:
        void assign_group(TransmissionGroup* txg, uint32_t message_id);
        void assemble(std::pair<CANTime, CANFrame> sample);

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