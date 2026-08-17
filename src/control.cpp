/**
 * @file control.cpp
 * @brief Implements this tester's control schedule and datagram adapter.
 *
 * See control.h for the documented public API; this file is
 * implementation only.
 */
#include "control.h"

#include "fgprotocol/control_wire.h"

namespace control {

Controls controlsAt(double t) {
    Controls c;
    if (t < 5.0) {
        // neutral
    } else if (t < 15.0) {
        c.elevator = -0.06;
    } else if (t < 25.0) {
        c.elevator = -0.06;
        c.aileron = 0.12;
    } else {
        // back to neutral
    }
    return c;
}

std::string buildDatagram(const Controls& c, double t, double& lastTimestamp) {
    // {elevator, aileron, rudder}, in kControlProperties order -- the wire
    // format itself (comma-separated line, strictly-increasing timestamp)
    // is fgudp_input::buildDatagram()'s job, not this file's.
    const double values[] = {c.elevator, c.aileron, c.rudder};
    return fgudp_input::buildDatagram(values, 3, t, lastTimestamp);
}

} // namespace control
