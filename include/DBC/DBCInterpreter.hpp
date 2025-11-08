#pragma once


#include <iostream>
#include <limits>
#include <type_traits>
#include <optional>

#ifdef BOOST_USE_SYSTEM
    #include <boost/spirit/home/x3.hpp>
#else
    #include "boost/spirit/home/x3.hpp"
#endif

#include "DBC/DBCInterpreterConcepts.hpp"

namespace CAN {

    template <typename Derived>
    struct DBCInterpreter {
        void def_version(const std::string& v) {
            if constexpr (HasVersion<Derived>) {
                static_cast<Derived&>(*this).version(v);
            }
        }

        void def_bu(const std::vector<std::string>& nodes) {
            if constexpr (HasBu<Derived>) {
                static_cast<Derived&>(*this).bu(nodes);
            }
        }

        void def_bo(uint32_t id, const std::string& name, size_t size, size_t sender) {
            if constexpr (HasBo<Derived>) {
                static_cast<Derived&>(*this).bo(id, name, size, sender);
            }
        }

        void def_sg(uint32_t msg_id, std::optional<unsigned> mux_val, const std::string& name,
                    unsigned start_bit, unsigned size, char byte_order, char value_type,
                    double factor, double offset, double min, double max,
                    const std::string& unit, const std::vector<size_t>& receivers) {
            if constexpr (HasSg<Derived>) {
                static_cast<Derived&>(*this).sg(msg_id, mux_val, name, start_bit, size, byte_order, value_type,
                                                factor, offset, min, max, unit, receivers);
            }
        }

        void def_sg_mux(uint32_t msg_id, const std::string& name, unsigned start_bit, unsigned size,
                        char byte_order, char value_type, const std::string& unit, const std::vector<size_t>& receivers) {
            if constexpr (HasSgMux<Derived>) {
                static_cast<Derived&>(*this).sg_mux(msg_id, name, start_bit, size, byte_order, value_type, unit, receivers);
            }
        }

        void def_ev(const std::string& name, unsigned id, double min, double max, const std::string& unit,
                    double initial_value, unsigned ev_type, const std::string& access,
                    const std::vector<size_t>& access_nodes) {
            if constexpr (HasEv<Derived>) {
                static_cast<Derived&>(*this).ev(name, id, min, max, unit, initial_value, ev_type, access, access_nodes);
            }
        }

        void def_envvar_data(const std::string& name, unsigned type) {
            if constexpr (HasEnvvarData<Derived>) {
                static_cast<Derived&>(*this).envvar_data(name, type);
            }
        }

        void def_sgtype(const std::string& name, unsigned size, char byte_order, char value_type,
                        double factor, double offset, double min, double max, const std::string& unit,
                        double default_val, size_t receiver_count) {
            if constexpr (HasSgType<Derived>) {
                static_cast<Derived&>(*this).sgtype(name, size, byte_order, value_type,
                                                    factor, offset, min, max, unit, default_val, receiver_count);
            }
        }

        void def_sgtype_ref(unsigned msg_id, const std::string& sig, const std::string& type) {
            if constexpr (HasSgTypeRef<Derived>) {
                static_cast<Derived&>(*this).sgtype_ref(msg_id, sig, type);
            }
        }

        void def_sig_group(unsigned msg_id, const std::string& name, unsigned repetitions,
                        const std::vector<std::string>& signals) {
            if constexpr (HasSigGroup<Derived>) {
                static_cast<Derived&>(*this).sig_group(msg_id, name, repetitions, signals);
            }
        }

        void def_cm_glob(const std::string& comment) {
            if constexpr (HasCm<Derived>) {
                static_cast<Derived&>(*this).cm_glob(comment);
            }
        }

        void def_cm_bu(unsigned id, const std::string& comment) {
            if constexpr (HasCmMsg<Derived>) {
                static_cast<Derived&>(*this).cm_bu(id, comment);
            }
        }

        void def_cm_bo(uint32_t msg_id, const std::string& comment) {
            if constexpr (HasCmBo<Derived>) {
                static_cast<Derived&>(*this).cm_bo(msg_id, comment);
            }
        }

        void def_cm_sg(uint32_t msg_id, const std::string& sig, const std::string& comment) {
            if constexpr (HasCmSg<Derived>) {
                static_cast<Derived&>(*this).cm_sg(msg_id, sig, comment);
            }
        }

        void def_cm_ev(const std::string& envvar, const std::string& comment) {
            if constexpr (HasCmEv<Derived>) {
                static_cast<Derived&>(*this).cm_ev(envvar, comment);
            }
        }

        void def_ba_def_enum(const std::string& scope, const std::string& name, const std::vector<std::string>& values) {
            if constexpr (HasBaDefEnum<Derived>) {
                static_cast<Derived&>(*this).ba_def_enum(scope, name, values);
            }
        }

        void def_ba_def_int(const std::string& scope, const std::string& name, int32_t min, int32_t max) {
            if constexpr (HasBaDefInt<Derived>) {
                static_cast<Derived&>(*this).ba_def_int(scope, name, min, max);
            }
        }

        void def_ba_def_float(const std::string& scope, const std::string& name, double min, double max) {
            if constexpr (HasBaDefFloat<Derived>) {
                static_cast<Derived&>(*this).ba_def_float(scope, name, min, max);
            }
        }

        void def_ba_def_string(const std::string& scope, const std::string& name) {
            if constexpr (HasBaDefString<Derived>) {
                static_cast<Derived&>(*this).ba_def_string(scope, name);
            }
        }

        void def_ba_def_def(const std::string& name, const std::variant<int32_t, double, std::string>& val) {
            if constexpr (HasBaDefDef<Derived>) {
                static_cast<Derived&>(*this).ba_def_def(name, val);
            }
        }

        void def_ba(const std::string& name, const std::string& scope, const std::string& target,
                    size_t index, unsigned type, const std::variant<int32_t, double, std::string>& val) {
            if constexpr (HasBa<Derived>) {
                static_cast<Derived&>(*this).ba(name, scope, target, index, type, val);
            }
        }

        void def_val_env(const std::string& env, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValEnv<Derived>) {
                static_cast<Derived&>(*this).val_env(env, mappings);
            }
        }

        void def_val_sg(uint32_t msg_id, const std::string& sig, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValSg<Derived>) {
                static_cast<Derived&>(*this).val_sg(msg_id, sig, mappings);
            }
        }

        void def_val_table(const std::string& table, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValTable<Derived>) {
                static_cast<Derived&>(*this).val_table(table, mappings);
            }
        }

        void def_sig_valtype(unsigned msg_id, const std::string& sig, unsigned valtype) {
            if constexpr (HasSigValType<Derived>) {
                static_cast<Derived&>(*this).sig_valtype(msg_id, sig, valtype);
            }
        }

        void def_bo_tx_bu(unsigned msg_id, const std::vector<std::string>& senders) {
            if constexpr (HasBoTxBu<Derived>) {
                static_cast<Derived&>(*this).bo_tx_bu(msg_id, senders);
            }
        }

        void def_sg_mul_val(unsigned msg_id, const std::string& signal, const std::string& selector, const std::vector<std::pair<unsigned, unsigned>>& ranges) {
            if constexpr (HasSgMulVal<Derived>) {
                static_cast<Derived&>(*this).sg_mul_val(msg_id, signal, selector, ranges);
            }
        }
    };


}

