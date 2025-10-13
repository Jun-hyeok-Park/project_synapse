#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <mutex>

class VehAuthServer {
public:
    explicit VehAuthServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehAuthServer() { stop(); }

private:
    void on_login(const std::shared_ptr<vsomeip::message> &req);

private:
    std::shared_ptr<vsomeip::application> app_;

    // CAN 이벤트 리스너 id
    vehcan::ListenerId auth_listener_id_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> authorized_{false};
};
