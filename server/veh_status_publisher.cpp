#include <vsomeip/vsomeip.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>
#include <sstream>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>

#include "veh_logger.hpp"
#include "veh_status_service.hpp"

using namespace std::chrono_literals;

namespace {
constexpr vsomeip::service_t  SERVICE_ID  = VEH_STATUS_SERVICE_ID;
constexpr vsomeip::instance_t INSTANCE_ID = VEH_STATUS_INSTANCE_ID;
constexpr vsomeip::event_t    EVENT_ID    = VEH_STATUS_EVENT_ID;
constexpr vsomeip::eventgroup_t EVENT_GROUP = VEH_STATUS_EVENTGROUP_ID;

std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }

constexpr std::chrono::milliseconds LOOP_DELAY{10};
} // namespace


class VehStatusPublisher {
public:
    VehStatusPublisher()
        : app_(vsomeip::runtime::get()->create_application("veh_server")) {}

    bool init() {
        if (!app_->init()) return false;
        app_->register_state_handler(
            std::bind(&VehStatusPublisher::on_state, this, std::placeholders::_1));
        return true;
    }

    void start() {
        std::thread([&]() { app_->start(); }).detach();
        while (g_running.load())
            std::this_thread::sleep_for(100ms);
    }

private:
    void on_state(vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->offer_service(SERVICE_ID, INSTANCE_ID);
            app_->offer_event(
                SERVICE_ID, INSTANCE_ID, EVENT_ID,
                {EVENT_GROUP},
                vsomeip::event_type_e::ET_EVENT,
                std::chrono::milliseconds::zero(),
                false, true);

            pub_thread_ = std::thread(&VehStatusPublisher::publish_loop, this);
        }
    }

    std::shared_ptr<vsomeip::payload>
    make_payload(uint8_t type, const std::vector<uint8_t>& val) {
        std::vector<uint8_t> data;
        data.push_back(type);
        data.insert(data.end(), val.begin(), val.end());
        auto pl = vsomeip::runtime::get()->create_payload();
        pl->set_data(data);
        return pl;
    }

    void publish_once(uint8_t type, const std::vector<uint8_t>& val) {
        auto payload = make_payload(type, val);
        app_->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, payload);

        std::ostringstream oss;
        oss << "[CAN→SOME/IP] Notify: type=0x"
            << std::hex << (int)type << " data=[";
        for (auto b : val) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
        oss << "]";
        std::cout << oss.str() << std::endl;
    }

    // ============================================================
    // SocketCAN 기반 publish_loop
    // ============================================================
    void publish_loop() {
        int s;
        struct sockaddr_can addr{};
        struct ifreq ifr{};
        struct can_frame frame{};

        // CAN 소켓 초기화
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

        std::cout << "[CAN] Listening on can0 (ID=0x210)...\n";

        while (g_running.load()) {
            int nbytes = read(s, &frame, sizeof(frame));
            if (nbytes < 0) continue;

            // TC375 → 0x310 : [status_type][payload...]
            if (frame.can_id == 0x310 && frame.can_dlc >= 2) {
                uint8_t status_type = frame.data[0];
                std::vector<uint8_t> payload(frame.data + 1, frame.data + frame.can_dlc);

                publish_once(status_type, payload);

                // 콘솔 로그 (디버깅용)
                std::ostringstream oss;
                oss << "[CAN→SOME/IP] TYPE=0x"
                    << std::hex << std::setw(2) << std::setfill('0')
                    << (int)status_type << " DATA=[";
                for (size_t i = 1; i < frame.can_dlc; ++i)
                    oss << std::hex << std::setw(2) << std::setfill('0')
                        << (int)frame.data[i] << (i < frame.can_dlc - 1 ? " " : "");
                oss << "]";
                std::cout << oss.str() << std::endl;
            }

            std::this_thread::sleep_for(LOOP_DELAY);
        }

        close(s);
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread pub_thread_;
};

int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    VehStatusPublisher node;
    if (!node.init()) return 1;
    node.start();
    return 0;
}
