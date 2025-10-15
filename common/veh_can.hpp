
#pragma once
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>
#include "veh_types.hpp"
#include "veh_logger.hpp"

// =======================================================
// veh_can.hpp — SocketCAN Interface Declaration
// SocketCAN 인터페이스 정의
// RPi에서 TC375로 CAN 메시지를 전송하거나 수신할 때 사용될 함수들을 선언
// =======================================================

class CanInterface {
private:
    int sock_fd;               // SocketCAN file descriptor
    std::string if_name;       // CAN interface name (e.g., "can0")

public:
    // ──────────────── 생성자 / 소멸자 ────────────────
    explicit CanInterface(const std::string& interface = "can0");
    ~CanInterface();

    // ──────────────── 초기화 / 종료 ────────────────
    bool init();   // 소켓 생성 + 바인딩
    void close();  // 소켓 닫기

    // ──────────────── 송신 / 수신 ────────────────
    bool sendFrame(const CanFrame& frame);
    bool receiveFrame(CanFrame& frame);

    // ──────────────── 상태 ────────────────
    bool isOpen() const { return sock_fd > 0; }
};
