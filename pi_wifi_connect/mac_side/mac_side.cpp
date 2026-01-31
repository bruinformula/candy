#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <unordered_set>

using namespace std;

/* ---------------- Reliable recv ---------------- */

bool recvAll(int sock, void* data, size_t len) {
    char* ptr = static_cast<char*>(data);
    while (len > 0) {
        ssize_t r = recv(sock, ptr, len, 0);
        if (r <= 0) return false;
        ptr += r;
        len -= r;
    }
    return true;
}

/* ---------------- Extract seq from JSON ---------------- */

int extractSeq(const string& json) {
    auto pos = json.find("\"seq\":");
    if (pos == string::npos) return -1;
    return stoi(json.substr(pos + 6));
}

/* ---------------- Main ---------------- */

int main() {
    const char* server_ip = "10.42.0.1";
    int port = 9000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    cout << "[MAC] Connecting to Pi...\n";
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    cout << "[MAC] Connected\n";

    int expected = 0;
    int received = 0;
    unordered_set<int> seen;

    while (true) {
        // 1️⃣ Receive length
        uint32_t len_net;
        if (!recvAll(sock, &len_net, sizeof(len_net))) {
            cout << "[MAC] Connection closed\n";
            break;
        }

        uint32_t len = ntohl(len_net);

        // 2️⃣ Receive JSON payload
        string json(len, '\0');
        if (!recvAll(sock, &json[0], len)) {
            cout << "[MAC] Payload receive failed\n";
            break;
        }

        // 3️⃣ Process message
        int seq = extractSeq(json);
        received++;

        if (seen.count(seq)) {
            cout << "[DUP] seq=" << seq << endl;
        } else {
            seen.insert(seq);
        }

        if (seq != expected) {
            cout << "[LOSS] expected=" << expected
                 << " got=" << seq << endl;
            expected = seq + 1;
        } else {
            expected++;
        }

        cout << "[RECV] seq=" << seq
             << " bytes=" << len << endl;
    }

    cout << "\n=== STATS ===\n";
    cout << "Received: " << received << endl;
    cout << "Unique:   " << seen.size() << endl;

    close(sock);
    return 0;
}
