/*
    목적: 네트워크, 인터페이스, 포트, 로그 파일 등 환경 설정값을 한 곳에서 관리
    특징: 전역 상수 + 함수 기반 설정 읽기 구조
*/
#pragma once
#include <string>

namespace veh {

// =======================================================
// System Configuration
// =======================================================

// ──────────────── Network / Interface ────────────────
constexpr const char* DEFAULT_CAN_INTERFACE = "can0";
constexpr const char* VSOMEIP_SERVER_NAME   = "veh_control_server";
constexpr const char* VSOMEIP_CLIENT_NAME   = "veh_control_client";

// ──────────────── Log File Paths ────────────────
constexpr const char* LOG_PATH_SERVER = "logs/veh_server.log";
constexpr const char* LOG_PATH_CLIENT = "logs/veh_client.log";
constexpr const char* LOG_PATH_STATUS = "logs/veh_status.log";

// ──────────────── CAN Identifiers (예시) ────────────────
constexpr uint32_t CAN_ID_CONTROL_TX = 0x100;   // RPi → TC375
constexpr uint32_t CAN_ID_STATUS_RX  = 0x101;   // TC375 → RPi

// ──────────────── System Constants ────────────────
constexpr uint8_t MAX_SPEED_DUTY = 100;
constexpr uint16_t STATUS_PUBLISH_INTERVAL_MS = 500;

// ──────────────── Utility Function ────────────────
inline void print_config_summary() {
    std::cout << "=== Project SYNAPSE Config Summary ===" << std::endl;
    std::cout << "CAN Interface : " << DEFAULT_CAN_INTERFACE << std::endl;
    std::cout << "Server Name   : " << VSOMEIP_SERVER_NAME << std::endl;
    std::cout << "Client Name   : " << VSOMEIP_CLIENT_NAME << std::endl;
    std::cout << "CAN TX ID     : 0x" << std::hex << CAN_ID_CONTROL_TX << std::dec << std::endl;
    std::cout << "CAN RX ID     : 0x" << std::hex << CAN_ID_STATUS_RX << std::dec << std::endl;
    std::cout << "======================================" << std::endl;
}

} // namespace veh
