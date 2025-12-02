
#include <charconv>
#include <string_view>

#include "Candy/Core/DBC/DBCParser.hpp"

namespace Candy {

    template<typename T>
    void SymbolTable<T>::add(const std::string& key, const T& value) {
        symbols_[key] = value;
    }
    template<typename T>
    bool SymbolTable<T>::lookup(std::string_view key, T& result) const {
        auto it = symbols_.find(std::string(key));
        if (it != symbols_.end()) {
            result = it->second;
            return true;
        }
        return false;
    }
    template<typename T>
    bool SymbolTable<T>::contains(std::string_view key) const {
        return symbols_.find(std::string(key)) != symbols_.end();
    }
    
    template class SymbolTable<size_t>;
    template class SymbolTable<int>;
    
    void Parser::skip_whitespace() {
        while (current < end) {
            if (*current == ' ' || *current == '\t') {
                ++current;
                continue;
            }
            
            // single-line comments
            if (current + 1 < end && *current == '/' && *(current + 1) == '/') {
                current += 2;
                while (current < end && *current != '\n') {
                    ++current;
                }
                continue;
            }
            
            // multi-line comments
            if (current + 1 < end && *current == '/' && *(current + 1) == '*') {
                current += 2;
                while (current + 1 < end) {
                    if (*current == '*' && *(current + 1) == '/') {
                        current += 2;
                        break;
                    }
                    ++current;
                }
                continue;
            }
            
            break;
        }
    }

    void Parser::skip_newlines() {
        skip_whitespace();
        while (current < end && (*current == '\n' || *current == '\r')) {
            ++current;
            skip_whitespace();
        }
    }

    bool Parser::expect_char(char c) {
        skip_whitespace();
        if (current < end && *current == c) {
            ++current;
            return true;
        }
        return false;
    }

    bool Parser::expect_literal(std::string_view lit) {
        skip_whitespace();
        if (std::string_view(current, end).starts_with(lit)) {
            // check that it's not part of a longer identifier
            auto after = current + lit.size();
            if (after >= end || !std::isalnum(*after) && *after != '_') {
                current = after;
                return true;
            }
        }
        return false;
    }

    bool Parser::parse_identifier(std::string& result) {
        skip_whitespace();
        if (current >= end || (!std::isalpha(*current) && *current != '_')) {
            return false;
        }
        
        auto start = current;
        ++current;
        while (current < end && (std::isalnum(*current) || *current == '_')) {
            ++current;
        }
        
        result.assign(start, current);
        return true;
    }

    bool Parser::parse_quoted_string(std::string& result) {
        skip_whitespace();
        if (current >= end || *current != '"') {
            return false;
        }
        
        ++current;
        result.clear();
        
        while (current < end) {
            if (*current == '\\' && current + 1 < end) {
                ++current;
                if (*current == '\\' || *current == '"') {
                    result += *current;
                } else {
                    result += '\\';
                    result += *current;
                }
                ++current;
            } else if (*current == '"') {
                ++current; 
                return true;
            } else {
                result += *current;
                ++current;
            }
        }
        
        return false;
    }

    bool Parser::parse_uint(unsigned& result) {
        skip_whitespace();
        if (current >= end || !std::isdigit(*current)) {
            return false;
        }
        
        auto start = current;
        while (current < end && std::isdigit(*current)) {
            ++current;
        }
        
        auto [ptr, ec] = std::from_chars(&*start, &*current, result);
        return ec == std::errc{};
    }

    bool Parser::parse_int(int32_t& result) {
        skip_whitespace();
        if (current >= end) {
            return false;
        }
        
        auto start = current;
        if (*current == '-' || *current == '+') {
            ++current;
        }
        
        if (current >= end || !std::isdigit(*current)) {
            current = start;
            return false;
        }
        
        while (current < end && std::isdigit(*current)) {
            ++current;
        }
        
        auto [ptr, ec] = std::from_chars(&*start, &*current, result);
        return ec == std::errc{};
    }

    bool Parser::parse_double(double& result) {
        skip_whitespace();
        if (current >= end) {
            return false;
        }
        
        auto start = current;
        bool has_sign = false, has_digit = false, has_dot = false, has_exp = false;
        
        if (*current == '-' || *current == '+') {
            has_sign = true; // -
            ++current;
        }
        
        while (current < end && std::isdigit(*current)) {
            has_digit = true; // .
            ++current;
        }
        
        if (current < end && *current == '.') {
            has_dot = true;
            ++current;
            while (current < end && std::isdigit(*current)) {
                has_digit = true;
                ++current;
            }
        }
        
        if (current < end && (*current == 'e' || *current == 'E')) {
            has_exp = true; // ^
            ++current;
            if (current < end && (*current == '-' || *current == '+')) {
                ++current;
            }
            bool has_exp_digit = false;
            while (current < end && std::isdigit(*current)) {
                has_exp_digit = true;
                ++current;
            }
            if (!has_exp_digit) {
                current = start;
                return false;
            }
        }
        
        if (!has_digit) {
            current = start;
            return false;
        }
        

        #if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
            auto [ptr, ec] = std::from_chars(&*start, &*current, result); // not all cpp compilers and toolchains have the ability to go from char
            return ec == std::errc{};
        #else // otherwise fall back to strtod
            char* endptr;
            std::string temp(start, current);
            result = strtod(temp.c_str(), &endptr);
            return endptr == temp.c_str() + temp.size();
        #endif
    }

}
