#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <conio.h>

namespace {
constexpr unsigned short kServerPort = 50000;
constexpr std::size_t kMaxClients = 1; 

constexpr std::uint32_t kHostPlayerId = 1;
constexpr std::uint32_t kParticipantPlayerId = 2;
constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint16_t kJoinRequestType = 1;
constexpr std::uint16_t kJoinAcceptType = 2;
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kJoinApprovalBytes = 20;
constexpr std::chrono::seconds kJoinRequestTimeout{5};

struct ClientSession {
    SOCKET socket;
    std::uint32_t playerId;
    std::uint32_t sessionId;
    std::array<unsigned char, kHeaderBytes> joinHeader{};
    std::size_t headerBytesReceived = 0;
    bool joinValidated = false;
    std::size_t approvalBytesSent = 0;
    std::chrono::steady_clock::time_point connectedAt = std::chrono::steady_clock::now();
};

struct ServerResources {
    bool winsockStarted = false;
    SOCKET listenSocket = INVALID_SOCKET;
    std::vector<ClientSession> clients;
    std::uint32_t nextSessionId = 1;

    ~ServerResources() {
        for (const ClientSession& client : clients) closesocket(client.socket);
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
        }
        if (winsockStarted) {
            WSACleanup();
        }
    }
};

std::uint16_t readNetworkU16(const unsigned char* bytes) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
}

std::uint32_t readNetworkU32(const unsigned char* bytes) {
    return (static_cast<std::uint32_t>(readNetworkU16(bytes)) << 16) | readNetworkU16(bytes + 2);
}

void writeNetworkU16(unsigned char* bytes, std::uint16_t value) {
    bytes[0] = static_cast<unsigned char>(value >> 8);
    bytes[1] = static_cast<unsigned char>(value);
}

void writeNetworkU32(unsigned char* bytes, std::uint32_t value) {
    writeNetworkU16(bytes, static_cast<std::uint16_t>(value >> 16));
    writeNetworkU16(bytes + 2, static_cast<std::uint16_t>(value));
}

bool sendJoinApproval(ClientSession& client) {
    if (client.approvalBytesSent == kJoinApprovalBytes) return true;
    std::array<unsigned char, kJoinApprovalBytes> approval{};
    writeNetworkU16(approval.data(), kProtocolVersion);
    writeNetworkU16(approval.data() + 2, kJoinAcceptType);
    writeNetworkU32(approval.data() + 4, 8);
    writeNetworkU32(approval.data() + 8, 1);
    writeNetworkU32(approval.data() + 12, client.playerId);
    writeNetworkU32(approval.data() + 16, client.sessionId);
    const int sent = send(client.socket,
        reinterpret_cast<const char*>(approval.data() + client.approvalBytesSent),
        static_cast<int>(kJoinApprovalBytes - client.approvalBytesSent), 0);
    if (sent == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) return true;
        std::cerr << "[Server] Join approval send failed: " << error << std::endl;
        return false;
    }
    if (sent == 0) return false;
    client.approvalBytesSent += static_cast<std::size_t>(sent);
    if (client.approvalBytesSent == kJoinApprovalBytes) {
        std::cout << "[Server] Client approved. Player ID: " << client.playerId
            << " | Session ID: " << client.sessionId << std::endl;
    }
    return true;
}

bool consumeJoinRequest(ClientSession& client, const char* bytes, std::size_t count) {
    if (client.joinValidated || count > kHeaderBytes - client.headerBytesReceived) {
        std::cerr << "[Server] Unexpected additional session data." << std::endl;
        return false;
    }
    std::memcpy(client.joinHeader.data() + client.headerBytesReceived, bytes, count);
    client.headerBytesReceived += count;
    if (client.headerBytesReceived < kHeaderBytes) return true;

    // Header: version(u16), type(u16), bodyBytes(u32), sequence(u32), big endian.
    const unsigned char* header = client.joinHeader.data();
    if (readNetworkU16(header) != kProtocolVersion) {
        std::cerr << "[Server] Join rejected: protocol version mismatch." << std::endl;
        return false;
    }
    if (readNetworkU16(header + 2) != kJoinRequestType) {
        std::cerr << "[Server] Join rejected: SESSION_JOIN_REQUEST must come first." << std::endl;
        return false;
    }
    if (readNetworkU32(header + 4) != 0) {
        std::cerr << "[Server] Join rejected: request body must be empty." << std::endl;
        return false;
    }
    if (readNetworkU32(header + 8) != 1) {
        std::cerr << "[Server] Join rejected: first sequence must be 1." << std::endl;
        return false;
    }
    client.joinValidated = true;
    std::cout << "[Server] Join request validated. Player ID: " << client.playerId
        << " | Session ID: " << client.sessionId << std::endl;
    return true;
}

