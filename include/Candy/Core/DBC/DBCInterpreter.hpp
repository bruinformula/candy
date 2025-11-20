#pragma once

#include <optional>

#include <boost/spirit/home/x3.hpp>

#include "Candy/Core/DBC/DBCParserUtils.hpp"
#include "Candy/Core/DBC/DBCInterpreterConcepts.hpp"
#include "Candy/Core/DBC/DBCParser.hpp"

namespace Candy {


    template <typename Derived>
    class DBCInterpreter : public DBCParser<Derived> {
    protected:
        void version_vrtl(const std::string& v) {
            if constexpr (HasVersion<Derived>) {
                static_cast<Derived&>(*this).version(v);
            }
        }

        void bu_vrtl(const std::vector<std::string>& nodes) {
            if constexpr (HasBu<Derived>) {
                static_cast<Derived&>(*this).bu(nodes);
            }
        }

        void bo_vrtl(uint32_t id, const std::string& name, size_t size, size_t sender) {
            if constexpr (HasBo<Derived>) {
                static_cast<Derived&>(*this).bo(id, name, size, sender);
            }
        }

        void sg_vrtl(uint32_t msg_id, std::optional<unsigned> mux_val, const std::string& name,
                    unsigned start_bit, unsigned size, char byte_order, char value_type,
                    double factor, double offset, double min, double max,
                    const std::string& unit, const std::vector<size_t>& receivers) {
            if constexpr (HasSg<Derived>) {
                static_cast<Derived&>(*this).sg(msg_id, mux_val, name, start_bit, size, byte_order, value_type,
                                                factor, offset, min, max, unit, receivers);
            }
        }

        void sg_mux_vrtl(uint32_t msg_id, const std::string& name, unsigned start_bit, unsigned size,
                        char byte_order, char value_type, const std::string& unit, const std::vector<size_t>& receivers) {
            if constexpr (HasSgMux<Derived>) {
                static_cast<Derived&>(*this).sg_mux(msg_id, name, start_bit, size, byte_order, value_type, unit, receivers);
            }
        }

        void ev_vrtl(const std::string& name, unsigned id, double min, double max, const std::string& unit,
                    double initial_value, unsigned ev_type, const std::string& access,
                    const std::vector<size_t>& access_nodes) {
            if constexpr (HasEv<Derived>) {
                static_cast<Derived&>(*this).ev(name, id, min, max, unit, initial_value, ev_type, access, access_nodes);
            }
        }

        void envvar_data_vrtl(const std::string& name, unsigned type) {
            if constexpr (HasEnvvarData<Derived>) {
                static_cast<Derived&>(*this).envvar_data(name, type);
            }
        }

        void sgtype_vrtl(const std::string& name, unsigned size, char byte_order, char value_type,
                        double factor, double offset, double min, double max, const std::string& unit,
                        double default_val, size_t receiver_count) {
            if constexpr (HasSgType<Derived>) {
                static_cast<Derived&>(*this).sgtype(name, size, byte_order, value_type,
                                                    factor, offset, min, max, unit, default_val, receiver_count);
            }
        }

        void sgtype_ref_vrtl(unsigned msg_id, const std::string& sig, const std::string& type) {
            if constexpr (HasSgTypeRef<Derived>) {
                static_cast<Derived&>(*this).sgtype_ref(msg_id, sig, type);
            }
        }

        void sig_group_vrtl(unsigned msg_id, const std::string& name, unsigned repetitions,
                        const std::vector<std::string>& signals) {
            if constexpr (HasSigGroup<Derived>) {
                static_cast<Derived&>(*this).sig_group(msg_id, name, repetitions, signals);
            }
        }

        void cm_glob_vrtl(const std::string& comment) {
            if constexpr (HasCm<Derived>) {
                static_cast<Derived&>(*this).cm_glob(comment);
            }
        }

        void cm_bu_vrtl(unsigned id, const std::string& comment) {
            if constexpr (HasCmMsg<Derived>) {
                static_cast<Derived&>(*this).cm_bu(id, comment);
            }
        }

        void cm_bo_vrtl(uint32_t msg_id, const std::string& comment) {
            if constexpr (HasCmBo<Derived>) {
                static_cast<Derived&>(*this).cm_bo(msg_id, comment);
            }
        }

        void cm_sg_vrtl(uint32_t msg_id, const std::string& sig, const std::string& comment) {
            if constexpr (HasCmSg<Derived>) {
                static_cast<Derived&>(*this).cm_sg(msg_id, sig, comment);
            }
        }

        void cm_ev_vrtl(const std::string& envvar, const std::string& comment) {
            if constexpr (HasCmEv<Derived>) {
                static_cast<Derived&>(*this).cm_ev(envvar, comment);
            }
        }

