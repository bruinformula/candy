#pragma once 

#include <random>
#include <fstream>

#include "Candy/Core/CANKernelTypes.hpp"

namespace Candy {

    using CANTime = std::chrono::system_clock::time_point;

    inline CANFrame generate_frame() {
        static std::default_random_engine generator; 
        static std::uniform_int_distribution<uint8_t> byte_dist(0, 255); // random byte
        static std::uniform_int_distribution<canid_t> can_id_dist(1, 1029); // 7 is the highest can_id in example.dbc
        static std::uniform_int_distribution<int> dlc_dist(1, 8); // CAN frame length 1-8 bytes

        CANFrame frame;
        frame.can_id = can_id_dist(generator);
        frame.len = dlc_dist(generator);
        
        // Fill with random byte data
        for (int i = 0; i < frame.len; i++) {
            frame.data[i] = byte_dist(generator);
        }
        
        // Zero out unused bytes
        for (int i = frame.len; i < 8; i++) {
            frame.data[i] = 0;
        }

        return frame;
    }

    inline std::string transmit_file(const std::string& dbc_path) {
        FILE* file = fopen(dbc_path.c_str(), "rb");
        if (!file) return "";
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Read entire file
        std::string content(file_size, '\0');
        fread(content.data(), 1, file_size, file);
        fclose(file);
        
        return content;
    }
    
}