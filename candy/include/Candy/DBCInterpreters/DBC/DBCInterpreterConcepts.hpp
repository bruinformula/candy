#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <variant>

namespace Candy {

    template<typename T>
    concept HasVersion = requires(T t, const std::string& v) {
        { t.version(v) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBu = requires(T t, const std::vector<std::string>& v) {
        { t.bu(v) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBo = requires(T t, uint32_t id, const std::string& name, size_t size, size_t sender) {
        { t.bo(id, name, size, sender) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSg = requires(
        T t,
        uint32_t id,
        std::optional<unsigned> mux_val,
        std::string name,
        unsigned start,
        unsigned size,
        char byte_order,
        char value_type,
        double factor,
        double offset,
        double min,
        double max,
        std::string unit,
        std::vector<size_t> receivers
    ) {
        { t.sg(id, mux_val, name, start, size, byte_order, value_type, factor, offset, min, max, unit, receivers) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSgMux = requires(
        T t,
        uint32_t id,
        std::string name,
        unsigned start,
        unsigned size,
        char byte_order,
        char value_type,
        std::string unit,
        std::vector<size_t> receivers
    ) {
        { t.sg_mux(id, name, start, size, byte_order, value_type, unit, receivers) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasEv = requires(
        T t,
        std::string name,
        unsigned id,
        double min,
        double max,
        std::string unit,
        double initial,
        unsigned type,
        std::string access,
        std::vector<size_t> nodes
    ) {
        { t.ev(name, id, min, max, unit, initial, type, access, nodes) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasEnvvarData = requires(T t, std::string name, unsigned type) {
        { t.envvar_data(name, type) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSgType = requires(
        T t,
        std::string name,
        unsigned size,
        char byte_order,
        char value_type,
        double factor,
        double offset,
        double min,
        double max,
        std::string unit,
        double defval,
        size_t rcvr_cnt
    ) {
        { t.sgtype(name, size, byte_order, value_type, factor, offset, min, max, unit, defval, rcvr_cnt) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSgTypeRef = requires(T t, unsigned msg_id, std::string name, std::string type) {
        { t.sgtype_ref(msg_id, name, type) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSigGroup = requires(T t, unsigned msg_id, std::string name, unsigned rep, std::vector<std::string> signals) {
        { t.sig_group(msg_id, name, rep, signals) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasCm = requires(T t, std::string c) {
        { t.cm_glob(c) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasCmMsg = requires(T t, unsigned id, std::string c) {
        { t.cm_msg(id, c) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasCmBo = requires(T t, uint32_t id, std::string c) {
        { t.cm_bo(id, c) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasCmSg = requires(T t, uint32_t id, std::string sig, std::string c) {
        { t.cm_sg(id, sig, c) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasCmEv = requires(T t, std::string name, std::string c) {
        { t.cm_ev(name, c) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBaDefEnum = requires(T t, std::string scope, std::string name, std::vector<std::string> vals) {
        { t.ba_def_enum(scope, name, vals) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBaDefInt = requires(T t, std::string scope, std::string name, int32_t min, int32_t max) {
        { t.ba_def_int(scope, name, min, max) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBaDefFloat = requires(T t, std::string scope, std::string name, double min, double max) {
        { t.ba_def_float(scope, name, min, max) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBaDefString = requires(T t, std::string scope, std::string name) {
        { t.ba_def_string(scope, name) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBaDefDef = requires(T t, std::string name, std::variant<int32_t, double, std::string> v) {
        { t.ba_def_def(name, v) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBa = requires(
        T t,
        std::string name,
        std::string scope,
        std::string target,
        size_t index,
        unsigned type,
        std::variant<int32_t, double, std::string> v
    ) {
        { t.ba(name, scope, target, index, type, v) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasValEnv = requires(T t, std::string env, std::vector<std::pair<unsigned, std::string>> map) {
        { t.val_env(env, map) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasValSg = requires(T t, uint32_t id, std::string sig, std::vector<std::pair<unsigned, std::string>> map) {
        { t.val_sg(id, sig, map) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasValTable = requires(T t, std::string name, std::vector<std::pair<unsigned, std::string>> map) {
        { t.val_table(name, map) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSigValType = requires(T t, unsigned id, std::string sig, unsigned valtype) {
        { t.sig_valtype(id, sig, valtype) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasBoTxBu = requires(T t, unsigned id, std::vector<std::string> senders) {
        { t.bo_tx_bu(id, senders) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasSgMulVal = requires(T t, unsigned id, std::string signal, std::string selector, std::vector<std::pair<unsigned, unsigned>> ranges) {
        { t.sg_mul_val(id, signal, selector, ranges) } -> std::same_as<void>;
    };

} // namespace Candy
