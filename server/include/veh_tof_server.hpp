#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <mutex>
#include <thread>

class VehTofServer {
public:
    explicit VehTofServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehTofServer() { stop(); }

private:
    void publish_loop();
    bool on_subscription(vsomeip::service_t sid, vsomeip::instance_t iid,
                         vsomeip::eventgroup_t eg, bool subscribed);

private:
    std::shared_ptr<vsomeip::application> app_;

    std::atomic<bool> running_{false};
    std::atomic<bool> client_subscribed_{false};

    std::thread pub_;
    std::mutex m_;
    uint16_t last_distance_{0};                 // 최근 수신 거리(cm) 캐시
    vehcan::ListenerId tof_listener_id_{0};     // vehcan 이벤트 리스너 ID
};
