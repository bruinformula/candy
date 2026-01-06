#pragma once 

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdio.h>
#include <string_view>
#include <array>

namespace Candy {

    template<size_t T>
    struct CSVHeader {
        const char* filename;
        std::array<const char*, T> headers;
    };

    template <size_t T>
    class CSVWriter {
    public:
        static std::optional<CSVWriter> create(
            const std::string_view& base_path, 
            const CSVHeader<T>& header)
        {
            std::array<char, 192> buf;
            
            if (base_path.size() + std::strlen(header.filename) + 1 > buf.size()) {
                printf("CSVWriter: Base path and filename too long.\n");
                return std::nullopt;
            }
            
            char* it = std::copy(base_path.begin(), base_path.end(), buf.begin());
            it = std::copy_n(header.filename, std::strlen(header.filename), it);
            *it = '\0';
            
            FILE* f = fopen(buf.data(), "w");
            if (!f) {
                printf("CSVWriter: Failed to open file %s for writing.\n", buf.data());
                return std::nullopt;
            }
            
            return CSVWriter(f, header);
        }
        
        ~CSVWriter() {
            if (file) {
                fclose(file);
            }
        }

        // Delete becuase FILE* isn't copyable
        CSVWriter(const CSVWriter&) = delete;
        CSVWriter& operator=(const CSVWriter&) = delete;
        
        // Move operations
        CSVWriter(CSVWriter&& other) :
            file(other.file), 
            csv_header(other.csv_header),
            first_field(other.first_field) 
        {
            other.file = nullptr;
        }
        
        CSVWriter& operator=(CSVWriter&& other) {
            if (this != &other) {
                if (file) fclose(file);
                file = other.file;
                csv_header = other.csv_header;
                first_field = other.first_field;
                other.file = nullptr;
            }
            return *this;
        }
        
        bool write_header() {
            if (!file) return false;
            
            for (size_t i = 0; i < T; ++i) {
                if (i > 0) {
                    if (fputc(',', file) == EOF) return false;
                }
                if (!write_escaped(csv_header.headers[i])) return false;
            }
            return fputc('\n', file) != EOF;
        }
        
        bool start_row() {
            if (!file) return false;
            first_field = true;
            return true;
        }
        
        bool field(std::string_view sv) {
            if (!file) return false;
            
            if (!first_field) {
                if (fputc(',', file) == EOF) return false;
            }
            first_field = false;
            
            return write_escaped(sv);
        }
        
        bool end_row() {
            if (!file) return false;
            return fputc('\n', file) != EOF;
        }
        
        bool flush() {
            if (!file) return false;
            return fflush(file) == 0;
        }

        const CSVHeader<T>& get_header() const {
            return csv_header;
        }

    private:
        explicit CSVWriter(FILE* f, const CSVHeader<T>& h) : 
            file(f), 
            csv_header(h),
            first_field(true) 
        {}
        
        bool write_escaped(std::string_view sv) {
            if (!file) return false;
            
            bool needs_quotes = false;
            for (char c : sv) {
                if (c == ',' || c == '"' || c == '\n' || c == '\r') {
                    needs_quotes = true;
                    break;
                }
            }
            
            if (needs_quotes) {
                if (fputc('"', file) == EOF) return false;
                
                for (char c : sv) {
                    if (c == '"') {
                        if (fputs("\"\"", file) == EOF) return false;
                    } else {
                        if (fputc(c, file) == EOF) return false;
                    }
                }
                
                return fputc('"', file) != EOF;
            }
            
            return fwrite(sv.data(), 1, sv.size(), file) == sv.size();
        }

        private:
        FILE* file;
        CSVHeader<T> csv_header;
        bool first_field;
    };
    
}