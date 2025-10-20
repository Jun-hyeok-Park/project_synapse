#include <vsomeip/vsomeip.hpp>
#include <csignal>
#include <iostream>
#include <thread>
#include <atomic>
#include <sstream>
#include "veh_logger.hpp"
#include "veh_status_service.hpp"

namespace {
constexpr vsomeip::service_t  SERVICE_ID  = VEH_STATUS_SERVICE_ID;
constexpr vsomeip::instance_t INSTANCE_ID = VEH_STATUS_INSTANCE_ID;
constexpr vsomeip::event_t    EVENT_ID    = VEH_STATUS_EVENT_ID;
constexpr vsomeip::eventgroup_t EVENT_GROUP = VEH_STATUS_EVENTGROUP_ID;

std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }
}

class VehStatusSubscriber {
public:
    VehStatusSubscriber()
        : app_(vsomeip::runtime::get()->create_application("veh_client")) {}

    bool init() {
        if (!app_->init()) return false;

        app_->register_state_handler(
            std::bind(&VehStatusSubscriber::on_state, this, std::placeholders::_1));

        app_->register_availability_handler(
            SERVICE_ID, INSTANCE_ID,
            std::bind(&VehStatusSubscriber::on_availability, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        app_->register_message_handler(
            SERVICE_ID, INSTANCE_ID, EVENT_ID,
            std::bind(&VehStatusSubscriber::on_event, this, std::placeholders::_1));

        return true;
    }

    void start() {
        std::thread([&]() { app_->start(); }).detach();
        while (g_running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

private:
    void on_state(vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(SERVICE_ID, INSTANCE_ID);
        }
    }

    void on_availability(vsomeip::service_t, vsomeip::instance_t, bool available) {
        if (available) {
            app_->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENT_GROUP},
                                vsomeip::event_type_e::ET_EVENT,
                                vsomeip::reliability_type_e::RT_UNRELIABLE);
            app_->subscribe(SERVICE_ID, INSTANCE_ID, EVENT_GROUP);
            std::cout << "[SUB] Connected to VEH_STATUS service." << std::endl;
        } else {
            std::cout << "[SUB] Service unavailable." << std::endl;
        }
    }

    void on_event(const std::shared_ptr<vsomeip::message>& msg) {
        auto pl = msg->get_payload();
        if (!pl || pl->get_length() < 2) return;

        const auto *data = pl->get_data();
        uint8_t type = data[0];
        const uint8_t *val = data + 1;
        size_t len = pl->get_length() - 1;

        switch (type) {
            case (uint8_t)StatusType::AEB_STATE:
                std::cout << "[EVT] AEB_STATE → " << (val[0] ? "ON" : "OFF") << std::endl;
                break;

            case (uint8_t)StatusType::AUTOPARK_STATE:
                std::cout << "[EVT] AUTOPARK_STATE → Step " << (int)val[0] << std::endl;
                break;

            case (uint8_t)StatusType::TOF_DISTANCE: {
                // ToF 거리(mm): 3바이트 Big Endian
                uint32_t mm = (val[0] << 16) | (val[1] << 8) | val[2];
                std::cout << "[EVT] TOF_DISTANCE → " << mm << " mm" << std::endl;
                break;
            }

            case (uint8_t)StatusType::AUTH_STATE:
                std::cout << "[EVT] AUTH_STATE → " << (val[0] ? "SUCCESS" : "FAIL") << std::endl;
                break;

            default:
                std::cout << "[EVT] Unknown TYPE=0x" << std::hex << (int)type
                          << std::dec << " (len=" << len << ")" << std::endl;
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
};

int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    VehStatusSubscriber node;
    if (!node.init()) return 1;
    node.start();
    return 0;
}
