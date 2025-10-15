#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include "veh_control_service.hpp"
#include "veh_logger.hpp"
#include "config.hpp"

// 명령 전송 클라이언트 — GUI·CLI 공통 호출용

veh::Logger client_logger(veh::LOG_CLIENT);

class VehControlClient {
public:
    VehControlClient();
    bool init();
    void start();
    void send_command(uint8_t cmd_type, const std::vector<uint8_t>& cmd_value);

private:
    std::shared_ptr<vsomeip::application> app_;
};

VehControlClient::VehControlClient() {
    app_ = vsomeip::runtime::get()->create_application(veh::VSOMEIP_CLIENT_NAME);
}

bool VehControlClient::init() {
    if (!app_->init()) {
        LOG_ERROR(client_logger, "vsomeip init failed.");
        return false;
    }

    app_->register_state_handler([this](vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            LOG_INFO(client_logger, "veh_control_client registered.");
        }
    });

    return true;
}

void VehControlClient::start() {
    app_->start();
}

void VehControlClient::send_command(uint8_t cmd_type, const std::vector<uint8_t>& cmd_value) {
    auto req = vsomeip::runtime::get()->create_request();
    req->set_service(VEH_CONTROL_SERVICE_ID);
    req->set_instance(VEH_CONTROL_INSTANCE_ID);
    req->set_method(VEH_CONTROL_METHOD_ID);

    std::vector<uint8_t> payload_data;
    payload_data.push_back(cmd_type);
    payload_data.insert(payload_data.end(), cmd_value.begin(), cmd_value.end());

    auto payload = vsomeip::runtime::get()->create_payload();
    payload->set_data(payload_data);
    req->set_payload(payload);

    app_->send(req);
    LOG_INFO(client_logger, "Command sent: type=" + std::to_string(cmd_type));
}

int main() {
    veh::print_config();

    VehControlClient client;
    if (client.init()) {
        std::thread app_thread([&client]() { client.start(); });
        std::this_thread::sleep_for(std::chrono::seconds(1));

        LOG_INFO(client_logger, "Client ready. Sending test command...");
        client.send_command(static_cast<uint8_t>(CmdType::DRIVE_DIRECTION), {0x08});
        app_thread.join();
    }
    return 0;
}
