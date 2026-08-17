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

UdpSocket::Readable UdpSocket::waitReadable(int timeoutMs) {
    if (sock_ == kInvalid) return Readable::Error;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock_, &readfds);

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    // First argument is ignored on Windows; harmless to pass it there too.
    int ready = select(0, &readfds, nullptr, nullptr, &tv);
    if (ready == SOCKET_ERROR) return Readable::Error;
    if (ready == 0) return Readable::Timeout;
    return Readable::Ready;
}

int UdpSocket::recvInto(void* buf, std::size_t len) {
    if (sock_ == kInvalid) return -1;

    int n = recv(sock_, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
    // n == 0: a zero-length datagram (legal but useless here).
    // n < 0: WSAEWOULDBLOCK (no data queued) is the expected common case
    // right after a spurious wakeup; any other error also just means
    // "nothing usable right now" here -- caller treats <= 0 uniformly.
    return n;
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

UdpSocket::Readable UdpSocket::waitReadable(int timeoutMs) {
    if (sock_ == kInvalid) return Readable::Error;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock_, &readfds);

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ready = select(sock_ + 1, &readfds, nullptr, nullptr, &tv);
    if (ready < 0) return Readable::Error;
    if (ready == 0) return Readable::Timeout;
    return Readable::Ready;
}

int UdpSocket::recvInto(void* buf, std::size_t len) {
    if (sock_ == kInvalid) return -1;

    ssize_t n = recv(sock_, buf, len, 0);
    return static_cast<int>(n);
}

#endif
