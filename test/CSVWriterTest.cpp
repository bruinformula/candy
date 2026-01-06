#include "Candy/Core/CSVWriter.hpp"
#include <Candy/Candy.h>

using namespace Candy;

int main() {

    std::optional<CSVHeader<5>> header_opt = CSVHeader<5>{
        "Test.csv",
        {"Time", "MessageID", "SignalName", "Value", "Unit"}
    };

    if (!header_opt) {
        printf("Failed to create CSV header.\n");
        return 1;
    }

    CSVHeader<5> header = header_opt.value();

    auto writer_opt = CSVWriter<5>::create("./", std::move(header));

    if (!writer_opt) {
        printf("Failed to create CSV writer.\n");
        return 1;
    }

    CSVWriter<5> writer = std::move(writer_opt.value());

    writer.write_header();
    writer.field("123");
    writer.field("Signal1");
    writer.field("45.67");      

    

}