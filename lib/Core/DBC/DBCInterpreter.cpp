#include <iostream>
#include <string_view>
#include <algorithm>
#include <limits>

#include "Candy/Interpreters/V2CTranscoder.hpp"
#include "Candy/Interpreters/SQLTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"
#include "Candy/Interpreters/LoggingTranscoder.hpp"

#include "Candy/Core/DBC/DBCParser.hpp"
#include "Candy/Core/DBC/DBCInterpreter.hpp"

namespace Candy {

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_version(std::string_view rng) {
        Parser p(rng);
        
        if (!p.expect_literal("VERSION")) {
            return make_rv(rng, true); // VERSION is optional
        }
        
        std::string version;
        if (!p.parse_quoted_string(version)) {
            return make_rv(p.remaining(), false);
        }
        
        p.skip_newlines();
        
        version_vrtl(std::move(version));
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ns_(std::string_view rng) {
        Parser p(rng);
        
        if (!p.expect_literal("NS_")) {
            return make_rv(rng, true); // NS_ is optional
        }
        
        if (!p.expect_char(':')) {
            return make_rv(p.remaining(), false);
        }
        
        p.skip_newlines();
        
        // optional namespace keywords
        const char* keywords[] = {
            "NS_DESC_", "CM_", "BA_DEF_", "BA_", "VAL_",
            "CAT_DEF_", "CAT_", "FILTER", "BA_DEF_DEF_",
            "EV_DATA_", "ENVVAR_DATA_", "SGTYPE_", "SGTYPE_VAL_",
            "BA_DEF_SGTYPE_", "BA_SGTYPE_", "SIG_TYPE_REF_",
            "VAL_TABLE_", "SIG_GROUP_", "SIG_VALTYPE_",
            "SIGTYPE_VALTYPE_", "BO_TX_BU_", "BA_DEF_REL_",
            "BA_REL_", "BA_DEF_DEF_REL_", "BU_SG_REL_",
            "BU_EV_REL_", "BU_BO_REL_", "SG_MUL_VAL_"
        };
        
        while (!p.at_end()) {
            bool matched = false;
            for (const char* kw : keywords) {
                if (p.expect_literal(kw)) {
                    matched = true;
                    p.skip_newlines();
                    break;
                }
            }
            if (!matched) break;
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bs_(std::string_view rng) {
        Parser p(rng);
        
        if (!p.expect_literal("BS_")) {
            return make_rv(rng, false);
        }
        
        if (!p.expect_char(':')) {
            return make_rv(p.remaining(), false);
        }
        
        // optional: baudrate:BTR1,BTR2
        p.parse_optional([&]() {
            unsigned val1, val2, val3;
            return p.parse_uint(val1) && p.expect_char(':') && 
                   p.parse_uint(val2) && p.expect_char(',') && p.parse_uint(val3);
        });
        
        p.skip_newlines();
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bu_(std::string_view rng, nodes_t& nodes) {
        Parser p(rng);
        
        if (!p.expect_literal("BU_")) {
            return make_rv(rng, false);
        }
        
        if (!p.expect_char(':')) {
            return make_rv(p.remaining(), false);
        }
        
        std::vector<std::string> node_names;
        std::string name;
        
        while (p.parse_identifier(name)) {
            node_names.push_back(name);
        }
        
        p.skip_newlines();
        
        unsigned node_ord = 0;
        for (const auto& nn : node_names) {
            nodes.add(nn, node_ord++);
        }
        nodes.add("Vector__XXX", node_ord);
        
        bu_vrtl(std::move(node_names));
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sg_(std::string_view rng, const nodes_t& nodes, uint32_t can_id) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("SG_")) {
                break;
            }
            
            std::string sg_name;
            if (!p.parse_identifier(sg_name)) {
                return make_rv(p.remaining(), false);
            }
            
            // Parse optional multiplexer indicator
            std::optional<unsigned> sg_mux_switch_val;
            std::optional<char> sg_mux_switch;
            
            auto saved = p.position();
            if (p.expect_char('m')) {
                unsigned val;
                if (p.parse_uint(val)) {
                    sg_mux_switch_val = val;
                }
            }
            
            if (p.expect_char('M')) {
                sg_mux_switch = 'M';
            }
            
            if (!p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            unsigned sg_start_bit, sg_size;
            if (!p.parse_uint(sg_start_bit) || !p.expect_char('|') || !p.parse_uint(sg_size)) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char('@')) {
                return make_rv(p.remaining(), false);
            }
            
            char sg_byte_order, sg_sign;
            p.skip_whitespace();
            if (p.at_end() || (*p.position() != '0' && *p.position() != '1')) {
                return make_rv(p.remaining(), false);
            }
            sg_byte_order = *p.position();
            auto it = p.position();
            ++it;
            Parser p2({it, p.remaining().end()});
            p = p2;
            
            p.skip_whitespace();
            if (p.at_end() || (*p.position() != '+' && *p.position() != '-')) {
                return make_rv(p.remaining(), false);
            }
            sg_sign = *p.position();
            it = p.position();
            ++it;
            Parser p3({it, p.remaining().end()});
            p = p3;
            
            if (!p.expect_char('(')) {
                return make_rv(p.remaining(), false);
            }
            
            double sg_factor, sg_offset;
            if (!p.parse_double(sg_factor) || !p.expect_char(',') || !p.parse_double(sg_offset)) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char(')')) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char('[')) {
                return make_rv(p.remaining(), false);
            }
            
            double sg_min, sg_max;
            if (!p.parse_double(sg_min) || !p.expect_char('|') || !p.parse_double(sg_max)) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char(']')) {
                return make_rv(p.remaining(), false);
            }
            
            std::string sg_unit;
            if (!p.parse_quoted_string(sg_unit)) {
                return make_rv(p.remaining(), false);
            }
            
            // Parse receiver nodes (comma-separated list)
            std::vector<size_t> rec_ords;
            std::string node_name;
            
            if (p.parse_identifier(node_name)) {
                size_t ord;
                if (nodes.lookup(node_name, ord)) {
                    rec_ords.push_back(ord);
                }
                
                while (p.expect_char(',')) {
                    if (p.parse_identifier(node_name)) {
                        if (nodes.lookup(node_name, ord)) {
                            rec_ords.push_back(ord);
                        }
                    }
                }
            }
            
            p.skip_newlines();
            
            // Check for valid factor
            if (std::abs(sg_factor) <= std::numeric_limits<double>::epsilon()) {
                return make_rv(p.remaining(), false);
            }
            
            if (sg_mux_switch.value_or(' ') == 'M') {
                sg_mux_vrtl(
                    can_id, sg_name, sg_start_bit, sg_size, sg_byte_order,
                    sg_sign, std::move(sg_unit), std::move(rec_ords)
                );
            } else {
                sg_vrtl(
                    can_id, sg_mux_switch_val, sg_name, sg_start_bit, sg_size, sg_byte_order,
                    sg_sign, sg_factor, sg_offset, sg_min, sg_max, std::move(sg_unit), std::move(rec_ords)
                );
            }
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bo_(std::string_view rng, const nodes_t& nodes) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("BO_")) {
                break;
            }
            
