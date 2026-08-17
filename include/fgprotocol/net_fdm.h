/**
 * @file net_fdm.h
 * @brief FlightGear's native-fdm wire format ("FGNetFDM"), protocol version 24.
 *
 * Defined by FlightGear's `src/Network/net_fdm.hxx`; JSBSim's
 * `<output type="FLIGHTGEAR" protocol="UDP">`
 * (`src/input_output/FGOutputFG.cpp`) serializes exactly this layout, every
 * field big-endian, no host padding on the wire. This is the same format the
 * fginst project's `NetFdm.h` documents; field order/types/units are
 * reproduced here rather than re-derived.
 *
 * Header-only, dependency-free (just `<array>`/`<cstddef>`/`<cstdint>`/
 * `<cstring>`): deliberately so this file (and control_wire.h alongside it)
 * can be reused outside this repo -- copy-pasted into another project's
 * include tree, or pulled in via add_subdirectory()/FetchContent against
 * this directory's own CMakeLists.txt. It knows nothing about sockets,
 * timing, or what a caller wants to do with a decoded Packet -- see the
 * jsbsim_tester project's own `src/` for that (`udp_socket.h`/`.cpp`,
 * `main.cpp`).
 */
#ifndef FGPROTOCOL_NET_FDM_H
#define FGPROTOCOL_NET_FDM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

/// FlightGear/JSBSim FGNetFDM wire protocol: decode-only, header-only, no
/// platform dependencies.
namespace net_fdm {

/// FGNetFDM protocol version this library decodes. A mismatch is reported
/// via DecodeResult::WrongVersion but does not stop the decode.
constexpr uint32_t kVersion = 24;
/// Exact size in bytes of one FGNetFDM datagram on the wire.
constexpr std::size_t kPacketSize = 408;

constexpr int kMaxEngines = 4; ///< Number of engine slots in FGNetFDM.
constexpr int kMaxWheels = 3;  ///< Number of wheel slots in FGNetFDM.
constexpr int kMaxTanks = 4;   ///< Number of fuel tank slots in FGNetFDM.

/**
 * @brief The wire struct: FlightGear's FGNetFDM, byte-for-byte, big-endian, no compiler padding.
 *
 * This is what `recv()` writes into directly on the live path (see the
 * jsbsim_tester project's `UdpSocket::recvInto()` / `main.cpp`) -- field
 * order, types and counts below must match FlightGear's
 * `src/Network/net_fdm.hxx` exactly, or every field past the mismatch reads
 * as garbage. `#pragma pack(1)` removes the alignment padding a normal
 * struct would get (the doubles in particular would otherwise be padded to
 * 8-byte alignment), matching FlightGear's own wire layout. The
 * static_assert() below is the actual safety net: if this struct's field
 * list ever drifts from `net_fdm.hxx`, the build fails here with "FGNetFDM
 * layout mismatch" instead of the program silently decoding garbage at
 * runtime.
 *
 * @note All multi-byte fields are big-endian on the wire; use ntoh32(),
 * ntohf(), ntohd() (or decode()) to get host-native values.
 */
#pragma pack(push, 1)
struct FGNetFDM {
    uint32_t version; ///< Protocol version; compare against kVersion.

    /**
     * 4 bytes of wire padding. Keeps the following doubles 8-byte aligned in
     * FlightGear's own (unpacked) struct; carried here only so the byte
     * layout matches, not for alignment (this struct is packed to 1).
     */
    uint32_t padding;

    double longitude;  ///< Radians.
    double latitude;   ///< Radians.
    double altitude;   ///< Meters, above sea level.
    float agl;         ///< Meters, above ground level.
    float phi;         ///< Roll, radians.
    float theta;       ///< Pitch, radians.
    float psi;         ///< Yaw / true heading, radians.
    float alpha;       ///< Angle of attack, radians.
    float beta;        ///< Sideslip, radians.

    float phidot;      ///< Roll rate, radians/sec.
    float thetadot;    ///< Pitch rate, radians/sec.
    float psidot;      ///< Yaw rate, radians/sec.
    float vcas;        ///< Calibrated airspeed, knots.
    float climb_rate;  ///< Feet per second.
    float v_north;     ///< Feet per second.
    float v_east;      ///< Feet per second.
    float v_down;      ///< Feet per second.
    float v_body_u;    ///< Feet per second.
    float v_body_v;    ///< Feet per second.
    float v_body_w;    ///< Feet per second.

