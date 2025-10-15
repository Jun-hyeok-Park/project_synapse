#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <memory>

#include "veh_control_service.hpp"
#include "veh_logger.hpp"
#include "veh_types.hpp"
#include "config.hpp"

using namespace std;

// =======================================================
// veh_control_client — vsomeip Client
// =======================================================

class VehControlClient {
private:
    std::shared_ptr<vsomeip::application> app;
    veh::Logger logger{veh::LOG_PATH_CLIENT};

public:
    VehControlClient()
        : app(vsomeip::runtime::get()->create_application(veh::VSOMEIP_CLIENT_NAME)) {}

    bool init() {
        LOG_INFO(logger, "Initializing veh_control_client...");

        if (!app->init()) {
            LOG_ERROR(logger, "vsomeip init failed.");
            return false;
        }

        app->register_availability_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            std::bind(&VehControlClient::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );

        app->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            std::bind(&VehControlClient::on_response, this, std::placeholders::_1)
        );

        LOG_INFO(logger, "veh_control_client initialized.");
        return true;
    }

    void start() {
        LOG_INFO(logger, "Starting veh_control_client...");
        app->start();
    }

    void on_availability(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
        if (is_available) {
            LOG_INFO(logger, "veh_control_service available! Ready to send commands.");
        } else {
            LOG_WARN(logger, "veh_control_service not available.");
        }
    }

    void on_response(const std::shared_ptr<vsomeip::message>& response) {
        auto payload = response->get_payload();
        auto data = payload->get_data();
        if (payload->get_length() > 0) {
            uint8_t result = data[0];
            LOG_INFO(logger, std::string("Response from server: ") + (result == VEH_RESP_OK ? "OK" : "ERROR"));
        }
    }

    // ──────────────── 명령 전송 ────────────────
    void send_command(uint8_t cmd_type, const std::vector<uint8_t>& values) {
        std::vector<vsomeip::byte_t> payload_data(8, 0x00);
        payload_data[0] = cmd_type;
        for (size_t i = 0; i < values.size() && i < 7; ++i)
            payload_data[i + 1] = values[i];

        auto req = vsomeip::runtime::get()->create_request();
        req->set_service(VEH_CONTROL_SERVICE_ID);
        req->set_instance(VEH_CONTROL_INSTANCE_ID);
        req->set_method(VEH_CONTROL_METHOD_ID);

        auto payload = vsomeip::runtime::get()->create_payload(payload_data);
        req->set_payload(payload);

        app->send(req);

        std::ostringstream oss;
        oss << "Sent Command [0x" << std::hex << (int)cmd_type << "] : ";
        for (auto b : payload_data) oss << std::setw(2) << std::setfill('0') << std::hex << (int)b << " ";
        LOG_INFO(logger, oss.str());
    }
};

// =======================================================
// main()
// =======================================================
int main() {
    veh::print_config_summary();

    VehControlClient client;
    if (!client.init()) return -1;

    std::thread vsomeip_thread([&client]() { client.start(); });

    std::this_thread::sleep_for(std::chrono::seconds(2)); // vsomeip 연결 안정화

    LOG_INFO(client_logger, "Command input mode started.");
    LOG_INFO(client_logger, "예시: 1=전진, 2=후진, 3=AEB ON, 4=AutoPark Start, q=종료");

    char key;
    while (std::cin >> key) {
        if (key == 'q') break;

        switch (key) {
            case '1': client.send_command(0x01, {0x08}); break; // Forward
            case '2': client.send_command(0x01, {0x02}); break; // Backward
            case '3': client.send_command(0x03, {0x01}); break; // AEB ON
            case '4': client.send_command(0x04, {0x01}); break; // AutoPark Start
            default: LOG_WARN(client_logger, "Unknown input."); break;
        }
    }

    vsomeip_thread.join();
    return 0;
}
