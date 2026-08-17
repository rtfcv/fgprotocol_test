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
 * Header-only, dependency-free (just `<array>`/`<cstddef>`/`<cstdint>`):
 * deliberately so this file (and control_wire.h alongside it) can be
 * reused outside this repo -- copy-pasted into another project's include
 * tree, or pulled in via add_subdirectory()/FetchContent against this
 * directory's own CMakeLists.txt. It knows nothing about sockets, timing,
 * or what a caller wants to do with a decoded FGNetFDMReversed -- see the
 * jsbsim_tester project's own `src/` for that (`udp_socket.h`/`.cpp`,
 * `main.cpp`).
 *
 * @warning decode() does NOT correct array element order. See
 * FGNetFDMReversed's comment: every array field (`eng_state`, `rpm`,
 * `wow`, `gear_pos`, etc.) comes back with its elements in reverse index
 * order (index 0 holds what was originally the *last* element). This is
 * a known, currently-accepted limitation, not an oversight -- see the
 * project history for the reasoning. Scalar fields are unaffected and
 * decode correctly.
 */
#ifndef FGPROTOCOL_NET_FDM_H
#define FGPROTOCOL_NET_FDM_H

#include <array>
#include <cstddef>
#include <cstdint>

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
 * @note All multi-byte fields are big-endian on the wire; use decode() to
 * get host-native values.
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
 * @brief FGNetFDM's fields, in exactly reverse declaration order -- decode()'s output type.
 *
 * decode() writes the whole-buffer-reversed datagram directly into an
 * instance of this struct (via a `uint8_t*` alias): reversing a
 * concatenation of fields reverses each field's own bytes *and* flips the
 * order the fields appear in, so laying the reversed bytes over a struct
 * declared in reverse field order lands every field already
 * byte-order-native at exactly the offset this declaration implies. No
 * separate output type, no per-field renaming step -- this struct's own
 * fields (`version`, `altitude`, `spoilers`, ...) are what a caller reads
 * directly.
 *
 * @warning Scalar fields are correct. Array fields (`eng_state`, `rpm`,
 * `fuel_flow`, `fuel_px`, `egt`, `cht`, `mp_osi`, `tit`, `oil_temp`,
 * `oil_px`, `fuel_quantity`, `wow`, `gear_pos`, `gear_steer`,
 * `gear_compression`) are NOT index-corrected: the whole-buffer reversal
 * flips each array's element order too, not just each element's bytes, so
 * e.g. `wow[0]` here holds what was originally the *last* wheel's value.
 * Not yet fixed up -- known, not silent-by-accident.
 */
#pragma pack(push, 1)
struct FGNetFDMReversed {
    float spoilers;
    float speedbrake;
    float nose_wheel;
    float rudder;
    float right_aileron;
    float left_aileron;
    float right_flap;
    float left_flap;
    float elevator_trim_tab;
    float elevator;

    float visibility;
    int32_t warp;
    uint32_t cur_time;

    float gear_compression[kMaxWheels];
    float gear_steer[kMaxWheels];
    float gear_pos[kMaxWheels];
    uint32_t wow[kMaxWheels];
    uint32_t num_wheels;

    float fuel_quantity[kMaxTanks];
    uint32_t num_tanks;

    float oil_px[kMaxEngines];
    float oil_temp[kMaxEngines];
    float tit[kMaxEngines];
    float mp_osi[kMaxEngines];
    float cht[kMaxEngines];
    float egt[kMaxEngines];
    float fuel_px[kMaxEngines];
    float fuel_flow[kMaxEngines];
    float rpm[kMaxEngines];
    uint32_t eng_state[kMaxEngines];
    uint32_t num_engines;

    float slip_deg;
    float stall_warning;

    float A_Z_pilot;
    float A_Y_pilot;
    float A_X_pilot;

