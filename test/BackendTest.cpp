#include <iostream>
#include <thread>
#include <filesystem>

#include "Candy/Candy.h"

using namespace Candy;

void example_usage() {
    // Create a CAN data buffer with SQL backend
    auto sql_buffer = CANDataBuffer::create_with_sql("Test", "./engine_data.db");
    
    // Set up metadata
    sql_buffer->update_description("Uhhh");
    sql_buffer->set_auto_flush(true, 500); // Auto-flush every 500 messages
    
    // Simulate adding CAN messages
    auto now = std::chrono::system_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        CANFrame frame;
        frame.can_id = 0x123; // RPM message
        frame.len = 8;
        
        uint16_t rpm = 800 + (i * 10) % 6000; // RPM from 800 to 6800
        frame.data[0] = rpm & 0xFF;
        frame.data[1] = (rpm >> 8) & 0xFF;
        
        // Add decoded signals
        std::unordered_map<std::string, double> signals = {
            {"RPM", static_cast<double>(rpm)}
        };
        
        std::unordered_map<std::string, std::string> units = {
            {"Test_RPM", "rpm"},
        };
        
        auto timestamp = now + std::chrono::milliseconds(i * 10); // 10ms intervals
        sql_buffer->add_frame(timestamp, frame, "Status", signals, units);
    }

        for (int i = 0; i < 1000; ++i) {
        CANFrame frame;
        frame.can_id = 0x123; // RPM message
        frame.len = 8;
        
        uint16_t rpm = 800 + (i * 10) % 6000; // RPM from 800 to 6800
        frame.data[0] = rpm & 0xFF;
        frame.data[1] = (rpm >> 8) & 0xFF;
        
        // Add decoded signals
        std::unordered_map<std::string, double> signals = {
            {"RPM", static_cast<double>(rpm)}
        };
        
        std::unordered_map<std::string, std::string> units = {
            {"Test_RPM", "rpm"},
        };
        
        auto timestamp = now + std::chrono::milliseconds(i * 10); // 10ms intervals
        sql_buffer->add_frame(timestamp, frame, "Status", signals, units);
    }
    
    std::cout << "Total messages: " << sql_buffer->get_total_messages() << std::endl;
    std::cout << "Buffer size for 0x123: " << sql_buffer->get_buffer_size(0x123) << std::endl;
    
    // Get messages from last 5 seconds
    auto recent_messages = sql_buffer->get_messages_in_range(
        0x123,
        now + std::chrono::milliseconds(9500),
        now + std::chrono::milliseconds(10000)
    );
    
    std::cout << "Recent messages count: " << recent_messages.size() << std::endl;
    
    // Filter for high RPM messages using C++20 ranges
    auto high_rpm_messages = sql_buffer->filter_messages(0x123, 
        utils::signal_value_filter("Test_RPM", 5000.0, 8000.0));
    
    std::cout << "High RPM messages: " << high_rpm_messages.size() << std::endl;
    
    // Async flush
    auto flush_future = sql_buffer->flush_async();
    std::cout << "Flushing data asynchronously..." << std::endl;
    flush_future.wait();
    std::cout << "Flush complete!" << std::endl;
    
    // Print metadata
    const auto& metadata = sql_buffer->get_metadata();
    std::cout << "Stream: " << metadata.stream_name << std::endl;
    std::cout << "Description: " << metadata.description << std::endl;
    std::cout << "Duration: " 
              << std::chrono::duration_cast<std::chrono::seconds>(metadata.get_duration()).count()
              << " seconds" << std::endl;
    std::cout << "Message rate for 0x123: " 
              << metadata.get_message_rate(0x123) << " Hz" << std::endl;
}

// Example comparing SQL vs CSV backends
void backend_comparison_example() {
    auto now = std::chrono::system_clock::now();
    
    // Create same data in both backends
    auto sql_buffer = CANDataBuffer::create_with_sql("Test_SQL", "./test_sql.db");
    auto csv_buffer = CANDataBuffer::create_with_csv("Test_CSV", "./test_csv");
    
    // Add identical data to both
    for (int i = 0; i < 100; ++i) {
        CANMessage message;
        message.timestamp = now + std::chrono::milliseconds(i);
        message.frame.can_id = 0x456;
        message.frame.len = 4;
        message.frame.data[0] = i & 0xFF;
        message.frame.data[1] = (i >> 8) & 0xFF;
        message.message_name = "Test_Message";
        message.decoded_signals["Counter"] = static_cast<double>(i);
        message.signal_units["Counter"] = "count";
        
        sql_buffer->add_message(message);
        csv_buffer->add_message(message);
    }
    
    // Flush both
    sql_buffer->flush_all();
    csv_buffer->flush_all();
    
    std::cout << "Data written to both SQL and CSV backends" << std::endl;
    std::cout << "SQL metadata: " << sql_buffer->get_metadata().stream_name << std::endl;
    std::cout << "CSV metadata: " << csv_buffer->get_metadata().stream_name << std::endl;
}

