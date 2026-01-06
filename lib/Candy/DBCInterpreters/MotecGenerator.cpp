#include "Candy/DBCInterpreters/MotecGenerator.hpp"

#include <algorithm>
#include <vector>
#include <cctype>

namespace Candy {
    
    static std::vector<std::string> parse_csv_line(const std::string& line) {
        std::vector<std::string> result;
        std::string current;
        bool in_quotes = false;
        bool escape_next = false;
        
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            
            if (escape_next) {
                current += c;
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                result.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        
        result.push_back(current);
        return result;
    }
    
    static std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), 
            [](unsigned char ch) { return std::isspace(ch); });
        auto end = std::find_if_not(str.rbegin(), str.rend(),
            [](unsigned char ch) { return std::isspace(ch); }).base();
        return (start < end) ? std::string(start, end) : std::string();
    }

    MotecGenerator::MotecGenerator(const std::string& csv_path) 
        : path(csv_path), current(0) {
    }

    bool MotecGenerator::parse_csv() {
        return false;
    }
}