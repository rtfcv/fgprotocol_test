/**
 * @file main.cpp
 * @brief jsbsim_tester: demonstrates the minimum needed for a C++ program
 * and JSBSim to establish two-way UDP communication.
 *
 * Per this project's own spec (`initial_instruction.txt`): "its main
 * purpose is to demonstrate absolute minimum on how cpp program and
 * jsbsim can establish communication... every addition (control input
 * and print telem) should not shadow that." That's a requirement to keep
 * two things visibly separate, not to strip the second one out --
 * "bare minimum function of... inputting... control input. retrieving
 * telemetry. and printing" already asks for a schedule and for output a
 * human can read. So this file marks the split explicitly, at each point
 * below, rather than leaving a reader to guess which lines are load-
 * bearing for the communication demonstration and which are polish on
 * top of it:
 *
 * - **CORE PROTOCOL**: open the two UDP sockets JSBSim expects, send one
 *   hardcoded control datagram in its wire format, receive and decode
 *   one telemetry datagram, print *something* to show it arrived. Cut
 *   any of this and the program stops demonstrating cpp<->JSBSim
 *   communication.
 * - **DEMO / READABILITY**: a multi-phase control schedule instead of
 *   one constant value (`src/control.cpp`), a human-readable formatted
 *   telemetry line instead of raw numbers, "NO LINK" detection, a
 *   de-duplicated version-mismatch warning, the `--print-input-xml` dev
 *   helper, and defensive handling of an oversized datagram that can't
 *   happen in normal operation. All explicitly allowed by the spec above
 *   -- kept, not required.
 */
#include "udp_socket.h"

#include "control.h"
#include "fgprotocol/net_fdm.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// CORE PROTOCOL: ports must match scripts/demo.xml's <input>/<output>
// blocks exactly -- these aren't free choices, JSBSim won't talk to the
// wrong port.
constexpr int kTelemetryPort = 5500;        ///< JSBSim `<output>` -> us (`output/telemetry.xml`).
constexpr const char* kJsbsimHost = "127.0.0.1";
constexpr int kControlPort = 5501;          ///< Us -> JSBSim `<input>` (`scripts/demo.xml`).

// DEMO / READABILITY: how *often* to send/print/wait. Any reasonable
// values work -- these are tuning, not protocol requirements. (The act
// of sending periodically instead of once is CORE: JSBSim's UDP is
// unreliable, so a single hardcoded send could just be dropped and never
// demonstrate anything. How often is not.)
constexpr double kControlSendPeriod = 1.0 / 20.0; ///< Hz at which we send controls.
constexpr double kPrintPeriod = 1.0 / 5.0;        ///< Hz at which we print telemetry.
constexpr double kNoLinkTimeout = 1.0;            ///< Seconds of silence before "NO LINK".
constexpr int kSelectTimeoutMs = 5;               ///< How long each waitReadable() blocks.

/**
 * @brief recv() target for one telemetry datagram, sized one byte past a real FGNetFDM.
 *
 * DEMO / READABILITY: defensive, not required. A well-formed 408-byte
 * datagram fills exactly `pkt`; anything larger spills into `overflow`,
 * so the size net_fdm::decode() sees comes back as 409+ and is rejected
 * as DecodeResult::WrongSize instead of being silently truncated by the
 * OS (POSIX) or bounced with `WSAEMSGSIZE` (Windows) -- see README's
 * "Gotchas". A plain `uint8_t[kPacketSize]` would demonstrate the happy
 * path identically; this only matters if something sends a malformed
 * datagram, which JSBSim never does in normal operation.
 */
struct RecvBuffer {
    net_fdm::FGNetFDM pkt; ///< Receives exactly one FGNetFDM's worth of bytes.
    uint8_t overflow;      ///< Catches byte 409+ of an oversize datagram.
};
static_assert(sizeof(RecvBuffer) == net_fdm::kPacketSize + 1, "unexpected padding in RecvBuffer");

/// Shared infrastructure (both the CORE send loop and the DEMO print loop need a clock).
/// @return Seconds elapsed since the first call to this function.
double nowSeconds() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

/// DEMO / READABILITY: only needed because the printed telemetry line
/// below chooses degrees over the raw radians decode() returns.
/// @return `rad` converted from radians to degrees.
double radToDeg(double rad) { return rad * 180.0 / 3.14159265358979323846; }