            uint32_t can_id;
            if (!p.parse_uint(can_id)) {
                return make_rv(p.remaining(), false);
            }
            
            std::string msg_name;
            if (!p.parse_identifier(msg_name)) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            unsigned msg_size;
            if (!p.parse_uint(msg_size)) {
                return make_rv(p.remaining(), false);
            }
            
            std::string transmitter_name;
            size_t transmitter_ord = 0;
            if (p.parse_identifier(transmitter_name)) {
                nodes.lookup(transmitter_name, transmitter_ord);
            }
            
            p.skip_newlines();
            
            bo_vrtl(can_id, std::move(msg_name), msg_size, transmitter_ord);
            
            // Parse signals for this message
            auto [remain_rng, expected] = parse_sg_(p.remaining(), nodes, can_id);
            if (!expected) {
                return make_rv(remain_rng, false);
            }
            
            Parser p_next(remain_rng);
            p = p_next;
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ev_(std::string_view rng, const nodes_t& nodes) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("EV_")) {
                break;
            }
            
            std::string ev_name;
            if (!p.parse_identifier(ev_name) || !p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            unsigned ev_type;
            if (!p.parse_uint(ev_type) || ev_type > 2) {
                return make_rv(p.remaining(), false);
            }
            
            double ev_min, ev_max;
            if (!p.expect_char('[') || !p.parse_double(ev_min) || !p.expect_char('|') ||
                !p.parse_double(ev_max) || !p.expect_char(']')) {
                return make_rv(p.remaining(), false);
            }
            
            std::string ev_unit;
            if (!p.parse_quoted_string(ev_unit)) {
                return make_rv(p.remaining(), false);
            }
            
            double ev_initial;
            unsigned ev_id;
            if (!p.parse_double(ev_initial) || !p.parse_uint(ev_id)) {
                return make_rv(p.remaining(), false);
            }
            
            // Parse access type: could be "DUMMY_NODE_VECTOR{digit}" or similar identifier
            p.skip_whitespace();
            std::string ev_access_type;
            
            // Try to parse as an identifier (handles DUMMY_NODE_VECTOR1, DUMMY_NODE_VECTOR8000, etc.)
            if (!p.parse_identifier(ev_access_type)) {
                // If no identifier, leave it empty
                ev_access_type = "";
            }
            
            // Parse access nodes (comma-separated)
            std::vector<size_t> ev_access_nodes_ords;
            std::string node_name;
            
            if (p.parse_identifier(node_name)) {
                size_t ord;
                if (nodes.lookup(node_name, ord)) {
                    ev_access_nodes_ords.push_back(ord);
                }
                
                while (p.expect_char(',')) {
                    if (p.parse_identifier(node_name) && nodes.lookup(node_name, ord)) {
                        ev_access_nodes_ords.push_back(ord);
                    }
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            ev_vrtl(std::move(ev_name), ev_type, ev_min, ev_max, std::move(ev_unit),
                ev_initial, ev_id, std::move(ev_access_type), std::move(ev_access_nodes_ords));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_envvar_data_(std::string_view rng) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("ENVVAR_DATA_")) {
                break;
            }
            
            std::string ev_name;
            unsigned data_size;
            
            if (!p.parse_identifier(ev_name) || !p.expect_char(':') || !p.parse_uint(data_size)) {
                return make_rv(p.remaining(), false);
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            envvar_data_vrtl(std::move(ev_name), data_size);
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sgtype_(std::string_view rng, const nodes_t& val_tables) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("SGTYPE_")) {
                break;
            }
            
            // Try to parse as signal type definition or reference
            auto saved_pos = p.position();
            
            // Try reference first: msg_id signal_name : type_name ;
            unsigned msg_id_temp;
            if (p.parse_uint(msg_id_temp)) {
                std::string sg_name, sg_type_name;
                if (p.parse_identifier(sg_name) && p.expect_char(':') && p.parse_identifier(sg_type_name)) {
                    p.expect_char(';');
                    p.skip_newlines();
                    sgtype_ref_vrtl(msg_id_temp, std::move(sg_name), std::move(sg_type_name));
                    continue;
                }
            }
            
            // Not a reference, try type definition
            Parser p_reset({saved_pos, p.remaining().end()});
            p = p_reset;
            
            std::string sg_type_name;
            if (!p.parse_identifier(sg_type_name) || !p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            unsigned sg_size;
            char sg_byte_order, sg_sign;
            double sg_factor, sg_offset, sg_min, sg_max;
            std::string sg_unit;
            double sg_default_val;
            
            if (!p.parse_uint(sg_size) || !p.expect_char('@')) {
                return make_rv(p.remaining(), false);
            }
            
            p.skip_whitespace();
            if (p.at_end() || (*p.position() != '0' && *p.position() != '1')) {
                return make_rv(p.remaining(), false);
            }
            sg_byte_order = *p.position();
            auto it = p.position();
            ++it;
            Parser p2({it, p.remaining().end()});
            p = p2;
            
            p.skip_whitespace();
            if (p.at_end() || (*p.position() != '+' && *p.position() != '-')) {
                return make_rv(p.remaining(), false);
            }
            sg_sign = *p.position();
            it = p.position();
            ++it;
            Parser p3({it, p.remaining().end()});
            p = p3;
            
            if (!p.expect_char('(') || !p.parse_double(sg_factor) || !p.expect_char(',') ||
                !p.parse_double(sg_offset) || !p.expect_char(')')) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.expect_char('[') || !p.parse_double(sg_min) || !p.expect_char('|') ||
                !p.parse_double(sg_max) || !p.expect_char(']')) {
                return make_rv(p.remaining(), false);
            }
            
            if (!p.parse_quoted_string(sg_unit) || !p.parse_double(sg_default_val)) {
                return make_rv(p.remaining(), false);
            }
            
            std::string val_table_name;
            size_t val_table_ord = 0;
            
            if (p.expect_char(',') && p.parse_identifier(val_table_name)) {
                val_tables.lookup(val_table_name, val_table_ord);
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            sgtype_vrtl(std::move(sg_type_name), sg_size, sg_byte_order, sg_sign, sg_factor, sg_offset,
                sg_min, sg_max, std::move(sg_unit), sg_default_val, val_table_ord);
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sig_group_(std::string_view rng) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("SIG_GROUP_")) {
                break;
            }
            
            unsigned msg_id;
            std::string sig_group_name;
            unsigned repetitions;
            
            if (!p.parse_uint(msg_id) || !p.parse_identifier(sig_group_name) ||
                !p.parse_uint(repetitions) || !p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            std::vector<std::string> sig_names;
            std::string sig_name;
            
            if (p.parse_identifier(sig_name)) {
                sig_names.push_back(sig_name);
                
                while (p.expect_char(',')) {
                    if (p.parse_identifier(sig_name)) {
                        sig_names.push_back(sig_name);
                    }
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            sig_group_vrtl(msg_id, std::move(sig_group_name), repetitions, std::move(sig_names));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_cm_(std::string_view rng, const nodes_t& nodes) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("CM_")) {
                break;
            }
            
            std::string comment_text;
            std::string object_type;
            
            // Try different comment types
            auto saved = p.position();
            
            // CM_ SG_ msg_id signal_name "comment" ;
            if (p.expect_literal("SG_")) {
                object_type = "SG_";
                unsigned message_id;
                std::string object_name;
                
                if (!p.parse_uint(message_id) || !p.parse_identifier(object_name) ||
                    !p.parse_quoted_string(comment_text)) {
                    return make_rv(p.remaining(), false);
                }
                
                p.expect_char(';');
                p.skip_newlines();
                
                cm_sg_vrtl(message_id, std::move(object_name), std::move(comment_text));
                continue;
            }
            
            // CM_ BO_ msg_id "comment" ;
            if (p.expect_literal("BO_")) {
                object_type = "BO_";
                unsigned message_id;
                
                if (!p.parse_uint(message_id) || !p.parse_quoted_string(comment_text)) {
                    return make_rv(p.remaining(), false);
                }
                
                p.expect_char(';');
                p.skip_newlines();
                
                cm_bo_vrtl(message_id, std::move(comment_text));
                continue;
            }
            
            // CM_ BU_ node_name "comment" ;
            if (p.expect_literal("BU_")) {
                object_type = "BU_";
                std::string node_name;
                size_t bu_ord = 0;
                
                if (!p.parse_identifier(node_name) || !p.parse_quoted_string(comment_text)) {
                    return make_rv(p.remaining(), false);
                }
                
                nodes.lookup(node_name, bu_ord);
                
                p.expect_char(';');
                p.skip_newlines();
                
                cm_bu_vrtl(bu_ord, std::move(comment_text));
                continue;
            }
            
            // CM_ EV_ env_var_name "comment" ;
            if (p.expect_literal("EV_")) {
                object_type = "EV_";
                std::string object_name;
                
                if (!p.parse_identifier(object_name) || !p.parse_quoted_string(comment_text)) {
                    return make_rv(p.remaining(), false);
                }
                
                p.expect_char(';');
                p.skip_newlines();
                
                cm_ev_vrtl(std::move(object_name), std::move(comment_text));
                continue;
            }
            
            // CM_ "comment" ; (global comment)
            if (p.parse_quoted_string(comment_text)) {
                p.expect_char(';');
                p.skip_newlines();
                
                cm_glob_vrtl(std::move(comment_text));
                continue;
            }
            
            return make_rv(p.remaining(), false);
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_def_(std::string_view rng, attr_types_t& ats_) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("BA_DEF_")) {
                break;
            }
            
            std::string object_type;
            std::string attr_name;
            std::string data_type;
            
            // Optional object type: BU_, BO_, SG_, EV_
            auto saved = p.position();
            if (p.expect_literal("BU_")) {
                object_type = "BU_";
            } else if (p.expect_literal("BO_")) {
                object_type = "BO_";
            } else if (p.expect_literal("SG_")) {
                object_type = "SG_";
            } else if (p.expect_literal("EV_")) {
                object_type = "EV_";
            }
            
            if (!p.parse_quoted_string(attr_name)) {
                return make_rv(p.remaining(), false);
            }
            
            // Parse attribute type
            if (p.expect_literal("INT") || p.expect_literal("HEX")) {
                data_type = "INT";
                int32_t int_min, int_max;
                
                if (!p.parse_int(int_min) || !p.parse_int(int_max)) {
                    return make_rv(p.remaining(), false);
                }
                
                ats_.add(attr_name, 0); // 0 = int type
                ba_int_vrtl(std::move(attr_name), std::move(object_type), int_min, int_max);
                
            } else if (p.expect_literal("FLOAT")) {
                data_type = "FLOAT";
                double dbl_min, dbl_max;
                
                if (!p.parse_double(dbl_min) || !p.parse_double(dbl_max)) {
                    return make_rv(p.remaining(), false);
                }
                
                ats_.add(attr_name, 1); // 1 = double type
                ba_float_vrtl(std::move(attr_name), std::move(object_type), dbl_min, dbl_max);
                
            } else if (p.expect_literal("STRING")) {
                data_type = "STRING";
                ats_.add(attr_name, 2); // 2 = string type
                ba_string_vrtl(std::move(attr_name), std::move(object_type));
                
            } else if (p.expect_literal("ENUM")) {
                data_type = "ENUM";
                std::vector<std::string> enum_vals;
                std::string eval;
                
                if (p.parse_quoted_string(eval)) {
                    enum_vals.push_back(eval);
                    
                    while (p.expect_char(',')) {
                        if (p.parse_quoted_string(eval)) {
                            enum_vals.push_back(eval);
                        }
                    }
                }
                
                ats_.add(attr_name, 2); // Enum treated as string
                ba_enum_vrtl(std::move(attr_name), std::move(object_type), std::move(enum_vals));
            } else {
                return make_rv(p.remaining(), false);
            }
            
            p.expect_char(';');
            p.skip_newlines();
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_def_def_(std::string_view rng, const attr_types_t& ats_) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("BA_DEF_DEF_REL_") && !p.expect_literal("BA_DEF_DEF_")) {
                break;
            }
            
            std::string attr_name;
            if (!p.parse_quoted_string(attr_name)) {
                return make_rv(p.remaining(), false);
            }
            
            attr_val_t attr_val;
            int type = 0;
            ats_.lookup(attr_name, type);
            
            if (type == 0) { // int
                int32_t val;
                if (p.parse_int(val)) {
                    attr_val = val;
                }
            } else if (type == 1) { // double
                double val;
                if (p.parse_double(val)) {
                    attr_val = val;
                }
            } else { // string
                std::string val;
                if (p.parse_quoted_string(val)) {
                    attr_val = val;
                } else if (p.parse_int(reinterpret_cast<int32_t&>(val))) {
                    // Sometimes enum values are given as ints
                    attr_val = std::to_string(*reinterpret_cast<int32_t*>(&val));
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            ba_def_vrtl(std::move(attr_name), std::move(attr_val));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_ba_(std::string_view rng, const nodes_t& nodes, const attr_types_t& ats_) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("BA_")) {
                break;
            }
            
            std::string attr_name;
            if (!p.parse_quoted_string(attr_name)) {
                return make_rv(p.remaining(), false);
            }
            
            std::string object_type;
            std::string object_name;
            size_t bu_ord = 0;
            uint32_t message_id = 0;
            
            // Try different object types
            if (p.expect_literal("SG_")) {
                object_type = "SG_";
                if (!p.parse_uint(message_id) || !p.parse_identifier(object_name)) {
                    return make_rv(p.remaining(), false);
                }
            } else if (p.expect_literal("BO_")) {
                object_type = "BO_";
                if (!p.parse_uint(message_id)) {
                    return make_rv(p.remaining(), false);
                }
            } else if (p.expect_literal("BU_")) {
                object_type = "BU_";
                std::string node_name;
                if (!p.parse_identifier(node_name)) {
                    return make_rv(p.remaining(), false);
                }
                nodes.lookup(node_name, bu_ord);
            } else if (p.expect_literal("EV_")) {
                object_type = "EV_";
                if (!p.parse_identifier(object_name)) {
                    return make_rv(p.remaining(), false);
                }
            }
            
            // Parse attribute value based on type
            attr_val_t attr_val;
            int type = 0;
            ats_.lookup(attr_name, type);
            
            if (type == 0) { // int
                int32_t val;
                if (p.parse_int(val)) {
                    attr_val = val;
                }
            } else if (type == 1) { // double
                double val;
                if (p.parse_double(val)) {
                    attr_val = val;
                }
            } else { // string
                std::string val;
                if (p.parse_quoted_string(val)) {
                    attr_val = val;
                } else {
                    int32_t ival;
                    if (p.parse_int(ival)) {
                        attr_val = std::to_string(ival);
                    }
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            ba_vrtl(std::move(attr_name), std::move(object_type), std::move(object_name),
                bu_ord, message_id, std::move(attr_val));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_val_(std::string_view rng) {
        using val_desc = std::pair<unsigned, std::string>;
        
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("VAL_")) {
                break;
            }
            
            // Try to determine if this is for a signal or environment variable
            auto saved = p.position();
            unsigned msg_id_temp;
            
            std::string signal_name, env_var_name;
            std::optional<uint32_t> msg_id;
            
            if (p.parse_uint(msg_id_temp)) { // VAL_ msg_id signal_name val desc val desc ... ;
                msg_id = msg_id_temp;
                
                if (!p.parse_identifier(signal_name)) {
                    return make_rv(p.remaining(), false);
                }
            } else { // VAL_ env_var_name val desc val desc ... ;
                Parser p_reset({saved, p.remaining().end()});
                p = p_reset;
                
                if (!p.parse_identifier(env_var_name)) {
                    return make_rv(p.remaining(), false);
                }
            }
            
            // Parse value descriptions
            std::vector<val_desc> val_descs;
            unsigned val;
            std::string desc;
            
            while (p.parse_uint(val) && p.parse_quoted_string(desc)) {
                val_descs.emplace_back(val, desc);
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            if (!msg_id.has_value()) {
                val_env_vrtl(env_var_name, std::move(val_descs));
            } else {
                val_sg_vrtl(msg_id.value(), signal_name, std::move(val_descs));
            }
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_val_table_(std::string_view rng, nodes_t& val_tables) {
        using val_desc = std::pair<unsigned, std::string>;
        
        Parser p(rng);
        unsigned val_table_ord = 0;
        
        while (!p.at_end()) {
            if (!p.expect_literal("VAL_TABLE_")) {
                break;
            }
            
            std::string table_name;
            if (!p.parse_identifier(table_name)) {
                return make_rv(p.remaining(), false);
            }
            
            std::vector<val_desc> val_descs;
            unsigned val;
            std::string desc;
            
            while (p.parse_uint(val) && p.parse_quoted_string(desc)) {
                val_descs.emplace_back(val, desc);
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            val_tables.add(table_name, val_table_ord++);
            val_table_vrtl(std::move(table_name), std::move(val_descs));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sig_valtype_(std::string_view rng) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("SIG_VALTYPE_")) {
                break;
            }
            
            unsigned msg_id;
            std::string sig_name;
            unsigned sig_ext_val_type;
            
            if (!p.parse_uint(msg_id) || !p.parse_identifier(sig_name) ||
                !p.expect_char(':') || !p.parse_uint(sig_ext_val_type)) {
                return make_rv(p.remaining(), false);
            }
            
            if (sig_ext_val_type > 3) {
                return make_rv(p.remaining(), false);
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            sig_valtype_vrtl(msg_id, std::move(sig_name), sig_ext_val_type);
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_bo_tx_bu(std::string_view rng) {
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("BO_TX_BU_")) {
                break;
            }
            
            unsigned msg_id;
            if (!p.parse_uint(msg_id) || !p.expect_char(':')) {
                return make_rv(p.remaining(), false);
            }
            
            std::vector<std::string> transmitters;
            std::string trans;
            
            if (p.parse_identifier(trans)) {
                transmitters.push_back(trans);
                
                while (p.expect_char(',')) {
                    if (p.parse_identifier(trans)) {
                        transmitters.push_back(trans);
                    }
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            bo_tx_bu_vrtl(msg_id, std::move(transmitters));
        }
        
        return make_rv(p.remaining(), true);
    }

    template<typename T>
    ParseResult DBCInterpreter<T>::parse_sg_mul_val_(std::string_view rng) {
        using value_range = std::pair<unsigned, unsigned>;
        
        Parser p(rng);
        
        while (!p.at_end()) {
            if (!p.expect_literal("SG_MUL_VAL_")) {
                break;
            }
            
            unsigned msg_id;
            std::string mux_sig_name, mux_switch_name;
            
            if (!p.parse_uint(msg_id) || !p.parse_identifier(mux_sig_name) ||
                !p.parse_identifier(mux_switch_name)) {
                return make_rv(p.remaining(), false);
            }
            
            std::vector<value_range> val_ranges;
            unsigned start, end;
            
            if (p.parse_uint(start)) {
                if (p.expect_char('-') && p.parse_uint(end)) {
                    val_ranges.emplace_back(start, end);
                } else {
                    val_ranges.emplace_back(start, start);
                }
                
                while (p.expect_char(',')) {
                    if (p.parse_uint(start)) {
                        if (p.expect_char('-') && p.parse_uint(end)) {
                            val_ranges.emplace_back(start, end);
                        } else {
                            val_ranges.emplace_back(start, start);
                        }
                    }
                }
            }
            
            p.expect_char(';');
            p.skip_newlines();
            
            sg_mul_val_vrtl(msg_id, std::move(mux_sig_name), std::move(mux_switch_name), std::move(val_ranges));
        }
        
        return make_rv(p.remaining(), true);
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
        Parser p(dbc_src);
        p.skip_newlines();
        
        auto pv = p.remaining();
        bool expected = true;

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

        Parser p_final(pv);
        p_final.skip_newlines();
        if (!p_final.at_end())
            return syntax_error(pv);

        return true;
    }

    template class DBCInterpreter<CSVTranscoder>;
    template class DBCInterpreter<SQLTranscoder>;
    template class DBCInterpreter<V2CTranscoder>;
    template class DBCInterpreter<LoggingTranscoder>;

}