        void ba_enum_vrtl(const std::string& scope, const std::string& name, const std::vector<std::string>& values) {
            if constexpr (HasBaDefEnum<Derived>) {
                static_cast<Derived&>(*this).ba_enum(scope, name, values);
            }
        }

        void ba_int_vrtl(const std::string& scope, const std::string& name, int32_t min, int32_t max) {
            if constexpr (HasBaDefInt<Derived>) {
                static_cast<Derived&>(*this).ba_int(scope, name, min, max);
            }
        }

        void ba_float_vrtl(const std::string& scope, const std::string& name, double min, double max) {
            if constexpr (HasBaDefFloat<Derived>) {
                static_cast<Derived&>(*this).ba_float(scope, name, min, max);
            }
        }

        void ba_string_vrtl(const std::string& scope, const std::string& name) {
            if constexpr (HasBaDefString<Derived>) {
                static_cast<Derived&>(*this).ba_string(scope, name);
            }
        }

        void ba_def_vrtl(const std::string& name, const std::variant<int32_t, double, std::string>& val) {
            if constexpr (HasBaDefDef<Derived>) {
                static_cast<Derived&>(*this).ba_def(name, val);
            }
        }

        void ba_vrtl(const std::string& name, const std::string& scope, const std::string& target,
                    size_t index, unsigned type, const std::variant<int32_t, double, std::string>& val) {
            if constexpr (HasBa<Derived>) {
                static_cast<Derived&>(*this).ba(name, scope, target, index, type, val);
            }
        }

        void val_env_vrtl(const std::string& env, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValEnv<Derived>) {
                static_cast<Derived&>(*this).val_env(env, mappings);
            }
        }

        void val_sg_vrtl(uint32_t msg_id, const std::string& sig, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValSg<Derived>) {
                static_cast<Derived&>(*this).val_sg(msg_id, sig, mappings);
            }
        }

        void val_table_vrtl(const std::string& table, const std::vector<std::pair<unsigned, std::string>>& mappings) {
            if constexpr (HasValTable<Derived>) {
                static_cast<Derived&>(*this).val_table(table, mappings);
            }
        }

        void sig_valtype_vrtl(unsigned msg_id, const std::string& sig, unsigned valtype) {
            if constexpr (HasSigValType<Derived>) {
                static_cast<Derived&>(*this).sig_valtype(msg_id, sig, valtype);
            }
        }

        void bo_tx_bu_vrtl(unsigned msg_id, const std::vector<std::string>& senders) {
            if constexpr (HasBoTxBu<Derived>) {
                static_cast<Derived&>(*this).bo_tx_bu(msg_id, senders);
            }
        }

        void sg_mul_val_vrtl(unsigned msg_id, const std::string& signal, const std::string& selector, const std::vector<std::pair<unsigned, unsigned>>& ranges) {
            if constexpr (HasSgMulVal<Derived>) {
                static_cast<Derived&>(*this).sg_mul_val(msg_id, signal, selector, ranges);
            }
        }
    
    public:

        //there are no static assertions to impliment a DBC Interpreter. It is up to the Derived classes

        ParseResult parse_version(std::string_view rng);
        ParseResult parse_ns_(std::string_view rng);
        ParseResult parse_bs_(std::string_view rng);
        ParseResult parse_bu_(std::string_view rng, nodes_t& nodes);
        ParseResult parse_sg_(std::string_view rng, const nodes_t& nodes, uint32_t can_id);
        ParseResult parse_bo_(std::string_view rng, const nodes_t& nodes);
        ParseResult parse_ev_(std::string_view rng, const nodes_t& nodes);
        ParseResult parse_envvar_data_(std::string_view rng);
        ParseResult parse_sgtype_(std::string_view rng, const nodes_t& val_tables);
        ParseResult parse_cm_(std::string_view rng, const nodes_t& nodes);
        ParseResult parse_sig_group_(std::string_view rng);
        ParseResult parse_ba_def_(std::string_view rng, attr_types_t& ats_);
        ParseResult parse_ba_def_def_(std::string_view rng, const attr_types_t& ats_);
        ParseResult parse_ba_(std::string_view rng, const nodes_t& nodes, const attr_types_t& ats_);
        ParseResult parse_val_(std::string_view rng);
        ParseResult parse_val_table_(std::string_view rng, nodes_t& val_tables);
        ParseResult parse_sig_valtype_(std::string_view rng);
        ParseResult parse_bo_tx_bu(std::string_view rng);
        ParseResult parse_sg_mul_val_(std::string_view rng);

        bool parse_dbc(std::string_view dbc_src);
    };
    
}