// Example demonstrating advanced querying
void advanced_query_example() {
    auto buffer = CANDataBuffer::create_with_sql("Advanced_Test", "./advanced.db");
    auto now = std::chrono::system_clock::now();
    
    // Add mixed CAN messages
    std::vector<canid_t> message_ids = {0x100, 0x200, 0x300};
    
    for (int i = 0; i < 300; ++i) {
        for (auto can_id : message_ids) {
            CANMessage message;
            message.timestamp = now + std::chrono::milliseconds(i * 5);
            message.frame.can_id = can_id;
            message.frame.len = 8;
            
            // Different signal patterns for each message ID
            switch (can_id) {
                case 0x100: // Speed data
                    message.message_name = "Vehicle_Speed";
                    message.decoded_signals["Speed"] = 60.0 + (i % 80); // 60-140 km/h
                    message.signal_units["Speed"] = "km/h";
                    break;
                case 0x200: // Engine data
                    message.message_name = "Engine_Data";
                    message.decoded_signals["RPM"] = 1000.0 + (i * 20) % 5000;
                    message.decoded_signals["Torque"] = 100.0 + (i % 200);
                    message.signal_units["RPM"] = "rpm";
                    message.signal_units["Torque"] = "Nm";
                    break;
                case 0x300: // Temperature data
                    message.message_name = "Thermal_Data";
                    message.decoded_signals["Intake_Temp"] = 25.0 + (i % 30);
                    message.signal_units["Intake_Temp"] = "°C";
                    break;
            }
            
            buffer->add_message(std::move(message));
        }
    }

    // 1. High speed messages
    auto high_speed = buffer->filter_messages(0x100, [](const CANMessage& msg) {
        auto it = msg.decoded_signals.find("Speed");
        return it != msg.decoded_signals.end() && it->second > 120.0;
    });
    
    std::cout << "High speed messages (>120 km/h): " << high_speed.size() << std::endl;
    
    // 2. Recent engine data with high RPM
    auto recent_start = now + std::chrono::milliseconds(1000);
    auto recent_end = now + std::chrono::milliseconds(1500);
    
    auto recent_engine = buffer->get_messages_in_range(0x200, recent_start, recent_end);
    auto filtered_view = recent_engine 
        | std::views::filter([](const CANMessage& msg) {
            auto it = msg.decoded_signals.find("RPM");
            return it != msg.decoded_signals.end() && it->second > 4000.0;
        });
    std::vector<CANMessage> high_rpm_recent(filtered_view.begin(), filtered_view.end());
    
    std::cout << "Recent high RPM messages: " << high_rpm_recent.size() << std::endl;
    
    // Print final statistics
    const auto& metadata = buffer->get_metadata();
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "Total messages: " << metadata.total_messages << std::endl;
    
    for (auto can_id : buffer->get_all_message_ids()) {
        std::cout << "CAN ID 0x" << std::hex << can_id 
                  << ": " << std::dec << buffer->get_buffer_size(can_id) 
                  << " messages, " << metadata.get_message_rate(can_id) << " Hz" << std::endl;
    }
}

