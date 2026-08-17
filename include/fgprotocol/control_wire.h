/**
 * @file control_wire.h
 * @brief JSBSim's FGUDPInputSocket wire format.
 *
 * (`src/input_output/FGUDPInputSocket.cpp`): a comma-separated ASCII line,
 * `"timestamp,v1,v2,...,vN\n"`, where each `vN` is the next value in the
 * `<input>`'s declared `<property>` list, in order. JSBSim drops the whole
 * datagram, silently, on either a comma-count mismatch against the
 * property count it was configured with, or a timestamp that doesn't
 * strictly increase over the last one it accepted.
 *
 * This header only knows the wire format -- not which properties a caller
 * is driving, nor what values to send when. That's application policy (see
 * the jsbsim_tester project's `src/control.h` for this repo's own choice: a
 * 3-property elevator/aileron/rudder schedule built on top of this).
 * Keeping that split is what makes this header reusable for a different
 * property list or a different aircraft without touching it at all.
 *
 * Header-only, dependency-free (standard library only): meant to be reused
 * the same way as net_fdm.h alongside it -- copy-pasted, or pulled in via
 * this directory's own CMakeLists.txt.
 */
#ifndef FGPROTOCOL_CONTROL_WIRE_H
#define FGPROTOCOL_CONTROL_WIRE_H

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

/// JSBSim FGUDPInputSocket wire protocol: encode-only, header-only, no
/// platform dependencies.
namespace fgudp_input {

/**
 * @brief Builds one FGUDPInputSocket datagram from `count` values, in order.
 *
 * `t` is the caller's current time; `lastTimestamp` is both read and
 * updated. The embedded timestamp is guaranteed strictly greater than the
 * previous call's, bumped via `std::nextafter()` if `t` didn't advance
 * enough on its own (e.g. two sends landing in the same clock tick, or `t`
 * going backwards) -- JSBSim silently drops any packet whose timestamp
 * doesn't strictly increase, so this is the actual protocol requirement,
 * not just a nicety.
 *
 * @param values Ordered array of `count` values to send, matching the
 * receiving `<input>`'s declared `<property>` order.
 * @param count Number of entries in `values`.
 * @param t Caller's current time.
 * @param lastTimestamp In/out: the last timestamp actually sent; updated
 * to the timestamp embedded in the returned datagram.
 * @return The datagram, e.g. `"1.500000,-0.06,0.12,0.00\n"`.
 */
inline std::string buildDatagram(const double* values, std::size_t count,
                                  double t, double& lastTimestamp) {
    double stamped = (t > lastTimestamp) ? t : std::nextafter(lastTimestamp, DBL_MAX);
    lastTimestamp = stamped;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << stamped;
    for (std::size_t i = 0; i < count; ++i) {
        oss << ',' << values[i];
    }
    oss << '\n';
    return oss.str();
}

} // namespace fgudp_input

#endif // FGPROTOCOL_CONTROL_WIRE_H
