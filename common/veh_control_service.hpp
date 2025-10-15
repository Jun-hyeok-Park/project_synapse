#pragma once
#include <cstdint>

// =======================================================
// veh_control_service — Request/Command (Client → Server)
// =======================================================

#define VEH_CONTROL_SERVICE_ID      0x1100
#define VEH_CONTROL_INSTANCE_ID     0x0001
#define VEH_CONTROL_METHOD_ID       0x0100

// Response byte (optional)
#define VEH_RESP_OK                 0x00
#define VEH_RESP_ERR                0xFF

// 🔹 Command Type (cmd_type)
enum class CmdType : uint8_t {
    DRIVE_DIRECTION   = 0x01,  // 방향
    DRIVE_SPEED       = 0x02,  // 듀티(0~100)
    AEB_CONTROL       = 0x03,  // ON/OFF
    AUTOPARK_CONTROL  = 0x04,  // START/CANCEL
    AUTH_PASSWORD     = 0x05,  // ASCII pw (<=7B)
    FAULT_EMERGENCY   = 0xFE   // 리셋/정지 등 확장
};

// 🔹 Direction values (cmd_value)
enum class DriveDir : uint8_t {
    BWD_LEFT   = 0x01,
    BACKWARD   = 0x02,
    BWD_RIGHT  = 0x03,
    LEFT       = 0x04,
    STOP       = 0x05,
    RIGHT      = 0x06,
    FWD_LEFT   = 0x07,
    FORWARD    = 0x08,
    FWD_RIGHT  = 0x09
};

enum class AebState : uint8_t {
    OFF = 0x00, ON = 0x01
};

enum class AutoParkState : uint8_t {
    CANCEL = 0x00, START = 0x01
};

// 8B 고정 payload로 전송할 때 참고용
struct ControlPayload {
    uint8_t cmd_type;      // CmdType
    uint8_t cmd_value[7];  // 값/문자열 (패딩 포함)
};
