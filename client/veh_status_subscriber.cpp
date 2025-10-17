#include <vsomeip/vsomeip.hpp>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>

#include "veh_logger.hpp"
#include "veh_status_service.hpp"  // IDs / enums
#include "veh_types.hpp"

namespace {

constexpr vsomeip::service_t  SERVICE_ID  = 0x1200; // veh_status_service
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::event_t    EVENT_ID    = 0x0200;
constexpr vsomeip::eventgroup_t EVENT_GROUP = 0x1001;

// 종료 제어 플래그
std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }

} // namespace

class VehStatusSubscriber {
public:
    VehStatusSubscriber()
        : app_(vsomeip::runtime::get()->create_application("veh_client")),
          is_registered_(false) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(logger_, "[ERROR] app_->init() failed");
            return false;
        }

        // 상태 핸들러 등록
        app_->register_state_handler(
            std::bind(&VehStatusSubscriber::on_state, this, std::placeholders::_1));

        // 서비스 가용성 핸들러 등록 (Offer 감지 시점)
        app_->register_availability_handler(
            SERVICE_ID, INSTANCE_ID,
            std::bind(&VehStatusSubscriber::on_availability, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 이벤트(Notification) 수신 핸들러 등록
        app_->register_message_handler(
            SERVICE_ID, INSTANCE_ID, EVENT_ID,
            std::bind(&VehStatusSubscriber::on_event, this, std::placeholders::_1));

        return true;
    }

    void start() {
        // 별도 스레드에서 vsomeip 런타임 실행
        std::thread([this]() { app_->start(); }).detach();

        // 메인 스레드는 종료 신호 대기
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        LOG_INFO(logger_, "[INFO] veh_status_subscriber shutting down...");
        app_->stop();
    }

private:
    // vsomeip 상태 콜백 (REGISTERED / DEREGISTERED)
    void on_state(vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED && !is_registered_) {
            is_registered_ = true;
            LOG_INFO(logger_, "[INFO] App registered, requesting service...");
            app_->request_service(SERVICE_ID, INSTANCE_ID);
        } else if (state == vsomeip::state_type_e::ST_DEREGISTERED) {
            LOG_WARN(logger_, "[WARN] App deregistered, releasing service...");
            app_->release_service(SERVICE_ID, INSTANCE_ID);
        }
    }

    // 서비스 가용성 콜백
    void on_availability(vsomeip::service_t sid, vsomeip::instance_t iid, bool available) {
        if (available) {
            std::ostringstream oss;
            oss << "[INFO] Service available: 0x" << std::hex << sid
                << " / instance 0x" << iid << " — subscribing event...";
            LOG_INFO(logger_, oss.str());

            // 이벤트 요청 + 구독
            app_->request_event(
                SERVICE_ID,
                INSTANCE_ID,
                EVENT_ID,
                {EVENT_GROUP},
                vsomeip::event_type_e::ET_EVENT,
                vsomeip::reliability_type_e::RT_UNRELIABLE
            );

            app_->subscribe(SERVICE_ID, INSTANCE_ID, EVENT_GROUP);
        } else {
            std::ostringstream oss;
            oss << "[WARN] Service 0x" << std::hex << sid
                << " / instance 0x" << iid << " unavailable";
            LOG_WARN(logger_, oss.str());
        }
    }

    // 이벤트 수신 콜백
    void on_event(const std::shared_ptr<vsomeip::message>& msg) {
        auto pl = msg->get_payload();
        auto data = pl ? pl->get_data() : nullptr;
        auto len = pl ? pl->get_length() : 0u;

        if (!data || len < 1) {
            LOG_WARN(logger_, "[WARN] Empty event payload");
            return;
        }

        uint8_t status_type = data[0];
        const uint8_t* value = data + 1;
        std::size_t value_len = len - 1;

        parse_and_print(status_type, value, value_len);
    }

    // 수신된 상태값 파싱 및 출력
    void parse_and_print(uint8_t type, const uint8_t* val, std::size_t n) {
        std::ostringstream oss;

        switch (type) {
            case 0x01: { // Drive 상태
                if (n < 1) break;
                oss << "[EVT] Drive=0x" << std::hex << std::uppercase << (int)val[0];
                break;
            }
            case 0x02: { // AEB 상태
                if (n < 1) break;
                oss << "[EVT] AEB=" << (val[0] ? "ON" : "OFF");
                break;
            }
            case 0x03: { // AutoPark 단계
                if (n < 1) break;
                oss << "[EVT] AutoPark step=" << std::dec << (int)val[0];
                break;
            }
            case 0x04: { // ToF 거리
                if (n < 2) break;
                uint16_t cm = (val[0] << 8) | val[1];
                oss << "[EVT] ToF=" << cm << " cm";
                break;
            }
            case 0x05: { // Fault 코드
                if (n < 1) break;
                oss << "[EVT] Fault=0x" << std::hex << std::uppercase << (int)val[0];
                break;
            }
            case 0x06: { // Auth 상태
                if (n < 1) break;
                oss << "[EVT] Auth=" << (val[0] ? "SUCCESS" : "FAIL");
                break;
            }
            default: {
                oss << "[EVT] Unknown type=0x" << std::hex << std::uppercase << (int)type
                    << " (len=" << std::dec << n << ")";
                break;
            }
        }

        LOG_INFO(logger_, oss.str());
    }

private:
    veh::Logger logger_{"logs/veh_status_subscriber.log"};
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
};

int main() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    VehStatusSubscriber node;
    if (!node.init())
        return 1;

    node.start();
    return 0;
}
