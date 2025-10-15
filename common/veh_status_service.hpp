#pragma once
#include <cstdint>

// =======================================================
// veh_status_service — Event/Status (Server → Client)
// =======================================================

#define VEH_STATUS_SERVICE_ID       0x1200
#define VEH_STATUS_INSTANCE_ID      0x0001

// vsomeip 3.5+: offer_event() 호출 시 std::set<eventgroup_t> 필요
#define VEH_STATUS_EVENT_ID         0x0200
#define VEH_STATUS_EVENTGROUP_ID    0x0001

// 🔹 status_type
enum class StatusType : uint8_t {
    DRIVE_STATE   = 0x01,  // 0x08=전진 등
    AEB_STATE     = 0x02,  // 0x00/0x01
    AUTOPARK_STEP = 0x03,  // 1~N 단계
    TOF_DISTANCE  = 0x04,  // uint16(cm) big-endian
    FAULT_CODE    = 0x05,  // 코드 값
    AUTH_STATE    = 0x06   // 0x00/0x01
};

enum class FaultCode : uint8_t {
    NONE         = 0x00,
    SENSOR_FAULT = 0x10,
    CAN_FAULT    = 0x20,
    MOTOR_FAULT  = 0x30,
    UNKNOWN      = 0xFF
};

// 8B 고정 payload로 알릴 때 참고용
struct StatusPayload {
    uint8_t status_type;     // StatusType
    uint8_t status_value[7]; // 값/센서데이터 (패딩 포함)
};
