// Tests for control::controlsAt() and control::buildDatagram(). Written
// against src/control.h before src/control.cpp exists (CLAUDE.md: write
// tests before implementing). No JSBSim, no network, no wall clock --
// buildDatagram takes its timestamp as an explicit parameter so these stay
// deterministic and fast.
#include "control.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"

namespace {

// Splits "t,v1,v2,v3\n" into its comma-separated fields, dropping the
// trailing newline.
std::vector<std::string> splitFields(const std::string& datagram) {
    std::vector<std::string> fields;
    std::string cur;
    for (char c : datagram) {
        if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else if (c == '\n') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) fields.push_back(cur);
    return fields;
}

} // namespace

int main() {
    // --- controlsAt: schedule boundaries and range -------------------

    // Before 5s: neutral.
    {
        control::Controls c = control::controlsAt(0.0);
        CHECK_NEAR(c.elevator, 0.0, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
        CHECK_NEAR(c.rudder, 0.0, 1e-9);
    }
    {
        control::Controls c = control::controlsAt(4.999);
        CHECK_NEAR(c.elevator, 0.0, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
    }

    // [5s, 15s): elevator down, aileron still neutral.
    {
        control::Controls c = control::controlsAt(5.0);
        CHECK_NEAR(c.elevator, -0.15, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
    }
    {
        control::Controls c = control::controlsAt(14.999);
        CHECK_NEAR(c.elevator, -0.15, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
    }

    // [15s, 25s): elevator still down, aileron rolled in.
    {
        control::Controls c = control::controlsAt(15.0);
        CHECK_NEAR(c.elevator, -0.15, 1e-9);
        CHECK_NEAR(c.aileron, 0.30, 1e-9);
    }
    {
        control::Controls c = control::controlsAt(24.999);
        CHECK_NEAR(c.elevator, -0.15, 1e-9);
        CHECK_NEAR(c.aileron, 0.30, 1e-9);
    }

    // >=25s: back to neutral.
    {
        control::Controls c = control::controlsAt(25.0);
        CHECK_NEAR(c.elevator, 0.0, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
        CHECK_NEAR(c.rudder, 0.0, 1e-9);
    }
    {
        control::Controls c = control::controlsAt(1000.0);
        CHECK_NEAR(c.elevator, 0.0, 1e-9);
        CHECK_NEAR(c.aileron, 0.0, 1e-9);
    }

    // Every value across the whole schedule must stay within JSBSim's
    // expected [-1, 1] *-cmd-norm range.
    for (double t = 0.0; t <= 40.0; t += 0.5) {
        control::Controls c = control::controlsAt(t);
        CHECK(c.elevator >= -1.0 && c.elevator <= 1.0);
        CHECK(c.aileron >= -1.0 && c.aileron <= 1.0);
        CHECK(c.rudder >= -1.0 && c.rudder <= 1.0);
    }

    // --- buildDatagram: field count, format, ordering -----------------

    {
        double lastTimestamp = -1.0;
        control::Controls c{-0.15, 0.30, 0.0};
        std::string d = control::buildDatagram(c, 1.0, lastTimestamp);

        // Must end in exactly one trailing newline.
        CHECK(!d.empty());
        CHECK_EQ(d.back(), '\n');
        CHECK_EQ(std::count(d.begin(), d.end(), '\n'), 1);

        std::vector<std::string> fields = splitFields(d);
        // timestamp + one field per kControlProperties entry.
        CHECK_EQ(fields.size(), control::kControlProperties.size() + 1);

        CHECK_NEAR(std::stod(fields[0]), 1.0, 1e-9);
        CHECK_NEAR(std::stod(fields[1]), -0.15, 1e-6); // elevator
        CHECK_NEAR(std::stod(fields[2]), 0.30, 1e-6);  // aileron
        CHECK_NEAR(std::stod(fields[3]), 0.0, 1e-6);   // rudder
    }

    // --- buildDatagram: strictly increasing timestamp ------------------

    {
        double lastTimestamp = 0.0;
        control::Controls c{};

        // Normal case: caller-supplied t already increases.
        std::string d1 = control::buildDatagram(c, 1.0, lastTimestamp);
        CHECK_NEAR(lastTimestamp, 1.0, 1e-9);

        // Same t as before (simulating two sends landing in the same
        // instant) must still bump strictly past the last one, not repeat
        // or go backwards -- JSBSim drops any non-increasing timestamp.
        std::string d2 = control::buildDatagram(c, 1.0, lastTimestamp);
        CHECK(lastTimestamp > 1.0);

        // A t that's actually earlier than the last accepted timestamp must
        // also still bump forward, never move backward.
        std::string d3 = control::buildDatagram(c, 0.5, lastTimestamp);
        double afterD2 = std::stod(splitFields(d2)[0]);
        CHECK(lastTimestamp > afterD2);

        (void)d1;
        (void)d3;
    }

    std::cout << "test_control: all checks passed\n";
    return 0;
}
