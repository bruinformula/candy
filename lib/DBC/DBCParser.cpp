#include <string_view>

#include "boost/spirit/home/x3.hpp"
#include "boost/fusion/adapted/std_pair.hpp"

#include "transcoders/V2CTranscoder.hpp"
#include "transcoders/SQLTranscoder.hpp"
#include "transcoders/CSVTranscoder.hpp"

#include "DBC/DBCParser.hpp"
#include "DBC/DBCInterpreter.hpp"

namespace x3 = boost::spirit::x3;

namespace CAN {

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_version(std::string_view rng) {
        //std::cerr << "[parseVersion] input: " << std::string(rng.begin(), rng.end()) << "\n";
        bool has_sect = false;
        std::string version;

        const auto version_ = x3::omit[x3::lexeme[x3::lit("VERSION") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> quoted_name_[to(version)] >> end_cmd_;

        auto iter = rng.begin();
        if (!phrase_parse(iter, rng.end(), version_, skipper_))
            //std::cout << "f " << std::endl;
            return make_rv(rng, !has_sect);

        version_vrtl(std::move(version));
        
        return make_rv(iter, rng.end(), true);
    };

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ns_(std::string_view rng) {
        struct ns_syms : x3::symbols<unsigned> {
            ns_syms() {
                add
                ("NS_DESC_", 0) ("CM_", 1) ("BA_DEF_", 2) ("BA_", 3) ("VAL_", 4)
                    ("CAT_DEF_", 5)	("CAT_", 6) ("FILTER", 7) ("BA_DEF_DEF_", 8)
                    ("EV_DATA_", 9)	("ENVVAR_DATA_", 10) ("SGTYPE_", 11) ("SGTYPE_VAL_", 12)
                    ("BA_DEF_SGTYPE_", 13) ("BA_SGTYPE_", 14) ("SIG_TYPE_REF_", 15)
                    ("VAL_TABLE_", 16) ("SIG_GROUP_", 17) ("SIG_VALTYPE_", 18)
                    ("SIGTYPE_VALTYPE_", 19) ("BO_TX_BU_", 20) ("BA_DEF_REL_", 21)
                    ("BA_REL_", 22) ("BA_DEF_DEF_REL_", 23) ("BU_SG_REL_", 24)
                    ("BU_EV_REL_", 25) ("BU_BO_REL_", 26) ("SG_MUL_VAL_", 27);
            }
        } ns_syms_;
        bool has_sect = false;

        const auto ns_ = x3::omit[x3::lexeme[x3::lit("NS_") >> +x3::blank >> ':']] >>
            x3::attr(true)[to(has_sect)] >> x3::omit[+x3::space] >>
            x3::omit[*(ns_syms_ >> x3::omit[+x3::space])] >> end_cmd_;

        auto iter = rng.begin();
        if (!phrase_parse(iter, rng.end(), ns_, skipper_))
            return make_rv(rng, !has_sect);

        // DBCInterpreter<T> callback deliberately omitted
        return make_rv(iter, rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bs_(std::string_view rng) {
        const auto bs_ = x3::omit[x3::lexeme[x3::lit("BS_:")]] >>
            x3::omit[-(x3::uint_ >> ':' >> x3::uint_ >> ',' >> x3::uint_)] >> end_cmd_;

        auto iter = rng.begin();
        if (!phrase_parse(iter, rng.end(), bs_, skipper_))
            return make_rv(rng, false);

        // DBCInterpreter<T> callback deliberately omitted
        return make_rv(iter, rng.end(), true);
    };

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bu_(std::string_view rng, nodes_t& nodes) {
        std::vector<std::string> node_names;

        const auto bu_ = x3::omit[x3::lexeme[x3::lit("BU_:")]] >>
            (*name_)[to(node_names)] >> end_cmd_;

        auto iter = rng.begin();
        if (!phrase_parse(iter, rng.end(), bu_, skipper_))
            return make_rv(rng, false);

        unsigned node_ord = 0;
        for (const auto& nn : node_names)
            nodes.add(nn, node_ord++);
        nodes.add("Vector__XXX", node_ord);

        bu_vrtl(std::move(node_names));
        return make_rv(iter, rng.end(), true);
    };

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sg_(std::string_view rng, const nodes_t& nodes, uint32_t can_id) {
        bool has_sect = false;
        std::optional<unsigned> sg_mux_switch_val;
        std::string sg_name;
        std::optional<char> sg_mux_switch;
        unsigned sg_start_bit, sg_size;
        char sg_byte_order, sg_sign;
        double sg_factor, sg_offset;
        double sg_min, sg_max;
        std::string sg_unit;
        std::vector<size_t> rec_ords;

        const auto mux_ = -(x3::char_('m') >> x3::uint_[to(sg_mux_switch_val)]) >>
            -x3::char_('M')[to(sg_mux_switch)];

        const auto sg_ = x3::omit[x3::lexeme[x3::lit("SG_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> name_[to(sg_name)] >> mux_ >>
            od(':') >> x3::uint_[to(sg_start_bit)] >> od('|') >>
            x3::uint_[to(sg_size)] >> od('@') >> as<char>(x3::char_('0') | x3::char_('1'))[to(sg_byte_order)] >>
            as<char>(x3::char_('+') | x3::char_('-'))[to(sg_sign)] >>
            od('(') >> x3::double_[to(sg_factor)] >> od(',') >> x3::double_[to(sg_offset)] >> od(')') >>
            od('[') >> x3::double_[to(sg_min)] >> od('|') >> x3::double_[to(sg_max)] >> od(']') >>
            quoted_name_[to(sg_unit)] >> (nodes % ',')[to(rec_ords)] >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            sg_mux_switch_val.reset();
            sg_mux_switch.reset();

            if (!phrase_parse(iter, rng.end(), sg_, skipper_) || std::abs(sg_factor) <= std::numeric_limits<double>::epsilon())
                return make_rv(iter, rng.end(), !has_sect);

            if (sg_mux_switch.value_or(' ') == 'M')
                sg_mux_vrtl(
                    can_id, sg_name, sg_start_bit, sg_size, sg_byte_order,
                    sg_sign, std::move(sg_unit), std::move(rec_ords)
                );
            else
                sg_vrtl(
                    can_id, sg_mux_switch_val, sg_name, sg_start_bit, sg_size, sg_byte_order,
                    sg_sign, sg_factor, sg_offset, sg_min, sg_max, std::move(sg_unit), std::move(rec_ords)
                );
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bo_(std::string_view rng, const nodes_t& nodes) {
        bool has_sect = false;
        uint32_t can_id;
        std::string msg_name;
        size_t msg_size;
        size_t transmitter_ord;

        const auto bo_ = x3::omit[x3::lexeme[x3::lit("BO_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >>
            x3::uint_[to(can_id)] >> name_[to(msg_name)] >> od(':') >>
            x3::uint_[to(msg_size)] >> nodes[to(transmitter_ord)] >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), bo_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            bo_vrtl(can_id, std::move(msg_name), msg_size, transmitter_ord);

            auto [remain_rng, expected] = parse_sg_({ iter, rng.end() }, nodes, can_id);
            if (!expected)
                return make_rv(remain_rng, false);
            iter = remain_rng.begin();
        }

        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ev_(std::string_view rng, const nodes_t& nodes) {
        bool has_sect = false;
        std::string ev_name;
        unsigned ev_type;
        double ev_min, ev_max;
        std::string ev_unit;
        double ev_initial;
        unsigned ev_id;
        std::string ev_access_type;
        std::vector<size_t> ev_access_nodes_ords;

        const auto access_type_ = x3::lexeme[x3::lit("DUMMY_NODE_VECTOR") >>
            -x3::lit("800") >> x3::char_("0-3")];

        const auto ev_ = x3::omit[x3::lexeme[x3::lit("EV_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >>
            name_[to(ev_name)] >> od(':') >> &x3::char_("0-2") >> x3::uint_[to(ev_type)] >>
            od('[') >> x3::double_[to(ev_min)] >> od('|') >> x3::double_[to(ev_max)] >> od(']') >>
            quoted_name_[to(ev_unit)] >> x3::double_[to(ev_initial)] >> x3::uint_[to(ev_id)] >>
            as<std::string>(access_type_)[to(ev_access_type)] >>
            (nodes % ',')[to(ev_access_nodes_ords)] >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), ev_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            ev_vrtl(std::move(ev_name), ev_type, ev_min, ev_max, std::move(ev_unit),
                ev_initial, ev_id, std::move(ev_access_type), std::move(ev_access_nodes_ords)
            );
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_envvar_data_(std::string_view rng) {
        bool has_sect = false;
        std::string ev_name;
        unsigned data_size;

        const auto envar_data_ = x3::omit[x3::lexeme[x3::lit("ENVVAR_DATA_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> name_[to(ev_name)] >> od(':') >>
            x3::uint_[to(data_size)] >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), envar_data_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            envvar_data_vrtl(std::move(ev_name), data_size);
        }

        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sgtype_(std::string_view rng, const nodes_t& val_tables) {
        bool has_sect = false;
        std::optional<unsigned> msg_id;
        std::string sg_name, sg_type_name;
        unsigned sg_size;
        char sg_byte_order, sg_sign;
        double sg_factor, sg_offset;
        double sg_min, sg_max;
        std::string sg_unit;
        double sg_default_val;
        size_t val_table_ord;

        const auto sg_type_t_ = name_[to(sg_type_name)] >> od(':') >> x3::uint_[to(sg_size)] >>
            od('@') >> as<char>(x3::char_('0') | x3::char_('1'))[to(sg_byte_order)] >>
            as<char>(x3::char_('+') | x3::char_('-'))[to(sg_sign)] >> od('(') >>
            x3::double_[to(sg_factor)] >> od(',') >> x3::double_[to(sg_offset)] >> od(')') >>
            od('[') >> x3::double_[to(sg_min)] >> od('|') >> x3::double_[to(sg_max)] >>
            od(']') >> quoted_name_[to(sg_unit)] >> x3::double_[to(sg_default_val)] >>
            od(',') >> val_tables[to(val_table_ord)];

        const auto sg_type_ref_ = x3::uint_[to(msg_id)] >> name_[to(sg_name)] >>
            od(':') >> name_[to(sg_type_name)];

        const auto sg_type_ = x3::omit[x3::lexeme[x3::lit("SGTYPE_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> (sg_type_t_ | sg_type_ref_) >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            msg_id.reset();

            if (!phrase_parse(iter, rng.end(), sg_type_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            if (msg_id.has_value()) {
                sgtype_ref_vrtl(msg_id.value(), std::move(sg_name), std::move(sg_type_name));
            }
            else {
                sgtype_vrtl(
                    std::move(sg_type_name), sg_size, sg_byte_order, sg_sign, sg_factor, sg_offset,
                    sg_min, sg_max, std::move(sg_unit), sg_default_val, val_table_ord
                );
            }
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sig_group_(std::string_view rng) {
        bool has_sect = false;
        unsigned msg_id;
        std::string sig_group_name;
        unsigned repetitions;
        std::vector<std::string> sig_names;

        const auto sig_group_ = x3::omit[x3::lexeme[x3::lit("SIG_GROUP_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> x3::uint_[to(msg_id)] >> name_[to(sig_group_name)] >>
            x3::uint_[to(repetitions)] >> od(':') >> (name_ % ',')[to(sig_names)] >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), sig_group_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            sig_group_vrtl(msg_id, std::move(sig_group_name), repetitions, std::move(sig_names));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_cm_(std::string_view rng, const nodes_t& nodes) {
        bool has_sect = false;
        std::string object_type, comment_text;

        unsigned bu_ord;
        uint32_t message_id;
        std::string object_name;

        const auto comment_ = as<std::string>(
            x3::lexeme['"' >> *(('\\' >> x3::char_("\\\"")) | ~x3::char_('"') | x3::eol) >> '"']
        );

        const auto cm_glob_ = comment_[to(comment_text)];
        const auto cm_bu_ = x3::lexeme[x3::string("BU_")[to(object_type)] >> +x3::blank] >>
            nodes[to(bu_ord)] >> comment_[to(comment_text)];
        const auto cm_sg_ = x3::lexeme[x3::string("SG_")[to(object_type)] >> +x3::blank] >>
            x3::uint_[to(message_id)] >> name_[to(object_name)] >> comment_[to(comment_text)];
        const auto cm_bo_ = x3::lexeme[x3::string("BO_")[to(object_type)] >> +x3::blank] >>
            x3::uint_[to(message_id)] >> comment_[to(comment_text)];
        const auto cm_ev_ = x3::lexeme[x3::string("EV_")[to(object_type)] >> +x3::blank] >>
            name_[to(object_name)] >> comment_[to(comment_text)];

        const auto cm_ = x3::omit[x3::lexeme[x3::lit("CM_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> (cm_sg_ | cm_bo_ | cm_bu_ | cm_ev_ | cm_glob_) >>
            od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            object_type.clear();

            if (!phrase_parse(iter, rng.end(), cm_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            if (object_type.empty())
                cm_glob_vrtl(std::move(comment_text));
            else if (object_type == "BU_")
                cm_bu_vrtl(bu_ord, std::move(comment_text));
            else if (object_type == "BO_")
                cm_bo_vrtl(message_id, std::move(comment_text));
            else if (object_type == "SG_")
                cm_sg_vrtl(message_id, std::move(object_name), std::move(comment_text));
            else if (object_type == "EV_")
                cm_ev_vrtl(std::move(object_name), std::move(comment_text));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_def_(std::string_view rng, attr_types_t& ats_) {
        bool has_sect = false;
        std::string object_type, attr_name, data_type;
        int32_t int_min, int_max;
        double dbl_min, dbl_max;
        std::vector<std::string> enum_vals;

        const auto obj_type_ =
            as<std::string>(x3::lexeme[
                (x3::string("BU_") | x3::string("BO_") | x3::string("SG_") | x3::string("EV_")) >>
                    x3::omit[+x3::blank]
            ]);

        const auto att_hexint_ =
            as<std::string>(x3::lexeme[x3::string("HEX") | x3::string("INT")])[to(data_type)] >>
            x3::int_[to(int_min)] >> x3::int_[to(int_max)];

        const auto att_float_ =
            as<std::string>(x3::lexeme[x3::lit("FLOAT")])[to(data_type)] >>
            x3::double_[to(dbl_min)] >> x3::double_[to(dbl_max)];

        const auto att_str_ =
            as<std::string>(x3::lexeme[x3::lit("STRING")])[to(data_type)];

        const auto att_enum_ =
            as<std::string>(x3::lexeme[x3::lit("ENUM")])[to(data_type)] >>
            -(quoted_name_ % ',')[to(enum_vals)];


        const auto ba_def_ = x3::omit[x3::lexeme[x3::lit("BA_DEF_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >>
            -obj_type_[to(object_type)] >> quoted_name_[to(attr_name)] >>
            (att_hexint_ | att_float_ | att_str_ | att_enum_) >> od(';') >>
            end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            object_type.clear();
            if (!phrase_parse(iter, rng.end(), ba_def_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            if (data_type == "ENUM") {
                ats_.add("\"" + attr_name + "\"", quoted_name_ | x3::int_);
                ba_enum_vrtl(std::move(attr_name), std::move(object_type), std::move(enum_vals));
            }
            else if (data_type == "INT" || data_type == "HEX") {
                ats_.add("\"" + attr_name + "\"", x3::int_);
                ba_int_vrtl(std::move(attr_name), std::move(object_type), int_min, int_max);
            }
            else if (data_type == "FLOAT") {
                ats_.add("\"" + attr_name + "\"", x3::double_);
                ba_float_vrtl(std::move(attr_name), std::move(object_type), dbl_min, dbl_max);
            }
            else if (data_type == "STRING") {
                ats_.add("\"" + attr_name + "\"", quoted_name_);
                ba_string_vrtl(std::move(attr_name), std::move(object_type));
            }
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_def_def_(std::string_view rng, const attr_types_t& ats_) {
        bool has_sect = false;
        std::string attr_name;
        attr_val_t attr_val;

        const auto val_ = x3::with<attr_val_rule>(attr_val_rule{})[
            &select_parser<attr_val_rule>(ats_) >> quoted_name_[to(attr_name)] >>
                lazy<attr_val_rule>[to(attr_val)]
        ];

        const auto ba_def_def_ = x3::with<attr_val_rule>(attr_val_rule{})[
            x3::omit[x3::lexeme[(x3::string("BA_DEF_DEF_REL_") | x3::string("BA_DEF_DEF_")) >> +x3::blank]] >>
                x3::attr(true)[to(has_sect)] >> val_ >> od(';') >> end_cmd_
        ];

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), ba_def_def_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            ba_def_vrtl(std::move(attr_name), std::move(attr_val));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_(std::string_view rng, const nodes_t& nodes, const attr_types_t& ats_) {
        bool has_sect = false;
        std::string attr_name, object_type;
        attr_val_t attr_val;

        unsigned bu_ord;
        uint32_t message_id;
        std::string object_name;

        const auto av_glob_ = lazy<attr_val_rule>[to(attr_val)];
        const auto av_bu_ = x3::lexeme[x3::string("BU_")[to(object_type)] >> +x3::blank] >>
            nodes[to(bu_ord)] >> lazy<attr_val_rule>[to(attr_val)];
        const auto av_sg_ = x3::lexeme[x3::string("SG_")[to(object_type)] >> +x3::blank] >>
            x3::uint_[to(message_id)] >> name_[to(object_name)] >> lazy<attr_val_rule>[to(attr_val)];
        const auto av_bo_ = x3::lexeme[x3::string("BO_")[to(object_type)] >> +x3::blank] >>
            x3::uint_[to(message_id)] >> lazy<attr_val_rule>[to(attr_val)];
        const auto av_ev_ = x3::lexeme[x3::string("EV_")[to(object_type)] >> +x3::blank] >>
            name_[to(object_name)] >> lazy<attr_val_rule>[to(attr_val)];

        const auto ba_ = x3::with<attr_val_rule>(attr_val_rule{})[
            x3::omit[x3::lexeme[x3::lit("BA_") >> +x3::blank]] >> x3::attr(true)[to(has_sect)] >>
                &select_parser<attr_val_rule>(ats_) >> quoted_name_[to(attr_name)] >>
                (av_sg_ | av_bo_ | av_bu_ | av_ev_ | av_glob_) >> od(';') >> end_cmd_
        ];

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            object_type.clear();

            if (!phrase_parse(iter, rng.end(), ba_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            // TODO: can break this up into multiple CPOs

            ba_vrtl( std::move(attr_name), std::move(object_type), std::move(object_name),
                bu_ord, message_id, std::move(attr_val)
            );
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_val_(std::string_view rng) {
        using val_desc = std::pair<unsigned, std::string>;

        bool has_sect = false;
        std::string signal_name, env_var_name;
        std::optional<uint32_t> msg_id;
        std::vector<val_desc> val_descs;

        const auto val_ = x3::omit[x3::lexeme[x3::lit("VAL_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >>
            (name_[to(env_var_name)] | (x3::uint_[to(msg_id)] >> name_[to(signal_name)])) >>
            (*as<val_desc>(x3::uint_ >> quoted_name_))[to(val_descs)] >>
            od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            msg_id.reset();
            if (!phrase_parse(iter, rng.end(), val_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            if (!msg_id.has_value())
                val_env_vrtl(env_var_name, std::move(val_descs));
            else
                val_sg_vrtl(msg_id.value(), signal_name, std::move(val_descs));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_val_table_(std::string_view rng, nodes_t& val_tables) {
        using val_desc = std::pair<unsigned, std::string>;

        bool has_sect = false;
        std::string table_name;
        std::vector<val_desc> val_descs;

        const auto val_table_ = x3::omit[x3::lexeme[x3::lit("VAL_TABLE_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> name_[to(table_name)] >>
            (*as<val_desc>(x3::uint_ >> quoted_name_))[to(val_descs)] >>
            od(';') >> end_cmd_;

        unsigned val_table_ord = 0;
        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), val_table_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            val_tables.add(table_name, val_table_ord++);

            val_table_vrtl(std::move(table_name), std::move(val_descs));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sig_valtype_(std::string_view rng) {
        bool has_sect = false;
        unsigned msg_id;
        std::string sig_name;
        unsigned sig_ext_val_type;

        const auto sig_valtype_ = x3::omit[x3::lexeme[x3::lit("SIG_VALTYPE_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> x3::uint_[to(msg_id)] >> name_[to(sig_name)] >>
            od(':') >> &x3::char_("0-3") >> x3::uint_[to(sig_ext_val_type)] >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), sig_valtype_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            sig_valtype_vrtl(msg_id, std::move(sig_name), sig_ext_val_type);
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bo_tx_bu(std::string_view rng) {
        bool has_sect = false;
        unsigned msg_id;
        std::vector<std::string> transmitters;

        const auto sig_valtype_ = x3::omit[x3::lexeme[x3::lit("BO_TX_BU_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> x3::uint_[to(msg_id)] >> od(':') >>
            (name_ % ',')[to(transmitters)] >> od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), sig_valtype_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            bo_tx_bu_vrtl(msg_id, std::move(transmitters));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sg_mul_val_(std::string_view rng) {
        using value_range = std::pair<unsigned, unsigned>;

        bool has_sect = false;
        unsigned msg_id;
        std::string mux_sig_name, mux_switch_name;
        std::vector<value_range> val_ranges;

        const auto val_range_ = x3::uint_ >> od('-') >> x3::uint_;

        const auto sg_mul_val_ = x3::omit[x3::lexeme[x3::lit("SG_MUL_VAL_") >> +x3::blank]] >>
            x3::attr(true)[to(has_sect)] >> x3::uint_[to(msg_id)] >>
            name_[to(mux_sig_name)] >> name_[to(mux_switch_name)] >>
            (as<value_range>(val_range_) % ',')[to(val_ranges)] >>
            od(';') >> end_cmd_;

        for (auto iter = rng.begin(); iter != rng.end(); has_sect = false) {
            if (!phrase_parse(iter, rng.end(), sg_mul_val_, skipper_))
                return make_rv(iter, rng.end(), !has_sect);

            sg_mul_val_vrtl(msg_id, std::move(mux_sig_name), std::move(mux_switch_name), std::move(val_ranges));
        }
        return make_rv(rng.end(), rng.end(), true);
    }

    bool syntax_error(std::string_view where, std::string_view what) {
        auto eol = where.find('\n');
        std::string_view line{
            where.begin(),
            eol != std::string_view::npos ? where.begin() + eol : where.end()
        };
        fprintf(stderr, "Syntax error: %s\n => %s\n", what.data(), std::string(line).c_str());
        return false;
    }

    template<typename T>
    bool DBCInterpreter<T>::parse_dbc(std::string_view dbc_src) {
        auto pv = skip_blines(dbc_src);
        bool expected = true;

        auto result = parse_version(pv);
        std::cerr << "parseVersion returned: {view_len=" << result.first.size() << ", success=" << result.second << "}\n";
        pv = result.first;
        expected = result.second;

        if (!expected) return syntax_error(pv);

        if (std::tie(pv, expected) = parse_version(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_ns_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_bs_(pv); !expected)
            return syntax_error(pv, "(expected correct BS_)");

        nodes_t nodes;

        if (std::tie(pv, expected) = parse_bu_(pv, nodes); !expected)
            return syntax_error(pv, "(expected correct BU_)");

        nodes_t val_tables;

        if (std::tie(pv, expected) = parse_val_table_(pv, val_tables); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_bo_(pv, nodes); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_bo_tx_bu(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_ev_(pv, nodes); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_envvar_data_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_val_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_sgtype_(pv, val_tables); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_sig_group_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_cm_(pv, nodes); !expected)
            return syntax_error(pv);

        attr_types_t attr_types;

        if (std::tie(pv, expected) = parse_ba_def_(pv, attr_types); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_ba_def_def_(pv, attr_types); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_ba_(pv, nodes, attr_types); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_val_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_sig_valtype_(pv); !expected)
            return syntax_error(pv);

        if (std::tie(pv, expected) = parse_sg_mul_val_(pv); !expected)
            return syntax_error(pv);

        if (!pv.empty())
            return syntax_error(pv);

        return true;
    }

    template class DBCInterpreter<V2CTranscoder>;
    template class DBCInterpreter<CANTranscoder<SQLTranscoder, SQLTask>>;
    template class DBCInterpreter<CANTranscoder<CSVTranscoder, CSVTask>>;

}