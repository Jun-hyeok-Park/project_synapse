/*
    CLI 전용 테스트 프로그램
    GUI 없이, 키보드 입력으로 vsomeip 명령만 빠르게 테스트
*/
#include <iostream>
#include <thread>
#include <memory>
#include <map>
#include <vsomeip/vsomeip.hpp>

#include "veh_control_service.hpp"
#include "veh_logger.hpp"
#include "veh_types.hpp"
#include "config.hpp"

using namespace std;

class VehCli {
private:
    shared_ptr<vsomeip::application> app;
    veh::Logger logger{veh::LOG_PATH_CLIENT};
    bool is_running = true;

public:
    VehCli() {
        app = vsomeip::runtime::get()->create_application(veh::VSOMEIP_CLIENT_NAME);
    }

    bool init() {
        LOG_INFO(logger, "Initializing veh_cli...");

        if (!app->init()) {
            LOG_ERROR(logger, "vsomeip init failed.");
            return false;
        }

        app->register_availability_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            std::bind(&VehCli::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );

        app->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            std::bind(&VehCli::on_response, this, std::placeholders::_1)
        );

        return true;
    }

    void start() {
        LOG_INFO(logger, "Starting veh_cli...");
        thread vsomeip_thread([&]() { app->start(); });

        this->command_loop();

        vsomeip_thread.join();
    }

private:
    void on_availability(vsomeip::service_t, vsomeip::instance_t, bool available) {
        if (available)
            LOG_INFO(logger, "veh_control_service available.");
        else
            LOG_WARN(logger, "veh_control_service unavailable.");
    }

    void on_response(const shared_ptr<vsomeip::message> &resp) {
        auto payload = resp->get_payload();
        if (payload->get_length() > 0) {
            uint8_t code = payload->get_data()[0];
            LOG_INFO(logger, string("Response from server: ") + (code == VEH_RESP_OK ? "OK" : "ERROR"));
        }
    }

    void send_command(uint8_t cmd_type, const vector<uint8_t>& value) {
        vector<vsomeip::byte_t> data(8, 0x00);
        data[0] = cmd_type;
        for (size_t i = 0; i < value.size() && i < 7; ++i)
            data[i + 1] = value[i];

        auto req = vsomeip::runtime::get()->create_request();
        req->set_service(VEH_CONTROL_SERVICE_ID);
        req->set_instance(VEH_CONTROL_INSTANCE_ID);
        req->set_method(VEH_CONTROL_METHOD_ID);
        auto payload = vsomeip::runtime::get()->create_payload(data);
        req->set_payload(payload);
        app->send(req);

        ostringstream oss;
        oss << "CLI → Sent [0x" << hex << (int)cmd_type << "] : ";
        for (auto b : data) oss << setw(2) << setfill('0') << hex << (int)b << " ";
        LOG_INFO(logger, oss.str());
    }

    void command_loop() {
        LOG_INFO(logger, "CLI started. Enter commands (q=exit):");
        cout << "\n==== Command List ====\n"
             << "1: Forward\n"
             << "2: Backward\n"
             << "3: Left\n"
             << "4: Right\n"
             << "5: Stop\n"
             << "6: AEB ON\n"
             << "7: AutoPark Start\n"
             << "p: Send Password (1234)\n"
             << "q: Quit\n"
             << "======================\n";

        char key;
        while (is_running && cin >> key) {
            switch (key) {
                case '1': send_command(0x01, {0x08}); break; // Forward
                case '2': send_command(0x01, {0x02}); break; // Backward
                case '3': send_command(0x01, {0x04}); break; // Left
                case '4': send_command(0x01, {0x06}); break; // Right
                case '5': send_command(0x01, {0x05}); break; // Stop
                case '6': send_command(0x03, {0x01}); break; // AEB ON
                case '7': send_command(0x04, {0x01}); break; // AutoPark Start
                case 'p': send_command(0x05, {'1','2','3','4'}); break; // Auth
                case 'q': 
                    LOG_INFO(logger, "Exiting CLI...");
                    is_running = false; 
                    app->stop();
                    return;
                default:
                    LOG_WARN(logger, "Unknown command input.");
                    break;
            }
        }
    }
};

// =======================================================
// main()
// =======================================================
int main() {
    veh::print_config_summary();

    VehCli cli;
    if (!cli.init()) return -1;
    cli.start();

    return 0;
}
