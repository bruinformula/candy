#include "interpreters/MotecGenerator.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <future>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace CAN {
    
    static std::vector<std::string> parse_csv_line(const std::string& line) {
        using tokenizer = boost::tokenizer<boost::escaped_list_separator<char>>;
        boost::escaped_list_separator<char> sep('\\', ',', '\"');
        tokenizer tok(line, sep);
        return std::vector<std::string>(tok.begin(), tok.end());
    }
    
    static std::string trim(const std::string& str) {
        return boost::algorithm::trim_copy(str);
    }

    MotecGenerator::MotecGenerator(const std::string& csv_path) 
        : path(csv_path), current(0) {
    }

    bool MotecGenerator::parse_csv() {
        return false;
    }
}