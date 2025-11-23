
#include "Candy/Core/CANBundle.hpp"
#include "Candy/Interpreters/LoggingTranscoder.hpp"

#include <Candy/Candy.h>

using namespace Candy;

int main() {

    SQLTranscoder sql_transcoder("./test.db");

    CSVTranscoder csv_transcoder("./test_csv_output_run");  // batch size of 1000
    LoggingTranscoder logger;

    DBCBundle node(sql_transcoder, csv_transcoder, logger);

    bool parsed = node.parse_dbc(Candy::read_file("test/network.dbc"));

    NullReader null = {};
    
    if (!parsed) {
		return 1;
    }


/*
    std::cout << "\n1. Parsing DBC file..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool parsed = node.parse_dbc(Candy::read_file("test/network.dbc"));

    if (!parsed) {
        std::cerr << "Failed to parse DBC file." << std::endl;
        return 1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "   ✓ DBC parsed successfully in " << parse_time.count() << "ms" << std::endl;
    
    // Test 2: Generate and process CAN frames
    std::cout << "\n2. Processing CAN frames..." << std::endl;
    const int num_frames = 100000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> can_id_dist(0x100, 0x7FF);
    std::uniform_int_distribution<> data_dist(0, 255);
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_frames; ++i) {
        // Generate realistic CAN frame
        std::pair<CANTime, CANFrame> sample;
        sample.second.can_id = can_id_dist(gen);
        sample.second.len = 8;
        
        // Fill with random data
        for (int j = 0; j < 8; ++j) {
            sample.second.data[j] = data_dist(gen);
        }
        
        Candy::CANTime timestamp = std::chrono::system_clock::now();

        node.write_raw_message(sample);
    }
    
    // Final processing statistics
    end = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "\n   Frame generation completed in " << processing_time.count() << "ms" << std::endl;
    std::cout << "   Average generation rate: " << (double)num_frames / (processing_time.count() / 1000.0) << " frames/sec" << std::endl;
    
    // Only use blocking flush at the very end
    std::cout << "   Final flush (blocking)..." << std::endl;
    node.flush_sync();

    return 0;
    */
}