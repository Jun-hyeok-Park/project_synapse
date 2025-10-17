#include <vsomeip/vsomeip.hpp>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>

#include "veh_logger.hpp"
#include "veh_status_service.hpp"  // IDs / enums (status_type 등)
#include "veh_types.hpp"           // 공용 타입 있으면 사용
#include "veh_can.hpp"             // (선택) 센서 연동 예시 시 가정

using namespace std::chrono_literals;

namespace {

// Service / Instance / Event 정의
constexpr vsomeip::service_t  SERVICE_ID  = 0x1200; // veh_status_service
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
constexpr vsomeip::event_t    EVENT_ID    = 0x0200;

// Event Group 하나만 사용 (필요시 여러 개 확장 가능)
constexpr vsomeip::eventgroup_t EVENT_GROUP = 0x1001;

// 기본 주기 500 ms (문서 규격)
constexpr std::chrono::milliseconds PERIOD{500};

// graceful stop 플래그
std::atomic<bool> g_running{true};

// Ctrl+C, SIGTERM 수신 시 종료 플래그 설정
void on_signal(int) {
    std::cout << "\n[INFO] Received termination signal. Stopping veh_status_publisher..." << std::endl;
    g_running = false;
}

} // namespace

class VehStatusPublisher {
public:
    VehStatusPublisher()
        : app_(vsomeip::runtime::get()->create_application("veh_server")),
          is_registered_(false) {}

    bool init() {
        // 로그 시작 안내
        {
            std::ostringstream oss;
            oss << "[INFO] Starting veh_status_publisher...";
            LOG_INFO(logger_, oss.str());
        }

        if (!app_->init()) {
            std::ostringstream oss;
            oss << "[ERROR] app_->init() failed";
            LOG_ERROR(logger_, oss.str());
            return false;
        }

        // vsomeip 상태 콜백 등록
        app_->register_state_handler(
            std::bind(&VehStatusPublisher::on_state, this, std::placeholders::_1));

        // 이벤트 수신 핸들러는 필요 없음(서버=Publisher)
        return true;
    }

    void start() {
        // 서비스 오퍼는 on_state에서 처리
        app_->start();
    }

