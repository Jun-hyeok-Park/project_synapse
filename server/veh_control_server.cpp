#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <vsomeip/vsomeip.hpp>
#include "veh_can.hpp"
#include "veh_control_service.hpp"
#include "veh_logger.hpp"

// 명령 수신 → CAN 송신 서버

veh::Logger server_logger("logs/veh_server.log");

class VehControlServer {
public:
    VehControlServer()
        : app_(vsomeip::runtime::get()->create_application("veh_control_server")),
          can_("can0") {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(server_logger, "vsomeip init failed!");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                offer_service();
            }
        });

        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            std::bind(&VehControlServer::on_message, this, std::placeholders::_1));

        if (!can_.init()) {
            LOG_ERROR(server_logger, "CAN init failed!");
            return false;
        }

        return true;
    }

    void start() {
        LOG_INFO(server_logger, "Starting veh_control_server...");
        app_->start();
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    CanInterface can_;

    void offer_service() {
        app_->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        LOG_INFO(server_logger, "veh_control_service offered.");
    }

    static std::string to_hex(uint8_t v) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << int(v);
        return oss.str();
    }

    void on_message(const std::shared_ptr<vsomeip::message> &msg) {
        auto payload  = msg->get_payload();
        auto data_ptr = payload->get_data();
        auto len      = payload->get_length();

        if (!data_ptr || len == 0) {
            LOG_WARN(server_logger, "Empty payload received.");
            return;
        }

        std::vector<uint8_t> data(data_ptr, data_ptr + len);

        uint8_t cmd_type = data[0];
        uint8_t cmd_val  = (len > 1) ? data[1] : 0x00;

        LOG_INFO(server_logger,
                 "Received Command → Type: 0x" + to_hex(cmd_type) +
                 "  Value: 0x" + to_hex(cmd_val));

        // CAN 프레임 전송
        CanFrame frame{};
        frame.id  = 0x100;
        frame.dlc = static_cast<uint8_t>(std::min<size_t>(8, size_t(len)));
        std::copy_n(data.begin(), frame.dlc, frame.data.begin());

        can_.sendFrame(frame);
        LOG_INFO(server_logger, "Sent CAN frame with " + std::to_string(frame.dlc) + " bytes.");

        // 응답(ACK)
        auto resp          = vsomeip::runtime::get()->create_response(msg);
        auto resp_payload  = vsomeip::runtime::get()->create_payload();
        std::vector<uint8_t> ack = { 0x00 };
        resp_payload->set_data(ack);
        resp->set_payload(resp_payload);
        app_->send(resp);
    }
};

int main() {
    VehControlServer server;
    if (!server.init()) {
        LOG_ERROR(server_logger, "Server initialization failed!");
        return 1;
    }
    server.start();
    return 0;
}
