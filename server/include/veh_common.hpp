#pragma once
#include <vsomeip/vsomeip.hpp>

// 공통 인스턴스
#define VEH_INSTANCE_ID         0x0001

// =======================================================
// 🚗 veh.drive — Manual Driving Control
// =======================================================
#define VEH_DRIVE_SERVICE_ID    0x1100
#define DRIVE_SET_SPEED_ID      0x0100
#define DRIVE_SET_DIR_ID        0x0101
#define DRIVE_SOFT_STOP_ID      0x0102
#define DRIVE_STATE_EVENT_ID    0x0200
#define DRIVE_EG_ID             0x0001

// =======================================================
// 🛑 veh.aeb — Automatic Emergency Braking
// =======================================================
#define VEH_AEB_SERVICE_ID      0x1101
#define AEB_ENABLE_ID           0x0100
#define AEB_GET_STATE_ID        0x0101
#define AEB_STATE_EVENT_ID      0x0200
#define AEB_EG_ID               0x0002

// =======================================================
// 📡 veh.tof — ToF Distance Sensor
// =======================================================
#define VEH_TOF_SERVICE_ID      0x1300
#define TOF_DISTANCE_EVENT_ID   0x0200
#define TOF_EG_ID               0x0003

// =======================================================
// 🚨 veh.fcw — Forward Collision Warning
// =======================================================
#define VEH_FCW_SERVICE_ID      0x1102
#define FCW_WARN_METHOD_ID      0x0100
#define FCW_ALERT_EVENT_ID      0x0200
#define FCW_FAULT_NOTIFY_ID     0x0300
#define FCW_EG_ID               0x0004

// =======================================================
// 🅿️ veh.autopark — Auto Parking System
// =======================================================
#define VEH_AP_SERVICE_ID       0x1103
#define AP_START_METHOD_ID      0x0100
#define AP_CANCEL_METHOD_ID     0x0101
#define AP_GET_PROGRESS_ID      0x0102
#define AP_PROGRESS_EVENT_ID    0x0200
#define AP_EG_ID                0x0005

// =======================================================
// 🔐 veh.auth — Authentication / Login
// =======================================================
#define VEH_AUTH_SERVICE_ID     0x1104
#define AUTH_LOGIN_METHOD_ID    0x0100
#define AUTH_STATE_NOTIFY_ID    0x0300
#define AUTH_EG_ID              0x0006

// =======================================================
// ⚠️ veh.fault — Fault Broadcast
// =======================================================
#define VEH_FAULT_SERVICE_ID    0x1400
#define FAULT_CODE_NOTIFY_ID    0x0300
#define FAULT_EG_ID             0x0007

// =======================================================
// 🧩 diag.doip — Diagnostic over IP
// =======================================================
#define DOIP_SERVICE_ID         0x1200
#define DOIP_REQUEST_ID         0x0100
#define DOIP_RESPONSE_ID        0x0101
