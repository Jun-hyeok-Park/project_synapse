#include <vsomeip/vsomeip.hpp>
#include <cstring>
#include <iostream>
#include <mutex>

static constexpr vsomeip::service_t  SERVICE_ID  = 0x1100;
static constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
static constexpr vsomeip::method_t   METHOD_ID   = 0x0100;

static constexpr uint8_t RESP_OK  = 0x00;
static constexpr uint8_t RESP_ERR = 0xFF;

class veh_server {
public:
    veh_server()
        : app_(vsomeip::runtime::get()->create_application("veh_control_server")) {}

    bool init() {
        if (!app_->init()) return false;

        // 수신 콜백 등록
        app_->register_message_handler(
            SERVICE_ID, INSTANCE_ID, METHOD_ID,
            std::bind(&veh_server::on_request, this,
                      std::placeholders::_1));

        // 서비스 오퍼
        app_->offer_service(SERVICE_ID, INSTANCE_ID);

        return true;
    }

    void start() {
        app_->start();
    }

private:
    void on_request(const std::shared_ptr<vsomeip::message> &req) {
        auto resp = vsomeip::runtime::get()->create_response(req);
        auto payload = req->get_payload();
        auto data = payload->get_data();
        auto size = payload->get_length();

        uint8_t result = RESP_ERR;
        if (size >= 1) {
            uint8_t cmd_type = data[0];
            // cmd_value는 가변
            const uint8_t* cmd_value = (size > 1) ? (data + 1) : nullptr;
            std::size_t cmd_len = (size > 1) ? (size - 1) : 0;

            // 여기서 실제 동작 분기 (하드웨어/펌웨어 연계는 TODO 훅으로)
            switch (cmd_type) {
                case 0x01: // Drive Direction
                    if (cmd_len >= 1) {
                        uint8_t dir = cmd_value[0];
                        // TODO: Drive_SetDirection(dir);
                        std::cout << "[SRV] Direction: 0x" << std::hex << int(dir) << std::dec << "\n";
                        result = RESP_OK;
                    }
                    break;
                case 0x02: // Drive Speed (Duty 0~100)
                    if (cmd_len >= 1) {
                        uint8_t duty = cmd_value[0];
                        // TODO: Drive_SetDuty(duty);
                        std::cout << "[SRV] Duty: " << int(duty) << "%\n";
                        result = RESP_OK;
                    }
                    break;
                case 0x03: // AEB Control
                    if (cmd_len >= 1) {
                        bool on = (cmd_value[0] != 0);
                        // TODO: AEB_Set(on);
                        std::cout << "[SRV] AEB: " << (on ? "ON" : "OFF") << "\n";
                        result = RESP_OK;
                    }
                    break;
                case 0x04: // AutoPark Control
                    if (cmd_len >= 1) {
                        uint8_t sub = cmd_value[0]; // 0x01 start, 0x00 cancel 등
                        // TODO: AutoPark_Command(sub);
                        std::cout << "[SRV] AutoPark cmd: 0x" << std::hex << int(sub) << std::dec << "\n";
                        result = RESP_OK;
                    }
                    break;
                case 0x05: { // Auth Password (ASCII)
                    std::string pwd(reinterpret_cast<const char*>(cmd_value), cmd_len);
                    // TODO: 실제 비교. 여기서는 "1234"만 OK
                    bool ok = (pwd == "1234");
                    std::cout << "[SRV] Auth pwd=\"" << pwd << "\" -> " << (ok ? "OK" : "FAIL") << "\n";
                    result = ok ? RESP_OK : RESP_ERR;
                    break;
                }
                case 0xFE: // Fault/E-Stop
                    if (cmd_len >= 1) {
                        uint8_t sub = cmd_value[0]; // 0x01 Emergency Stop 등
                        // TODO: Fault_Handle(sub);
                        std::cout << "[SRV] Fault cmd: 0x" << std::hex << int(sub) << std::dec << "\n";
                        result = RESP_OK;
                    }
                    break;
                default:
                    std::cerr << "[SRV] Unknown cmd_type: 0x" << std::hex << int(cmd_type) << std::dec << "\n";
            }
        }

        // 응답(선택) – 1바이트 OK/ERR
        auto resp_pl = vsomeip::runtime::get()->create_payload();
        uint8_t b = result;
        resp_pl->set_data(&b, 1);
        resp->set_payload(resp_pl);
        app_->send(resp);
    }

    std::shared_ptr<vsomeip::application> app_;
};

int main() {
    veh_server s;
    if (!s.init()) return 1;
    s.start();
    return 0;
}
