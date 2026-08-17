/**
 * @file udp_socket.h
 * @brief Thin, non-blocking UDP socket wrapper used by this tester.
 *
 * Deliberately not part of the fgprotocol library -- see
 * `include/fgprotocol/net_fdm.h`'s file comment and `README.md`'s
 * "How the pieces fit together" section for why sockets stay tester-side.
 */
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

/**
 * @brief Process-wide Winsock lifetime.
 *
 * Construct exactly one of these in `main()` before touching any
 * UdpSocket, and keep it alive for as long as any UdpSocket is. On
 * non-Windows this is a no-op.
 */
class WinsockGuard {
public:
    WinsockGuard();
    ~WinsockGuard();

    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;

    /// @return Whether Winsock initialized successfully (always true on non-Windows).
    bool ok() const { return ok_; }

private:
    bool ok_ = false;
};

/**
 * @brief A single non-blocking UDP endpoint.
 *
 * This program opens two: one bound to the telemetry port (JSBSim -> us)
 * and one pointed at JSBSim's control input port (us -> JSBSim). Each
 * direction uses only the calls it needs, but both are supported on one
 * instance since neither JSBSim socket type used here requires
 * otherwise.
 */
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    /**
     * @brief Opens the underlying socket. Must succeed before any other call.
     * @return Whether the socket was opened successfully.
     */
    bool open();

    /**
     * @brief Binds to `INADDR_ANY:localPort` and puts the socket in non-blocking mode.
     *
     * Non-blocking mode is what lets waitReadable()/recvInto() pick up
     * JSBSim's telemetry feed without stalling the caller's loop.
     *
     * @param localPort Local UDP port to bind.
     * @return Whether the bind succeeded.
     */
    bool bindLocal(int localPort);

    /**
     * @brief Fixes the destination used by send().
     *
     * Also puts the socket in non-blocking mode, since `main.cpp` shares
     * one poll loop for both directions.
     *
     * @param host Destination IPv4 address (dotted-decimal).
     * @param port Destination UDP port.
     * @return Whether the destination was set successfully.
     */
    bool setDestination(const std::string& host, int port);

    /**
     * @brief Sends one datagram to the destination set by setDestination().
     * @param data Payload to send.
     * @return Whether the full payload was sent in one datagram.
     */
    bool send(const std::string& data);

    /// Result of waitReadable().
    enum class Readable {
        Ready,   ///< select() says a recv() right now won't block.
        Timeout, ///< No datagram within timeoutMs -- the expected common case for a poll loop, not an error.
        Error,   ///< select() itself failed.
    };

    /**
     * @brief Blocks up to `timeoutMs` waiting for this socket to become readable.
     *
     * Implemented via `select()` on a single-socket `fd_set`. This is the
     * wait; recvInto() below still runs non-blocking, so a spurious
     * wakeup can't stall the caller's loop (`main.cpp` shares one loop
     * between this and a periodic sender).
     *
     * @param timeoutMs Maximum time to block, in milliseconds.
     * @return Readable::Ready, Readable::Timeout, or Readable::Error.
     */
    Readable waitReadable(int timeoutMs);

    /**
     * @brief Non-blocking receive of exactly one datagram into `buf`.
     *
     * Callers that want to detect an oversize datagram (`recv()`
     * truncates it silently on POSIX, and returns an error on Windows)
     * should pass a `len` one byte larger than the expected payload and
     * check the returned count.
     *
     * @param buf Destination buffer, at least `len` bytes.
     * @param len Capacity of `buf` in bytes.
     * @return Number of bytes written, or a value <= 0 if nothing usable
     * was read -- either no datagram was queued
     * (`EWOULDBLOCK`/`WSAEWOULDBLOCK`, the expected common case right
     * after a Readable::Timeout) or `recv()` itself failed.
     */
    int recvInto(void* buf, std::size_t len);

    /// @return Whether open() has succeeded and the socket hasn't been closed.
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
