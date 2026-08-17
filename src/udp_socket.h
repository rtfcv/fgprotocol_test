#ifndef JSBSIM_TESTER_UDP_SOCKET_H
#define JSBSIM_TESTER_UDP_SOCKET_H

#include <cstddef>
#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h> // fd_set/FD_SET/select() for waitReadable()
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

    enum class Readable {
        Ready,   // select() says a recv() right now won't block
        Timeout, // no datagram within timeoutMs -- the expected common case
                 // for a poll loop, not an error
        Error,   // select() itself failed
    };

    // Blocks up to timeoutMs waiting for this socket to become readable, via
    // select() on a single-socket fd_set. This is the wait; recv() below
    // still runs non-blocking, so a spurious wakeup can't stall the caller's
    // loop (main.cpp shares one loop between this and a periodic sender).
    Readable waitReadable(int timeoutMs);

    // Non-blocking receive of exactly one datagram into `buf` (up to `len`
    // bytes). Returns the number of bytes written, or a value <= 0 if
    // nothing usable was read -- either no datagram was queued
    // (EWOULDBLOCK/WSAEWOULDBLOCK, the expected common case right after a
    // Timeout) or recv() itself failed. Callers that want to detect an
    // oversize datagram (recv() truncates it silently on POSIX, and returns
    // an error on Windows) should pass a `len` one byte larger than the
    // expected payload and check the returned count.
    int recvInto(void* buf, std::size_t len);

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
