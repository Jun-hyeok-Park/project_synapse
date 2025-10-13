#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <mutex>

class VehAutoparkServer {
public:
    explicit VehAutoparkServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehAutoparkServer() { stop(); }

private:
    void on_request(const std::shared_ptr<vsomeip::message>& req);

private:
    std::shared_ptr<vsomeip::application> app_;

    // CAN 이벤트 리스너
    vehcan::ListenerId autopark_listener_id_{0};

    // (옵션) 최근 진행률 캐시 → getProgress 응답에 사용
    std::mutex m_;
    uint8_t last_progress_{0};
    std::atomic<bool> running_{false};
};
