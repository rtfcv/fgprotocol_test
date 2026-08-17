// Tests for fgudp_input::buildDatagram() (include/fgprotocol/control_wire.h),
// exercised directly against a plain double[] rather than through this
// tester's own Controls struct (see tests/test_control.cpp for that side).
// The point of this file is to prove the library function is genuinely
// decoupled from any specific property list or aircraft's control scheme --
// it doesn't know about "elevator" or "3 values," only "some ordered list
// of numbers."
#include "fgprotocol/control_wire.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "check.h"

namespace {

// Splits "t,v1,v2,...\n" into its comma-separated fields, dropping the
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
    // --- field count, format, ordering ---------------------------------

    // Two values -- this tester's own Controls happens to have 3 fields
    // (see test_control.cpp), but the library must not know that.
    {
        double lastTimestamp = -1.0;
        double values[] = {0.25, -0.75};
        std::string d = fgudp_input::buildDatagram(values, 2, 1.0, lastTimestamp);

        CHECK(!d.empty());
        CHECK_EQ(d.back(), '\n');
        CHECK_EQ(std::count(d.begin(), d.end(), '\n'), 1);

        std::vector<std::string> fields = splitFields(d);
        CHECK_EQ(fields.size(), static_cast<std::size_t>(3)); // timestamp + 2 values

        CHECK_NEAR(std::stod(fields[0]), 1.0, 1e-9);
        CHECK_NEAR(std::stod(fields[1]), 0.25, 1e-6);
        CHECK_NEAR(std::stod(fields[2]), -0.75, 1e-6);
    }

    // Four values, in a deliberately non-monotonic order -- ordering must
    // be preserved exactly, not resorted or grouped.
    {
        double lastTimestamp = -1.0;
        double values[] = {3.0, 1.0, 4.0, 1.5};
        std::string d = fgudp_input::buildDatagram(values, 4, 2.0, lastTimestamp);
        std::vector<std::string> fields = splitFields(d);
        CHECK_EQ(fields.size(), static_cast<std::size_t>(5)); // timestamp + 4 values
        CHECK_NEAR(std::stod(fields[1]), 3.0, 1e-6);
        CHECK_NEAR(std::stod(fields[2]), 1.0, 1e-6);
        CHECK_NEAR(std::stod(fields[3]), 4.0, 1e-6);
        CHECK_NEAR(std::stod(fields[4]), 1.5, 1e-6);
    }

    // Zero values: still just the timestamp + newline, not an error --
    // "some ordered list of numbers" includes the empty list.
    {
        double lastTimestamp = -1.0;
        std::string d = fgudp_input::buildDatagram(nullptr, 0, 1.0, lastTimestamp);
        std::vector<std::string> fields = splitFields(d);
        CHECK_EQ(fields.size(), static_cast<std::size_t>(1));
        CHECK_NEAR(std::stod(fields[0]), 1.0, 1e-9);
    }

    // --- strictly increasing timestamp ---------------------------------
    // JSBSim's FGUDPInputSocket drops any datagram whose timestamp doesn't
    // strictly increase over the last one it accepted -- this is the actual
    // protocol requirement the library exists to get right, not a nicety.

    {
        double lastTimestamp = 0.0;
        double values[] = {0.0};

        // Normal case: caller-supplied t already increases.
        std::string d1 = fgudp_input::buildDatagram(values, 1, 1.0, lastTimestamp);
        CHECK_NEAR(lastTimestamp, 1.0, 1e-9);

        // Same t as before (two sends landing in the same instant) must
        // still bump strictly past the last one, not repeat or go
        // backwards.
        std::string d2 = fgudp_input::buildDatagram(values, 1, 1.0, lastTimestamp);
        CHECK(lastTimestamp > 1.0);
        double afterD2 = std::stod(splitFields(d2)[0]);

        // A t that's actually earlier than the last accepted timestamp must
        // also still bump forward, never move backward.
        std::string d3 = fgudp_input::buildDatagram(values, 1, 0.5, lastTimestamp);
        CHECK(lastTimestamp > afterD2);

        (void)d1;
        (void)d3;
    }

    std::cout << "test_control_wire: all checks passed\n";
    return 0;
}
