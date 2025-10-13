#pragma once
#include "veh_server_common.hpp"
#include "veh_can.hpp"
#include <atomic>
#include <thread>
#include <mutex>

class VehFaultServer {
public:
    explicit VehFaultServer(std::shared_ptr<vsomeip::application> app);
    void init();
    void stop();
    ~VehFaultServer() { stop(); }

private:
    void fault_loop(); // (옵션) 데모용 랜덤 Fault 퍼블리셔

private:
    std::shared_ptr<vsomeip::application> app_;
    std::atomic<bool> running_{false};
    std::thread f_;

    // CAN 이벤트 브리징용
    vehcan::ListenerId fault_listener_id_{0};
};
