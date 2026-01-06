#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <cstdlib>

namespace Candy {

    template<typename T>
    class SymbolTable {
        std::unordered_map<std::string, T> symbols_;
    public:
        void add(const std::string& key, const T& value);
        bool lookup(std::string_view key, T& result) const;
        bool contains(std::string_view key) const;
    };

    extern template class SymbolTable<size_t>;
    extern template class SymbolTable<int>;

    using nodes_t = SymbolTable<size_t>;
    using attr_types_t = SymbolTable<int>; // 0 = int, 1 = double, 2 = string

    class Parser {
        std::string_view::iterator current;
        std::string_view::iterator end;

    public:
        Parser(std::string_view input) : 
            current(input.begin()), 
            end(input.end()) 
        {}

        std::string_view::iterator position() const { return current; }
        std::string_view remaining() const { return { current, end }; }
        bool at_end() const { return current >= end; }

        void skip_whitespace();
        void skip_newlines();

        bool expect_char(char c);
        bool expect_literal(std::string_view lit);
        bool parse_identifier(std::string& result);
        bool parse_quoted_string(std::string& result);
        bool parse_uint(unsigned& result);
        bool parse_int(int32_t& result);
        bool parse_double(double& result);

        template<typename F>
        bool parse_optional(F&& parse_func) {
            auto saved = current;
            if (!parse_func()) {
                current = saved;
            }
            return true; // Optional always succeeds
        }

        template<typename... Funcs>
        bool parse_alternative(Funcs&&... funcs) {
            return (try_parse(std::forward<Funcs>(funcs)) || ...);
        }

        template<typename F>
        bool try_parse(F&& func) {
            auto saved = current;
            if (func()) {
                return true;
            }
            current = saved;
            return false;
        }

        template<typename T, typename ParseFunc>
        bool parse_list(std::vector<T>& results, ParseFunc parse_func, char delimiter) {
            results.clear();
            T item;
            
            if (!parse_func(item)) {
                return true;
            }
            
            results.push_back(std::move(item));
            
            while (true) {
                auto saved = current;
                skip_whitespace();
                
                if (current >= end || *current != delimiter) {
                    current = saved;
                    break;
                }
                
                ++current; 
                
                if (!parse_func(item)) {
                    current = saved;
                    break;
                }
                
                results.push_back(std::move(item));
            }
            
            return true;
        }
    };

    inline auto make_rv(std::string_view::iterator b, std::string_view::iterator e, bool v) {
        return std::make_pair(std::string_view{ b, e }, v);
    }

    inline auto make_rv(std::string_view s, bool v) {
        return std::make_pair(s, v);
    }

    static bool syntax_error(std::string_view where, std::string_view what = "");

}