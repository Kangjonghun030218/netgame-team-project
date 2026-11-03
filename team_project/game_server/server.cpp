#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <vector>
#include <conio.h>

namespace {
constexpr unsigned short kServerPort = 9000;

// Destroy the listening socket before cleaning up Winsock on every exit path.
struct ServerResources {
    bool winsockStarted = false;
    SOCKET listenSocket = INVALID_SOCKET;
    std::vector<SOCKET> clients;

    ~ServerResources() {
        for (SOCKET client : clients) closesocket(client);
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
        }
        if (winsockStarted) {
            WSACleanup();
        }
    }
};
}

int main() {
    ServerResources server;
    WSADATA wsaData{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupResult != 0) {
        std::cerr << "WSAStartup failed: " << startupResult << '\n';
        return 1;
    }
    server.winsockStarted = true;
    if (wsaData.wVersion != MAKEWORD(2, 2)) {
        std::cerr << "Winsock 2.2 is required.\n";
        return 1;
    }

    server.listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server.listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << '\n';
        return 1;
    }

    // Prevent another process from sharing this server's address and port.
    const BOOL exclusiveAddress = TRUE;
    if (setsockopt(server.listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusiveAddress), sizeof(exclusiveAddress)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << '\n';
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(kServerPort);

    if (bind(server.listenSocket, reinterpret_cast<const sockaddr*>(&serverAddress),
        sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << '\n';
        return 1;
    }

    // Queue incoming requests, then accept them in the loop below.
    if (listen(server.listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << '\n';
        return 1;
    }

    std::cout << "TCP server listening on 0.0.0.0:" << kServerPort << '\n'
        << "Press Enter to stop the server." << std::endl;
    while (true) {
        if (_kbhit() && _getch() == '\r') break;
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(server.listenSocket, &readable);
        for (SOCKET client : server.clients) FD_SET(client, &readable);
        timeval timeout{0, 100000};
        if (select(0, &readable, nullptr, nullptr, &timeout) == SOCKET_ERROR) {
            std::cerr << "select failed: " << WSAGetLastError() << std::endl;
            return 1;
        }
        for (size_t i = 0; i < server.clients.size();) {
            SOCKET client = server.clients[i];
            if (FD_ISSET(client, &readable)) {
                char buffer[256];
                const int received = recv(client, buffer, sizeof(buffer), 0);
                if (received <= 0) {
                    closesocket(client);
                    server.clients.erase(server.clients.begin() + i);
                    std::cout << "[Server] Client disconnected. Connected: "
                        << server.clients.size() << std::endl;
                    continue;
                }
            }
            ++i;
        }
        if (FD_ISSET(server.listenSocket, &readable)) {
            sockaddr_in peer{};
            int size = sizeof(peer);
            SOCKET client = accept(server.listenSocket, reinterpret_cast<sockaddr*>(&peer), &size);
            if (client == INVALID_SOCKET) {
                std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
                continue;
            }
            if (server.clients.size() >= FD_SETSIZE - 1) {
                closesocket(client);
                std::cerr << "[Server] Client limit reached." << std::endl;
                continue;
            }
            server.clients.push_back(client);
            char ip[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
            std::cout << "[Server] Client connected: " << ip << ':' << ntohs(peer.sin_port)
                << " | Connected: " << server.clients.size() << std::endl;
        }
    }
    return 0;
}
