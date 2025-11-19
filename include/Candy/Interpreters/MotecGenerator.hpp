#pragma once

#include <string>

#include "Candy/Core/DBC/DBCInterpreter.hpp"
#include "Candy/Core/CANKernelTypes.hpp"
#include "Candy/Core/CANIOHelperTypes.hpp"

namespace Candy {

    struct MotecHeader {
        std::string format;           // MoTeC CSV File
        std::string venue;            // southern-circuit
        std::string vehicle;          // mk11
        std::string driver;           // Tyler Limsnukan
        std::string device;           // ADL
        std::string comment;          // (optional)
        std::string log_date;         // 3-Sep-25
        std::string log_time;         // 11:54:02
        int sample_rate;              // 333 (Hz)
        double duration;              // 201.159 (s)
        std::string range;            // entire outing
        std::vector<double> beacon_markers; // 05.679 69.708 133.830 198.039
        // Workbook/Worksheet fields
        std::string workbook;
        std::string worksheet;
        std::string vehicle_desc;
        std::string engine_id;
        std::string session;
        std::string practice;
        double origin_time;           // 0 (s)
        double start_time;            // 0 (s)
        double end_time;              // 201.159 (s)
        double start_distance;        // 0 (ft)
        double end_distance;          // 11635 (ft)
    };


    class MotecGenerator : public DBCInterpreter<MotecGenerator> {
    public:
        MotecGenerator(const std::string& csv_path);
        ~MotecGenerator() = default;

        bool parse_csv();

        const std::tuple<CANTime, CANFrame>* get_frame() const;

    private:
        std::string path;
        std::vector<std::tuple<CANTime, CANFrame>> frames;

        int current;

        static void parse_header(MotecHeader& header, const std::string& data);
        static void parse_messages(std::vector<MessageDefinition>& messages, const std::string& data);
        
        void parse_data_row(const std::string& row_data, 
                           const std::vector<std::string>& column_names, 
                           const std::vector<std::string>& column_units);
    };

}