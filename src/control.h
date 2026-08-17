/**
 * @file control.h
 * @brief This tester's own control policy.
 *
 * Which JSBSim properties to drive (kControlProperties), what values to
 * send and when (controlsAt()), and nothing else -- deliberately the only
 * place control values are chosen, nothing else in this program may add
 * or infer a control input. The FGUDPInputSocket wire format itself (the
 * `"timestamp,v1,...,vN\n"` line and the strictly-increasing-timestamp
 * rule) lives in `include/fgprotocol/control_wire.h` instead: that part is
 * generic to any property list, so it isn't this file's concern.
 * buildDatagram() below is just the adapter between the two -- this
 * demo's 3-value Controls struct on one side, the library's
 * arbitrary-length value array on the other.
 */
#ifndef JSBSIM_TESTER_CONTROL_H
#define JSBSIM_TESTER_CONTROL_H

#include <array>
#include <string>

/// This tester's control policy: what to send to JSBSim and when.
namespace control {

/**
 * @brief JSBSim properties this demo drives, in wire order.
 *
 * Must stay in the same order as `<input>`'s `<property>` list in
 * `scripts/demo.xml`. `main.cpp`'s `--print-input-xml` regenerates that
 * block from this array so the two can't silently drift apart --
 * JSBSim's FGUDPInputSocket drops the whole datagram, with no reply, if
 * the comma-separated value count doesn't exactly match the property
 * count.
 */
constexpr std::array<const char*, 3> kControlProperties = {
    "fcs/elevator-cmd-norm",
    "fcs/aileron-cmd-norm",
    "fcs/rudder-cmd-norm",
};

/// One control-surface command, matching kControlProperties order.
struct Controls {
    double elevator = 0.0; ///< `fcs/elevator-cmd-norm`, -1..1.
    double aileron = 0.0;  ///< `fcs/aileron-cmd-norm`, -1..1.
    double rudder = 0.0;   ///< `fcs/rudder-cmd-norm`, -1..1.
};

/**
 * @brief Pure, hardcoded, and the entire demo control schedule.
 *
 * | time          | elevator | aileron |
 * |---------------|----------|---------|
 * | t < 5s        | 0        | 0       |
 * | 5s <= t < 15s | -0.06    | 0       |
 * | 15s <= t < 25s| -0.06    | 0.12    |
 * | t >= 25s      | 0        | 0       |
 *
 * -0.06 elevator pitches the nose up, given this aircraft's Cmde sign in
 * `aircraft/minimal/minimal.xml`.
 *
 * Every value stays within JSBSim's expected [-1, 1] `*-cmd-norm` range.
 * Kept deliberately mild: `aircraft/minimal/minimal.xml` has no stall or
 * angle limiting, so a large sustained deflection can walk this
 * aircraft's alpha or bank angle out past where its (also deliberately
 * simple) linear aerodynamics stay physically sane.
 *
 * @param t Simulation time in seconds.
 * @return The commanded Controls for time `t`.
 */
Controls controlsAt(double t);

/**
 * @brief Builds one FGUDPInputSocket datagram carrying {elevator, aileron, rudder}.
 *
 * Values are sent in that order, matching kControlProperties. Thin
 * adapter over fgudp_input::buildDatagram()
 * (`include/fgprotocol/control_wire.h`), which owns the actual wire
 * format and the strictly-increasing-timestamp rule; see that header for
 * what `lastTimestamp` does and why.
 *
 * @param c Values to send.
 * @param t Caller's current time.
 * @param lastTimestamp In/out: the last timestamp actually sent.
 * @return The datagram, ready to send as-is.
 */
std::string buildDatagram(const Controls& c, double t, double& lastTimestamp);

} // namespace control

#endif // JSBSIM_TESTER_CONTROL_H
