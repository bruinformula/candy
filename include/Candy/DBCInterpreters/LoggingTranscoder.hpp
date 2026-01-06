#pragma once

#include <string>
#include "Candy/DBCInterpreters/DBC/DBCInterpreter.hpp"

namespace Candy {

    class LoggingTranscoder : public DBCInterpreter<LoggingTranscoder> {
    public:
        // Log signal definitions
        void sg(const std::string& messageName, const std::string& signalName, const std::string& details) {
            printf("[Signal] Message: %s, Signal: %s, Details: %s\n", messageName.c_str(), signalName.c_str(), details.c_str());
        }

        // Log message definitions
        void bo(const std::string& messageId, const std::string& messageName, const std::string& details) {
            printf("[Message] ID: %s, Name: %s, Details: %s\n", messageId.c_str(), messageName.c_str(), details.c_str());
        }

        // Log environment variable definitions
        void ev(const std::string& envVarName, const std::string& details) {
            printf("[EnvVar] Name: %s, Details: %s\n", envVarName.c_str(), details.c_str());
        }

        // Log comments
        void cm(const std::string& commentType, const std::string& targetName, const std::string& comment) {
            printf("[Comment] Type: %s, Target: %s, Comment: %s\n", commentType.c_str(), targetName.c_str(), comment.c_str());
        }

        // Log value tables
        void val(const std::string& signalName, const std::string& valueTable) {
            printf("[ValueTable] Signal: %s, Table: %s\n", signalName.c_str(), valueTable.c_str());
        }

    };
    
}