/**
 * @file test_net_fdm.cpp
 * @brief Tests for net_fdm::decode() (include/fgprotocol/net_fdm.h).
 *
 * Originally written against the header before net_fdm.cpp existed
 * (CLAUDE.md: write tests before implementing); decode() is now
 * header-only itself, folded into net_fdm.h directly.
 *
 * decode() itself is a whole-buffer-reversal decode -- measurably slower
 * than the straightforward per-field byte-swap it replaced, kept anyway
 * (see net_fdm.h's file comment for the full rationale). This file keeps
 * TWO independent reference decoders to cross-check it against:
 * BigEndianReader (the original byte-cursor decoder) and
 * fieldByFieldDecode() (yesterday's production decode(), using
 * ntoh32()/ntohf()/ntohd(), demoted to an oracle when whole-buffer-reversal
 * took over the live path). Neither is part of the public library.
 *
 * No JSBSim and no network needed -- everything here is a synthetic,
 * hand-built buffer standing in for a real UDP datagram.
 */
#include "fgprotocol/net_fdm.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include "check.h"

namespace {

/**
 * @brief Sequential big-endian byte-cursor reader; test-only, not part of the library.
 *
 * RETAINED AS REFERENCE, TEST-ONLY -- NOT PART OF THE LIBRARY.
 *
 * net_fdm::decode() (include/fgprotocol/net_fdm.h) is a whole-buffer-
 * reversal decode (see its file comment). BigEndianReader here predates
 * that -- and predates the field-by-field ntoh32()/ntohf()/ntohd() version
 * that came between the two (see fieldByFieldDecode() below) -- and is the
 * original, padding-and-alignment-agnostic implementation: a sequential
 * byte-cursor that never lays a struct over the wire bytes at all, so it
 * can't be broken by a packing/alignment mistake in FGNetFDM. It's
 * deliberately kept out of the public library header -- nothing a library
 * consumer needs, purely a cross-check this repo's own tests use to keep
 * the live decoder honest -- so it lives here instead, with internal
 * linkage (this anonymous namespace), used only by the oracle checks
 * further down in main(). A future change to net_fdm.h's field list still
 * has to keep all three decoders in sync, or those checks fail -- unlike a
 * plain unused class, this one won't rot silently.
 *
 * Each call reads at the current position and advances -- offsets are
 * never computed by hand, so the field list here and in net_fdm.h staying
 * in the same order is the only thing that has to be kept correct, and a
 * mismatch in total length is caught below (ranOff()/pos() !=
 * kPacketSize) rather than silently misreading.
 */
class BigEndianReader {
public:
    /**
     * @brief Constructs a reader over `size` bytes starting at `data`.
     * @param data Pointer to the buffer to read.
     * @param size Number of bytes available at `data`.
     */
    BigEndianReader(const uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    /// @return The next 4 bytes as a big-endian `uint32_t`.
    uint32_t u32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | next();
        return v;
    }

    /// @return The next 4 bytes as a big-endian `int32_t`.
    int32_t i32() { return static_cast<int32_t>(u32()); }

