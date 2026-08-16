#include "net_fdm.h"

#include <cstring>

namespace net_fdm {
namespace {

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
    // If it didn't, the field list above doesn't match net_fdm.h's byte
    // count and every offset past the mismatch is wrong -- treat that as a
    // decode failure rather than trusting partially-misaligned data.
    if (r.ranOff() || r.pos() != kPacketSize) {
        return DecodeResult::WrongSize;
    }

    if (p.version != kVersion) {
        return DecodeResult::WrongVersion;
    }

    out = p;
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
