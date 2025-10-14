#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"

class VehDriveServer {
public:
    explicit VehDriveServer(std::shared_ptr<vsomeip::application> app);
    void init(); // 핸들러/이벤트 등록 및 주기 publish 스레드 시작
    void stop(); // 스레드/리스너 정리
    ~VehDriveServer() { stop(); }

private:
    void on_set_speed(const std::shared_ptr<vsomeip::message>& msg);
    void on_set_dir(const std::shared_ptr<vsomeip::message>& msg);
    void on_soft_stop(const std::shared_ptr<vsomeip::message>& msg);
    void publish_event();
    void update_payload(uint8_t spd, uint8_t dir);

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::payload> payload_;
    std::thread publish_thread_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    uint8_t speed_{0};
    uint8_t direction_{5}; // 정지(5) 초기값

    vehcan::ListenerId drive_listener_id_ = 0;
    uint8_t last_sent_spd_{0xFF};
    uint8_t last_sent_dir_{0xFF};
};