    float A_X_pilot;   ///< Feet per second^2.
    float A_Y_pilot;   ///< Feet per second^2.
    float A_Z_pilot;   ///< Feet per second^2.

    float stall_warning; ///< 0..1.
    float slip_deg;      ///< Degrees.

    uint32_t num_engines;              ///< Number of valid entries in the engine arrays below.
    uint32_t eng_state[kMaxEngines];   ///< Per-engine running state.
    float rpm[kMaxEngines];            ///< Per-engine RPM.
    float fuel_flow[kMaxEngines];      ///< Per-engine fuel flow, gal/hr.
    float fuel_px[kMaxEngines];        ///< Per-engine fuel pressure, psi.
    float egt[kMaxEngines];            ///< Per-engine exhaust gas temperature, degF.
    float cht[kMaxEngines];            ///< Per-engine cylinder head temperature, degF.
    float mp_osi[kMaxEngines];         ///< Per-engine manifold pressure, inHg.
    float tit[kMaxEngines];            ///< Per-engine turbine inlet temperature.
    float oil_temp[kMaxEngines];       ///< Per-engine oil temperature, degF.
    float oil_px[kMaxEngines];         ///< Per-engine oil pressure, psi.

    uint32_t num_tanks;                ///< Number of valid entries in fuel_quantity.
    float fuel_quantity[kMaxTanks];    ///< Per-tank fuel quantity, lbs.

    uint32_t num_wheels;                    ///< Number of valid entries in the wheel arrays below.
    uint32_t wow[kMaxWheels];               ///< Per-wheel weight-on-wheels flag.
    float gear_pos[kMaxWheels];             ///< Per-wheel gear position, 0..1.
    float gear_steer[kMaxWheels];           ///< Per-wheel steering angle, degrees.
    float gear_compression[kMaxWheels];     ///< Per-wheel strut compression, 0..1.

    uint32_t cur_time; ///< Unix time, seconds.
    int32_t warp;      ///< Simulator time warp factor.
    float visibility;  ///< Meters.