/**
 * @brief Emits the `<input>` block `scripts/demo.xml` must contain.
 *
 * DEMO / READABILITY: a dev tool, not part of the runtime communication
 * demonstration -- `--print-input-xml` exits immediately, it never talks
 * to JSBSim. Generated from control::kControlProperties rather than
 * typed out by hand twice -- the two are required to match exactly
 * (JSBSim drops the whole datagram, silently, on any property-count
 * mismatch) and this is how they're kept in sync after either one
 * changes.
 */
void printInputXml() {
    std::cout << "<input type=\"QTJSBSIM\" port=\"" << kControlPort << "\" rate=\"30\">\n";
    for (const char* prop : control::kControlProperties) {
        std::cout << "  <property> " << prop << " </property>\n";
    }
    std::cout << "</input>\n";
}

} // namespace

/**
 * @brief Entry point.
 *
 * `--print-input-xml` prints the `<input>` block and exits; otherwise
 * runs the send/receive/print loop until killed.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on a clean `--print-input-xml` exit; 1 on any setup or
 * `select()` failure. The main loop otherwise runs forever.
 */
int main(int argc, char** argv) {
    // DEMO / READABILITY: dev tool branch, see printInputXml()'s comment.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--print-input-xml") {
            printInputXml();
            return 0;
        }
    }

    // ------------------------------------------------------------------
    // CORE PROTOCOL: everything from here to the end of socket setup is
    // strictly necessary. Without a working Winsock, a bound telemetry
    // socket, and a socket aimed at JSBSim's control port, there is no
    // communication to demonstrate.
    // ------------------------------------------------------------------
    WinsockGuard winsock;
    if (!winsock.ok()) {
        std::cerr << "Failed to initialize Winsock\n";
        return 1;
    }

    UdpSocket telemetry;
    if (!telemetry.open() || !telemetry.bindLocal(kTelemetryPort)) {
        std::cerr << "Failed to bind telemetry socket on port " << kTelemetryPort << "\n";
        return 1;
    }

    UdpSocket controlOut;
    if (!controlOut.open() || !controlOut.setDestination(kJsbsimHost, kControlPort)) {
        std::cerr << "Failed to set control destination " << kJsbsimHost << ":" << kControlPort << "\n";
        return 1;
    }

    // DEMO / READABILITY: tells a human what's about to happen; JSBSim
    // never sees this.
    std::cout << "jsbsim_tester: sending controls -> " << kJsbsimHost << ":" << kControlPort
              << ", listening for telemetry on :" << kTelemetryPort << "\n";
    std::cout << "(run .\\run_jsbsim.ps1 in another terminal to start JSBSim; Ctrl+C to quit)\n\n";

    // CORE PROTOCOL state: needed to run the send/receive loop at all --
    // lastControlSend/controlTimestamp gate and timestamp the required
    // control send; the loop cannot function without them.
    double lastControlSend = -1.0;
    double controlTimestamp = -1.0;
    net_fdm::FGNetFDMReversed packet;

    // DEMO / READABILITY state: exists only to pace/format printed
    // output and report link status to a human. Communication would
    // still happen with none of this.
    double lastPrint = -1.0;
    double lastPacketTime = -1.0;
    bool haveLink = false;
    bool haveWarnedVersion = false; // only warn on the first mismatch, and
    uint32_t warnedVersion = 0;     // again if the mismatched version changes
                                     // -- JSBSim emits at 30 Hz and an
                                     // unconditional per-packet warning would
                                     // bury the telemetry rows below it.

    for (;;) {
        double t = nowSeconds();

        // --------------------------------------------------------------
        // CORE PROTOCOL: "inputting predetermined, hardcoded control
        // input." What values get sent (control::controlsAt()) is
        // DEMO / READABILITY -- a single constant would satisfy this
        // requirement just as well; the multi-phase schedule exists to
        // give the demo something to watch change over time. What's
        // core is that *some* hardcoded value gets sent, in JSBSim's
        // wire format, repeatedly (see kControlSendPeriod's comment for
        // why "repeatedly" is also core).
        // --------------------------------------------------------------
        if (t - lastControlSend >= kControlSendPeriod) {
            lastControlSend = t;
            control::Controls c = control::controlsAt(t);
            std::string datagram = control::buildDatagram(c, t, controlTimestamp);
            controlOut.send(datagram);
        }

        UdpSocket::Readable ready = telemetry.waitReadable(kSelectTimeoutMs);
        if (ready == UdpSocket::Readable::Error) {
            std::cerr << "[ERROR] select() failed\n";
            return 1;
        }

        // --------------------------------------------------------------
        // CORE PROTOCOL: "retrieving telemetry" -- wait for a datagram,
        // receive it, decode it. The draining `for(;;)` (versus handling
        // one packet per loop iteration) is DEMO / READABILITY: JSBSim's
        // 30 Hz feed can outrun a single wakeup, and without draining,
        // a backlog would build silently instead of self-correcting.
        // The happy-path demonstration works either way.
        // --------------------------------------------------------------
        if (ready == UdpSocket::Readable::Ready) {
            for (;;) {
                RecvBuffer buf;
                int n = telemetry.recvInto(&buf.pkt, sizeof(buf));
                if (n <= 0) break;

                net_fdm::FGNetFDMReversed p;
                net_fdm::DecodeResult r = net_fdm::decode(
                    reinterpret_cast<const uint8_t*>(&buf.pkt), static_cast<std::size_t>(n), p);

                // DEMO / READABILITY: graceful reporting of a malformed
                // datagram. The minimum demonstration could just ignore
                // r == WrongSize and move on.
                if (r == net_fdm::DecodeResult::WrongSize) {
                    std::cerr << "telemetry decode failed: " << net_fdm::describe(r)
                              << " (" << n << " bytes)\n";
                    continue;
                }

                // DEMO / READABILITY: de-duplicated warning UX.
                if (r == net_fdm::DecodeResult::WrongVersion &&
                    (!haveWarnedVersion || warnedVersion != p.version)) {
                    std::cerr << "[WARN] FDM version mismatch: got " << p.version
                              << ", expected " << net_fdm::kVersion << "\n";
                    haveWarnedVersion = true;
                    warnedVersion = p.version;
                }

                // CORE PROTOCOL: this line is "retrieving telemetry" --
                // Ok and WrongVersion both leave `p` populated (net_fdm.h)
                // since a version mismatch alone doesn't make the fields
                // garbage.
                packet = p;

                // DEMO / READABILITY: link-status bookkeeping for the
                // "NO LINK" print below.
                lastPacketTime = t;
                haveLink = true;
            }
        }

        // DEMO / READABILITY: "NO LINK" detection is UX, not protocol.
        if (haveLink && (t - lastPacketTime > kNoLinkTimeout)) {
            haveLink = false;
        }

        // ------------------------------------------------------------------
        // CORE PROTOCOL (bare requirement): print *something* to show
        // telemetry was retrieved -- "retrieving telemetry. and
        // printing" is explicitly the spec. DEMO / READABILITY (this
        // block's actual content): degrees instead of radians, feet
        // instead of meters, fixed-width columns, "NO LINK" wording --
        // none of that is required, only that a line gets printed.
        // ------------------------------------------------------------------
        if (t - lastPrint >= kPrintPeriod) {
            lastPrint = t;
            control::Controls c = control::controlsAt(t);

            std::ostringstream line;
            line.setf(std::ios::fixed);
            line << "t=" << std::setw(6) << std::setprecision(1) << t;

            if (haveLink) {
                double altFt = packet.altitude * 3.280839895;
                double vsFpm = packet.climb_rate * 60.0;
                line << "  alt=" << std::setw(6) << std::setprecision(0) << altFt << "ft"
                     << "  ias=" << std::setw(5) << std::setprecision(0) << packet.vcas << "kt"
                     << "  pitch=" << std::showpos << std::setprecision(1) << radToDeg(packet.theta) << std::noshowpos
                     << "  roll=" << std::showpos << std::setprecision(1) << radToDeg(packet.phi) << std::noshowpos
                     << "  hdg=" << std::setprecision(1) << radToDeg(packet.psi)
                     << "  vs=" << std::showpos << std::setprecision(0) << vsFpm << "fpm" << std::noshowpos
                     << "  elev=" << std::showpos << std::setprecision(3) << c.elevator << std::noshowpos
                     << "  ail=" << std::showpos << std::setprecision(3) << c.aileron << std::noshowpos;
            } else {
                line << "  NO LINK";
            }
            std::cout << line.str() << "\n";
        }

        // No sleep here: waitReadable() above already blocked up to
        // kSelectTimeoutMs whenever nothing was queued, so this loop only
        // spins fast while telemetry is actively arriving.
    }
}
