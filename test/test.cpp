
#include "Candy/Core/CANVisitor.hpp"
#include "Candy/Interpreters/LoggingTranscoder.hpp"

#include "Candy/Core/CANDataBufferVisitor.hpp"
#include <Candy/Candy.h>

using namespace Candy;

int main() {

    SQLTranscoder sql_transcoder("./test.db");

    CSVTranscoder csv_transcoder("./test_csv_output");  // batch size of 1000
    LoggingTranscoder logger;

    //CANVisitor visitor(sql_transcoder, csv_transcoder, logger);

    //visitor.parse_dbc(Candy::read_file("test/network.dbc"));

    return 0;
}