void acceptClient(ServerResources& server) {
    sockaddr_in peer{};
    int size = sizeof(peer);
    SOCKET client = accept(server.listenSocket, reinterpret_cast<sockaddr*>(&peer), &size);
    if (client == INVALID_SOCKET) {
        std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
        return;
    }
    if (server.clients.size() >= kMaxClients) {
        closesocket(client);
        std::cerr << "[Server] Room full. Only one remote client can join." << std::endl;
        return;
    }
  
    if (server.nextSessionId == 0) {
        closesocket(client);
        std::cerr << "[Server] Session ID limit reached." << std::endl;
        return;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(client, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        std::cerr << "[Server] Nonblocking socket failed: " << WSAGetLastError() << std::endl;
        closesocket(client);
        return;
    }
    const ClientSession newClient{client, kParticipantPlayerId, server.nextSessionId++};
    server.clients.push_back(newClient);
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    std::cout << "[Server] Client connected: " << ip << ':' << ntohs(peer.sin_port)
        << " | Player ID: " << newClient.playerId
        << " | Session ID: " << newClient.sessionId
        << " | Connected: " << server.clients.size() << std::endl;
}
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
        << "Host player ID: " << kHostPlayerId << '\n'
        << "Press Enter to stop the server." << std::endl;
    while (true) {
        if (_kbhit() && _getch() == '\r') break;
        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_ZERO(&writable);
        FD_SET(server.listenSocket, &readable);
        for (const ClientSession& client : server.clients) {
            FD_SET(client.socket, &readable);
            if (client.joinValidated && client.approvalBytesSent < kJoinApprovalBytes)
                FD_SET(client.socket, &writable);
        }
        timeval timeout{0, 100000};
        if (select(0, &readable, &writable, nullptr, &timeout) == SOCKET_ERROR) {
            std::cerr << "select failed: " << WSAGetLastError() << std::endl;
            return 1;
        }
        for (size_t i = 0; i < server.clients.size();) {
            ClientSession& client = server.clients[i];
            bool keepConnection = true;
            if (!client.joinValidated &&
                std::chrono::steady_clock::now() - client.connectedAt >= kJoinRequestTimeout) {
                std::cerr << "[Server] Join request timed out. Player ID: " << client.playerId
                    << " | Session ID: " << client.sessionId << std::endl;
                keepConnection = false;
            }
            if (keepConnection && FD_ISSET(client.socket, &readable)) {
                char buffer[256];
                const int received = recv(client.socket, buffer, sizeof(buffer), 0);
                if (received > 0)
                    keepConnection = consumeJoinRequest(client, buffer, static_cast<std::size_t>(received));
                else if (received == 0)
                    keepConnection = false;
                else if (WSAGetLastError() != WSAEWOULDBLOCK)
                    keepConnection = false;
            }
            if (keepConnection && FD_ISSET(client.socket, &writable))
                keepConnection = sendJoinApproval(client);
            if (!keepConnection) {
                const std::uint32_t playerId = client.playerId;
                const std::uint32_t sessionId = client.sessionId;
                closesocket(client.socket);
                server.clients.erase(server.clients.begin() + i);
                std::cout << "[Server] Client disconnected. Player ID: " << playerId
                    << " | Session ID: " << sessionId << " | Connected: "
                    << server.clients.size() << std::endl;
                continue;
            }
            ++i;
        }
        if (FD_ISSET(server.listenSocket, &readable)) {
            acceptClient(server);
        }
    }
    return 0;
}
