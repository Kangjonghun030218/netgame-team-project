#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#pragma comment(lib, "Ws2_32.lib")

// Temporary connection check. Keep the socket alive while the game runs.
class ConnectionCheck {
    SOCKET socket_ = INVALID_SOCKET;
    bool started_ = false;
public:
    ConnectionCheck() = default;
    ConnectionCheck(const ConnectionCheck&) = delete;
    ConnectionCheck& operator=(const ConnectionCheck&) = delete;
    ~ConnectionCheck() {
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
        if (started_) WSACleanup();
    }

    bool connectTo(const char* ip, unsigned short port) {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            std::cerr << "[Client] WSAStartup failed: " << result << std::endl;
            return false;
        }
        started_ = true;
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) return failed("socket", WSAGetLastError());
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &address.sin_addr) != 1) return failed("IPv4 address", WSAEINVAL);
        u_long nonblocking = 1;
        if (ioctlsocket(socket_, FIONBIO, &nonblocking) == SOCKET_ERROR)
            return failed("ioctlsocket", WSAGetLastError());
        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) return failed("connect", error);
            fd_set writable, errors;
            FD_ZERO(&writable); FD_ZERO(&errors);
            FD_SET(socket_, &writable); FD_SET(socket_, &errors);
            timeval timeout{2, 0};
            const int ready = select(0, nullptr, &writable, &errors, &timeout);
            if (ready == 0) return failed("connect timeout", WSAETIMEDOUT);
            if (ready == SOCKET_ERROR) return failed("select", WSAGetLastError());
            int socketError = 0;
            int length = sizeof(socketError);
            if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &length) == SOCKET_ERROR)
                return failed("getsockopt", WSAGetLastError());
            if (socketError != 0) return failed("connect", socketError);
        }
        std::cout << "[Client] Connected to " << ip << ':' << port << std::endl;
        return true;
    }
private:
    bool failed(const char* step, int error) {
        std::cerr << "[Client] " << step << " failed: " << error
            << ". Continuing offline." << std::endl;
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }
};
