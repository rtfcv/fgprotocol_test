#include "control.h"

#include <cfloat>
#include <cmath>
#include <iomanip>
#include <sstream>

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
    // FGUDPInputSocket drops any datagram whose timestamp does not strictly
    // increase over the last one it accepted. The caller's clock normally
    // satisfies that on its own; nextafter() guarantees it even if two sends
    // land in the same tick, or if t itself doesn't advance.
    double stamped = (t > lastTimestamp) ? t : std::nextafter(lastTimestamp, DBL_MAX);
    lastTimestamp = stamped;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << stamped;
    for (double v : {c.elevator, c.aileron, c.rudder}) {
        oss << ',' << v;
    }
    oss << '\n';
    return oss.str();
}

} // namespace control
