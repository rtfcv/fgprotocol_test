#ifndef JSBSIM_TESTER_CONTROL_H
#define JSBSIM_TESTER_CONTROL_H

#include <array>
#include <string>

// Everything to do with the hardcoded control input this demo sends to
// JSBSim, and the small subset of JSBSim's FGUDPInputSocket wire format
// needed to send it. Deliberately the only place control values are chosen
// -- nothing else in this program may add or infer a control input.
namespace control {

// Must stay in the same order as <input>'s <property> list in
// scripts/demo.xml. main.cpp's --print-input-xml regenerates that block
// from this array so the two can't silently drift apart -- JSBSim's
// FGUDPInputSocket drops the whole datagram, with no reply, if the
// comma-separated value count doesn't exactly match the property count.
constexpr std::array<const char*, 3> kControlProperties = {
    "fcs/elevator-cmd-norm",
    "fcs/aileron-cmd-norm",
    "fcs/rudder-cmd-norm",
};

struct Controls {
    double elevator = 0.0;
    double aileron = 0.0;
    double rudder = 0.0;
};

// Pure, hardcoded, and the entire demo control schedule:
//   t <  5s : neutral
//   t < 15s : elevator -0.15 (nose down input)
//   t < 25s : elevator -0.15, aileron +0.30 (roll in)
//   t >=25s : neutral
// Every value stays within JSBSim's expected [-1, 1] *-cmd-norm range.
Controls controlsAt(double t);

// Builds one FGUDPInputSocket datagram: "timestamp,v1,v2,v3\n", matching
// kControlProperties order. `lastTimestamp` is both read and updated: the
// returned timestamp is guaranteed strictly greater than the previous call's,
// bumping via nextafter() if wall-clock time didn't advance enough on its
// own -- JSBSim silently drops any packet whose timestamp doesn't increase.
std::string buildDatagram(const Controls& c, double t, double& lastTimestamp);

} // namespace control

#endif // JSBSIM_TESTER_CONTROL_H
