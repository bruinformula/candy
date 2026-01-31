#include <chrono>
#include <random>

#include <Candy/Candy.h>

int main() {
    // Clean up any existing test database
    //std::filesystem::remove("./test_csv_output");
    
    auto transcoder_optional = Candy::CSVTranscoder::create("./test_csv_output/", 1000);  // batch size of 1000
    if (!transcoder_optional) {
        printf("Failed to create CSVTranscoder.\n");
        return 1;
    }
    auto& transcoder = transcoder_optional.value();  
    printf("=== CAN CSV Transcoder Test ===\n");
    
    // Test 1: Parse DBC file
    printf("\n1. Parsing DBC file...\n");
    auto start = std::chrono::high_resolution_clock::now();
    
    bool parsed = transcoder.parse_dbc(Candy::transmit_file("test/network.dbc"));
    if (!parsed) {
        printf("Failed to parse DBC file.\n");
        return 1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("   ✓ DBC parsed successfully in %lld ms\n", parse_time.count());
    
    // Test 2: Generate and process CAN frames
    printf("\n2. Processing CAN frames...\n");
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

        transcoder.receive_raw_message(sample);
    }
    
    // Final processing statistics
    end = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    printf("\n   Frame generation completed in %lld ms\n", processing_time.count());
    printf("   Average generation rate: %f frames/sec\n", (double)num_frames / (processing_time.count() / 1000.0));
    
    // Only use blocking flush at the very end
    printf("   Final flush (blocking)...");
    
}