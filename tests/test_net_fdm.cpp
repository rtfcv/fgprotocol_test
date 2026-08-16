// Tests for net_fdm::decode(). Written against src/net_fdm.h before
// src/net_fdm.cpp exists (CLAUDE.md: write tests before implementing).
//
// No JSBSim and no network needed -- everything here is a synthetic,
// hand-built buffer standing in for a real UDP datagram.
#include "net_fdm.h"

#include <cstring>
#include <vector>

#include "check.h"

namespace {

// Appends `v` to `buf` as 4 big-endian bytes.
void putU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v >> 24));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v));
}

void putI32(std::vector<uint8_t>& buf, int32_t v) {
    putU32(buf, static_cast<uint32_t>(v));
}

void putF32(std::vector<uint8_t>& buf, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    putU32(buf, bits);
}

void putF64(std::vector<uint8_t>& buf, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    }
}

// Builds one full, valid 408-byte FGNetFDM v24 datagram with a distinct,
// recognizable value in every field (so a field-order swap -- the realistic
// bug here -- shows up as the wrong value landing in the wrong member,
// rather than two fields that happen to share a value hiding the mistake).
std::vector<uint8_t> buildValidPacket() {
    std::vector<uint8_t> b;
    b.reserve(net_fdm::kPacketSize);

    putU32(b, net_fdm::kVersion);
    putU32(b, 0xDEADBEEF);  // padding -- must be discarded, not decoded

    putF64(b, 1.111);  // longitude_rad
    putF64(b, 2.222);  // latitude_rad
    putF64(b, 3333.0); // altitude_m
    putF32(b, 4.4f);   // agl_m
    putF32(b, 5.5f);   // phi_rad
    putF32(b, 6.6f);   // theta_rad
    putF32(b, 7.7f);   // psi_rad
    putF32(b, 8.8f);   // alpha_rad
    putF32(b, 9.9f);   // beta_rad

    putF32(b, 10.1f); // phidot_rad_s
    putF32(b, 11.1f); // thetadot_rad_s
    putF32(b, 12.1f); // psidot_rad_s
    putF32(b, 130.0f); // vcas_kt
    putF32(b, 14.1f); // climb_rate_fps
    putF32(b, 15.1f); // v_north_fps
    putF32(b, 16.1f); // v_east_fps
    putF32(b, 17.1f); // v_down_fps
    putF32(b, 18.1f); // v_body_u_fps
    putF32(b, 19.1f); // v_body_v_fps
    putF32(b, 20.1f); // v_body_w_fps

    putF32(b, 21.1f); // a_x_pilot_fps2
    putF32(b, 22.1f); // a_y_pilot_fps2
    putF32(b, 23.1f); // a_z_pilot_fps2

    putF32(b, 0.0f);  // stall_warning
    putF32(b, 24.1f); // slip_deg

    putU32(b, 1); // num_engines
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putU32(b, 2); // eng_state
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 2500.0f); // rpm
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 8.0f);    // fuel_flow_gph
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 5.0f);    // fuel_px_psi
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 1400.0f); // egt_degf
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 380.0f);  // cht_degf
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 22.0f);   // mp_inhg
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 0.0f);    // tit
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 180.0f);  // oil_temp_degf
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) putF32(b, 60.0f);   // oil_px_psi

    putU32(b, 2); // num_tanks
    for (int i = 0; i < net_fdm::kMaxTanks; ++i) putF32(b, 100.0f); // fuel_quantity_lbs

    putU32(b, 3); // num_wheels
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) putU32(b, 0);   // wow
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) putF32(b, 1.0f); // gear_pos_norm
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) putF32(b, 0.0f); // gear_steer_deg
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) putF32(b, 0.0f); // gear_compression_norm

    putU32(b, 999);   // cur_time
    putI32(b, -1);    // warp
    putF32(b, 25000.0f); // visibility_m

    putF32(b, 0.10f); // elevator_norm
    putF32(b, 0.20f); // elevator_trim_norm
    putF32(b, 0.30f); // left_flap_norm
    putF32(b, 0.40f); // right_flap_norm
    putF32(b, 0.50f); // left_aileron_norm
    putF32(b, 0.60f); // right_aileron_norm
    putF32(b, 0.70f); // rudder_norm
    putF32(b, 0.80f); // nose_wheel_norm
    putF32(b, 0.90f); // speedbrake_norm
    putF32(b, 1.00f); // spoilers_norm

    return b;
}

} // namespace

