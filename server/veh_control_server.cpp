#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "veh_control_service.hpp"
#include "veh_logger.hpp"
#include "veh_can.hpp"   // 나중에 실제 구현 연결

veh::Logger server_logger("logs/veh_control_server.log");

// 임시 더미 함수 (빌드용)
void veh_can_send(uint16_t can_id, const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << "[CAN TX] ID=0x" << std::hex << can_id << " Data=";
    for (size_t i = 0; i < len; i++)
        oss << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    LOG_INFO(server_logger, oss.str().c_str());
}

class VehControlServer {
public:
    VehControlServer()
        : app_(vsomeip::runtime::get()->create_application("veh_server")) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(server_logger, "vsomeip init failed!");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                offer_service();
        });

        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            [this](const std::shared_ptr<vsomeip::message> &request) {
                on_request(request);
            });

        return true;
    }

    void start() {
        LOG_INFO(server_logger, "Starting veh_control_server...");
        app_->start();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    void offer_service() {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "Offered veh_control_service (0x%04X, inst 0x%04X)",
                 VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        LOG_INFO(server_logger, buf);
        app_->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
    }

    void on_request(const std::shared_ptr<vsomeip::message> &request) {
        auto payload = request->get_payload();
        auto data = payload->get_data();
        auto len = payload->get_length();

        if (len < 1) {
            LOG_WARN(server_logger, "Received empty payload.");
            return;
        }

        uint8_t cmd_type = data[0];
        std::vector<uint8_t> cmd_value(data + 1, data + len);

        char rxbuf[64];
        snprintf(rxbuf, sizeof(rxbuf), "RX cmd_type=0x%02X len=%zu", cmd_type, len - 1);
        LOG_INFO(server_logger, rxbuf);

        handle_command(cmd_type, cmd_value);

        auto resp = vsomeip::runtime::get()->create_response(request);
        std::vector<uint8_t> resp_data = { VEH_RESP_OK };
        resp->set_payload(vsomeip::runtime::get()->create_payload(resp_data));
        app_->send(resp);
    }

    void handle_command(uint8_t cmd_type, const std::vector<uint8_t> &cmd_value) {
        char msgbuf[80];

        switch (cmd_type) {
            case static_cast<uint8_t>(CmdType::DRIVE_DIRECTION): {
                if (!cmd_value.empty()) {
                    snprintf(msgbuf, sizeof(msgbuf), "→ Drive Direction: 0x%02X", cmd_value[0]);
                    LOG_INFO(server_logger, msgbuf);
                    veh_can_send(0x201, cmd_value.data(), cmd_value.size());
                }
                break;
            }

            case static_cast<uint8_t>(CmdType::DRIVE_SPEED): {
                if (!cmd_value.empty()) {
                    snprintf(msgbuf, sizeof(msgbuf), "→ Drive Speed: %u%%", cmd_value[0]);
                    LOG_INFO(server_logger, msgbuf);
                    veh_can_send(0x202, cmd_value.data(), cmd_value.size());
                }
                break;
            }

            case static_cast<uint8_t>(CmdType::AEB_CONTROL): {
                if (!cmd_value.empty()) {
                    snprintf(msgbuf, sizeof(msgbuf), "→ AEB Control: %s",
                             cmd_value[0] == static_cast<uint8_t>(AebState::ON) ? "ON" : "OFF");
                    LOG_INFO(server_logger, msgbuf);
                    veh_can_send(0x203, cmd_value.data(), cmd_value.size());
                }
                break;
            }

            case static_cast<uint8_t>(CmdType::AUTOPARK_CONTROL): {
                if (!cmd_value.empty()) {
                    snprintf(msgbuf, sizeof(msgbuf), "→ AutoPark: %s",
                             cmd_value[0] == static_cast<uint8_t>(AutoParkState::START)
                                 ? "START"
                                 : "CANCEL");
                    LOG_INFO(server_logger, msgbuf);
                    veh_can_send(0x204, cmd_value.data(), cmd_value.size());
                }
                break;
            }

            case static_cast<uint8_t>(CmdType::AUTH_PASSWORD): {
                std::string pw(cmd_value.begin(), cmd_value.end());
                snprintf(msgbuf, sizeof(msgbuf), "→ Auth Password: %s", pw.c_str());
                LOG_INFO(server_logger, msgbuf);
                veh_can_send(0x205, cmd_value.data(), cmd_value.size());
                break;
            }

            case static_cast<uint8_t>(CmdType::FAULT_EMERGENCY): {
                LOG_WARN(server_logger, "→ Emergency Stop Triggered!");
                veh_can_send(0x2FE, cmd_value.data(), cmd_value.size());
                break;
            }

            default:
                snprintf(msgbuf, sizeof(msgbuf), "Unknown cmd_type=0x%02X", cmd_type);
                LOG_WARN(server_logger, msgbuf);
                break;
        }
    }
};

int main() {
    VehControlServer server;
    if (!server.init()) {
        LOG_ERROR(server_logger, "Server init failed.");
        return -1;
    }
    server.start();
    return 0;
}
