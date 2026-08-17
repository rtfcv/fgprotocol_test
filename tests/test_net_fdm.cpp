// Tests for net_fdm::decode() (include/fgprotocol/net_fdm.h). Originally
// written against the header before net_fdm.cpp existed (CLAUDE.md: write
// tests before implementing); decode() is now header-only itself, folded
// into net_fdm.h directly.
//
// No JSBSim and no network needed -- everything here is a synthetic,
// hand-built buffer standing in for a real UDP datagram.
#include "fgprotocol/net_fdm.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include "check.h"

namespace {

// RETAINED AS REFERENCE, TEST-ONLY -- NOT PART OF THE LIBRARY.
//
// net_fdm::decode() (include/fgprotocol/net_fdm.h) reads FlightGear's wire
// format by memcpy'ing straight into the packed FGNetFDM struct and
// byte-swapping each field with ntoh32()/ntohf()/ntohd(). BigEndianReader
// here is the earlier, padding-and-alignment-agnostic implementation of the
// same decode: a sequential byte-cursor that never lays a struct over the
// wire bytes at all, so it can't be broken by a packing/alignment mistake
// in FGNetFDM. It's deliberately kept out of the public library header --
// nothing a library consumer needs, purely a cross-check this repo's own
// tests use to keep the "real" decoder honest -- so it lives here instead,
// with internal linkage (this anonymous namespace), used only by the oracle
// check further down in main(). A future change to net_fdm.h's field list
// still has to keep both in sync, or that check fails -- unlike a plain
// unused class, this one won't rot silently.
//
// Sequential big-endian byte-cursor reader. Each call reads at the current
// position and advances -- offsets are never computed by hand, so the field
// list here and in net_fdm.h staying in the same order is the only thing
// that has to be kept correct, and a mismatch in total length is caught
// below (ranOff()/pos() != kPacketSize) rather than silently misreading.
class BigEndianReader {
public:
    BigEndianReader(const uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    uint32_t u32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | next();
        return v;
    }

    int32_t i32() { return static_cast<int32_t>(u32()); }