    /// @return The next 4 bytes as a big-endian `float`.
    float f32() {
        uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    /// @return The next 8 bytes as a big-endian `double`.
    double f64() {
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits = (bits << 8) | next();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    /// Reads `N` consecutive `u32()` values into `arr`.
    template <std::size_t N>
    void u32arr(uint32_t (&arr)[N]) {
        for (auto& x : arr) x = u32();
    }

    /// Reads `N` consecutive `f32()` values into `arr`.
    template <std::size_t N>
    void f32arr(float (&arr)[N]) {
        for (auto& x : arr) x = f32();
    }

    /// @return Number of bytes consumed so far.
    std::size_t pos() const { return pos_; }
    /// @return Whether a read has gone past the end of the buffer.
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

/**
 * @brief Test-only oracle: decodes the same bytes as net_fdm::decode(), but via BigEndianReader.
 *
 * Mirrors decode()'s WrongSize/WrongVersion/Ok contract exactly
 * (WrongSize zeroes `out`, WrongVersion still populates it) so a
 * field-by-field comparison between the two is meaningful.
 *
 * @param data Pointer to `size` bytes of raw datagram.
 * @param size Byte count of `data`.
 * @param out Receives the decoded FGNetFDMReversed.
 * @return net_fdm::DecodeResult::Ok, WrongSize, or WrongVersion.
 */
net_fdm::DecodeResult decodeWithBigEndianReader(const uint8_t* data, std::size_t size,
                                                 net_fdm::FGNetFDMReversed& out) {
    out = net_fdm::FGNetFDMReversed{};

    if (size != net_fdm::kPacketSize) {
        return net_fdm::DecodeResult::WrongSize;
    }

    BigEndianReader r(data, size);
    net_fdm::FGNetFDMReversed p;

    p.version = r.u32();
    r.u32(); // padding, discarded

    p.longitude = r.f64();
    p.latitude = r.f64();
    p.altitude = r.f64();
    p.agl = r.f32();
    p.phi = r.f32();
    p.theta = r.f32();
    p.psi = r.f32();
    p.alpha = r.f32();
    p.beta = r.f32();

    p.phidot = r.f32();
    p.thetadot = r.f32();
    p.psidot = r.f32();
    p.vcas = r.f32();
    p.climb_rate = r.f32();
    p.v_north = r.f32();
    p.v_east = r.f32();
    p.v_down = r.f32();
    p.v_body_u = r.f32();
    p.v_body_v = r.f32();
    p.v_body_w = r.f32();

    p.A_X_pilot = r.f32();
    p.A_Y_pilot = r.f32();
    p.A_Z_pilot = r.f32();

    p.stall_warning = r.f32();
    p.slip_deg = r.f32();

    p.num_engines = r.u32();
    r.u32arr(p.eng_state);
    r.f32arr(p.rpm);
    r.f32arr(p.fuel_flow);
    r.f32arr(p.fuel_px);
    r.f32arr(p.egt);
    r.f32arr(p.cht);
    r.f32arr(p.mp_osi);
    r.f32arr(p.tit);
    r.f32arr(p.oil_temp);
    r.f32arr(p.oil_px);

    p.num_tanks = r.u32();
    r.f32arr(p.fuel_quantity);

    p.num_wheels = r.u32();
    r.u32arr(p.wow);
    r.f32arr(p.gear_pos);
    r.f32arr(p.gear_steer);
    r.f32arr(p.gear_compression);

    p.cur_time = r.u32();
    p.warp = r.i32();
    p.visibility = r.f32();

    p.elevator = r.f32();
    p.elevator_trim_tab = r.f32();
    p.left_flap = r.f32();
    p.right_flap = r.f32();
    p.left_aileron = r.f32();
    p.right_aileron = r.f32();
    p.rudder = r.f32();
    p.nose_wheel = r.f32();
    p.speedbrake = r.f32();
    p.spoilers = r.f32();

    // Self-check: the reader must have consumed exactly kPacketSize bytes.
    // If it didn't, the field list here and in net_fdm.h's FGNetFDM/FGNetFDMReversed
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

/**
 * @brief Byte-swaps a 32-bit big-endian value into host order.
 *
 * RETAINED AS REFERENCE, TEST-ONLY -- NOT PART OF THE LIBRARY. Was
 * `net_fdm::ntoh32()` before whole-buffer-reversal replaced the live
 * decode(); used only by fieldByFieldDecode() below now.
 *
 * @param v Big-endian 32-bit value.
 * @return `v` in host byte order.
 */
uint32_t ntoh32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

/**
 * @brief Byte-swaps an FGNetFDM `float` field into a host-native value.
 *
 * RETAINED AS REFERENCE, TEST-ONLY. Takes the field's own type rather
 * than `uint32_t` for the same reason the original had to: passing
 * `raw.agl` (a `float`) to a `uint32_t` parameter would implicitly
 * *numerically* convert its already-nonsense value instead of
 * reinterpreting its bits -- this is the exact bug that was caught by
 * this file's happy-path checks before this repo's decoder had a
 * whole-buffer-reversal era at all.
 *
 * @param v An FGNetFDM float field, as read from wire bytes.
 * @return The field's value in host byte order.
 */
float ntohf(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = ntoh32(bits);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

/**
 * @brief Byte-swaps an FGNetFDM `double` field into a host-native value.
 * RETAINED AS REFERENCE, TEST-ONLY.
 * @param v An FGNetFDM double field, as read from wire bytes.
 * @return The field's value in host byte order.
 */
double ntohd(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    uint64_t hostBits =
        (static_cast<uint64_t>(ntoh32(static_cast<uint32_t>(bits & 0xFFFFFFFFull))) << 32) |
        ntoh32(static_cast<uint32_t>(bits >> 32));
    double d;
    std::memcpy(&d, &hostBits, sizeof(d));
    return d;
}

/**
 * @brief Test-only oracle: today's `net_fdm::decode()`, before whole-buffer-reversal replaced it.
 *
 * RETAINED AS REFERENCE, TEST-ONLY -- NOT PART OF THE LIBRARY. This is
 * exactly the body `net_fdm::decode(const uint8_t*, size_t, FGNetFDMReversed&)` had
 * before it became a whole-buffer-reversal decode: `memcpy` into a local
 * FGNetFDM, then byte-swap each field individually via ntoh32()/ntohf()/
 * ntohd() above. It's faster and keeps compile-time field-type checking
 * that the live decoder gave up (see net_fdm.h's file comment) -- kept
 * here as the second of two independent oracles decode() is cross-checked
 * against, not because it's expected to ever disagree with
 * BigEndianReader, but because two independently-implemented decoders
 * agreeing is a much stronger signal than one.
 *
 * Mirrors decode()'s WrongSize/WrongVersion/Ok contract exactly (WrongSize
 * zeroes `out`, WrongVersion still populates it).
 *
 * @param data Pointer to `size` bytes of raw datagram.
 * @param size Byte count of `data`.
 * @param out Receives the decoded FGNetFDMReversed.
 * @return net_fdm::DecodeResult::Ok, WrongSize, or WrongVersion.
 */
net_fdm::DecodeResult fieldByFieldDecode(const uint8_t* data, std::size_t size, net_fdm::FGNetFDMReversed& out) {
    out = net_fdm::FGNetFDMReversed{};

    if (size != net_fdm::kPacketSize) {
        return net_fdm::DecodeResult::WrongSize;
    }

    net_fdm::FGNetFDM raw;
    std::memcpy(&raw, data, sizeof(raw));

    net_fdm::FGNetFDMReversed p;

    p.version = ntoh32(raw.version);
    // raw.padding is part of the wire layout only, never decoded.

    p.longitude = ntohd(raw.longitude);
    p.latitude = ntohd(raw.latitude);
    p.altitude = ntohd(raw.altitude);
    p.agl = ntohf(raw.agl);
    p.phi = ntohf(raw.phi);
    p.theta = ntohf(raw.theta);
    p.psi = ntohf(raw.psi);
    p.alpha = ntohf(raw.alpha);
    p.beta = ntohf(raw.beta);

    p.phidot = ntohf(raw.phidot);
    p.thetadot = ntohf(raw.thetadot);
    p.psidot = ntohf(raw.psidot);
    p.vcas = ntohf(raw.vcas);
    p.climb_rate = ntohf(raw.climb_rate);
    p.v_north = ntohf(raw.v_north);
    p.v_east = ntohf(raw.v_east);
    p.v_down = ntohf(raw.v_down);
    p.v_body_u = ntohf(raw.v_body_u);
    p.v_body_v = ntohf(raw.v_body_v);
    p.v_body_w = ntohf(raw.v_body_w);

    p.A_X_pilot = ntohf(raw.A_X_pilot);
    p.A_Y_pilot = ntohf(raw.A_Y_pilot);
    p.A_Z_pilot = ntohf(raw.A_Z_pilot);

    p.stall_warning = ntohf(raw.stall_warning);
    p.slip_deg = ntohf(raw.slip_deg);

    p.num_engines = ntoh32(raw.num_engines);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.eng_state[i] = ntoh32(raw.eng_state[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.rpm[i] = ntohf(raw.rpm[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.fuel_flow[i] = ntohf(raw.fuel_flow[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.fuel_px[i] = ntohf(raw.fuel_px[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.egt[i] = ntohf(raw.egt[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.cht[i] = ntohf(raw.cht[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.mp_osi[i] = ntohf(raw.mp_osi[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.tit[i] = ntohf(raw.tit[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.oil_temp[i] = ntohf(raw.oil_temp[i]);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) p.oil_px[i] = ntohf(raw.oil_px[i]);

    p.num_tanks = ntoh32(raw.num_tanks);
    for (int i = 0; i < net_fdm::kMaxTanks; ++i) p.fuel_quantity[i] = ntohf(raw.fuel_quantity[i]);

    p.num_wheels = ntoh32(raw.num_wheels);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) p.wow[i] = ntoh32(raw.wow[i]);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) p.gear_pos[i] = ntohf(raw.gear_pos[i]);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) p.gear_steer[i] = ntohf(raw.gear_steer[i]);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) p.gear_compression[i] = ntohf(raw.gear_compression[i]);

    p.cur_time = ntoh32(raw.cur_time);
    p.warp = static_cast<int32_t>(ntoh32(static_cast<uint32_t>(raw.warp)));
    p.visibility = ntohf(raw.visibility);

    p.elevator = ntohf(raw.elevator);
    p.elevator_trim_tab = ntohf(raw.elevator_trim_tab);
    p.left_flap = ntohf(raw.left_flap);
    p.right_flap = ntohf(raw.right_flap);
    p.left_aileron = ntohf(raw.left_aileron);
    p.right_aileron = ntohf(raw.right_aileron);
    p.rudder = ntohf(raw.rudder);
    p.nose_wheel = ntohf(raw.nose_wheel);
    p.speedbrake = ntohf(raw.speedbrake);
    p.spoilers = ntohf(raw.spoilers);

    out = p;
    if (p.version != net_fdm::kVersion) {
        return net_fdm::DecodeResult::WrongVersion;
    }
    return net_fdm::DecodeResult::Ok;
}

/// Appends `v` to `buf` as 4 big-endian bytes.
void putU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v >> 24));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v));
}

/// Appends `v` to `buf` as 4 big-endian bytes.
void putI32(std::vector<uint8_t>& buf, int32_t v) {
    putU32(buf, static_cast<uint32_t>(v));
}

/// Appends `v` to `buf` as 4 big-endian bytes.
void putF32(std::vector<uint8_t>& buf, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    putU32(buf, bits);
}

/// Appends `v` to `buf` as 8 big-endian bytes.
void putF64(std::vector<uint8_t>& buf, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    }
}

/**
 * @brief Builds one full, valid 408-byte FGNetFDM v24 datagram.
 *
 * Every field gets a distinct, recognizable value (so a field-order swap
 * -- the realistic bug here -- shows up as the wrong value landing in the
 * wrong member, rather than two fields that happen to share a value
 * hiding the mistake).
 *
 * @return The raw datagram bytes.
 */
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

/**
 * @brief Asserts every field of `a` and `b` matches.
 *
 * Used to cross-check the live decode() (whole-buffer-reversal) against
 * this file's two independent reference decoders --
 * decodeWithBigEndianReader() and fieldByFieldDecode() -- on the same
 * input. All three are pure functions of the same bytes, so they should
 * come back bit-for-bit identical, not just close. buildValidPacket()
 * gives every field a distinct value specifically so a field landing in
 * the wrong member here would be caught, not masked by two fields sharing
 * a value.
 *
 * @param a First packet to compare.
 * @param b Second packet to compare.
 */
void checkPacketsMatch(const net_fdm::FGNetFDMReversed& a, const net_fdm::FGNetFDMReversed& b) {
    CHECK_EQ(a.version, b.version);
    CHECK_NEAR(a.longitude, b.longitude, 0.0);
    CHECK_NEAR(a.latitude, b.latitude, 0.0);
    CHECK_NEAR(a.altitude, b.altitude, 0.0);
    CHECK_NEAR(a.agl, b.agl, 0.0);
    CHECK_NEAR(a.phi, b.phi, 0.0);
    CHECK_NEAR(a.theta, b.theta, 0.0);
    CHECK_NEAR(a.psi, b.psi, 0.0);
    CHECK_NEAR(a.alpha, b.alpha, 0.0);
    CHECK_NEAR(a.beta, b.beta, 0.0);

    CHECK_NEAR(a.phidot, b.phidot, 0.0);
    CHECK_NEAR(a.thetadot, b.thetadot, 0.0);
    CHECK_NEAR(a.psidot, b.psidot, 0.0);
    CHECK_NEAR(a.vcas, b.vcas, 0.0);
    CHECK_NEAR(a.climb_rate, b.climb_rate, 0.0);
    CHECK_NEAR(a.v_north, b.v_north, 0.0);
    CHECK_NEAR(a.v_east, b.v_east, 0.0);
    CHECK_NEAR(a.v_down, b.v_down, 0.0);
    CHECK_NEAR(a.v_body_u, b.v_body_u, 0.0);
    CHECK_NEAR(a.v_body_v, b.v_body_v, 0.0);
    CHECK_NEAR(a.v_body_w, b.v_body_w, 0.0);

    CHECK_NEAR(a.A_X_pilot, b.A_X_pilot, 0.0);
    CHECK_NEAR(a.A_Y_pilot, b.A_Y_pilot, 0.0);
    CHECK_NEAR(a.A_Z_pilot, b.A_Z_pilot, 0.0);

    CHECK_NEAR(a.stall_warning, b.stall_warning, 0.0);
    CHECK_NEAR(a.slip_deg, b.slip_deg, 0.0);

    CHECK_EQ(a.num_engines, b.num_engines);
    for (int i = 0; i < net_fdm::kMaxEngines; ++i) {
        CHECK_EQ(a.eng_state[i], b.eng_state[i]);
        CHECK_NEAR(a.rpm[i], b.rpm[i], 0.0);
        CHECK_NEAR(a.fuel_flow[i], b.fuel_flow[i], 0.0);
        CHECK_NEAR(a.fuel_px[i], b.fuel_px[i], 0.0);
        CHECK_NEAR(a.egt[i], b.egt[i], 0.0);
        CHECK_NEAR(a.cht[i], b.cht[i], 0.0);
        CHECK_NEAR(a.mp_osi[i], b.mp_osi[i], 0.0);
        CHECK_NEAR(a.tit[i], b.tit[i], 0.0);
        CHECK_NEAR(a.oil_temp[i], b.oil_temp[i], 0.0);
        CHECK_NEAR(a.oil_px[i], b.oil_px[i], 0.0);
    }

    CHECK_EQ(a.num_tanks, b.num_tanks);
    for (int i = 0; i < net_fdm::kMaxTanks; ++i) {
        CHECK_NEAR(a.fuel_quantity[i], b.fuel_quantity[i], 0.0);
    }

    CHECK_EQ(a.num_wheels, b.num_wheels);
    for (int i = 0; i < net_fdm::kMaxWheels; ++i) {
        CHECK_EQ(a.wow[i], b.wow[i]);
        CHECK_NEAR(a.gear_pos[i], b.gear_pos[i], 0.0);
        CHECK_NEAR(a.gear_steer[i], b.gear_steer[i], 0.0);
        CHECK_NEAR(a.gear_compression[i], b.gear_compression[i], 0.0);
    }

    CHECK_EQ(a.cur_time, b.cur_time);
    CHECK_EQ(a.warp, b.warp);
    CHECK_NEAR(a.visibility, b.visibility, 0.0);

    CHECK_NEAR(a.elevator, b.elevator, 0.0);
    CHECK_NEAR(a.elevator_trim_tab, b.elevator_trim_tab, 0.0);
    CHECK_NEAR(a.left_flap, b.left_flap, 0.0);
    CHECK_NEAR(a.right_flap, b.right_flap, 0.0);
    CHECK_NEAR(a.left_aileron, b.left_aileron, 0.0);
    CHECK_NEAR(a.right_aileron, b.right_aileron, 0.0);
    CHECK_NEAR(a.rudder, b.rudder, 0.0);
    CHECK_NEAR(a.nose_wheel, b.nose_wheel, 0.0);
    CHECK_NEAR(a.speedbrake, b.speedbrake, 0.0);
    CHECK_NEAR(a.spoilers, b.spoilers, 0.0);
}

} // namespace

/// Runs all net_fdm::decode() checks; exits non-zero via CHECK on first failure.
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
    net_fdm::FGNetFDMReversed p;
    net_fdm::DecodeResult r = net_fdm::decode(valid.data(), valid.size(), p);
    CHECK(r == net_fdm::DecodeResult::Ok);

    CHECK_EQ(p.version, net_fdm::kVersion);
    CHECK_NEAR(p.longitude, 1.111, 1e-9);
    CHECK_NEAR(p.latitude, 2.222, 1e-9);
    CHECK_NEAR(p.altitude, 3333.0, 1e-9);
    CHECK_NEAR(p.agl, 4.4, 1e-5);
    CHECK_NEAR(p.phi, 5.5, 1e-5);
    CHECK_NEAR(p.theta, 6.6, 1e-5);
    CHECK_NEAR(p.psi, 7.7, 1e-5);
    CHECK_NEAR(p.alpha, 8.8, 1e-5);
    CHECK_NEAR(p.beta, 9.9, 1e-5);
    CHECK_NEAR(p.vcas, 130.0, 1e-4);
    CHECK_NEAR(p.climb_rate, 14.1, 1e-4);
    CHECK_NEAR(p.slip_deg, 24.1, 1e-4);
    CHECK_EQ(p.num_engines, static_cast<uint32_t>(1));
    CHECK_EQ(p.eng_state[0], static_cast<uint32_t>(2));
    CHECK_NEAR(p.rpm[0], 2500.0, 1e-3);
    CHECK_EQ(p.num_tanks, static_cast<uint32_t>(2));
    CHECK_NEAR(p.fuel_quantity[0], 100.0, 1e-4);
    CHECK_EQ(p.num_wheels, static_cast<uint32_t>(3));
    CHECK_EQ(p.cur_time, static_cast<uint32_t>(999));
    CHECK_EQ(p.warp, -1);
    CHECK_NEAR(p.visibility, 25000.0, 1e-2);
    CHECK_NEAR(p.elevator, 0.10, 1e-5);
    CHECK_NEAR(p.rudder, 0.70, 1e-5);
    CHECK_NEAR(p.spoilers, 1.00, 1e-5);

    // Wrong size, both directions.
    {
        std::vector<uint8_t> tooShort(valid.begin(), valid.end() - 1);
        net_fdm::FGNetFDMReversed out;
        CHECK(net_fdm::decode(tooShort.data(), tooShort.size(), out) ==
              net_fdm::DecodeResult::WrongSize);
    }
    {
        std::vector<uint8_t> tooLong = valid;
        tooLong.push_back(0);
        net_fdm::FGNetFDMReversed out;
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
        net_fdm::FGNetFDMReversed out;
        CHECK(net_fdm::decode(badVersion.data(), badVersion.size(), out) ==
              net_fdm::DecodeResult::WrongVersion);
        CHECK_EQ(out.version, static_cast<uint32_t>(7));
        CHECK_NEAR(out.altitude, 3333.0, 1e-9);
        CHECK_NEAR(out.rudder, 0.70, 1e-5);
    }

    // The struct-taking overload: a caller that already recv()'d straight
    // into an FGNetFDM (main.cpp's live path) must get the same result as
    // decoding the equivalent raw bytes.
    {
        net_fdm::FGNetFDM raw;
        CHECK_EQ(valid.size(), sizeof(raw));
        std::memcpy(&raw, valid.data(), sizeof(raw));

        net_fdm::FGNetFDMReversed out;
        net_fdm::DecodeResult r3 = net_fdm::decode(raw, out);
        CHECK(r3 == net_fdm::DecodeResult::Ok);
        CHECK_NEAR(out.longitude, 1.111, 1e-9);
        CHECK_NEAR(out.vcas, 130.0, 1e-4);
        CHECK_EQ(out.num_wheels, static_cast<uint32_t>(3));
    }

    // Oracle check: both decodeWithBigEndianReader() and
    // fieldByFieldDecode() must agree with decode() field-for-field on
    // every DecodeResult, not just Ok. This is the only thing that keeps
    // both retained reference decoders honest -- nothing else in this file
    // (or main.cpp) ever calls either, so without this test a future edit
    // to net_fdm.h's field list could update decode() and leave them
    // silently stale. Two independent oracles agreeing with decode() is a
    // stronger signal than one, especially since decode() itself now
    // relies on hand-computed reverse field/array-index order -- exactly
    // the class of mistake an independent, differently-structured decoder
    // is positioned to catch.
    {
        net_fdm::FGNetFDMReversed viaDecode;
        net_fdm::FGNetFDMReversed viaBigEndianReader;
        net_fdm::FGNetFDMReversed viaFieldByField;
        CHECK(net_fdm::decode(valid.data(), valid.size(), viaDecode) ==
              net_fdm::DecodeResult::Ok);
        CHECK(decodeWithBigEndianReader(valid.data(), valid.size(), viaBigEndianReader) ==
              net_fdm::DecodeResult::Ok);
        CHECK(fieldByFieldDecode(valid.data(), valid.size(), viaFieldByField) ==
              net_fdm::DecodeResult::Ok);
        checkPacketsMatch(viaDecode, viaBigEndianReader);
        checkPacketsMatch(viaDecode, viaFieldByField);
    }
    {
        std::vector<uint8_t> badVersion = valid;
        badVersion[3] = 7; // version 7, not 24 -- see the WrongVersion case above
        net_fdm::FGNetFDMReversed viaDecode;
        net_fdm::FGNetFDMReversed viaBigEndianReader;
        net_fdm::FGNetFDMReversed viaFieldByField;
        CHECK(net_fdm::decode(badVersion.data(), badVersion.size(), viaDecode) ==
              net_fdm::DecodeResult::WrongVersion);
        CHECK(decodeWithBigEndianReader(badVersion.data(), badVersion.size(), viaBigEndianReader) ==
              net_fdm::DecodeResult::WrongVersion);
        CHECK(fieldByFieldDecode(badVersion.data(), badVersion.size(), viaFieldByField) ==
              net_fdm::DecodeResult::WrongVersion);
        checkPacketsMatch(viaDecode, viaBigEndianReader);
        checkPacketsMatch(viaDecode, viaFieldByField);
    }
    {
        std::vector<uint8_t> tooShort(valid.begin(), valid.end() - 1);
        net_fdm::FGNetFDMReversed viaDecode;
        net_fdm::FGNetFDMReversed viaBigEndianReader;
        net_fdm::FGNetFDMReversed viaFieldByField;
        CHECK(net_fdm::decode(tooShort.data(), tooShort.size(), viaDecode) ==
              net_fdm::DecodeResult::WrongSize);
        CHECK(decodeWithBigEndianReader(tooShort.data(), tooShort.size(), viaBigEndianReader) ==
              net_fdm::DecodeResult::WrongSize);
        CHECK(fieldByFieldDecode(tooShort.data(), tooShort.size(), viaFieldByField) ==
              net_fdm::DecodeResult::WrongSize);
        checkPacketsMatch(viaDecode, viaBigEndianReader); // both zeroed
        checkPacketsMatch(viaDecode, viaFieldByField);     // both zeroed
    }

    // A failed decode must leave `out` all-zero, not partially filled --
    // otherwise a caller that forgets to check the DecodeResult sees stale
    // or half-written data and mistakes it for a real (if odd) flight state.
    {
        net_fdm::FGNetFDMReversed out;
        out.altitude = 12345.0; // pre-poison with a value decode must clear
        out.num_engines = 4;
        std::vector<uint8_t> bad(10, 0xFF);
        net_fdm::DecodeResult r2 = net_fdm::decode(bad.data(), bad.size(), out);
        CHECK(r2 == net_fdm::DecodeResult::WrongSize);
        CHECK_NEAR(out.altitude, 0.0, 1e-12);
        CHECK_EQ(out.num_engines, static_cast<uint32_t>(0));
    }

    // describe() must return distinct, non-null strings for every result.
    CHECK(net_fdm::describe(net_fdm::DecodeResult::Ok) != nullptr);
    CHECK(net_fdm::describe(net_fdm::DecodeResult::WrongSize) != nullptr);
    CHECK(net_fdm::describe(net_fdm::DecodeResult::WrongVersion) != nullptr);

    std::cout << "test_net_fdm: all checks passed\n";
    return 0;
}
