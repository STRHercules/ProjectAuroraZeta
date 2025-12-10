#include "UdpSocket.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#    include <WinSock2.h>
#    include <Ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include "../core/Logger.h"

namespace Engine::Net {

namespace {
bool setNonBlocking(int fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

int lastErrorCode() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool wouldBlock(int err) {
#ifdef _WIN32
    return err == WSAEWOULDBLOCK;
#else
    return err == EWOULDBLOCK || err == EAGAIN;
#endif
}
}  // namespace

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(uint16_t port) {
    close();
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    wsaInit_ = true;
#endif
    fd_ = static_cast<int>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (fd_ < 0) {
        Engine::logError("UDP socket creation failed");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        Engine::logError("UDP bind failed");
        close();
        return false;
    }

    if (!setNonBlocking(fd_)) {
        Engine::logWarn("Failed to set UDP socket non-blocking; continuing anyway.");
    }

#ifndef _WIN32
    int enable = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
    return true;
}

void UdpSocket::close() {
    if (fd_ >= 0) {
#ifdef _WIN32
        closesocket(fd_);
        if (wsaInit_) {
            WSACleanup();
            wsaInit_ = false;
        }
#else
        ::close(fd_);
#endif
    }
    fd_ = -1;
}

bool UdpSocket::isOpen() const { return fd_ >= 0; }

bool UdpSocket::send(const NetAddress& to, const uint8_t* data, std::size_t len) {
    if (fd_ < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(to.port);
    if (inet_pton(AF_INET, to.ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    auto sent = ::sendto(fd_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0,
                         reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<int>(len);
}

int UdpSocket::receive(NetAddress& from, uint8_t* buffer, std::size_t maxLen) {
    if (fd_ < 0) return -1;
    sockaddr_in addr{};
#ifdef _WIN32
    int addrLen = sizeof(addr);
#else
    socklen_t addrLen = sizeof(addr);
#endif
    int received = static_cast<int>(recvfrom(fd_, reinterpret_cast<char*>(buffer), static_cast<int>(maxLen), 0,
                                             reinterpret_cast<sockaddr*>(&addr), &addrLen));
    if (received < 0) {
        int err = lastErrorCode();
        if (wouldBlock(err)) return 0;
        return -1;
    }
    char ipBuf[64];
    inet_ntop(AF_INET, &addr.sin_addr, ipBuf, sizeof(ipBuf));
    from.ip = std::string(ipBuf);
    from.port = ntohs(addr.sin_port);
    return received;
}

}  // namespace Engine::Net