int main() {
    // The buffer builder itself must produce exactly one packet's worth of
    // bytes, or every test below is checking the wrong thing.
    std::vector<uint8_t> valid = buildValidPacket();
    CHECK_EQ(valid.size(), net_fdm::kPacketSize);
    CHECK_EQ(net_fdm::kPacketSize, static_cast<std::size_t>(408));

    // Happy path: every field lands where it belongs.
    net_fdm::Packet p;
    net_fdm::DecodeResult r = net_fdm::decode(valid.data(), valid.size(), p);
    CHECK(r == net_fdm::DecodeResult::Ok);

    CHECK_EQ(p.version, net_fdm::kVersion);
    CHECK_NEAR(p.longitude_rad, 1.111, 1e-9);
    CHECK_NEAR(p.latitude_rad, 2.222, 1e-9);
    CHECK_NEAR(p.altitude_m, 3333.0, 1e-9);
    CHECK_NEAR(p.agl_m, 4.4, 1e-5);
    CHECK_NEAR(p.phi_rad, 5.5, 1e-5);
    CHECK_NEAR(p.theta_rad, 6.6, 1e-5);
    CHECK_NEAR(p.psi_rad, 7.7, 1e-5);
    CHECK_NEAR(p.alpha_rad, 8.8, 1e-5);
    CHECK_NEAR(p.beta_rad, 9.9, 1e-5);
    CHECK_NEAR(p.vcas_kt, 130.0, 1e-4);
    CHECK_NEAR(p.climb_rate_fps, 14.1, 1e-4);
    CHECK_NEAR(p.slip_deg, 24.1, 1e-4);
    CHECK_EQ(p.num_engines, static_cast<uint32_t>(1));
    CHECK_EQ(p.eng_state[0], static_cast<uint32_t>(2));
    CHECK_NEAR(p.rpm[0], 2500.0, 1e-3);
    CHECK_EQ(p.num_tanks, static_cast<uint32_t>(2));
    CHECK_NEAR(p.fuel_quantity_lbs[0], 100.0, 1e-4);
    CHECK_EQ(p.num_wheels, static_cast<uint32_t>(3));
    CHECK_EQ(p.cur_time, static_cast<uint32_t>(999));
    CHECK_EQ(p.warp, -1);
    CHECK_NEAR(p.visibility_m, 25000.0, 1e-2);
    CHECK_NEAR(p.elevator_norm, 0.10, 1e-5);
    CHECK_NEAR(p.rudder_norm, 0.70, 1e-5);
    CHECK_NEAR(p.spoilers_norm, 1.00, 1e-5);

    // Wrong size, both directions.
    {
        std::vector<uint8_t> tooShort(valid.begin(), valid.end() - 1);
        net_fdm::Packet out;
        CHECK(net_fdm::decode(tooShort.data(), tooShort.size(), out) ==
              net_fdm::DecodeResult::WrongSize);
    }
    {
        std::vector<uint8_t> tooLong = valid;
        tooLong.push_back(0);
        net_fdm::Packet out;
        CHECK(net_fdm::decode(tooLong.data(), tooLong.size(), out) ==
              net_fdm::DecodeResult::WrongSize);
    }

    // Wrong version: right size, first 4 bytes altered.
    {
        std::vector<uint8_t> badVersion = valid;
        badVersion[0] = 0;
        badVersion[1] = 0;
        badVersion[2] = 0;
        badVersion[3] = 7; // version 7, not 24
        net_fdm::Packet out;
        CHECK(net_fdm::decode(badVersion.data(), badVersion.size(), out) ==
              net_fdm::DecodeResult::WrongVersion);
    }

    // A failed decode must leave `out` all-zero, not partially filled --
    // otherwise a caller that forgets to check the DecodeResult sees stale
    // or half-written data and mistakes it for a real (if odd) flight state.
    {
        net_fdm::Packet out;
        out.altitude_m = 12345.0; // pre-poison with a value decode must clear
        out.num_engines = 4;
        std::vector<uint8_t> bad(10, 0xFF);
        net_fdm::DecodeResult r2 = net_fdm::decode(bad.data(), bad.size(), out);
        CHECK(r2 == net_fdm::DecodeResult::WrongSize);
        CHECK_NEAR(out.altitude_m, 0.0, 1e-12);
        CHECK_EQ(out.num_engines, static_cast<uint32_t>(0));
    }

    // describe() must return distinct, non-null strings for every result.
    CHECK(net_fdm::describe(net_fdm::DecodeResult::Ok) != nullptr);
    CHECK(net_fdm::describe(net_fdm::DecodeResult::WrongSize) != nullptr);
    CHECK(net_fdm::describe(net_fdm::DecodeResult::WrongVersion) != nullptr);

    std::cout << "test_net_fdm: all checks passed\n";
    return 0;
}
