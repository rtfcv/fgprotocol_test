#ifndef JSBSIM_TESTER_UDP_SOCKET_H
#define JSBSIM_TESTER_UDP_SOCKET_H

#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// Process-wide Winsock lifetime. Construct exactly one of these in main()
// before touching any UdpSocket, and keep it alive for as long as any
// UdpSocket is. On non-Windows this is a no-op.
class WinsockGuard {
public:
    WinsockGuard();
    ~WinsockGuard();

    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;

    bool ok() const { return ok_; }

private:
    bool ok_ = false;
};

// A single non-blocking UDP endpoint. This program opens two: one bound to
// the telemetry port (JSBSim -> us) and one pointed at JSBSim's control
// input port (us -> JSBSim). Each direction uses only the calls it needs,
// but both are supported on one instance since neither JSBSim socket type
// used here requires otherwise.
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Opens the underlying socket. Must succeed before any other call.
    bool open();

    // Binds to INADDR_ANY:localPort and puts the socket in non-blocking
    // mode, so recvDatagram() can pick up JSBSim's telemetry feed.
    bool bindLocal(int localPort);

    // Fixes the destination used by send(). Also puts the socket in
    // non-blocking mode, since main.cpp shares one poll loop for both
    // directions.
    bool setDestination(const std::string& host, int port);

    // Sends one datagram to the destination set by setDestination().
    bool send(const std::string& data);

    // Non-blocking receive. Returns false immediately if no datagram is
    // currently queued (EWOULDBLOCK/WSAEWOULDBLOCK) -- the expected common
    // case for a poll loop, not an error.
    bool recvDatagram(std::vector<uint8_t>& buf);

    bool isOpen() const { return sock_ != kInvalid; }

private:
    void setNonBlocking();

#if defined(_WIN32)
    SOCKET sock_;
    static constexpr SOCKET kInvalid = INVALID_SOCKET;
#else
    int sock_;
    static constexpr int kInvalid = -1;
#endif

    sockaddr_in dest_{};
    bool haveDest_ = false;
};

#endif // JSBSIM_TESTER_UDP_SOCKET_H
