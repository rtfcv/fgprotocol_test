#include "net_fdm.h"

#include <cstring>

namespace net_fdm {
namespace {

// RETAINED AS REFERENCE, NOT ON THE LIVE PATH -- BUT EXERCISED BY TESTS.
//
// decode() below reads FlightGear's wire format by recv()'ing/memcpy'ing
// straight into the packed FGNetFDM struct (net_fdm.h) and byte-swapping
// each field with ntoh32()/ntohf()/ntohd(). This class is the earlier,
// padding-and-alignment-agnostic implementation of the same decode: a
// sequential byte-cursor that never lays a struct over the wire bytes at
// all, so it can't be broken by a packing/alignment mistake in FGNetFDM.
// main.cpp never calls it -- decodeWithBigEndianReader() below (declared in
// net_fdm.h, test-only) is the only caller, letting
// tests/test_net_fdm.cpp assert the two decoders agree field-for-field on
// the same bytes. A future change to the wire field list still has to keep
// both in sync, or that comparison test fails -- unlike a plain unused
// class, this one won't rot silently.
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

static_assert(sizeof(float) == 4, "float must be 32 bits for the FGNetFDM decode");
static_assert(sizeof(double) == 8, "double must be 64 bits for the FGNetFDM decode");

} // namespace

DecodeResult decode(const uint8_t* data, std::size_t size, Packet& out) {
    out = Packet{};

    if (size != kPacketSize) {
        return DecodeResult::WrongSize;
    }

    // memcpy, not reinterpret_cast: `data` is a caller-owned buffer (e.g.
    // UdpSocket's recv scratch space) with no alignment guarantee, and
    // FGNetFDM is packed to alignment 1 anyway, so there is nothing safe to
    // cast onto. sizeof(raw) == kPacketSize is enforced by the
    // static_assert in net_fdm.h, so this copies exactly `size` bytes.
    FGNetFDM raw;
    std::memcpy(&raw, data, sizeof(raw));
    return decode(raw, out);
}

DecodeResult decode(const FGNetFDM& raw, Packet& out) {
    Packet p;

    p.version = ntoh32(raw.version);
    // raw.padding is part of the wire layout only, never decoded.

    p.longitude_rad = ntohd(raw.longitude);
    p.latitude_rad = ntohd(raw.latitude);
    p.altitude_m = ntohd(raw.altitude);
    p.agl_m = ntohf(raw.agl);
    p.phi_rad = ntohf(raw.phi);
    p.theta_rad = ntohf(raw.theta);
    p.psi_rad = ntohf(raw.psi);
    p.alpha_rad = ntohf(raw.alpha);
    p.beta_rad = ntohf(raw.beta);

    p.phidot_rad_s = ntohf(raw.phidot);
    p.thetadot_rad_s = ntohf(raw.thetadot);
    p.psidot_rad_s = ntohf(raw.psidot);
    p.vcas_kt = ntohf(raw.vcas);
    p.climb_rate_fps = ntohf(raw.climb_rate);
    p.v_north_fps = ntohf(raw.v_north);
    p.v_east_fps = ntohf(raw.v_east);
    p.v_down_fps = ntohf(raw.v_down);
    p.v_body_u_fps = ntohf(raw.v_body_u);
    p.v_body_v_fps = ntohf(raw.v_body_v);
    p.v_body_w_fps = ntohf(raw.v_body_w);

    p.a_x_pilot_fps2 = ntohf(raw.A_X_pilot);
    p.a_y_pilot_fps2 = ntohf(raw.A_Y_pilot);
    p.a_z_pilot_fps2 = ntohf(raw.A_Z_pilot);

    p.stall_warning = ntohf(raw.stall_warning);
    p.slip_deg = ntohf(raw.slip_deg);

    p.num_engines = ntoh32(raw.num_engines);
    for (int i = 0; i < kMaxEngines; ++i) p.eng_state[i] = ntoh32(raw.eng_state[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.rpm[i] = ntohf(raw.rpm[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.fuel_flow_gph[i] = ntohf(raw.fuel_flow[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.fuel_px_psi[i] = ntohf(raw.fuel_px[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.egt_degf[i] = ntohf(raw.egt[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.cht_degf[i] = ntohf(raw.cht[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.mp_inhg[i] = ntohf(raw.mp_osi[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.tit[i] = ntohf(raw.tit[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.oil_temp_degf[i] = ntohf(raw.oil_temp[i]);
    for (int i = 0; i < kMaxEngines; ++i) p.oil_px_psi[i] = ntohf(raw.oil_px[i]);

    p.num_tanks = ntoh32(raw.num_tanks);
    for (int i = 0; i < kMaxTanks; ++i) p.fuel_quantity_lbs[i] = ntohf(raw.fuel_quantity[i]);

    p.num_wheels = ntoh32(raw.num_wheels);
    for (int i = 0; i < kMaxWheels; ++i) p.wow[i] = ntoh32(raw.wow[i]);
    for (int i = 0; i < kMaxWheels; ++i) p.gear_pos_norm[i] = ntohf(raw.gear_pos[i]);
    for (int i = 0; i < kMaxWheels; ++i) p.gear_steer_deg[i] = ntohf(raw.gear_steer[i]);
    for (int i = 0; i < kMaxWheels; ++i) p.gear_compression_norm[i] = ntohf(raw.gear_compression[i]);

    p.cur_time = ntoh32(raw.cur_time);
    p.warp = static_cast<int32_t>(ntoh32(static_cast<uint32_t>(raw.warp)));
    p.visibility_m = ntohf(raw.visibility);

    p.elevator_norm = ntohf(raw.elevator);
    p.elevator_trim_norm = ntohf(raw.elevator_trim_tab);
    p.left_flap_norm = ntohf(raw.left_flap);
    p.right_flap_norm = ntohf(raw.right_flap);
    p.left_aileron_norm = ntohf(raw.left_aileron);
    p.right_aileron_norm = ntohf(raw.right_aileron);
    p.rudder_norm = ntohf(raw.rudder);
    p.nose_wheel_norm = ntohf(raw.nose_wheel);
    p.speedbrake_norm = ntohf(raw.speedbrake);
    p.spoilers_norm = ntohf(raw.spoilers);

    // Unlike WrongSize (handled by the buffer-taking overload above), a
    // version mismatch here does NOT mean `raw`'s layout was wrong -- only
    // that the sender is a different protocol revision. `out` is populated
    // either way; the caller (main.cpp) decides whether to warn and use it.
    out = p;
    if (p.version != kVersion) {
        return DecodeResult::WrongVersion;
    }
    return DecodeResult::Ok;
}

DecodeResult decodeWithBigEndianReader(const uint8_t* data, std::size_t size, Packet& out) {
    out = Packet{};

    if (size != kPacketSize) {
        return DecodeResult::WrongSize;
    }

    BigEndianReader r(data, size);
    Packet p;

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
    if (r.ranOff() || r.pos() != kPacketSize) {
        return DecodeResult::WrongSize;
    }

    // Same WrongVersion contract as decode() above: `out` is still
    // populated so the two can be compared field-for-field regardless of
    // which DecodeResult came back.
    out = p;
    if (p.version != kVersion) {
        return DecodeResult::WrongVersion;
    }
    return DecodeResult::Ok;
}

const char* describe(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return "ok";
        case DecodeResult::WrongSize: return "wrong datagram size";
        case DecodeResult::WrongVersion: return "wrong FGNetFDM version";
    }
    return "unknown";
}

} // namespace net_fdm