// Performance test example
void performance_test() {
    constexpr size_t NUM_MESSAGES = 10000;
    constexpr size_t NUM_CAN_IDS = 10;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto buffer = CANDataBuffer::create_with_sql("Performance_Test", "./perf_test.db");
    buffer->set_auto_flush(false); // Disable auto-flush for performance test
    
    auto data_start = std::chrono::system_clock::now();
    
    // Add messages
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
        canid_t can_id = 0x100 + (i % NUM_CAN_IDS);
        
        CANFrame frame;
        frame.can_id = can_id;
        frame.len = 8;
        std::fill_n(frame.data, 8, static_cast<uint8_t>(i & 0xFF));
        
        auto timestamp = data_start + std::chrono::microseconds(i * 100); // 100μs intervals
        
        std::unordered_map<std::string, double> signals = {
            {"Signal_A", static_cast<double>(i % 1000)},
            {"Signal_B", static_cast<double>((i * 2) % 1000)}
        };
        
        std::unordered_map<std::string, std::string> units = {
            {"Signal_A", "V"},
            {"Signal_B", "A"}
        };
        
        buffer->add_frame(timestamp, frame, "Test_Message_" + std::to_string(can_id), signals, units);
    }
    
    auto add_time = std::chrono::high_resolution_clock::now();
    
    // Flush all data
    buffer->flush_all();
    
    auto flush_time = std::chrono::high_resolution_clock::now();
    
    // Performance measurements
    auto add_duration = std::chrono::duration_cast<std::chrono::milliseconds>(add_time - start_time);
    auto flush_duration = std::chrono::duration_cast<std::chrono::milliseconds>(flush_time - add_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(flush_time - start_time);
    
    std::cout << "Performance Test Results:" << std::endl;
    std::cout << "Messages added: " << NUM_MESSAGES << std::endl;
    std::cout << "Add time: " << add_duration.count() << "ms" << std::endl;
    std::cout << "Flush time: " << flush_duration.count() << "ms" << std::endl;
    std::cout << "Total time: " << total_duration.count() << "ms" << std::endl;
    std::cout << "Messages/second: " << (NUM_MESSAGES * 1000.0 / total_duration.count()) << std::endl;
    
    // Test querying performance
    auto query_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_CAN_IDS; ++i) {
        canid_t can_id = 0x100 + i;
        auto messages = buffer->get_messages(can_id);
        auto filtered = buffer->filter_messages(can_id, [](const CANMessage& msg) {
            auto it = msg.decoded_signals.find("Signal_A");
            return it != msg.decoded_signals.end() && it->second > 500.0;
        });
    }
    
    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(query_end - query_start);
    
    std::cout << "Query time for " << NUM_CAN_IDS << " CAN IDs: " << query_duration.count() << "ms" << std::endl;
}

// Multi-threaded example
void multithreaded_example() {
    auto buffer = CANDataBuffer::create_with_csv("MT_Test", "./mt_test");
    buffer->set_auto_flush(false); // Disable auto-flush for multi-threaded test
    
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t MESSAGES_PER_THREAD = 1000;
    
    std::vector<std::thread> producers;
    auto start_time = std::chrono::system_clock::now();
    std::mutex buffer_mutex; // Add mutex for thread safety
    
    // Create producer threads
    for (size_t thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        producers.emplace_back([&buffer, &buffer_mutex, thread_id, start_time]() {
            for (size_t i = 0; i < MESSAGES_PER_THREAD; ++i) {
                CANFrame frame;
                frame.can_id = 0x500 + thread_id; // Different CAN ID per thread
                frame.len = 8;
                
                // Thread-specific data pattern
                uint32_t value = (thread_id << 16) | (i & 0xFFFF);
                std::memcpy(frame.data, &value, sizeof(value));
                
                auto timestamp = start_time + std::chrono::microseconds(
                    (thread_id * MESSAGES_PER_THREAD + i) * 50);
                
                std::unordered_map<std::string, double> signals = {
                    {"Thread_ID", static_cast<double>(thread_id)},
                    {"Message_Index", static_cast<double>(i)},
                    {"Combined_Value", static_cast<double>(value)}
                };
                
                std::unordered_map<std::string, std::string> units = {
                    {"Thread_ID", "id"},
                    {"Message_Index", "count"},
                    {"Combined_Value", "raw"}
                };
                
                // Protect buffer access with mutex
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    buffer->add_frame(timestamp, frame, 
                                    "Thread_Message_" + std::to_string(thread_id), 
                                    signals, units);
                }
                
                // Simulate some processing delay
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Wait for all producers to finish
    for (auto& thread : producers) {
        thread.join();
    }
    
    std::cout << "Multi-threaded test completed" << std::endl;
    std::cout << "Total messages from " << NUM_THREADS << " threads: " 
              << buffer->get_total_messages() << std::endl;
    
    // Verify data integrity
    for (size_t thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        canid_t can_id = 0x500 + thread_id;
        auto messages = buffer->get_messages(can_id);
        std::cout << "Thread " << thread_id << " produced " << messages.size() 
                  << " messages for CAN ID 0x" << std::hex << can_id << std::dec << std::endl;
    }
    
    // Async flush all data
    auto flush_future = buffer->flush_async();
    flush_future.wait();
    std::cout << "All data flushed to CSV backend" << std::endl;
}

// Weel Wowld Exampole
class CANDataLogger {
private:
    std::unique_ptr<CANDataBuffer> buffer;
    std::thread logging_thread;
    std::atomic<bool> should_stop{false};
    std::queue<CANMessage> incoming_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

public:
    CANDataLogger(const std::string& session_name, const std::string& output_path, bool use_sql = true) {
        if (use_sql) {
            buffer = CANDataBuffer::create_with_sql(session_name, output_path + "/session.db");
        } else {
            buffer = CANDataBuffer::create_with_csv(session_name, output_path);
        }
        
        buffer->update_description("Real-time CAN data logging session");
        buffer->set_auto_flush(true, 1000);
        
        // Start background logging thread
        logging_thread = std::thread(&CANDataLogger::logging_loop, this);
    }
    
    ~CANDataLogger() {
        stop();
    }
    
    void log_frame(CANTime timestamp, CANFrame frame, 
                   const std::string& message_name = "",
                   const std::unordered_map<std::string, double>& signals = {}) {
        CANMessage message;
        message.timestamp = timestamp;
        message.frame = frame;
        message.message_name = message_name;
        message.decoded_signals = signals;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            incoming_queue.push(std::move(message));
        }
        queue_cv.notify_one();
    }
    
    void stop() {
        should_stop = true;
        queue_cv.notify_all();
        
        if (logging_thread.joinable()) {
            logging_thread.join();
        }
        
        buffer->flush_all();
    }
    
    // Query methods
    auto get_session_statistics() const {
        const auto& metadata = buffer->get_metadata();
        
        struct SessionStats {
            std::string name;
            size_t total_messages;
            std::chrono::milliseconds duration;
            std::vector<canid_t> active_can_ids;
            double overall_message_rate;
        };
        
        SessionStats stats;
        stats.name = metadata.stream_name;
        stats.total_messages = metadata.total_messages;
        stats.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            metadata.get_duration());
        stats.active_can_ids = buffer->get_all_message_ids();
        
        if (stats.duration.count() > 0) {
            stats.overall_message_rate = stats.total_messages * 1000.0 / stats.duration.count();
        } else {
            stats.overall_message_rate = 0.0;
        }
        
        return stats;
    }
    
private:
    void logging_loop() {
        while (!should_stop) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !incoming_queue.empty() || should_stop;
            });
            
            while (!incoming_queue.empty()) {
                auto message = std::move(incoming_queue.front());
                incoming_queue.pop();
                lock.unlock();
                
                buffer->add_message(std::move(message));
                
                lock.lock();
            }
        }
    }
};

