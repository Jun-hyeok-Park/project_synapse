#pragma once
#include <cstdint>

// =======================================================
// veh_status_service — Event / Status Interface Definition
// =======================================================

// Service / Instance / Event ID
#define VEH_STATUS_SERVICE_ID      0x1200
#define VEH_STATUS_INSTANCE_ID     0x0001
#define VEH_STATUS_EVENT_ID        0x0200

// =======================================================
// Status Type (status_type)
// =======================================================
enum class StatusType : uint8_t {
    DRIVE_STATE   = 0x01,
    AEB_STATE     = 0x02,
    AUTOPARK_STEP = 0x03,
    TOF_DISTANCE  = 0x04,
    FAULT_CODE    = 0x05,
    AUTH_STATE    = 0x06
};

// =======================================================
// Fault Code 정의
// =======================================================
enum class FaultCode : uint8_t {
    NONE          = 0x00,
    SENSOR_FAULT  = 0x10,
    CAN_FAULT     = 0x20,
    MOTOR_FAULT   = 0x30,
    UNKNOWN       = 0xFF
};

// =======================================================
// Event Payload 구조체
// =======================================================
struct StatusPayload {
    uint8_t status_type;    // StatusType
    uint8_t status_value[7]; // 세부 데이터 (ex. 거리, 상태 값 등)
};
