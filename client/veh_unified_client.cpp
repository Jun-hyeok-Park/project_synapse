#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <signal.h>
#include <poll.h>
#include <cstdlib>
#include <csignal>

#include "veh_logger.hpp"
#include "veh_control_service.hpp"
#include "veh_status_service.hpp"

using namespace std::chrono_literals;

veh::Logger g_logger("logs/veh_unified_client.log");
std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }

class VehUnifiedClient {
public:
    VehUnifiedClient()
    : app_(vsomeip::runtime::get()->create_application("veh_unified_client")) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(g_logger, "vsomeip init failed");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                LOG_INFO(g_logger, "Client registered.");
                app_->request_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
                app_->request_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

                app_->request_event(
                    VEH_STATUS_SERVICE_ID,
                    VEH_STATUS_INSTANCE_ID,
                    VEH_STATUS_EVENT_ID,
                    { VEH_STATUS_EVENTGROUP_ID },
                    vsomeip::event_type_e::ET_EVENT,
                    vsomeip::reliability_type_e::RT_UNRELIABLE);
                app_->subscribe(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENTGROUP_ID);
            }
        });

        app_->register_message_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            [this](const std::shared_ptr<vsomeip::message> &msg) {
                on_event(msg);
            });

        return true;
    }

    void start() {
        std::thread([&]() { app_->start(); }).detach();

        std::cout << "\n=== Unified Client Console ===\n";
        std::cout << "1: Forward, 2: Backward, 3: Stop, 4: AEB ON, 5: AutoPark START, 6: Exit\n";

        while (g_running) {
            int cmd; std::cin >> cmd;
            if (cmd == 6) { g_running = false; break; }

            switch (cmd) {
                case 1: send_command(CmdType::DRIVE_DIRECTION, {0x08}); break;
                case 2: send_command(CmdType::DRIVE_DIRECTION, {0x02}); break;
                case 3: send_command(CmdType::DRIVE_DIRECTION, {0x05}); break;
                case 4: send_command(CmdType::AEB_CONTROL, {0x01}); break;
                case 5: send_command(CmdType::AUTOPARK_CONTROL, {0x01}); break;
                default: break;
            }
        }

        app_->stop();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    void send_command(CmdType type, const std::vector<uint8_t>& val) {
        auto msg = vsomeip::runtime::get()->create_request();
        msg->set_service(VEH_CONTROL_SERVICE_ID);
        msg->set_instance(VEH_CONTROL_INSTANCE_ID);
        msg->set_method(VEH_CONTROL_METHOD_ID);

        std::vector<uint8_t> payload;
        payload.push_back(static_cast<uint8_t>(type));
        payload.insert(payload.end(), val.begin(), val.end());
        msg->set_payload(vsomeip::runtime::get()->create_payload(payload));

        app_->send(msg);

        std::ostringstream oss;
        oss << "[REQ] TYPE=0x" << std::hex << (int)type << " DATA=[";
        for (auto b : val) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
        oss << "]";
        LOG_INFO(g_logger, oss.str());
    }

    void on_event(const std::shared_ptr<vsomeip::message>& msg) {
        auto pl = msg->get_payload();
        if (!pl || pl->get_length() < 2) return;
        auto data = pl->get_data();

        uint8_t type = data[0];
        std::vector<uint8_t> val(data + 1, data + pl->get_length());

        std::ostringstream oss;
        oss << "[EVT] TYPE=0x" << std::hex << (int)type << " DATA=[";
        for (auto b : val) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
        oss << "]";
        LOG_INFO(g_logger, oss.str());

        std::cout << oss.str() << std::endl;
    }
};

int main() {
    signal(SIGINT, on_signal);
    VehUnifiedClient client;
    if (!client.init()) return -1;

    std::thread app_thread([&]() { client.start(); });

    while (g_running)
        std::this_thread::sleep_for(100ms);

    std::cout << "\n[INFO] Graceful shutdown..." << std::endl;
    app_thread.join();
    return 0;
}

