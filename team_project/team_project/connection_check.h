#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#pragma comment(lib, "Ws2_32.lib")

class ConnectionCheck {
    SOCKET socket_ = INVALID_SOCKET;
    bool started_ = false;
    std::uint32_t playerId_ = 0;
    std::uint32_t sessionId_ = 0;
public:
    ConnectionCheck() = default;
    ConnectionCheck(const ConnectionCheck&) = delete;
    ConnectionCheck& operator=(const ConnectionCheck&) = delete;
    ~ConnectionCheck() { closeConnection(); }

    std::uint32_t playerId() const { return playerId_; }
    std::uint32_t sessionId() const { return sessionId_; }

    bool connectTo(const char* ip, unsigned short port) {
        closeConnection();
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
        return joinSession();
    }
private:
    using Clock = std::chrono::steady_clock;

    static std::uint16_t readNetworkU16(const unsigned char* bytes) {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
    }
    static std::uint32_t readNetworkU32(const unsigned char* bytes) {
        return (static_cast<std::uint32_t>(readNetworkU16(bytes)) << 16) | readNetworkU16(bytes + 2);
    }

    bool transferHandshakeBytes(unsigned char* bytes, int size, bool sending, Clock::time_point deadline) {
        const char* step = sending ? "join request send" : "join approval receive";
        int offset = 0;
        while (offset < size) {
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
                deadline - Clock::now()).count();
            if (remaining <= 0) return failed("join timeout", WSAETIMEDOUT);
            fd_set ready;
            FD_ZERO(&ready); FD_SET(socket_, &ready);
            timeval timeout{static_cast<long>(remaining / 1000000),
                static_cast<long>(remaining % 1000000)};
            const int result = select(0, sending ? nullptr : &ready,
                sending ? &ready : nullptr, nullptr, &timeout);
            if (result == 0) return failed("join timeout", WSAETIMEDOUT);
            if (result == SOCKET_ERROR) return failed("join select", WSAGetLastError());
            char* current = reinterpret_cast<char*>(bytes + offset);
            const int transferred = sending ? send(socket_, current, size - offset, 0)
                : recv(socket_, current, size - offset, 0);
            if (transferred == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) continue;
                return failed(step, error);
            }
            if (transferred == 0) return failed(step, WSAECONNRESET);
            offset += transferred;
        }
        return true;
    }

    bool joinSession() {
       std::array<unsigned char, 12> request{{
            0, 1, 0, 1,
            0, 0, 0, 0,
            0, 0, 0, 1
        }};
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        if (!transferHandshakeBytes(request.data(), static_cast<int>(request.size()), true, deadline))
            return false;
        std::array<unsigned char, 20> approval{};
        if (!transferHandshakeBytes(approval.data(), 12, false, deadline)) return false;
        if (readNetworkU16(approval.data()) != 1 || readNetworkU16(approval.data() + 2) != 2 ||
            readNetworkU32(approval.data() + 4) != 8 || readNetworkU32(approval.data() + 8) != 1)
            return failed("join approval header", WSAEPROTONOSUPPORT);
        if (!transferHandshakeBytes(approval.data() + 12, 8, false, deadline)) return false;
        const std::uint32_t playerId = readNetworkU32(approval.data() + 12);
        const std::uint32_t sessionId = readNetworkU32(approval.data() + 16);
        if (playerId != 2 || sessionId == 0) return failed("join approval IDs", WSAEINVAL);
        playerId_ = playerId;
        sessionId_ = sessionId;
        std::cout << "[Client] Session approved. Player ID: " << playerId_
            << " | Session ID: " << sessionId_ << std::endl;
        return true;
    }

    void closeConnection() {
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
        socket_ = INVALID_SOCKET;
        if (started_) WSACleanup();
        started_ = false;
        playerId_ = 0;
        sessionId_ = 0;
    }

    bool failed(const char* step, int error) {
        std::cerr << "[Client] " << step << " failed: " << error
            << ". Continuing offline." << std::endl;
        closeConnection();
        return false;
    }
};
