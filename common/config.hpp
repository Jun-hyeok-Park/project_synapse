/*
    목적: 네트워크, 인터페이스, 포트, 로그 파일 등 환경 설정값을 한 곳에서 관리
    특징: 전역 상수 + 함수 기반 설정 읽기 구조
*/
#pragma once
#include <cstdint>
#include <iostream>

namespace veh {

// 앱 이름 (vsomeip json과 동일해야 함)
constexpr const char* VSOMEIP_SERVER_NAME   = "veh_control_server";
constexpr const char* VSOMEIP_CLIENT_NAME   = "veh_control_client";
constexpr const char* VSOMEIP_STATUS_PUB    = "veh_status_publisher";
constexpr const char* VSOMEIP_STATUS_CLIENT = "veh_status_client";

// CAN
constexpr const char* DEFAULT_CAN_IFACE     = "can0";
constexpr uint32_t CAN_ID_CONTROL_TX        = 0x100; // RPi → TC375
constexpr uint32_t CAN_ID_STATUS_RX         = 0x101; // TC375 → RPi

// 로그 파일 경로
constexpr const char* LOG_SERVER   = "logs/veh_server.log";
constexpr const char* LOG_CLIENT   = "logs/veh_client.log";
constexpr const char* LOG_STATUS   = "logs/veh_status.log";

// 기타
constexpr uint16_t STATUS_PUBLISH_PERIOD_MS = 200;

inline void print_config() {
    std::cout << "=== Project SYNAPSE Config ===\n"
              << "Server App : " << VSOMEIP_SERVER_NAME   << "\n"
              << "Client App : " << VSOMEIP_CLIENT_NAME   << "\n"
              << "Status App : " << VSOMEIP_STATUS_PUB    << "\n"
              << "CAN Iface  : " << DEFAULT_CAN_IFACE     << "\n"
              << "CAN TX ID  : 0x" << std::hex << CAN_ID_CONTROL_TX << std::dec << "\n"
              << "CAN RX ID  : 0x" << std::hex << CAN_ID_STATUS_RX  << std::dec << "\n"
              << "===============================\n";
}

} // namespace veh
