#include <fstream>
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h> // Necessary for tcp_info

using namespace std;

// Function to extract TCP statistics from the kernel
uint32_t getTotalRetransmits(int sock) {
    struct tcp_info info;
    socklen_t len = sizeof(info);
    if (getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len) == 0) {
        return info.tcpi_total_retrans;
    }
    return 0;
}

bool sendAll(int sock, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    while (len > 0) {
        ssize_t sent = send(sock, ptr, len, 0);
        if (sent <= 0) return false;
        ptr += sent;
        len -= sent;
    }
    return true;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Set socket option to reuse address (helps if you restart the app quickly)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 1);

    cout << "[PI] Waiting for Mac...\n";
    int client = accept(server_fd, nullptr, nullptr);
    if (client < 0) return 1;
    cout << "[PI] Connected\n";

    uint32_t last_retrans = 0;

    for (int seq = 0; seq < 1000; seq++) {
        string filename = "../json/" + to_string(seq) + ".json";
        ifstream file(filename);
        if (!file) break;

        string json((istreambuf_iterator<char>(file)),
                     istreambuf_iterator<char>());

        uint32_t len = htonl(json.size());

        // Send Length Header
        if (!sendAll(client, &len, sizeof(len))) break;
        // Send JSON Body
        if (!sendAll(client, json.data(), json.size())) break;

        // Check for retransmits after the send
        uint32_t current_retrans = getTotalRetransmits(client);

        cout << "[PI] Sent seq " << seq;
        if (current_retrans > last_retrans) {
            cout << " [!] Retransmission detected! (Total: " << current_retrans << ")";
            last_retrans = current_retrans;
        }
        cout << endl;

        usleep(1000);
    }

    close(client);
    close(server_fd);
    return 0;
}