    // Graceful stop (Ctrl+C 눌렀을 때 호출됨)
    void stop() {
        g_running = false;

        // 퍼블리시 스레드 종료 대기
        if (pub_thread_.joinable()) {
            pub_thread_.join();
        }

        // 서비스 중지 및 vsomeip 종료
        app_->stop_offer_service(SERVICE_ID, INSTANCE_ID);
        app_->stop();

        LOG_INFO(logger_, "[INFO] veh_status_publisher stopped cleanly.");
    }

private:
    // vsomeip 상태 변경 콜백 (REGISTERED 시 서비스 제공 시작)
    void on_state(vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED && !is_registered_) {
            is_registered_ = true;

            // 서비스 제공
            app_->offer_service(SERVICE_ID, INSTANCE_ID);

            // 이벤트 오퍼
            app_->offer_event(
                SERVICE_ID, INSTANCE_ID, EVENT_ID,
                {EVENT_GROUP},
                vsomeip::event_type_e::ET_EVENT,
                std::chrono::milliseconds::zero(),
                false,   // unreliable (UDP) — 문서 QoS 규격
                true     // enable event
            );

            // 퍼블리시 루프 시작(주기 500ms + on-change 예시 일부)
            pub_thread_ = std::thread(&VehStatusPublisher::publish_loop, this);
        } else if (state == vsomeip::state_type_e::ST_DEREGISTERED) {
            // 연결 해제 시 스레드 정리
            g_running = false;
            if (pub_thread_.joinable()) pub_thread_.join();

            app_->stop_offer_service(SERVICE_ID, INSTANCE_ID);
        }
    }

    // payload helper: [status_type(1B)][status_value...]
    std::shared_ptr<vsomeip::payload> make_payload(uint8_t status_type,
                                                   const std::vector<uint8_t>& value) {
        std::vector<uint8_t> data;
        data.reserve(1 + value.size());
        data.push_back(status_type);
        data.insert(data.end(), value.begin(), value.end());

        auto pl = vsomeip::runtime::get()->create_payload();
        pl->set_data(data);
        return pl;
    }

    // 단일 이벤트 송신
    void publish_once(uint8_t status_type, const std::vector<uint8_t>& value) {
        auto pl = make_payload(status_type, value);
        app_->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, pl);
    }

    // 이벤트 주기 송신 루프
    void publish_loop() {
        // 간단한 데모용 시뮬레이션 값들
        // - Drive 상태(0x01): Forward(0x08) / Stop(0x05) 토글
        // - AEB 상태(0x02): ON/OFF 토글
        // - AutoPark 단계(0x03): 1~4 순환
        // - ToF 거리(0x04): 100~130cm 사이 왕복

        uint8_t drive = 0x05; // Stop
        bool aeb_on = false;
        uint8_t autopark_step = 0x01; // 1~4
        int tof = 110; // cm
        int dir = +1;

        // 주기적으로 이벤트 보내며, 값이 바뀌는 순간 on-change처럼 동작
        while (g_running.load()) {
            static int tick = 0;

            // 1) Drive 상태 (on-change 예시: 3주기마다 토글)
            if ((tick % 3) == 0) {
                drive = (drive == 0x05 ? 0x08 : 0x05); // Stop <-> Forward
                publish_once(0x01, {drive});
                {
                    std::ostringstream oss;
                    oss << "[PUB] Drive=" << "0x" << std::hex << std::uppercase << (int)drive;
                    LOG_INFO(logger_, oss.str());
                }
            }

            // 2) AEB 상태 (5주기마다 토글)
            if ((tick % 5) == 0) {
                aeb_on = !aeb_on;
                publish_once(0x02, {static_cast<uint8_t>(aeb_on ? 0x01 : 0x00)});
                {
                    std::ostringstream oss;
                    oss << "[PUB] AEB=" << (aeb_on ? "ON" : "OFF");
                    LOG_INFO(logger_, oss.str());
                }
            }

            // 3) AutoPark 단계 (1→2→3→4→1 순환)
            autopark_step = (autopark_step < 4) ? (uint8_t)(autopark_step + 1) : (uint8_t)1;
            publish_once(0x03, {autopark_step});

            // 4) ToF 거리 (uint16 big-endian로 전송 예시)
            tof += dir;
            if (tof >= 130) dir = -1;
            if (tof <= 100) dir = +1;
            uint16_t tof_u16 = static_cast<uint16_t>(tof);
            std::vector<uint8_t> tof_be{
                static_cast<uint8_t>((tof_u16 >> 8) & 0xFF),
                static_cast<uint8_t>(tof_u16 & 0xFF)
            };
            publish_once(0x04, tof_be);

            // (선택) Fault / Auth 상태는 변화 시에만 송신하는 on-change 트리거로 사용 권장
            // 예시:
            // publish_once(0x06, {0x01}); // Auth SUCCESS
            // publish_once(0x05, {0x20}); // Fault: CAN Fault

            ++tick;
            std::this_thread::sleep_for(PERIOD);
        }

        LOG_INFO(logger_, "[INFO] Publish loop terminated.");
    }

private:
    veh::Logger logger_{"logs/veh_status_publisher.log"};
    std::shared_ptr<vsomeip::application> app_;
    std::thread pub_thread_;
    bool is_registered_;
};

int main() {
    // 종료 시그널 등록
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    VehStatusPublisher node;
    if (!node.init())
        return 1;

    // vsomeip 실행은 별도 스레드에서
    std::thread vsomeip_thread([&]() {
        node.start();
    });

    // 메인 스레드는 종료 신호를 대기
    while (g_running.load())
        std::this_thread::sleep_for(100ms);

    // Ctrl+C 입력 시 graceful 종료
    node.stop();
    if (vsomeip_thread.joinable())
        vsomeip_thread.join();

    std::cout << "[INFO] veh_status_publisher gracefully stopped." << std::endl;
    return 0;
}