// Example main function
int main() {
    try {
        std::cout << "=== Basic Usage Example ===" << std::endl;
        example_usage();
        
        std::cout << "\n=== Backend Comparison Example ===" << std::endl;
        backend_comparison_example();
        
        std::cout << "\n=== Advanced Query Example ===" << std::endl;
        advanced_query_example();
        
        std::cout << "\n=== Performance Test ===" << std::endl;
        performance_test();
        
        std::cout << "\n=== Multi-threaded Example ===" << std::endl;
        multithreaded_example();
        
        std::cout << "\n=== Real-world Logger Example ===" << std::endl;
        {
            // Create directory if it doesn't exist
            std::filesystem::create_directories("./vehicle_log");
            CANDataLogger logger("Vehicle_Test_Session", "./vehicle_log", true);
            
            // Simulate logging some CAN frames
            auto now = std::chrono::system_clock::now();
            for (int i = 0; i < 100; ++i) {
                CANFrame frame;
                frame.can_id = 0x7E0;
                frame.len = 8;
                frame.data[0] = 0x02;
                frame.data[1] = 0x01;
                frame.data[2] = 0x0C;
                frame.data[3] = 0x00;
                frame.data[4] = 0x00;
                frame.data[5] = 0x00;
                frame.data[6] = 0x00;
                frame.data[7] = 0x00;
                
                std::unordered_map<std::string, double> signals = {
                    {"Engine_RPM", 1500.0 + i * 10},
                    {"Vehicle_Speed", 50.0 + i * 0.5}
                };
                
                logger.log_frame(now + std::chrono::milliseconds(i * 10), frame, "OBD_Request", signals);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Let it process for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto stats = logger.get_session_statistics();
            std::cout << "Session '" << stats.name << "' logged " << stats.total_messages 
                      << " messages over " << stats.duration.count() << "ms" << std::endl;
            std::cout << "Message rate: " << stats.overall_message_rate << " Hz" << std::endl;
            std::cout << "Active CAN IDs: " << stats.active_can_ids.size() << std::endl;
        }
        
        std::cout << "\nAll examples completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}