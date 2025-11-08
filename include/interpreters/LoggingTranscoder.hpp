#pragma once

#include <iostream>
#include <string>
#include "DBC/DBCInterpreter.hpp"

namespace CAN {

    class LoggingTranscoder : public DBCInterpreter<LoggingTranscoder> {
    public:
        // Constructor
        LoggingTranscoder(std::ostream& output = std::cout) : log_stream(output) {}

        // Log signal definitions
        void sg(const std::string& messageName, const std::string& signalName, const std::string& details) override {
            log_stream << "[Signal] Message: " << messageName << ", Signal: " << signalName << ", Details: " << details << std::endl;
        }

        // Log message definitions
        void bo(const std::string& messageId, const std::string& messageName, const std::string& details) override {
            log_stream << "[Message] ID: " << messageId << ", Name: " << messageName << ", Details: " << details << std::endl;
        }

        // Log environment variable definitions
        void ev(const std::string& envVarName, const std::string& details) override {
            log_stream << "[EnvVar] Name: " << envVarName << ", Details: " << details << std::endl;
        }

        // Log comments
        void cm(const std::string& commentType, const std::string& targetName, const std::string& comment) override {
            log_stream << "[Comment] Type: " << commentType << ", Target: " << targetName << ", Comment: " << comment << std::endl;
        }

        // Log value tables
        void val(const std::string& signalName, const std::string& valueTable) override {
            log_stream << "[ValueTable] Signal: " << signalName << ", Table: " << valueTable << std::endl;
        }

    private:
        std::ostream& log_stream; // Stream to log output (default is std::cout)
    };
    
}
#endif // LOGGING_DBC_INTERPRETER_HPP