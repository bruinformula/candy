#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

using namespace std;

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    cout << "Waiting for Mac...\n";
    int client = accept(server_fd, nullptr, nullptr);
    cout << "Mac connected\n";

    for (int i = 0; i < 10000; i++) {
        // 1️⃣ SEND message first
        const char* msg = "Hello from Pi Penis69";
        send(client, msg, strlen(msg), 0);
        cout << "Sent: " << msg << endl;

        // 2️⃣ WAIT for reply
        char buffer[8192];
        ssize_t n = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;

        buffer[n] = '\0';
        cout << "Received reply: " << buffer << endl;
    cout << "Counter: " << i << endl;
 }

    close(client);
    close(server_fd);
    return 0;
}



//dope
