#pragma once
#include <cstdint>

// =======================================================
// veh_control_service — Command Interface Definition
// =======================================================

// Service / Instance / Method IDs
#define VEH_CONTROL_SERVICE_ID     0x1100
#define VEH_CONTROL_INSTANCE_ID    0x0001
#define VEH_CONTROL_METHOD_ID      0x0100

// Response code
#define VEH_RESP_OK                0x00
#define VEH_RESP_ERR               0xFF

// =======================================================
// Command Type (cmd_type)
// =======================================================
enum class CmdType : uint8_t {
    DRIVE_DIRECTION  = 0x01,  // 방향 제어
    DRIVE_SPEED      = 0x02,  // 듀티(속도)
    AEB_CONTROL      = 0x03,  // 자동 긴급 제동 ON/OFF
    AUTOPARK_CONTROL = 0x04,  // 자동 주차 Start/Cancel
    AUTH_PASSWORD    = 0x05,  // 인증 문자열
    FAULT_EMERGENCY  = 0xFE   // 비상 정지 / Fault 리셋
};

// =======================================================
// Direction Value (cmd_value)
// =======================================================
enum class DriveDir : uint8_t {
    STOP        = 0x05,
    FORWARD     = 0x08,
    BACKWARD    = 0x02,
    LEFT        = 0x04,
    RIGHT       = 0x06,
    FWD_LEFT    = 0x07,
    FWD_RIGHT   = 0x09,
    BWD_LEFT    = 0x01,
    BWD_RIGHT   = 0x03
};

// =======================================================
// AEB / AutoPark / Auth 값 정의
// =======================================================
enum class AebState : uint8_t {
    OFF = 0x00,
    ON  = 0x01
};

enum class AutoParkState : uint8_t {
    CANCEL = 0x00,
    START  = 0x01
};

enum class AuthResult : uint8_t {
    FAIL = 0x00,
    SUCCESS = 0x01
};

// =======================================================
// Payload 구조체 (vsomeip 전송용)
// =======================================================
struct ControlPayload {
    uint8_t cmd_type;    // CmdType
    uint8_t cmd_value[7]; // 나머지 데이터 (문자열, 값 등)
};
