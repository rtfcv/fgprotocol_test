#include "udp_socket.h"

#include <cstring>

#if defined(_WIN32)

WinsockGuard::WinsockGuard() {
    WSADATA wsaData;
    ok_ = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
}

WinsockGuard::~WinsockGuard() {
    if (ok_) {
        WSACleanup();
    }
}

UdpSocket::UdpSocket() : sock_(kInvalid) {}

UdpSocket::~UdpSocket() {
    if (sock_ != kInvalid) {
        closesocket(sock_);
    }
}

bool UdpSocket::open() {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sock_ != kInvalid;
}

void UdpSocket::setNonBlocking() {
    u_long mode = 1;
    ioctlsocket(sock_, FIONBIO, &mode);
}

bool UdpSocket::bindLocal(int localPort) {
    if (sock_ == kInvalid) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(localPort));

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }
    setNonBlocking();
    return true;
}

bool UdpSocket::setDestination(const std::string& host, int port) {
    if (sock_ == kInvalid) return false;

    std::memset(&dest_, 0, sizeof(dest_));
    dest_.sin_family = AF_INET;
    dest_.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &dest_.sin_addr) != 1) {
        return false;
    }
    haveDest_ = true;
    setNonBlocking();
    return true;
}

bool UdpSocket::send(const std::string& data) {
    if (sock_ == kInvalid || !haveDest_) return false;

    int sent = sendto(sock_, data.data(), static_cast<int>(data.size()), 0,
                       reinterpret_cast<sockaddr*>(&dest_), sizeof(dest_));
    return sent == static_cast<int>(data.size());
}

bool UdpSocket::recvDatagram(std::vector<uint8_t>& buf) {
    if (sock_ == kInvalid) return false;

    static thread_local uint8_t scratch[2048];
    int n = recvfrom(sock_, reinterpret_cast<char*>(scratch), static_cast<int>(sizeof(scratch)), 0, nullptr, nullptr);
    if (n <= 0) {
        // n == 0: a zero-length datagram (legal but useless here).
        // n < 0: WSAEWOULDBLOCK (no data queued) is the expected common
        // case for a non-blocking poll loop; any other error also just
        // means "nothing usable right now" here.
        return false;
    }
    buf.assign(scratch, scratch + n);
    return true;
}

#else // POSIX

WinsockGuard::WinsockGuard() : ok_(true) {}
WinsockGuard::~WinsockGuard() {}

UdpSocket::UdpSocket() : sock_(kInvalid) {}

UdpSocket::~UdpSocket() {
    if (sock_ != kInvalid) {
        close(sock_);
    }
}

bool UdpSocket::open() {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sock_ >= 0;
}

void UdpSocket::setNonBlocking() {
    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    }
}

bool UdpSocket::bindLocal(int localPort) {
    if (sock_ == kInvalid) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(localPort));

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return false;
    }
    setNonBlocking();
    return true;
}

bool UdpSocket::setDestination(const std::string& host, int port) {
    if (sock_ == kInvalid) return false;

    std::memset(&dest_, 0, sizeof(dest_));
    dest_.sin_family = AF_INET;
    dest_.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &dest_.sin_addr) != 1) {
        return false;
    }
    haveDest_ = true;
    setNonBlocking();
    return true;
}

bool UdpSocket::send(const std::string& data) {
    if (sock_ == kInvalid || !haveDest_) return false;

    ssize_t sent = sendto(sock_, data.data(), data.size(), 0,
                           reinterpret_cast<sockaddr*>(&dest_), sizeof(dest_));
    return sent == static_cast<ssize_t>(data.size());
}

bool UdpSocket::recvDatagram(std::vector<uint8_t>& buf) {
    if (sock_ == kInvalid) return false;

    static thread_local uint8_t scratch[2048];
    ssize_t n = recvfrom(sock_, scratch, sizeof(scratch), 0, nullptr, nullptr);
    if (n <= 0) {
        return false;
    }
    buf.assign(scratch, scratch + n);
    return true;
}

#endif
