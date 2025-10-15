#include <iostream>
#include <thread>
#include <string>
#include "veh_control_service.hpp"
#include "veh_logger.hpp"
#include "config.hpp"
#include <vsomeip/vsomeip.hpp>

// CLI 전용 테스트 프로그램 — 키보드 입력 → 명령 전송

veh::Logger cli_logger("logs/veh_cli.log");

class VehCli {
public:
    VehCli();
    bool init();
    void start();

private:
    void run_cli();
    void send(uint8_t type, const std::vector<uint8_t>& val);

    std::shared_ptr<vsomeip::application> app_;
};

VehCli::VehCli() {
    app_ = vsomeip::runtime::get()->create_application("veh_cli");
}

bool VehCli::init() {
    if (!app_->init()) {
        LOG_ERROR(cli_logger, "vsomeip init failed.");
        return false;
    }

    app_->register_state_handler([this](vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            LOG_INFO(cli_logger, "veh_cli registered to vsomeip daemon.");
        }
    });

    return true;
}

void VehCli::send(uint8_t type, const std::vector<uint8_t>& val) {
    auto req = vsomeip::runtime::get()->create_request();
    req->set_service(VEH_CONTROL_SERVICE_ID);
    req->set_instance(VEH_CONTROL_INSTANCE_ID);
    req->set_method(VEH_CONTROL_METHOD_ID);

    std::vector<uint8_t> payload_data;
    payload_data.push_back(type);
    payload_data.insert(payload_data.end(), val.begin(), val.end());

    auto payload = vsomeip::runtime::get()->create_payload();
    payload->set_data(payload_data);
    req->set_payload(payload);

    app_->send(req);
    LOG_INFO(cli_logger, "CLI sent command: type=" + std::to_string(type));
}

void VehCli::run_cli() {
    while (true) {
        std::cout << "\n[CLI] Input command type (1=Drive, 2=Speed, FE=E-Stop): ";
        std::string input;
        std::cin >> input;

        uint8_t cmd_type = std::stoi(input, nullptr, 16);
        std::vector<uint8_t> val;

        if (cmd_type == 0x01) {
            std::cout << "Direction (08=FWD, 02=BWD, 05=STOP): ";
            std::string d; std::cin >> d;
            val.push_back(std::stoi(d, nullptr, 16));
        } else if (cmd_type == 0x02) {
            std::cout << "Speed (0~100): ";
            int s; std::cin >> s;
            val.push_back((uint8_t)s);
        }

        send(cmd_type, val);
    }
}

void VehCli::start() {
    std::thread app_thread([this]() { app_->start(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    run_cli();
    app_thread.join();
}

int main() {
    VehCli cli;
    if (cli.init()) {
        cli.start();
    }
    return 0;
}

