#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <csignal>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "veh_logger.hpp"
#include "veh_control_service.hpp"
#include "veh_status_service.hpp"

using namespace std::chrono_literals;

veh::Logger g_logger("logs/veh_unified_server.log");
std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }

class VehUnifiedServer {
public:
    VehUnifiedServer()
    : app_(vsomeip::runtime::get()->create_application("veh_unified_server")) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(g_logger, "vsomeip init failed");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                offer_services();
        });

        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            [this](const std::shared_ptr<vsomeip::message> &req) {
                on_control_request(req);
            });

        return true;
    }

    void start() {
        std::thread([&]() { app_->start(); }).detach();
        std::thread([&]() { can_listener_loop(); }).detach();

        while (g_running)
            std::this_thread::sleep_for(200ms);

        app_->stop_offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        app_->stop_offer_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);
        app_->stop();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    void offer_services() {
        LOG_INFO(g_logger, "Offering veh_control_service + veh_status_service");

        // Control Service (Request/Response)
        app_->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);

        // Status Service (Event)
        app_->offer_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);
        app_->offer_event(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            { VEH_STATUS_EVENTGROUP_ID },
            vsomeip::event_type_e::ET_EVENT,
            std::chrono::milliseconds::zero(),
            false, true);
    }

    // SOME/IP Request 수신 (Client → Server)
    void on_control_request(const std::shared_ptr<vsomeip::message> &req) {
        auto payload = req->get_payload();
        auto data = payload->get_data();
        auto len = payload->get_length();
        if (len < 1) return;

        uint8_t cmd_type = data[0];
        std::vector<uint8_t> cmd_value(data + 1, data + len);

        std::ostringstream oss;
        oss << "[REQ] cmd_type=0x" << std::hex << (int)cmd_type << " len=" << std::dec << (len - 1);
        LOG_INFO(g_logger, oss.str());

        // CAN 송신 시뮬레이션 (로그만 출력)
        oss.str("");
        oss << "[CAN TX] ID=0x200 TYPE=0x" << std::hex << (int)cmd_type << " DATA=[";
        for (auto b : cmd_value) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
        oss << "]";
        LOG_INFO(g_logger, oss.str());

        // Response 송신
        auto resp = vsomeip::runtime::get()->create_response(req);
        resp->set_payload(vsomeip::runtime::get()->create_payload({ VEH_RESP_OK }));
        app_->send(resp);
    }

    // CAN 수신 → SOME/IP Event 송신
    void can_listener_loop() {
        int s;
        struct sockaddr_can addr{};
        struct ifreq ifr{};
        struct can_frame frame{};

        s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s < 0) {
            perror("socket");
            return;
        }

        strcpy(ifr.ifr_name, "can0");
        ioctl(s, SIOCGIFINDEX, &ifr);
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(s);
            return;
        }

        LOG_INFO(g_logger, "[CAN] Listening on can0 (ID=0x310)");

        while (g_running) {
            int nbytes = read(s, &frame, sizeof(frame));
            if (nbytes < 0) continue;

            if (frame.can_id == 0x310 && frame.can_dlc >= 2) {
                uint8_t status_type = frame.data[0];
                std::vector<uint8_t> val(frame.data + 1, frame.data + frame.can_dlc);

                publish_status(status_type, val);
            }
        }

        close(s);
    }

    void publish_status(uint8_t type, const std::vector<uint8_t>& val) {
        std::vector<uint8_t> payload;
        payload.push_back(type);
        payload.insert(payload.end(), val.begin(), val.end());

        auto pl = vsomeip::runtime::get()->create_payload(payload);
        app_->notify(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENT_ID, pl);

        std::ostringstream oss;
        oss << "[EVT] Notify TYPE=0x" << std::hex << (int)type << " DATA=[";
        for (auto b : val) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
        oss << "]";
        LOG_INFO(g_logger, oss.str());
    }
};

int main() {
    signal(SIGINT, on_signal);
    VehUnifiedServer server;
    if (!server.init()) return -1;
    server.start();
    return 0;
}
