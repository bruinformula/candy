#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <bit>
#include <sstream>
#include <filesystem>
#include <thread>
#include <random>

#include "DBC/DBCParser.hpp"
#include "transcoders/CSVTranscoder.hpp"
#include "CAN/frame/FrameIterator.hpp"
#include "CAN/CANHelpers.hpp"
#include "interpreters/MotecGenerator.hpp"

int main() {
    // Clean up any existing test database
    //std::filesystem::remove("./test_csv_output");
    
    // Initialize CSVTranscoder this will output 3 csv files
    CAN::CSVTranscoder transcoder("./test_csv_output", 1000);  // batch size of 1000
    
    std::cout << "=== Motec CSV Transcoder Test ===" << std::endl;
    
    // Test 1: Parse DBC file
    std::cout << "\n1. Parsing DBC file..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool parsed_dbc = transcoder.parse_dbc(CAN::read_file("test/network.dbc"));
    if (!parsed_dbc) {
        std::cerr << "Failed to parse DBC file." << std::endl;
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "   ✓ DBC parsed_dbc successfully in " << parse_time.count() << "ms" << std::endl;
    
    // Test 2: Generate and process CAN frames
    std::cout << "\n2. Processing CAN frames..." << std::endl;
    
    CAN::MotecGenerator generator("test/motec_test.csv");

    bool parsed_csv = generator.parse_csv();

    if (!parsed_csv) {
        std::cerr << "Failed to parse Motec CSV file." << std::endl;
        return 1;
    }


    /*
    for (int i = 0; i < num_frames; ++i) {
        // Generate realistic CAN frame
        CANFrame frame;
        frame.can_id = can_id_dist(gen);
        frame.len = 8;
        
        // Fill with random data
        for (int j = 0; j < 8; ++j) {
            frame.data[j] = data_dist(gen);
        }
        
        CAN::CANTime timestamp = std::chrono::system_clock::now();

        transcoder.transcode(timestamp, frame);
    }
    
    // Final processing statistics
    end = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "\n   Frame generation completed in " << processing_time.count() << "ms" << std::endl;
    std::cout << "   Average generation rate: " << (double)num_frames / (processing_time.count() / 1000.0) << " frames/sec" << std::endl;
    
    // Only use blocking flush at the very end
    std::cout << "   Final flush (blocking)..." << std::endl;
    transcoder.flush_sync();
    */
}