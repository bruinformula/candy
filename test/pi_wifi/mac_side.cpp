#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <cstdio>
#include <memory>

using namespace std;

// Get the SSID of the Wi-Fi interface if it exists (Airport interface)
string getSSID() {
    FILE* pipe = popen("networksetup -getairportnetwork en0 2>/dev/null | awk -F': ' '{print $2}'", "r");
    if (!pipe) return "";
    char buffer[128];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// Get the interface/IP in the Pi subnet
string getPiNetworkIP() {
    struct ifaddrs *ifaddr, *ifa;
    string result = "";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "Unknown";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4 only
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            string ip = inet_ntoa(sa->sin_addr);

            // Filter for Pi subnet
            if (ip.find("10.42.0.") == 0) {
                result = "Interface: " + string(ifa->ifa_name) + " | IP: " + ip;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return result.empty() ? "No Pi network interface found" : result;
}

int main() {
    const char* server_ip = "10.42.0.1"; // Pi IP
    int port = 9000;

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation failed\n";
        return 1;
    }

    // Setup server address
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    cout << "[DEBUG] Connecting to Pi...\n";
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }
    cout << "[DEBUG] Connected!\n";

    string lastSSID = "";

    // Main loop with counter
    for (int i = 1; i <= 10000; i++) {
        // 1️⃣ Detect current SSID
        string ssid = getSSID();
        if (ssid.empty()) ssid = "No Wi-Fi SSID detected";

        // Check for change
        if (!lastSSID.empty() && ssid != lastSSID) {
            cout << "[WARNING] Wi-Fi network changed from \"" << lastSSID << "\" to \"" << ssid << "\"\n";
        }
        lastSSID = ssid;

        // 2️⃣ Print current interface/IP to Pi subnet
        cout << "[INFO] " << getPiNetworkIP() << " | SSID: " << ssid << endl;

        // 3️⃣ WAIT for Pi message
        char buffer[8192];
        ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            cerr << "Pi closed connection\n";
            break;
        }

        buffer[n] = '\0';
        cout << "[DEBUG] Received from Pi: " << buffer << endl;

        // 4️⃣ SEND reply with counter
        string reply = "ACK from Mac #" + to_string(i);
        send(sock, reply.c_str(), reply.size(), 0);
        cout << "[DEBUG] Sent reply: " << reply << endl;
    }

    close(sock);
    return 0;
}
