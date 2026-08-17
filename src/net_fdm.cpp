#include "net_fdm.h"

#include <cstring>

namespace net_fdm {
namespace {

// RETAINED AS REFERENCE, NOT ON THE LIVE PATH.
//
// decode() below reads FlightGear's wire format by recv()'ing/memcpy'ing
// straight into the packed FGNetFDM struct (net_fdm.h) and byte-swapping
// each field with ntoh32()/ntohf()/ntohd(). This class is the earlier,
// padding-and-alignment-agnostic implementation of the same decode: a
// sequential byte-cursor that never lays a struct over the wire bytes at
// all, so it can't be broken by a packing/alignment mistake in FGNetFDM.
// Kept here, uncalled, as documentation of that approach and as a fallback
// to reach for if the packed-struct path ever needs debugging. Nothing
// currently references it, and nothing currently tests it -- a future
// change to the wire field list only has to keep the live decode below
// correct, not this one.
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

const char* describe(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return "ok";
        case DecodeResult::WrongSize: return "wrong datagram size";
        case DecodeResult::WrongVersion: return "wrong FGNetFDM version";
    }
    return "unknown";
}

} // namespace net_fdm
