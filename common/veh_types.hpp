#pragma once
#include <cstdint>
#include <array>
#include <string>

// ─────────────── 기본 타입 alias ───────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s16 = int16_t;
using s32 = int32_t;

constexpr std::size_t CAN_FRAME_SIZE   = 8;
constexpr std::size_t VSOMEIP_PAYLOAD8 = 8;

// ─────────────── SocketCAN 프레임 ───────────────
struct CanFrame {
    u32 id;                                  // CAN Identifier
    u8  dlc;                                 // 0..8
    std::array<u8, CAN_FRAME_SIZE> data{};   // payload
};

// ─────────────── VSOMEIP 8B 프레임(참고용) ───────────────
struct CommandFrame {
    u8 type;
    std::array<u8, 7> value{};
};

struct StatusFrame {
    u8 type;
    std::array<u8, 7> value{};
};

// ─────────────── 문자열 헬퍼 ───────────────
inline std::string dir_to_str(u8 d) {
    switch (d) {
        case 0x08: return "FORWARD";
        case 0x02: return "BACKWARD";
        case 0x04: return "LEFT";
        case 0x06: return "RIGHT";
        case 0x07: return "FWD_LEFT";
        case 0x09: return "FWD_RIGHT";
        case 0x01: return "BWD_LEFT";
        case 0x03: return "BWD_RIGHT";
        case 0x05: return "STOP";
        default:   return "UNKNOWN";
    }
}
