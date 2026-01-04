
#include "Candy/Core/CANBundle.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"
#include "Candy/DBCInterpreters/LoggingTranscoder.hpp"

#include <Candy/Candy.h>
#include <iostream>

using namespace Candy;

struct Transmitter : public CANTransmittable<Transmitter> {

    CANMessage message = {};
    std::pair<CANTime, CANFrame> raw_message = {};
    std::tuple<std::string, TableType> table_message = {};  
    CANDataStreamMetadata metadata = {};

    const CANMessage& transmit_message() {
        return message;
    }

    const std::pair<CANTime, CANFrame>& transmit_raw_message() {
        return raw_message;
    }

    const std::tuple<std::string, TableType>& transmit_table_message() {
        return table_message;
    }
    const CANDataStreamMetadata& transmit_metadata() {
        return metadata;
    }
};

struct Receiver : public CANReceivable<Receiver> {
    void receive_message(const CANMessage& message) {
    }
    
    void receive_metadata(const CANDataStreamMetadata& metadata) {
    }

    void receive_raw_message(const std::pair<CANTime, CANFrame>& sample) {
    }

    void receive_table_message(const std::string& table, const TableType& data) {
    }
};

struct Transceiver : public Receiver, public Transmitter {

};

int main() {

    auto sql_transcoder_optional = SQLTranscoder::create("./test.db");
    if (!sql_transcoder_optional) {
        std::cout << "Failed to create SQLTranscoder." << std::endl;
        return 1;
    }
    auto& sql_transcoder = sql_transcoder_optional.value();

    auto csv_transcoder_optional = CSVTranscoder::create("./test_csv_output_run", 1000);  // batch size of 1000
    if (!csv_transcoder_optional) {
        printf("Failed to create CSVTranscoder.\n");
        return 1;
    }
    auto& csv_transcoder = csv_transcoder_optional.value();

    LoggingTranscoder logger;

    DBCBundle dbc_bundle(sql_transcoder, csv_transcoder, logger);

    bool parsed = dbc_bundle.parse_dbc(Candy::transmit_file("test/network.dbc"));

    if (!parsed) {
		return 1;
    }

    Transmitter sensor_1;
    Transmitter sensor_2;

    Transceiver bms;
    Transceiver vcu;

    Receiver motor_output;
    Receiver steer_output;

    auto bundle1 = sensor_1.transmit_to(bms, vcu);
    auto bundle2 = sensor_2.transmit_to(bms, vcu);
    CANMessage input; 

    input.set_message_name("testing");

    bundle1.receive_message(input);
    bundle2.receive_message(input);

    



/*
    std::cout << "\n1. Parsing DBC file..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool parsed = node.parse_dbc(Candy::transmit_file("test/network.dbc"));

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

        node.receive_raw_message(sample);
    }
    
    // Final processing statistics
    end = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "\n   Frame generation completed in " << processing_time.count() << "ms" << std::endl;
    std::cout << "   Average generation rate: " << (double)num_frames / (processing_time.count() / 1000.0) << " frames/sec" << std::endl;
    
    // Only use blocking flush at the very end
    std::cout << "   Final flush (blocking)..." << std::endl;

    return 0;
    */
}