#include <vsomeip/vsomeip.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <thread>
#include <vector>
#include <atomic>
#include <filesystem>

#include "../common/veh_status_service.hpp" // VEH_STATUS_* 매크로들
#include "../common/veh_logger.hpp"         // veh::Logger
#include "../common/veh_types.hpp"          // 공통 typedefs (필요시)
#include "../common/config.hpp"             // 필요하면 사용

static std::atomic_bool g_running{true};

// 로그 파일 열기 실패 대비: 콘솔에도 찍히도록
veh::Logger status_logger("logs/veh_status.log");

static void handle_signal(int) {
    g_running = false;
}

// 헬퍼: 1바이트 타입 + 가변 데이터로 payload 만들기
static std::shared_ptr<vsomeip::payload> make_payload(uint8_t status_type,
                                                      const std::vector<uint8_t> &value) {
    std::vector<uint8_t> data;
    data.reserve(1 + value.size());
    data.push_back(status_type);
    data.insert(data.end(), value.begin(), value.end());
    return vsomeip::runtime::get()->create_payload(data);
}

// 헬퍼: 16비트 값을 big-endian(또는 little-endian)으로 보낼지 결정
// 여기서는 little-endian으로 전송 (GUI에서도 little로 파싱 권장)
static std::vector<uint8_t> u16_le(uint16_t v) {
    return { static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF) };
}

class VehStatusPublisher {
public:
    VehStatusPublisher()
        : app_(vsomeip::runtime::get()->create_application("veh_status_publisher")) {}

    bool init() {
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        // logs 폴더 자동 생성
        std::filesystem::create_directories("logs");

        if (!app_->init()) {
            LOG_ERROR(status_logger, "vsomeip init failed!");
            return false;
        }

        // 라우터 등록 상태 감시
        app_->register_state_handler([this](vsomeip::state_type_e st) {
            if (st == vsomeip::state_type_e::ST_REGISTERED) {
                LOG_INFO(status_logger, "Registered to routing. Offering service...");
                offer_service();
            }
        });

        return true;
    }

    void start() {
        LOG_INFO(status_logger, "Starting veh_status_publisher...");
        // 송신 스레드
        worker_ = std::thread([this]() { run_loop(); });
        // vsomeip 메인 루프
        app_->start();
    }

    void stop() {
        g_running = false;
        if (worker_.joinable())
            worker_.join();
        app_->stop();
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;

    // 이벤트 오퍼
    void offer_service() {
        // 서비스/인스턴스 offer
        app_->offer_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

        // vsomeip 3.5: offer_event(service, instance, event, {eventgroup...}, ...)
        // *여기서 eventgroup은 set으로 전달해야 함
        std::set<vsomeip::eventgroup_t> egs{VEH_STATUS_EVENTGROUP_ID};
        app_->offer_event(VEH_STATUS_SERVICE_ID,
                          VEH_STATUS_INSTANCE_ID,
                          VEH_STATUS_EVENT_ID,
                          egs,                           // <- std::set
                          vsomeip::event_type_e::ET_EVENT,
                          std::chrono::milliseconds::zero(),
                          true /* is_field */);

        LOG_INFO(status_logger, "veh_status_service offered. event=0x" + to_hex(VEH_STATUS_EVENT_ID));
    }

    // 주기 송신 루프
    void run_loop() {
        using namespace std::chrono_literals;

        std::mt19937 rng(static_cast<unsigned>(std::random_device{}()));
        std::uniform_int_distribution<uint16_t> dist_cm(30, 150);

        bool aeb = false;
        int park = 0; // 0~100

        auto last_aeb = std::chrono::steady_clock::now();
        auto last_tof = std::chrono::steady_clock::now();
        auto last_park = std::chrono::steady_clock::now();

        while (g_running.load()) {
            auto now = std::chrono::steady_clock::now();

            // 1) ToF: 500ms 간격
            if (now - last_tof >= 500ms) {
                last_tof = now;
                uint16_t cm = dist_cm(rng);
                auto pl = make_payload(
                    /*status_type*/ 0x04,  // ToF
                    u16_le(cm)             // 2바이트 little-endian
                );
                notify(pl);
            }

            // 2) AEB: 3초마다 토글
            if (now - last_aeb >= 3s) {
                last_aeb = now;
                aeb = !aeb;
                auto pl = make_payload(
                    /*status_type*/ 0x02,  // AEB
                    std::vector<uint8_t>{ static_cast<uint8_t>(aeb ? 1 : 0) }
                );
                notify(pl);
            }

            // 3) AutoPark: 200ms마다 진행률 증가 (0→100→COMPLETE 신호 한번 → 다시 0)
            if (now - last_park >= 200ms) {
                last_park = now;
                if (park < 100) {
                    park += 5;
                    if (park > 100) park = 100;
                    auto pl = make_payload(
                        /*status_type*/ 0x03, // AutoPark progress
                        std::vector<uint8_t>{ static_cast<uint8_t>(park) }
                    );
                    notify(pl);

                    if (park == 100) {
                        // 완료 알림 한번 더
                        auto done = make_payload(
                            /*status_type*/ 0x03,
                            std::vector<uint8_t>{ 100 }
                        );
                        notify(done);
                        // 다음 사이클을 위해 약간 쉬었다가 0으로
                        std::this_thread::sleep_for(800ms);
                        park = 0;
                    }
                }
            }

            std::this_thread::sleep_for(20ms);
        }
    }

    void notify(const std::shared_ptr<vsomeip::payload> &pl) {
        app_->notify(VEH_STATUS_SERVICE_ID,
                     VEH_STATUS_INSTANCE_ID,
                     VEH_STATUS_EVENT_ID,
                     pl,
                     true /* force_notification */);

        const auto *p = pl->get_data();
        const auto len = pl->get_length();
        if (p && len >= 1) {
            LOG_INFO(status_logger, "Published status type=0x" + to_hex(p[0]) +
                                    " len=" + std::to_string(len));
        }
    }

    static std::string to_hex(uint32_t v) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << v;
        return oss.str();
    }
};

int main() {
    VehStatusPublisher pub;
    if (!pub.init()) {
        LOG_ERROR(status_logger, "Publisher init failed!");
        return 1;
    }
    pub.start();
    return 0;
}
