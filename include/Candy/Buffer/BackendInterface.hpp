#pragma once 

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <concepts>
#include <ranges>
#include <functional>
#include <future>

#include "sqlite3.h"

#include "Candy/IO/CANIOHelperTypes.hpp"
#include "Candy/Interpreters/SQLTranscoder.hpp"
#include "Candy/Interpreters/CSVTranscoder.hpp"

namespace Candy {

// Abstract backend interface
class BackendInterface {
public:
    virtual ~BackendInterface() = default;
    
    virtual void write_message(const CANMessage& message) = 0;
    virtual void write_metadata(const CANDataStreamMetadata& metadata) = 0;
    virtual void flush() = 0;
    virtual void flush_sync() = 0;
    virtual std::future<void> flush_async() = 0;
    
    virtual std::vector<CANMessage> read_messages(canid_t can_id) = 0;
    virtual std::vector<CANMessage> read_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) = 0;
    virtual CANDataStreamMetadata read_metadata() = 0;
};

// SQL Backend implementation
class SQLBackend : public BackendInterface {
private:
    std::unique_ptr<class SQLTranscoder> transcoder;
    std::string db_path;

    void create_metadata_table();
    void load_decoded_signals_for_messages(sqlite3* db, std::vector<CANMessage>& messages);
    void parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len);
    void parse_message_names_json(const std::string& json_str, 
                                 std::unordered_map<canid_t, std::string>& names);
    void parse_message_counts_json(const std::string& json_str,
                                  std::unordered_map<canid_t, size_t>& counts);

public:
    explicit SQLBackend(const std::string& database_path, size_t batch_size = 1000);
    ~SQLBackend() override = default;

    void write_message(const CANMessage& message) override;
    void write_metadata(const CANDataStreamMetadata& metadata) override;
    void flush() override;
    void flush_sync() override;
    std::future<void> flush_async() override;
    
    std::vector<CANMessage> read_messages(canid_t can_id) override;
    std::vector<CANMessage> read_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) override;
    CANDataStreamMetadata read_metadata() override;
};

// CSV Backend implementation  
class CSVBackend : public BackendInterface {
private:
    std::unique_ptr<class CSVTranscoder> transcoder;
    std::string base_path;

    void ensure_metadata_file();
    std::vector<std::string> parse_csv_line(const std::string& line);
    void parse_hex_data(const std::string& hex_str, uint8_t* data, size_t len);
    void parse_serialized_data(const std::string& data_str,
                              std::unordered_map<canid_t, std::string>& names);
    void parse_serialized_counts(const std::string& counts_str,
                                std::unordered_map<canid_t, size_t>& counts);

public:
    explicit CSVBackend(const std::string& csv_base_path, size_t batch_size = 1000);
    ~CSVBackend() override = default;

    void write_message(const CANMessage& message) override;
    void write_metadata(const CANDataStreamMetadata& metadata) override;
    void flush() override;
    void flush_sync() override;
    std::future<void> flush_async() override;
    
    std::vector<CANMessage> read_messages(canid_t can_id) override;
    std::vector<CANMessage> read_messages_in_range(
        canid_t can_id, CANTime start, CANTime end) override;
    CANDataStreamMetadata read_metadata() override;
};
}