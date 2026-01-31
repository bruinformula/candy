#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>

using namespace std;
namespace fs = std::filesystem;

int main() {
    const int NUM_FILES = 1000;
    const string OUT_DIR = "json";

    // Create directory if it doesn't exist
    fs::create_directory(OUT_DIR);

    time_t now = time(nullptr);

    for (int i = 0; i < NUM_FILES; i++) {
        string filename = OUT_DIR + "/" + to_string(i) + ".json";
        ofstream file(filename);

        if (!file) {
            cerr << "Failed to create " << filename << endl;
            return 1;
        }

        file << "{\n";
        file << "  \"seq\": " << i << ",\n";
        file << "  \"timestamp\": " << now << ",\n";
        file << "  \"payload\": \"test message " << i << "\"\n";
        file << "}\n";

        file.close();
    }
    const string OUT_DIR = ".../json"; //Modify the path once you have the code onloaded onto the pi
    cout << "Generated " << NUM_FILES << " JSON files in ./" << OUT_DIR << endl;
    return 0;
}
