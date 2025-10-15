#pragma once
#include <string>
#include "veh_types.hpp"

// SocketCAN용 선언
class CanInterface {
public:
    explicit CanInterface(const std::string& ifname = "can0");
    ~CanInterface();

    bool init();              // 소켓 생성 + bind
    void close();             // 소켓 닫기
    bool isOpen() const;      // 소켓 상태

    bool sendFrame(const CanFrame& frame);
    bool receiveFrame(CanFrame& frame);

private:
    int sock_fd_;
    std::string if_name_;
};