    float elevator;            ///< -1..1.
    float elevator_trim_tab;   ///< -1..1.
    float left_flap;           ///< -1..1.
    float right_flap;          ///< -1..1.
    float left_aileron;        ///< -1..1.
    float right_aileron;       ///< -1..1.
    float rudder;              ///< -1..1.
    float nose_wheel;          ///< -1..1.
    float speedbrake;          ///< -1..1.
    float spoilers;            ///< -1..1.
};
#pragma pack(pop)

// Layout guards: fail the build, not a live run, if FGNetFDM ever drifts
// from FlightGear's net_fdm.hxx or this platform's basic type sizes.
static_assert(sizeof(FGNetFDM) == kPacketSize, "FGNetFDM layout mismatch");
static_assert(sizeof(float) == 4, "float must be 32 bits for the FGNetFDM decode");
static_assert(sizeof(double) == 8, "double must be 64 bits for the FGNetFDM decode");

/**
 * @brief Byte-swaps a 32-bit big-endian value into host order.
 *
 * Hand-written -- deliberately not ntohl() from `<winsock2.h>`/
 * `<arpa/inet.h>`: pulling either header into this file would force every
 * consumer (including a build with no sockets at all -- the whole point of
 * this being a standalone header) to link against a platform's socket
 * library just to decode a struct.
 *
 * @param v Big-endian 32-bit value.
 * @return `v` in host byte order.
 */
inline uint32_t ntoh32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

/**
 * @brief Byte-swaps an FGNetFDM `float` field into a host-native value.
 *
 * Takes the FGNetFDM field itself (a `float` already holding the raw
 * big-endian bit pattern, reinterpreted -- meaninglessly, as far as its
 * numeric value goes -- as a native float by simply being a struct member).
 * Deliberately NOT a `uint32_t` parameter: passing `raw.agl` (a `float`) to
 * a `uint32_t` parameter would implicitly *numerically* convert the
 * float's already-nonsense value to an integer -- rounding toward zero,
 * saturating, or invoking UB on an out-of-range/NaN value -- instead of
 * reinterpreting its bits, silently corrupting every field. `memcpy` is
 * what actually gets at the bits.
 *
 * @param v An FGNetFDM float field, as read from wire bytes.
 * @return The field's value in host byte order.
 */
inline float ntohf(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = ntoh32(bits);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

/**
 * @brief Byte-swaps an FGNetFDM `double` field into a host-native value.
 * @param v An FGNetFDM double field, as read from wire bytes.
 * @return The field's value in host byte order.
 * @see ntohf() for why this takes the field's own type rather than a raw
 * integer type.
 */
inline double ntohd(double v) {
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
 * @brief The decoded, host-native, unit-labeled form callers actually want to work with.
 *
 * Field names carry their units so a caller reading `climb_rate_fps` can't
 * mistake it for m/s, etc.
 */
struct Packet {
    uint32_t version = 0; ///< Protocol version read from the datagram.
    // 4 bytes of padding on the wire, not stored here.

    double longitude_rad = 0.0; ///< Radians.
    double latitude_rad = 0.0;  ///< Radians.
    double altitude_m = 0.0;    ///< Above sea level, METERS (not feet).
    float agl_m = 0.0f;         ///< Above ground level, meters.
    float phi_rad = 0.0f;       ///< Roll, radians.
    float theta_rad = 0.0f;     ///< Pitch, radians.
    float psi_rad = 0.0f;       ///< Yaw / true heading, radians.
    float alpha_rad = 0.0f;     ///< Angle of attack, radians.
    float beta_rad = 0.0f;      ///< Sideslip, radians.

    float phidot_rad_s = 0.0f;    ///< Roll rate, radians/sec.
    float thetadot_rad_s = 0.0f;  ///< Pitch rate, radians/sec.
    float psidot_rad_s = 0.0f;    ///< Yaw rate, radians/sec.
    float vcas_kt = 0.0f;         ///< Calibrated airspeed, knots.
    float climb_rate_fps = 0.0f;  ///< Feet per second (not m/s).
    float v_north_fps = 0.0f;     ///< Feet per second.
    float v_east_fps = 0.0f;      ///< Feet per second.
    float v_down_fps = 0.0f;      ///< Feet per second.
    float v_body_u_fps = 0.0f;    ///< Feet per second.
    float v_body_v_fps = 0.0f;    ///< Feet per second.
    float v_body_w_fps = 0.0f;    ///< Feet per second.

    float a_x_pilot_fps2 = 0.0f; ///< Feet per second^2.
    float a_y_pilot_fps2 = 0.0f; ///< Feet per second^2.
    float a_z_pilot_fps2 = 0.0f; ///< Feet per second^2.

    float stall_warning = 0.0f; ///< 0..1.
    float slip_deg = 0.0f;      ///< Degrees.

    uint32_t num_engines = 0;                       ///< Number of valid entries in the engine arrays below.
    std::array<uint32_t, kMaxEngines> eng_state{};   ///< Per-engine running state.
    std::array<float, kMaxEngines> rpm{};            ///< Per-engine RPM.
    std::array<float, kMaxEngines> fuel_flow_gph{};  ///< Per-engine fuel flow, gal/hr.
    std::array<float, kMaxEngines> fuel_px_psi{};    ///< Per-engine fuel pressure, psi.
    std::array<float, kMaxEngines> egt_degf{};       ///< Per-engine EGT, degF.
    std::array<float, kMaxEngines> cht_degf{};       ///< Per-engine CHT, degF.
    std::array<float, kMaxEngines> mp_inhg{};        ///< Per-engine manifold pressure, inHg.
    std::array<float, kMaxEngines> tit{};            ///< Per-engine turbine inlet temperature.
    std::array<float, kMaxEngines> oil_temp_degf{};  ///< Per-engine oil temperature, degF.
    std::array<float, kMaxEngines> oil_px_psi{};     ///< Per-engine oil pressure, psi.

    uint32_t num_tanks = 0;                          ///< Number of valid entries in fuel_quantity_lbs.
    std::array<float, kMaxTanks> fuel_quantity_lbs{}; ///< Per-tank fuel quantity, lbs.

    uint32_t num_wheels = 0;                              ///< Number of valid entries in the wheel arrays below.
    std::array<uint32_t, kMaxWheels> wow{};               ///< Per-wheel weight-on-wheels flag.
    std::array<float, kMaxWheels> gear_pos_norm{};        ///< Per-wheel gear position, 0..1.
    std::array<float, kMaxWheels> gear_steer_deg{};       ///< Per-wheel steering angle, degrees.
    std::array<float, kMaxWheels> gear_compression_norm{}; ///< Per-wheel strut compression, 0..1.

    uint32_t cur_time = 0;      ///< Unix time, seconds.
    int32_t warp = 0;           ///< Simulator time warp factor.
    float visibility_m = 0.0f;  ///< Meters.

    float elevator_norm = 0.0f;       ///< -1..1.
    float elevator_trim_norm = 0.0f;  ///< -1..1.
    float left_flap_norm = 0.0f;      ///< -1..1.
    float right_flap_norm = 0.0f;     ///< -1..1.
    float left_aileron_norm = 0.0f;   ///< -1..1.
    float right_aileron_norm = 0.0f;  ///< -1..1.
    float rudder_norm = 0.0f;         ///< -1..1.
    float nose_wheel_norm = 0.0f;     ///< -1..1.
    float speedbrake_norm = 0.0f;     ///< -1..1.
    float spoilers_norm = 0.0f;       ///< -1..1.
};

/// Result of decode(): whether the decoded Packet is trustworthy.
enum class DecodeResult {
    Ok,           ///< Decoded successfully; version matched kVersion.
    WrongSize,    ///< Buffer was not exactly kPacketSize bytes; `out` is zeroed.
    WrongVersion, ///< Size was right but the version field wasn't kVersion; `out` is still populated.
};

/**
 * @brief Decodes an already-received FGNetFDM into `out`.
 *
 * On version mismatch, `out` is still populated from `raw` and
 * DecodeResult::WrongVersion is returned -- callers are expected to warn
 * and use the data anyway, since a version bump alone doesn't make the
 * bytes garbage. Only a size problem (impossible to hit through this
 * overload, since FGNetFDM is fixed-size, but relevant to the
 * buffer-taking overload below) zeroes `out`.
 *
 * @param raw FGNetFDM already in memory (e.g. `recv()`'d directly into one).
 * @param out Receives the decoded, host-native Packet.
 * @return DecodeResult::Ok or DecodeResult::WrongVersion; `out` is
 * populated in both cases.
 */
inline DecodeResult decode(const FGNetFDM& raw, Packet& out) {
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

    // Unlike WrongSize (handled by the buffer-taking overload below), a
    // version mismatch here does NOT mean `raw`'s layout was wrong -- only
    // that the sender is a different protocol revision. `out` is populated
    // either way; the caller decides whether to warn and use it.
    out = p;
    if (p.version != kVersion) {
        return DecodeResult::WrongVersion;
    }
    return DecodeResult::Ok;
}

/**
 * @brief Decodes a raw big-endian FGNetFDM datagram into `out`.
 *
 * Validates the size, then `memcpy`'s into a local FGNetFDM (the incoming
 * pointer has no alignment guarantee, so this can't just
 * `reinterpret_cast` it) and delegates to the struct-taking overload
 * above.
 *
 * On DecodeResult::WrongSize, `out` is reset to a default-constructed
 * (all-zero) Packet rather than left partially filled -- callers must
 * check the DecodeResult, not just look at the data, to tell a real
 * zeroed-out flight state from "nothing decoded." DecodeResult::WrongVersion
 * is different: see the struct-taking overload above.
 *
 * @param data Pointer to `size` bytes of raw datagram.
 * @param size Byte count of `data`; must equal kPacketSize for anything
 * other than DecodeResult::WrongSize.
 * @param out Receives the decoded, host-native Packet.
 * @return DecodeResult::Ok, DecodeResult::WrongSize, or
 * DecodeResult::WrongVersion.
 */
inline DecodeResult decode(const uint8_t* data, std::size_t size, Packet& out) {
    out = Packet{};

    if (size != kPacketSize) {
        return DecodeResult::WrongSize;
    }

    // memcpy, not reinterpret_cast: `data` is a caller-owned buffer with no
    // alignment guarantee, and FGNetFDM is packed to alignment 1 anyway, so
    // there is nothing safe to cast onto. sizeof(raw) == kPacketSize is
    // enforced by the static_assert above, so this copies exactly `size`
    // bytes.
    FGNetFDM raw;
    std::memcpy(&raw, data, sizeof(raw));
    return decode(raw, out);
}

/**
 * @brief Returns a short, human-readable description of a DecodeResult.
 * @param r The result to describe.
 * @return A non-null, static, null-terminated string.
 */
inline const char* describe(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return "ok";
        case DecodeResult::WrongSize: return "wrong datagram size";
        case DecodeResult::WrongVersion: return "wrong FGNetFDM version";
    }
    return "unknown";
}

} // namespace net_fdm

#endif // FGPROTOCOL_NET_FDM_H
