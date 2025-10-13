#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <mutex>

class VehAebServer {
public:
    explicit VehAebServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehAebServer() { stop(); }

private:
    void on_enable(const std::shared_ptr<vsomeip::message> &req);
    void on_get_state(const std::shared_ptr<vsomeip::message> &req);

private:
    std::shared_ptr<vsomeip::application> app_;
    std::atomic<bool> running_{false};   // 재초기화 방지
    vehcan::ListenerId aeb_listener_id_{0};
    std::mutex m_;
    bool enabled_{false};                // 최근 enable 상태 캐시 (옵션)
    uint8_t state_{0};                   // 최근 AEB 상태 캐시 (옵션)
};
