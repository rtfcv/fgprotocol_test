#ifndef FGPROTOCOL_NET_FDM_H
#define FGPROTOCOL_NET_FDM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// FlightGear's native-fdm wire format ("FGNetFDM"), protocol version 24.
// Defined by FlightGear's src/Network/net_fdm.hxx; JSBSim's
// <output type="FLIGHTGEAR" protocol="UDP"> (src/input_output/FGOutputFG.cpp)
// serializes exactly this layout, every field big-endian, no host padding on
// the wire. This is the same format the fginst project's NetFdm.h documents;
// field order/types/units are reproduced here rather than re-derived.
//
// Header-only, dependency-free (just <array>/<cstddef>/<cstdint>/<cstring>):
// deliberately so this file (and control_wire.h alongside it) can be reused
// outside this repo -- copy-pasted into another project's include tree, or
// pulled in via add_subdirectory()/FetchContent against this directory's own
// CMakeLists.txt. It knows nothing about sockets, timing, or what a caller
// wants to do with a decoded Packet -- see the jsbsim_tester project's own
// src/ for that (udp_socket.h/.cpp, main.cpp).
namespace net_fdm {

constexpr uint32_t kVersion = 24;
constexpr std::size_t kPacketSize = 408;

constexpr int kMaxEngines = 4;
constexpr int kMaxWheels = 3;
constexpr int kMaxTanks = 4;

// The wire struct: FlightGear's FGNetFDM, byte-for-byte, big-endian, no
// compiler padding. This is what recv() writes into directly on the live
// path (see the jsbsim_tester project's UdpSocket::recvInto() / main.cpp)
// -- field order, types and counts below must match FlightGear's
// src/Network/net_fdm.hxx exactly, or every field past the mismatch reads
// as garbage. #pragma pack(1) removes the alignment padding a normal
// struct would get (the doubles in particular would otherwise be padded to
// 8-byte alignment), matching FlightGear's own wire layout. The
// static_assert below is the actual safety net: if this struct's field
// list ever drifts from net_fdm.hxx, the build fails here with "FGNetFDM
// layout mismatch" instead of the program silently decoding garbage at
// runtime.
#pragma pack(push, 1)
struct FGNetFDM {
    uint32_t version;
    uint32_t padding; // keeps the following doubles 8-byte aligned in
                       // FlightGear's own (unpacked) struct; carried here
                       // only so the byte layout matches, not for alignment
                       // (this struct is packed to 1).

    double longitude;  // radians
    double latitude;   // radians
    double altitude;   // meters
    float agl;         // meters
    float phi;         // roll, radians
    float theta;       // pitch, radians
    float psi;         // yaw / true heading, radians
    float alpha;       // angle of attack, radians
    float beta;        // sideslip, radians

    float phidot;      // roll rate, radians/sec
    float thetadot;    // pitch rate, radians/sec
    float psidot;      // yaw rate, radians/sec
    float vcas;        // calibrated airspeed, knots
    float climb_rate;  // ft/s
    float v_north;      // ft/s
    float v_east;       // ft/s
    float v_down;       // ft/s
    float v_body_u;      // ft/s
    float v_body_v;      // ft/s
    float v_body_w;      // ft/s

    float A_X_pilot;   // ft/s^2
    float A_Y_pilot;   // ft/s^2
    float A_Z_pilot;   // ft/s^2

    float stall_warning;
    float slip_deg;

    uint32_t num_engines;
    uint32_t eng_state[kMaxEngines];
    float rpm[kMaxEngines];
    float fuel_flow[kMaxEngines];   // gal/hr
    float fuel_px[kMaxEngines];     // psi
    float egt[kMaxEngines];         // degF
    float cht[kMaxEngines];         // degF
    float mp_osi[kMaxEngines];      // inHg
    float tit[kMaxEngines];
    float oil_temp[kMaxEngines];    // degF
    float oil_px[kMaxEngines];      // psi

    uint32_t num_tanks;
    float fuel_quantity[kMaxTanks]; // lbs

    uint32_t num_wheels;
    uint32_t wow[kMaxWheels];
    float gear_pos[kMaxWheels];         // 0..1
    float gear_steer[kMaxWheels];       // degrees
    float gear_compression[kMaxWheels]; // 0..1

    uint32_t cur_time; // unix seconds
    int32_t warp;
    float visibility; // meters