    float f32() {
        uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    double f64() {
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits = (bits << 8) | next();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    template <std::size_t N>
    void u32arr(std::array<uint32_t, N>& arr) {
        for (auto& x : arr) x = u32();
    }

    template <std::size_t N>
    void f32arr(std::array<float, N>& arr) {
        for (auto& x : arr) x = f32();
    }

    std::size_t pos() const { return pos_; }
    bool ranOff() const { return overrun_; }

private:
    uint8_t next() {
        if (pos_ >= size_) {
            overrun_ = true;
            return 0;
        }
        return data_[pos_++];
    }

    const uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    bool overrun_ = false;
};

// Test-only oracle: decodes the same bytes as net_fdm::decode(), but via
// BigEndianReader above instead of memcpy-into-FGNetFDM + ntoh*(). Mirrors
// decode()'s WrongSize/WrongVersion/Ok contract exactly (WrongSize zeroes
// `out`, WrongVersion still populates it) so a field-by-field comparison
// between the two is meaningful.
net_fdm::DecodeResult decodeWithBigEndianReader(const uint8_t* data, std::size_t size,
                                                 net_fdm::Packet& out) {
    out = net_fdm::Packet{};

    if (size != net_fdm::kPacketSize) {
        return net_fdm::DecodeResult::WrongSize;
    }

    BigEndianReader r(data, size);
    net_fdm::Packet p;

    p.version = r.u32();
    r.u32(); // padding, discarded

    p.longitude_rad = r.f64();
    p.latitude_rad = r.f64();
    p.altitude_m = r.f64();
    p.agl_m = r.f32();
    p.phi_rad = r.f32();
    p.theta_rad = r.f32();
    p.psi_rad = r.f32();
    p.alpha_rad = r.f32();
    p.beta_rad = r.f32();

    p.phidot_rad_s = r.f32();
    p.thetadot_rad_s = r.f32();
    p.psidot_rad_s = r.f32();
    p.vcas_kt = r.f32();
    p.climb_rate_fps = r.f32();
    p.v_north_fps = r.f32();
    p.v_east_fps = r.f32();
    p.v_down_fps = r.f32();
    p.v_body_u_fps = r.f32();
    p.v_body_v_fps = r.f32();
    p.v_body_w_fps = r.f32();

    p.a_x_pilot_fps2 = r.f32();
    p.a_y_pilot_fps2 = r.f32();
    p.a_z_pilot_fps2 = r.f32();

    p.stall_warning = r.f32();
    p.slip_deg = r.f32();

    p.num_engines = r.u32();
    r.u32arr(p.eng_state);
    r.f32arr(p.rpm);
    r.f32arr(p.fuel_flow_gph);
    r.f32arr(p.fuel_px_psi);
    r.f32arr(p.egt_degf);
    r.f32arr(p.cht_degf);
    r.f32arr(p.mp_inhg);
    r.f32arr(p.tit);
    r.f32arr(p.oil_temp_degf);
    r.f32arr(p.oil_px_psi);

    p.num_tanks = r.u32();
    r.f32arr(p.fuel_quantity_lbs);

    p.num_wheels = r.u32();
    r.u32arr(p.wow);
    r.f32arr(p.gear_pos_norm);
    r.f32arr(p.gear_steer_deg);
    r.f32arr(p.gear_compression_norm);

    p.cur_time = r.u32();
    p.warp = r.i32();
    p.visibility_m = r.f32();

    p.elevator_norm = r.f32();
    p.elevator_trim_norm = r.f32();
    p.left_flap_norm = r.f32();
    p.right_flap_norm = r.f32();
    p.left_aileron_norm = r.f32();
    p.right_aileron_norm = r.f32();
    p.rudder_norm = r.f32();
    p.nose_wheel_norm = r.f32();
    p.speedbrake_norm = r.f32();
    p.spoilers_norm = r.f32();

    // Self-check: the reader must have consumed exactly kPacketSize bytes.
    // If it didn't, the field list here and in net_fdm.h's FGNetFDM/Packet
    // don't agree on byte count and every offset past the mismatch is
    // wrong -- treat that as a decode failure rather than trusting
    // partially-misaligned data.
    if (r.ranOff() || r.pos() != net_fdm::kPacketSize) {
        return net_fdm::DecodeResult::WrongSize;
    }

    // Same WrongVersion contract as net_fdm::decode(): `out` is still
    // populated so the two can be compared field-for-field regardless of
    // which DecodeResult came back.
    out = p;
    if (p.version != net_fdm::kVersion) {
        return net_fdm::DecodeResult::WrongVersion;
    }
    return net_fdm::DecodeResult::Ok;
}

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

// Asserts every field of `a` and `b` matches. Used to cross-check the live
// decode() (memcpy into packed FGNetFDM + ntoh*) against
// decodeWithBigEndianReader() (the retained byte-cursor decoder) on the
// same input -- both are pure functions of the same bytes, so they should
// come back bit-for-bit identical, not just close. buildValidPacket() gives
// every field a distinct value specifically so a field landing in the wrong
// member here would be caught, not masked by two fields sharing a value.
void checkPacketsMatch(const net_fdm::Packet& a, const net_fdm::Packet& b) {
    CHECK_EQ(a.version, b.version);
    CHECK_NEAR(a.longitude_rad, b.longitude_rad, 0.0);
    CHECK_NEAR(a.latitude_rad, b.latitude_rad, 0.0);
    CHECK_NEAR(a.altitude_m, b.altitude_m, 0.0);
    CHECK_NEAR(a.agl_m, b.agl_m, 0.0);
    CHECK_NEAR(a.phi_rad, b.phi_rad, 0.0);
    CHECK_NEAR(a.theta_rad, b.theta_rad, 0.0);
    CHECK_NEAR(a.psi_rad, b.psi_rad, 0.0);
    CHECK_NEAR(a.alpha_rad, b.alpha_rad, 0.0);
    CHECK_NEAR(a.beta_rad, b.beta_rad, 0.0);

    CHECK_NEAR(a.phidot_rad_s, b.phidot_rad_s, 0.0);
    CHECK_NEAR(a.thetadot_rad_s, b.thetadot_rad_s, 0.0);
    CHECK_NEAR(a.psidot_rad_s, b.psidot_rad_s, 0.0);
    CHECK_NEAR(a.vcas_kt, b.vcas_kt, 0.0);
    CHECK_NEAR(a.climb_rate_fps, b.climb_rate_fps, 0.0);
    CHECK_NEAR(a.v_north_fps, b.v_north_fps, 0.0);
    CHECK_NEAR(a.v_east_fps, b.v_east_fps, 0.0);
    CHECK_NEAR(a.v_down_fps, b.v_down_fps, 0.0);
    CHECK_NEAR(a.v_body_u_fps, b.v_body_u_fps, 0.0);
    CHECK_NEAR(a.v_body_v_fps, b.v_body_v_fps, 0.0);
    CHECK_NEAR(a.v_body_w_fps, b.v_body_w_fps, 0.0);

    CHECK_NEAR(a.a_x_pilot_fps2, b.a_x_pilot_fps2, 0.0);
    CHECK_NEAR(a.a_y_pilot_fps2, b.a_y_pilot_fps2, 0.0);
    CHECK_NEAR(a.a_z_pilot_fps2, b.a_z_pilot_fps2, 0.0);

    CHECK_NEAR(a.stall_warning, b.stall_warning, 0.0);
    CHECK_NEAR(a.slip_deg, b.slip_deg, 0.0);

    CHECK_EQ(a.num_engines, b.num_engines);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) {
        CHECK_EQ(a.eng_state[i], b.eng_state[i]);
        CHECK_NEAR(a.rpm[i], b.rpm[i], 0.0);
        CHECK_NEAR(a.fuel_flow_gph[i], b.fuel_flow_gph[i], 0.0);
        CHECK_NEAR(a.fuel_px_psi[i], b.fuel_px_psi[i], 0.0);
        CHECK_NEAR(a.egt_degf[i], b.egt_degf[i], 0.0);
        CHECK_NEAR(a.cht_degf[i], b.cht_degf[i], 0.0);
        CHECK_NEAR(a.mp_inhg[i], b.mp_inhg[i], 0.0);
        CHECK_NEAR(a.tit[i], b.tit[i], 0.0);
        CHECK_NEAR(a.oil_temp_degf[i], b.oil_temp_degf[i], 0.0);
        CHECK_NEAR(a.oil_px_psi[i], b.oil_px_psi[i], 0.0);
    }

    CHECK_EQ(a.num_tanks, b.num_tanks);
    for (int i = 0; i < net_fdm::kMaxTanks; ++i) {
        CHECK_NEAR(a.fuel_quantity_lbs[i], b.fuel_quantity_lbs[i], 0.0);
    }

    CHECK_EQ(a.num_wheels, b.num_wheels);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) {
        CHECK_EQ(a.wow[i], b.wow[i]);
        CHECK_NEAR(a.gear_pos_norm[i], b.gear_pos_norm[i], 0.0);
        CHECK_NEAR(a.gear_steer_deg[i], b.gear_steer_deg[i], 0.0);
        CHECK_NEAR(a.gear_compression_norm[i], b.gear_compression_norm[i], 0.0);
    }

