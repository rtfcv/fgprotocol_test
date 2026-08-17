// jsbsim_tester: the whole point of this repo in one loop.
//
// Sends a hardcoded control schedule to JSBSim's UDP <input>, receives
// JSBSim's UDP <output type="FLIGHTGEAR"> telemetry, decodes it, and prints
// it. Nothing else -- see CLAUDE.md / the project plan for why that bar is
// deliberately kept this low.
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

constexpr int kTelemetryPort = 5500;        // JSBSim <output> -> us (output/telemetry.xml)
constexpr const char* kJsbsimHost = "127.0.0.1";
constexpr int kControlPort = 5501;          // us -> JSBSim <input> (scripts/demo.xml)

constexpr double kControlSendPeriod = 1.0 / 20.0; // Hz at which we send controls
constexpr double kPrintPeriod = 1.0 / 5.0;        // Hz at which we print telemetry
constexpr double kNoLinkTimeout = 1.0;            // seconds of silence before "NO LINK"
constexpr int kSelectTimeoutMs = 5;               // how long each waitReadable() blocks

// recv() target for one telemetry datagram, sized one byte past a real
// FGNetFDM. A well-formed 408-byte datagram fills exactly `pkt`; anything
// larger spills into `overflow`, so the size net_fdm::decode() sees (the
// byte count recv() returns) comes back as 409+ and is rejected as
// WrongSize instead of being silently truncated by the OS (POSIX) or
// bounced with WSAEMSGSIZE (Windows) -- see README's "Gotchas" for why
// this asymmetry matters enough to guard against explicitly.
struct RecvBuffer {
    net_fdm::FGNetFDM pkt;
    uint8_t overflow;
};
static_assert(sizeof(RecvBuffer) == net_fdm::kPacketSize + 1, "unexpected padding in RecvBuffer");

double nowSeconds() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

double radToDeg(double rad) { return rad * 180.0 / 3.14159265358979323846; }

// Emits the <input> block scripts/demo.xml must contain, generated from
// control::kControlProperties rather than typed out by hand twice -- the
// two are required to match exactly (JSBSim drops the whole datagram,
// silently, on any property-count mismatch) and this is how they're kept
// in sync after either one changes.
void printInputXml() {
    std::cout << "<input type=\"QTJSBSIM\" port=\"" << kControlPort << "\" rate=\"30\">\n";
    for (const char* prop : control::kControlProperties) {
        std::cout << "  <property> " << prop << " </property>\n";
    }
    std::cout << "</input>\n";
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--print-input-xml") {
            printInputXml();
            return 0;
        }
    }

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

    std::cout << "jsbsim_tester: sending controls -> " << kJsbsimHost << ":" << kControlPort
              << ", listening for telemetry on :" << kTelemetryPort << "\n";
    std::cout << "(run .\\run_jsbsim.ps1 in another terminal to start JSBSim; Ctrl+C to quit)\n\n";

    double lastControlSend = -1.0;
    double lastPrint = -1.0;
    double lastPacketTime = -1.0;
    double controlTimestamp = -1.0;

    net_fdm::Packet packet;
    bool haveLink = false;

    bool haveWarnedVersion = false; // only warn on the first mismatch, and
    uint32_t warnedVersion = 0;     // again if the mismatched version changes
                                     // -- JSBSim emits at 30 Hz and an
                                     // unconditional per-packet warning would
                                     // bury the telemetry rows below it.

    for (;;) {
        double t = nowSeconds();

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

        // Drain everything currently queued -- JSBSim's 30 Hz feed can
        // outrun a single waitReadable() wakeup, and non-blocking recvInto()
        // just returns <= 0 once the socket is empty.
        if (ready == UdpSocket::Readable::Ready) {
            for (;;) {
                RecvBuffer buf;
                int n = telemetry.recvInto(&buf.pkt, sizeof(buf));
                if (n <= 0) break;

                net_fdm::Packet p;
                net_fdm::DecodeResult r = net_fdm::decode(
                    reinterpret_cast<const uint8_t*>(&buf.pkt), static_cast<std::size_t>(n), p);

                if (r == net_fdm::DecodeResult::WrongSize) {
                    std::cerr << "telemetry decode failed: " << net_fdm::describe(r)
                              << " (" << n << " bytes)\n";
                    continue;
                }

                if (r == net_fdm::DecodeResult::WrongVersion &&
                    (!haveWarnedVersion || warnedVersion != p.version)) {
                    std::cerr << "[WARN] FDM version mismatch: got " << p.version
                              << ", expected " << net_fdm::kVersion << "\n";
                    haveWarnedVersion = true;
                    warnedVersion = p.version;
                }

                // Ok and WrongVersion both leave `p` populated (net_fdm.h)
                // -- a version mismatch alone doesn't make the fields
                // garbage, so use the packet either way, just warned about
                // above.
                packet = p;
                lastPacketTime = t;
                haveLink = true;
            }
        }

        if (haveLink && (t - lastPacketTime > kNoLinkTimeout)) {
            haveLink = false;
        }

        if (t - lastPrint >= kPrintPeriod) {
            lastPrint = t;
            control::Controls c = control::controlsAt(t);

            std::ostringstream line;
            line.setf(std::ios::fixed);
            line << "t=" << std::setw(6) << std::setprecision(1) << t;

            if (haveLink) {
                double altFt = packet.altitude_m * 3.280839895;
                double vsFpm = packet.climb_rate_fps * 60.0;
                line << "  alt=" << std::setw(6) << std::setprecision(0) << altFt << "ft"
                     << "  ias=" << std::setw(5) << std::setprecision(0) << packet.vcas_kt << "kt"
                     << "  pitch=" << std::showpos << std::setprecision(1) << radToDeg(packet.theta_rad) << std::noshowpos
                     << "  roll=" << std::showpos << std::setprecision(1) << radToDeg(packet.phi_rad) << std::noshowpos
                     << "  hdg=" << std::setprecision(1) << radToDeg(packet.psi_rad)
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
