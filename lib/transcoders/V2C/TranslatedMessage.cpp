#include <numeric>
#include <unordered_map>
#include <chrono>
#include <bit>
#include <variant>
#include <vector>
#include <cstdint>

#include "Candy/DBC/Transcoders/V2C/SignalAssembly.hpp"

#include "Candy/CAN/Signal/SignalCalculationType.hpp"
#include "Candy/DBC/Transcoders/V2C/TranslatedMessage.hpp"

namespace Candy {

    template <template <typename> typename AssemblerType>
    static SignalAssemblerVariant make_sig_agg(const TranslatedSignal& sig) {
        switch (sig.value_type()) {
            case Candy::NumericValueType::i64: return AssemblerType<int64_t>(sig);
            case Candy::NumericValueType::u64: return AssemblerType<uint64_t>(sig);
            case Candy::NumericValueType::f32: return AssemblerType<float>(sig);
            case Candy::NumericValueType::f64: return AssemblerType<double>(sig);
        }
        fprintf(stderr, "Signal %s doesn't have a valid value_type\n", sig.name().c_str());
        return { AverageSignal<int64_t>(sig) };
    }

    std::vector<uint64_t> TranslatedMessage::distinct_mux_vals() const {
        std::vector<uint64_t> mux_vals;

        for (const auto& sig : _signals) {
            if (sig.mux_val().has_value())
                mux_vals.push_back(sig.mux_val().value());
        }

        std::sort(mux_vals.begin(), mux_vals.end());
        mux_vals.erase(std::unique(mux_vals.begin(), mux_vals.end()), mux_vals.end());

        return mux_vals;
    }

    void TranslatedMessage::assign_group(TransmissionGroup* txg, uint32_t message_id) {
        transmission_group = txg;
        make_sig_assemblers();

        if (_mux.has_value())
            for (const auto mux_val : distinct_mux_vals())
                transmission_group->assign(message_id, mux_val);
        else
            transmission_group->assign(message_id, -1);
    }

    void TranslatedMessage::assemble(CANTime stamp, CANFrame frame) {
        if (!transmission_group) return;

        if (!transmission_group->within_interval(_last_stamp))
            reset_sig_asms();

        uint64_t clumped_val = 0;
        uint64_t fd = std::bit_cast<uint64_t>(frame.data);
        int64_t mux_val = _mux.has_value() ? _mux->decode(fd) : -1;

        for (auto& sc : _sig_asms)
            clumped_val |= std::visit([&](auto& assembler) { return assembler.assemble(mux_val, fd); }, sc);

        if (_mux.has_value())
            clumped_val |= _mux->encode(fd);

        transmission_group->add_clumped(stamp, frame.can_id, mux_val, clumped_val);

        _last_stamp = stamp;
    }

    void TranslatedMessage::make_sig_assemblers() {
        for (const TranslatedSignal& sig : _signals) {
            std::string_view atype = sig.agg_type();

            if (atype == "LAST") {
                SignalAssemblerVariant sasm = make_sig_agg<LastSignal>(sig);
                _sig_asms.emplace_back(std::move(sasm));
            }
            else if (atype == "AVG") {
                SignalAssemblerVariant sasm = make_sig_agg<AverageSignal>(sig);
                _sig_asms.emplace_back(std::move(sasm));
            }
            else continue;
        }
    }

    void TranslatedMessage::reset_sig_asms() {
        for (auto& sc : _sig_asms)
            std::visit([](auto& assembler) { assembler.reset(); }, sc);
    }

    TranslatedSignal* TranslatedMessage::find_signal(const std::string& sig_name) {
        auto sig_it = std::find_if(_signals.begin(), _signals.end(), [&](const auto& s) { return s.name() == sig_name; });
        return sig_it == _signals.end() ? nullptr : &(*sig_it);
    }

    void TranslatedMessage::sig_agg_type(const std::string& sig_name, const std::string& agg_type) {
        if (auto sig = find_signal(sig_name); sig)
            sig->agg_type(agg_type);
    }

    void TranslatedMessage::sig_val_type(const std::string& sig_name, unsigned sig_ext_val_type) {
        if (auto sig = find_signal(sig_name); sig)
            sig->value_type(sig_ext_val_type);
    }

    void TranslatedMessage::add_signal(TranslatedSignal sig) {
        _signals.push_back(std::move(sig));
    }

    void TranslatedMessage::add_muxer(TranslatedMultiplexer mux) {
        _mux = std::move(mux);
    }

}