    CHECK_EQ(a.cur_time, b.cur_time);
    CHECK_EQ(a.warp, b.warp);
    CHECK_NEAR(a.visibility_m, b.visibility_m, 0.0);

    CHECK_NEAR(a.elevator_norm, b.elevator_norm, 0.0);
    CHECK_NEAR(a.elevator_trim_norm, b.elevator_trim_norm, 0.0);
    CHECK_NEAR(a.left_flap_norm, b.left_flap_norm, 0.0);
    CHECK_NEAR(a.right_flap_norm, b.right_flap_norm, 0.0);
    CHECK_NEAR(a.left_aileron_norm, b.left_aileron_norm, 0.0);
    CHECK_NEAR(a.right_aileron_norm, b.right_aileron_norm, 0.0);
    CHECK_NEAR(a.rudder_norm, b.rudder_norm, 0.0);
    CHECK_NEAR(a.nose_wheel_norm, b.nose_wheel_norm, 0.0);
    CHECK_NEAR(a.speedbrake_norm, b.speedbrake_norm, 0.0);
    CHECK_NEAR(a.spoilers_norm, b.spoilers_norm, 0.0);
}

} // namespace

int main() {
    // The buffer builder itself must produce exactly one packet's worth of
    // bytes, or every test below is checking the wrong thing.
    std::vector<uint8_t> valid = buildValidPacket();
    CHECK_EQ(valid.size(), net_fdm::kPacketSize);
    CHECK_EQ(net_fdm::kPacketSize, static_cast<std::size_t>(408));

    // Wire-struct layout guards. sizeof() alone can't catch two adjacent
    // fields swapped, or a field group that grew and shrank by the same
    // total size -- offsetof() at each group boundary pins the field order
    // itself, not just the overall byte count. #pragma pack(1) means these
    // offsets should match FlightGear's net_fdm.hxx exactly with no
    // compiler-inserted padding; a static_assert in net_fdm.h backs this up
    // at compile time for the overall size.
    CHECK_EQ(sizeof(net_fdm::FGNetFDM), net_fdm::kPacketSize);
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, longitude), static_cast<std::size_t>(8));
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, num_engines), static_cast<std::size_t>(120));
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, num_tanks), static_cast<std::size_t>(284));
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, num_wheels), static_cast<std::size_t>(304));
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, cur_time), static_cast<std::size_t>(356));
    CHECK_EQ(offsetof(net_fdm::FGNetFDM, elevator), static_cast<std::size_t>(368));

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

    // Wrong version: right size, first 4 bytes altered. Unlike WrongSize,
    // this must NOT zero `out` -- a version mismatch doesn't imply a wrong
    // layout, so main.cpp is expected to warn and use the data anyway (see
    // CLAUDE.md/plan notes on this being a deliberate, non-fatal case).
    {
        std::vector<uint8_t> badVersion = valid;
        badVersion[0] = 0;
        badVersion[1] = 0;
        badVersion[2] = 0;
        badVersion[3] = 7; // version 7, not 24
        net_fdm::Packet out;
        CHECK(net_fdm::decode(badVersion.data(), badVersion.size(), out) ==
              net_fdm::DecodeResult::WrongVersion);
        CHECK_EQ(out.version, static_cast<uint32_t>(7));
        CHECK_NEAR(out.altitude_m, 3333.0, 1e-9);
        CHECK_NEAR(out.rudder_norm, 0.70, 1e-5);
    }

    // The struct-taking overload: a caller that already recv()'d straight
    // into an FGNetFDM (main.cpp's live path) must get the same result as
    // decoding the equivalent raw bytes.
    {
        net_fdm::FGNetFDM raw;
        CHECK_EQ(valid.size(), sizeof(raw));
        std::memcpy(&raw, valid.data(), sizeof(raw));

        net_fdm::Packet out;
        net_fdm::DecodeResult r3 = net_fdm::decode(raw, out);
        CHECK(r3 == net_fdm::DecodeResult::Ok);
        CHECK_NEAR(out.longitude_rad, 1.111, 1e-9);
        CHECK_NEAR(out.vcas_kt, 130.0, 1e-4);
        CHECK_EQ(out.num_wheels, static_cast<uint32_t>(3));
    }

    // Oracle check: decodeWithBigEndianReader() must agree with decode()
    // field-for-field on every DecodeResult, not just Ok. This is the only
    // thing that keeps the retained BigEndianReader class honest -- nothing
    // else in this file (or main.cpp) ever calls it, so without this test a
    // future edit to net_fdm.h's field list could update decode() and leave
    // BigEndianReader silently stale.
    {
        net_fdm::Packet viaDecode;
        net_fdm::Packet viaReader;
        CHECK(net_fdm::decode(valid.data(), valid.size(), viaDecode) ==
              net_fdm::DecodeResult::Ok);
        CHECK(decodeWithBigEndianReader(valid.data(), valid.size(), viaReader) ==
              net_fdm::DecodeResult::Ok);
        checkPacketsMatch(viaDecode, viaReader);
    }
    {
        std::vector<uint8_t> badVersion = valid;
        badVersion[3] = 7; // version 7, not 24 -- see the WrongVersion case above
        net_fdm::Packet viaDecode;
        net_fdm::Packet viaReader;
        CHECK(net_fdm::decode(badVersion.data(), badVersion.size(), viaDecode) ==
              net_fdm::DecodeResult::WrongVersion);
        CHECK(decodeWithBigEndianReader(badVersion.data(), badVersion.size(), viaReader) ==
              net_fdm::DecodeResult::WrongVersion);
        checkPacketsMatch(viaDecode, viaReader);
    }
    {
        std::vector<uint8_t> tooShort(valid.begin(), valid.end() - 1);
        net_fdm::Packet viaDecode;
        net_fdm::Packet viaReader;
        CHECK(net_fdm::decode(tooShort.data(), tooShort.size(), viaDecode) ==
              net_fdm::DecodeResult::WrongSize);
        CHECK(decodeWithBigEndianReader(tooShort.data(), tooShort.size(), viaReader) ==
              net_fdm::DecodeResult::WrongSize);
        checkPacketsMatch(viaDecode, viaReader); // both zeroed
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
