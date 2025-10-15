#pragma once
#include <cstdint>
#include <array>
#include <string>

// =======================================================
// Common Type Definitions
// =======================================================

// ──────────────── 기본 타입 ────────────────
using byte   = uint8_t;
using u16    = uint16_t;
using u32    = uint32_t;
using s16    = int16_t;
using s32    = int32_t;

// ──────────────── 상수 정의 ────────────────
constexpr size_t CAN_FRAME_SIZE = 8;
constexpr size_t MAX_PAYLOAD_SIZE = 8;

// =======================================================
// 공통 구조체 정의
// =======================================================

// ── CAN Frame ───────────────────────────────────────────
// 8바이트 데이터 프레임 구조 (SocketCAN 기준)
struct CanFrame {
    u32 id;                             // CAN Identifier
    u8  dlc;                            // Data Length Code (0~8)
    std::array<byte, CAN_FRAME_SIZE> data; // Payload data
};

// ── VSOMEIP Command Frame ───────────────────────────────
struct CommandFrame {
    byte cmd_type;                      // CmdType
    std::array<byte, 7> value;          // 세부 값 (direction, speed 등)
};

// ── VSOMEIP Status Frame ────────────────────────────────
struct StatusFrame {
    byte status_type;                   // StatusType
    std::array<byte, 7> value;          // 상태 값 (센서, AEB 등)
};

// =======================================================
// Enum Utilities (to_string helpers)
// =======================================================
inline std::string bool_to_str(bool flag) {
    return flag ? "ON" : "OFF";
}

inline std::string direction_to_str(uint8_t dir) {
    switch (dir) {
        case 0x08: return "FORWARD";
        case 0x02: return "BACKWARD";
        case 0x04: return "LEFT";
        case 0x06: return "RIGHT";
        case 0x07: return "FWD_LEFT";
        case 0x09: return "FWD_RIGHT";
        case 0x01: return "BWD_LEFT";
        case 0x03: return "BWD_RIGHT";
        case 0x05: return "STOP";
        default: return "UNKNOWN";
    }
}
