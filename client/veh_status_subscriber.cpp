#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <memory>

#include "veh_status_service.hpp"
#include "veh_logger.hpp"
#include "veh_types.hpp"
#include "config.hpp"

using namespace std;

// =======================================================
// veh_status_subscriber — vsomeip Event Subscriber
// 서버가 방송(Event)하는 ToF 거리, AEB 상태, Auth 결과 등의 상태를 실시간으로 받아서 출력
// =======================================================

class VehStatusSubscriber {
private:
    shared_ptr<vsomeip::application> app;
    veh::Logger logger{veh::LOG_PATH_CLIENT};
    bool running = true;

public:
    VehStatusSubscriber()
        : app(vsomeip::runtime::get()->create_application(veh::VSOMEIP_STATUS_CLIENT)) {}

    bool init() {
        LOG_INFO(logger, "Initializing veh_status_subscriber...");

        if (!app->init()) {
            LOG_ERROR(logger, "vsomeip init failed.");
            return false;
        }

        // 서비스 가용성 감시
        app->register_availability_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            std::bind(&VehStatusSubscriber::on_availability, this, placeholders::_1, placeholders::_2, placeholders::_3)
        );

        // 이벤트 수신 핸들러 등록
        app->register_message_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            std::bind(&VehStatusSubscriber::on_event, this, placeholders::_1)
        );

        LOG_INFO(logger, "veh_status_subscriber initialized successfully.");
        return true;
    }

    void start() {
        LOG_INFO(logger, "Starting veh_status_subscriber...");
        thread vsomeip_thread([&]() { app->start(); });

        while (running) {
            this_thread::sleep_for(chrono::seconds(1));
        }

        vsomeip_thread.join();
    }

private:
    // ──────────────── 가용성 콜백 ────────────────
    void on_availability(vsomeip::service_t, vsomeip::instance_t, bool available) {
        if (available) {
            LOG_INFO(logger, "veh_status_service available — subscribing to events...");
            app->request_event(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID,
                               VEH_STATUS_EVENT_ID, {VEH_STATUS_EVENTGROUP_ID}, true);
            app->subscribe(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID,
                           VEH_STATUS_EVENTGROUP_ID);
        } else {
            LOG_WARN(logger, "veh_status_service unavailable.");
        }
    }

    // ──────────────── 이벤트 수신 ────────────────
    void on_event(const shared_ptr<vsomeip::message>& msg) {
        auto payload = msg->get_payload();
        auto data = payload->get_data();
        size_t len = payload->get_length();

        if (len < 2) return;

        uint8_t type = data[0];
        LOG_INFO(logger, "Received status event type: 0x" + to_hex(type));

        switch (type) {
            case 0x01: handle_drive_status(data, len); break;
            case 0x02: handle_aeb_status(data, len); break;
            case 0x03: handle_autopark_status(data, len); break;
            case 0x04: handle_tof_status(data, len); break;
            case 0x05: handle_fault_status(data, len); break;
            case 0x06: handle_auth_status(data, len); break;
            default:
                LOG_WARN(logger, "Unknown status_type received.");
                break;
        }
    }

    // ──────────────── 상태별 처리 함수 ────────────────
    void handle_drive_status(const uint8_t* data, size_t len) {
        uint8_t dir = data[1];
        string desc;
        switch (dir) {
            case 0x08: desc = "Forward"; break;
            case 0x02: desc = "Backward"; break;
            case 0x04: desc = "Left"; break;
            case 0x06: desc = "Right"; break;
            case 0x05: desc = "Stop"; break;
            default: desc = "Unknown";
        }
        LOG_INFO(logger, "[Drive] Direction = " + desc);
    }

    void handle_aeb_status(const uint8_t* data, size_t len) {
        string state = (data[1] == 0x01) ? "ON" : "OFF";
        LOG_INFO(logger, "[AEB] State = " + state);
    }

    void handle_autopark_status(const uint8_t* data, size_t len) {
        uint8_t step = data[1];
        LOG_INFO(logger, "[AutoPark] Step = " + to_string(step));
    }

    void handle_tof_status(const uint8_t* data, size_t len) {
        if (len >= 3) {
            uint16_t dist = (data[1] << 8) | data[2];
            LOG_INFO(logger, "[ToF] Distance = " + to_string(dist) + " cm");
        }
    }

    void handle_fault_status(const uint8_t* data, size_t len) {
        uint8_t code = data[1];
        LOG_ERROR(logger, "[Fault] Code = 0x" + to_hex(code));
    }

    void handle_auth_status(const uint8_t* data, size_t len) {
        string result = (data[1] == 0x01) ? "SUCCESS" : "FAIL";
        LOG_INFO(logger, "[Auth] Result = " + result);
    }

    // ──────────────── 유틸: HEX 변환 ────────────────
    static string to_hex(uint8_t v) {
        ostringstream oss;
        oss << hex << uppercase << setw(2) << setfill('0') << (int)v;
        return oss.str();
    }
};

// =======================================================
// main()
// =======================================================
int main() {
    veh::print_config_summary();

    VehStatusSubscriber sub;
    if (!sub.init()) return -1;

    sub.start();
    return 0;
}
