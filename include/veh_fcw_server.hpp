#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <thread>
#include <mutex>

class VehFcwServer {
public:
    explicit VehFcwServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehFcwServer() { stop(); }

private:
    // SOME/IP method handler
    void on_warn(const std::shared_ptr<vsomeip::message> &req);

    // 주기 리퍼블리시(선택)
    void publish_loop();

private:
    std::shared_ptr<vsomeip::application> app_;

    std::atomic<bool> running_{false};
    std::thread pub_;

    // CAN 이벤트 구독 리스너 ID
    vehcan::ListenerId fcw_listener_id_{0};

    // 최근 상태 캐시(옵션)
    std::mutex m_;
    uint8_t last_alert_{0};   // 마지막 FCW alert 값
    uint8_t last_fault_{0};   // 마지막 fault 코드(있을 경우)
};
