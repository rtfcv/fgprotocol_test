#ifndef JSBSIM_TESTER_NET_FDM_H
#define JSBSIM_TESTER_NET_FDM_H

#include <array>
#include <cstddef>
#include <cstdint>

// FlightGear's native-fdm wire format ("FGNetFDM"), protocol version 24.
// Defined by FlightGear's src/Network/net_fdm.hxx; JSBSim's
// <output type="FLIGHTGEAR" protocol="UDP"> (src/input_output/FGOutputFG.cpp)
// serializes exactly this layout, every field big-endian, no host padding on
// the wire. This is the same format the fginst project's NetFdm.h documents;
// field order/types/units are reproduced here rather than re-derived.
namespace net_fdm {

constexpr uint32_t kVersion = 24;
constexpr std::size_t kPacketSize = 408;

constexpr int kMaxEngines = 4;
constexpr int kMaxWheels = 3;
constexpr int kMaxTanks = 4;

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

// Decodes a raw big-endian FGNetFDM datagram into `out`.
//
// On anything but Ok, `out` is reset to a default-constructed (all-zero)
// Packet rather than left partially filled -- callers must check the
// DecodeResult, not just look at the data, to tell a real zeroed-out flight
// state from "nothing decoded."
DecodeResult decode(const uint8_t* data, std::size_t size, Packet& out);

const char* describe(DecodeResult r);

} // namespace net_fdm

#endif // JSBSIM_TESTER_NET_FDM_H