    float elevator;
    float elevator_trim_tab;
    float left_flap;
    float right_flap;
    float left_aileron;
    float right_aileron;
    float rudder;
    float nose_wheel;
    float speedbrake;
    float spoilers;
};
#pragma pack(pop)

static_assert(sizeof(FGNetFDM) == kPacketSize, "FGNetFDM layout mismatch");
static_assert(sizeof(float) == 4, "float must be 32 bits for the FGNetFDM decode");
static_assert(sizeof(double) == 8, "double must be 64 bits for the FGNetFDM decode");

// Hand-written big-endian byte-swaps -- deliberately not ntohl()/ntohs() from
// <winsock2.h>/<arpa/inet.h>: pulling either header into this file would
// force every consumer (including a build with no sockets at all -- the
// whole point of this being a standalone header) to link against a
// platform's socket library just to decode a struct. memcpy, not a union or
// reinterpret_cast, keeps this clear of strict-aliasing UB.
inline uint32_t ntoh32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

// Takes the FGNetFDM field itself (a float/double already holding the raw
// big-endian bit pattern, reinterpreted -- meaninglessly, as far as its
// numeric value goes -- as a native float/double by simply being a struct
// member). Deliberately NOT a uint32_t/uint64_t parameter: passing
// `raw.agl` (a float) to a uint32_t parameter would implicitly *numerically*
// convert the float's already-nonsense value to an integer -- rounding
// toward zero, saturating, or invoking UB on an out-of-range/NaN value --
// instead of reinterpreting its bits, silently corrupting every field.
// memcpy is what actually gets at the bits.
inline float ntohf(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    bits = ntoh32(bits);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

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

// The decoded, host-native, unit-labeled form callers actually want to work
// with. Field names carry their units so a caller reading `climb_rate_fps`
// can't mistake it for m/s, etc.
struct Packet {
    uint32_t version = 0;
    // 4 bytes of padding on the wire, not stored here.

    double longitude_rad = 0.0;
    double latitude_rad = 0.0;
    double altitude_m = 0.0;   // above sea level, METERS (not feet)
    float agl_m = 0.0f;
    float phi_rad = 0.0f;      // roll
    float theta_rad = 0.0f;    // pitch
    float psi_rad = 0.0f;      // yaw / true heading
    float alpha_rad = 0.0f;    // angle of attack
    float beta_rad = 0.0f;     // sideslip

    float phidot_rad_s = 0.0f;
    float thetadot_rad_s = 0.0f;
    float psidot_rad_s = 0.0f;
    float vcas_kt = 0.0f;         // calibrated airspeed, knots
    float climb_rate_fps = 0.0f;  // feet per second (not m/s)
    float v_north_fps = 0.0f;
    float v_east_fps = 0.0f;
    float v_down_fps = 0.0f;
    float v_body_u_fps = 0.0f;
    float v_body_v_fps = 0.0f;
    float v_body_w_fps = 0.0f;

    float a_x_pilot_fps2 = 0.0f;
    float a_y_pilot_fps2 = 0.0f;
    float a_z_pilot_fps2 = 0.0f;

    float stall_warning = 0.0f;
    float slip_deg = 0.0f;

    uint32_t num_engines = 0;
    std::array<uint32_t, kMaxEngines> eng_state{};
    std::array<float, kMaxEngines> rpm{};
    std::array<float, kMaxEngines> fuel_flow_gph{};
    std::array<float, kMaxEngines> fuel_px_psi{};
    std::array<float, kMaxEngines> egt_degf{};
    std::array<float, kMaxEngines> cht_degf{};
    std::array<float, kMaxEngines> mp_inhg{};
    std::array<float, kMaxEngines> tit{};
    std::array<float, kMaxEngines> oil_temp_degf{};
    std::array<float, kMaxEngines> oil_px_psi{};

    uint32_t num_tanks = 0;
    std::array<float, kMaxTanks> fuel_quantity_lbs{};

    uint32_t num_wheels = 0;
    std::array<uint32_t, kMaxWheels> wow{};
    std::array<float, kMaxWheels> gear_pos_norm{};
    std::array<float, kMaxWheels> gear_steer_deg{};
    std::array<float, kMaxWheels> gear_compression_norm{};

    uint32_t cur_time = 0;
    int32_t warp = 0;
    float visibility_m = 0.0f;

    float elevator_norm = 0.0f;
    float elevator_trim_norm = 0.0f;
    float left_flap_norm = 0.0f;
    float right_flap_norm = 0.0f;
    float left_aileron_norm = 0.0f;
    float right_aileron_norm = 0.0f;
    float rudder_norm = 0.0f;
    float nose_wheel_norm = 0.0f;
    float speedbrake_norm = 0.0f;
    float spoilers_norm = 0.0f;
};

enum class DecodeResult {
    Ok,
    WrongSize,     // buffer was not exactly kPacketSize bytes
    WrongVersion,  // size was right but the version field wasn't kVersion
};

// Decodes an already-received FGNetFDM (e.g. recv()'d straight into one) into
// `out`.
//
// On version mismatch, `out` is still populated from `raw` and
// DecodeResult::WrongVersion is returned -- callers are expected to warn and
// use the data anyway, since a version bump alone doesn't make the bytes
// garbage. Only a size problem (impossible to hit through this overload,
// since FGNetFDM is fixed-size, but relevant to the buffer-taking overload
// below) zeroes `out`.
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

// Decodes a raw big-endian FGNetFDM datagram into `out`. Validates the size,
// then memcpy's into a local FGNetFDM (the incoming pointer has no alignment
// guarantee, so this can't just reinterpret_cast it) and delegates to the
// struct-taking overload above.
//
// On WrongSize, `out` is reset to a default-constructed (all-zero) Packet
// rather than left partially filled -- callers must check the DecodeResult,
// not just look at the data, to tell a real zeroed-out flight state from
// "nothing decoded." WrongVersion is different: see the struct-taking
// overload above.
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
