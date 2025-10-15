#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <vector>

#include "veh_control_service.hpp"
#include "veh_types.hpp"
#include "veh_can.hpp"
#include "veh_logger.hpp"
#include "config.hpp"

using namespace std;

// =======================================================
// veh_control_server — vsomeip Server
// =======================================================

class VehControlServer {
private:
    std::shared_ptr<vsomeip::application> app;
    CanInterface can;
    veh::Logger logger{veh::LOG_PATH_SERVER};

public:
    VehControlServer()
        : app(vsomeip::runtime::get()->create_application(veh::VSOMEIP_SERVER_NAME)) {}

    // ──────────────── 초기화 ────────────────
    bool init() {
        LOG_INFO(logger, "Initializing veh_control_server...");

        if (!app->init()) {
            LOG_ERROR(logger, "vsomeip application init failed!");
            return false;
        }

        // vsomeip 등록
        app->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            std::bind(&VehControlServer::on_message, this, std::placeholders::_1)
        );

        // CAN 초기화
        if (!can.init()) {
            LOG_ERROR(logger, "CAN interface initialization failed.");
            return false;
        }

        LOG_INFO(logger, "veh_control_server initialized successfully.");
        return true;
    }

    // ──────────────── 메시지 수신 콜백 ────────────────
    void on_message(const std::shared_ptr<vsomeip::message> &msg) {
        const auto payload = msg->get_payload();
        auto data = payload->get_data();
        size_t len = payload->get_length();

        if (len < 1) return;

        uint8_t cmd_type = data[0];
        LOG_INFO(logger, "Received Command Type: 0x" + to_hex(cmd_type));

        // CAN 프레임 구성
        CanFrame frame;
        frame.id = veh::CAN_ID_CONTROL_TX;
        frame.dlc = 8;
        std::fill(frame.data.begin(), frame.data.end(), 0x00);
        for (size_t i = 0; i < len && i < frame.data.size(); ++i)
            frame.data[i] = data[i];

        // CAN 전송
        bool tx_ok = can.sendFrame(frame);

        // 응답 메시지 생성
        auto resp = vsomeip::runtime::get()->create_response(msg);
        std::vector<vsomeip::byte_t> resp_data(1, tx_ok ? VEH_RESP_OK : VEH_RESP_ERR);
        auto resp_payload = vsomeip::runtime::get()->create_payload(resp_data);
        resp->set_payload(resp_payload);
        app->send(resp);

        LOG_INFO(logger, "Response sent to client: " + std::string(tx_ok ? "OK" : "ERROR"));
    }

    // ──────────────── 서비스 제공 시작 ────────────────
    void start() {
        LOG_INFO(logger, "Offering veh_control_service...");
        app->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        app->start();
    }

    // ──────────────── 유틸: HEX 변환 ────────────────
    static std::string to_hex(uint8_t val) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)val;
        return oss.str();
    }
};

// =======================================================
// main()
// =======================================================
int main() {
    veh::print_config_summary();

    VehControlServer server;
    if (!server.init()) {
        std::cerr << "Server initialization failed." << std::endl;
        return -1;
    }

    server.start();
    return 0;
}
