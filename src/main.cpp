// jsbsim_tester: the whole point of this repo in one loop.
//
// Sends a hardcoded control schedule to JSBSim's UDP <input>, receives
// JSBSim's UDP <output type="FLIGHTGEAR"> telemetry, decodes it, and prints
// it. Nothing else -- see CLAUDE.md / the project plan for why that bar is
// deliberately kept this low.
#include "udp_socket.h"

#include "control.h"
#include "net_fdm.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kTelemetryPort = 5500;        // JSBSim <output> -> us (output/telemetry.xml)
constexpr const char* kJsbsimHost = "127.0.0.1";
constexpr int kControlPort = 5501;          // us -> JSBSim <input> (scripts/demo.xml)

constexpr double kControlSendPeriod = 1.0 / 20.0; // Hz at which we send controls
constexpr double kPrintPeriod = 1.0 / 5.0;        // Hz at which we print telemetry
constexpr double kNoLinkTimeout = 1.0;            // seconds of silence before "NO LINK"

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
    std::cout << "<input type=\"QTJSBSIM\" port=\"" << kControlPort << "\">\n";
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

    for (;;) {
        double t = nowSeconds();

        if (t - lastControlSend >= kControlSendPeriod) {
            lastControlSend = t;
            control::Controls c = control::controlsAt(t);
            std::string datagram = control::buildDatagram(c, t, controlTimestamp);
            controlOut.send(datagram);
        }

        std::vector<uint8_t> buf;
        while (telemetry.recvDatagram(buf)) {
            net_fdm::Packet p;
            net_fdm::DecodeResult r = net_fdm::decode(buf.data(), buf.size(), p);
            if (r == net_fdm::DecodeResult::Ok) {
                packet = p;
                lastPacketTime = t;
                haveLink = true;
            } else {
                std::cerr << "telemetry decode failed: " << net_fdm::describe(r)
                          << " (" << buf.size() << " bytes)\n";
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

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