    float v_body_w;
    float v_body_v;
    float v_body_u;
    float v_down;
    float v_east;
    float v_north;
    float climb_rate;
    float vcas;
    float psidot;
    float thetadot;
    float phidot;

    float beta;
    float alpha;
    float psi;
    float theta;
    float phi;
    float agl;
    double altitude;
    double latitude;
    double longitude;

    uint32_t padding;
    uint32_t version;
};
#pragma pack(pop)

static_assert(sizeof(FGNetFDMReversed) == kPacketSize, "FGNetFDMReversed layout mismatch");

/// Result of decode(): whether the decoded FGNetFDMReversed is trustworthy.
enum class DecodeResult {
    Ok,           ///< Decoded successfully; version matched kVersion.
    WrongSize,    ///< Buffer was not exactly kPacketSize bytes; `out` is zeroed.
    WrongVersion, ///< Size was right but the version field wasn't kVersion; `out` is still populated.
};

/**
 * @brief Decodes a raw big-endian FGNetFDM datagram into `out`.
 *
 * Whole-buffer-reversal decode: reverses all `kPacketSize` bytes once,
 * writes the result directly into `out` (an FGNetFDMReversed). See that
 * struct's comment for the derivation of why this recovers every scalar
 * field already correctly byte-order-native -- and for the array-order
 * caveat this function does NOT correct.
 *
 * On DecodeResult::WrongSize, `out` is reset to a default-constructed
 * (all-zero) FGNetFDMReversed rather than left partially filled --
 * callers must check the DecodeResult, not just look at the data, to tell
 * a real zeroed-out flight state from "nothing decoded." On
 * DecodeResult::WrongVersion, `out` is still populated -- a version bump
 * alone doesn't mean the bytes are garbage, so callers are expected to
 * warn and use the data anyway.
 *
 * @param data Pointer to `size` bytes of raw datagram.
 * @param size Byte count of `data`; must equal kPacketSize for anything
 * other than DecodeResult::WrongSize.
 * @param out Receives the decoded FGNetFDMReversed.
 * @return DecodeResult::Ok, DecodeResult::WrongSize, or
 * DecodeResult::WrongVersion.
 */
inline DecodeResult decode(const uint8_t* data, std::size_t size, FGNetFDMReversed& out) {
    if (size != kPacketSize) {
        out = FGNetFDMReversed{};
        return DecodeResult::WrongSize;
    }

    // Writing raw bytes through a uint8_t* into a trivial struct's storage,
    // then reading it back through the struct's own declared field types,
    // is well-defined (the same thing recv() already does everywhere else
    // in this codebase) -- not a strict-aliasing violation, since uint8_t
    // is always permitted to alias any object's representation.
    uint8_t* dst = reinterpret_cast<uint8_t*>(&out);
    for (std::size_t i = 0; i < kPacketSize; ++i) {
        dst[i] = data[kPacketSize - 1 - i];
    }

    if (out.version != kVersion) {
        return DecodeResult::WrongVersion;
    }
    return DecodeResult::Ok;
}

/**
 * @brief Decodes an already-received FGNetFDM into `out`.
 *
 * Thin wrapper over the buffer-taking overload above: `FGNetFDM` is
 * packed to alignment 1 and always exactly `kPacketSize` bytes (enforced
 * by the static_assert() near its definition), so reading its bytes
 * through a `const uint8_t*` alias is safe and DecodeResult::WrongSize is
 * unreachable through this overload -- only DecodeResult::Ok or
 * DecodeResult::WrongVersion can come back, `out` populated either way.
 *
 * @param raw FGNetFDM already in memory (e.g. `recv()`'d directly into one).
 * @param out Receives the decoded FGNetFDMReversed.
 * @return DecodeResult::Ok or DecodeResult::WrongVersion; `out` is
 * populated in both cases.
 */
inline DecodeResult decode(const FGNetFDM& raw, FGNetFDMReversed& out) {
    return decode(reinterpret_cast<const uint8_t*>(&raw), sizeof(raw), out);
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
