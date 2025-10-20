#pragma once
#include <cstdint>

// =======================================================
// veh_status_service — Event/Status (Server → Client)
// =======================================================

#define VEH_STATUS_SERVICE_ID       0x1200
#define VEH_STATUS_INSTANCE_ID      0x0001
#define VEH_STATUS_EVENT_ID         0x0200
#define VEH_STATUS_EVENTGROUP_ID    0x0001

// 상태 구분용 enum (1~5)
enum class StatusType : uint8_t {
    AEB_STATE       = 0x01,  // 자동 긴급제동 활성 상태
    AUTOPARK_STATE  = 0x02,  // 자율주차 상태
    TOF_DISTANCE    = 0x03,  // ToF 거리(mm)
    AUTH_STATE      = 0x04   // 인증 결과
};

// 데이터 구조 (고정 8B 예시용)
struct StatusPayload {
    uint8_t status_type;
    uint8_t status_value[7];
};
