#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <iostream>
#include "veh_control_service.hpp"
#include "veh_logger.hpp"

namespace veh {

class ControlClient {
public:
    ControlClient()
        : app_(vsomeip::runtime::get()->create_application("veh_client")),
          logger_("logs/veh_control_client.log") {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(logger_, "vsomeip init failed!");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                LOG_INFO(logger_, "veh_control_client registered to routing manager.");
        });

        return true;
    }

    void start() {
        LOG_INFO(logger_, "Starting veh_control_client...");
        app_->start();
    }

    // ======================================================
    // 🚀 Command 송신 메서드
    // ======================================================
    void send_command(uint8_t cmd_type, const std::vector<uint8_t> &cmd_value) {
        auto msg = vsomeip::runtime::get()->create_request();
        msg->set_service(VEH_CONTROL_SERVICE_ID);
        msg->set_instance(VEH_CONTROL_INSTANCE_ID);
        msg->set_method(VEH_CONTROL_METHOD_ID);

        std::vector<uint8_t> payload;
        payload.push_back(cmd_type);
        payload.insert(payload.end(), cmd_value.begin(), cmd_value.end());

        msg->set_payload(vsomeip::runtime::get()->create_payload(payload));

        char buf[64];
        snprintf(buf, sizeof(buf), "Send cmd_type=0x%02X len=%zu", cmd_type, payload.size());
        LOG_INFO(logger_, buf);

        app_->send(msg);
    }

    // ------------------------------------------------------
    // 🔹 개별 제어 메서드
    // ------------------------------------------------------
    void send_drive_direction(DriveDir dir) {
        send_command(static_cast<uint8_t>(CmdType::DRIVE_DIRECTION), {static_cast<uint8_t>(dir)});
    }

    void send_drive_speed(uint8_t duty) {
        send_command(static_cast<uint8_t>(CmdType::DRIVE_SPEED), {duty});
    }

    void send_aeb_control(AebState state) {
        send_command(static_cast<uint8_t>(CmdType::AEB_CONTROL), {static_cast<uint8_t>(state)});
    }

    void send_autopark(AutoParkState state) {
        send_command(static_cast<uint8_t>(CmdType::AUTOPARK_CONTROL), {static_cast<uint8_t>(state)});
    }

    void send_auth_password(const std::string &pw) {
        std::vector<uint8_t> v(pw.begin(), pw.end());
        send_command(static_cast<uint8_t>(CmdType::AUTH_PASSWORD), v);
    }

    void send_emergency_stop() {
        send_command(static_cast<uint8_t>(CmdType::FAULT_EMERGENCY), {0x01});
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    veh::Logger logger_;
};

} // namespace veh

// ======================================================
// 🧩 테스트 시나리오 (GUI 입력 시뮬레이션)
// ======================================================
int main() {
    veh::ControlClient client;

    if (!client.init()) {
        std::cerr << "Client init failed." << std::endl;
        return -1;
    }

    std::thread app_thread([&]() { client.start(); });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "=== [TEST] GUI Input Simulation Start ===" << std::endl;

    client.send_drive_direction(DriveDir::FORWARD);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    client.send_drive_speed(70);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    client.send_aeb_control(AebState::ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    client.send_autopark(AutoParkState::START);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    client.send_auth_password("1234");
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    client.send_emergency_stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    std::cout << "=== [TEST] GUI Input Simulation End ===" << std::endl;

    app_thread.join();
    return 0;
}
