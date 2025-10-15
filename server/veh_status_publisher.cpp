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
static std::shared_ptr<vsomeip::application> g_app;

// 로그 파일 열기 실패 대비: 콘솔에도 찍히도록
veh::Logger status_logger("logs/veh_status.log");

// 전역 플래그
static bool g_test_mode = false;  // 기본 false (운영 모드)

static void handle_signal(int) {
    std::cout << "\n[Signal] SIGINT received, stopping veh_status_publisher..." << std::endl;
    g_running = false;
    if (g_app)
        g_app->stop();  // 즉시 vsomeip 종료 요청
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

// 헬퍼: 16비트 little-endian 변환
static std::vector<uint8_t> u16_le(uint16_t v) {
    return { static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF) };
}

class VehStatusPublisher {
public:
    VehStatusPublisher()
        : app_(vsomeip::runtime::get()->create_application("veh_status_publisher")) {
        g_app = app_;
    }

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
        // vsomeip 메인 루프 스레드 시작
        worker_ = std::thread([this]() { run_loop(); });
        app_->start();
    }

    void stop() {
        LOG_INFO(status_logger, "Stopping veh_status_publisher...");
        g_running = false;

        if (app_)
            app_->stop();

        if (worker_.joinable())
            worker_.join();
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;

    // 이벤트 오퍼
    void offer_service() {
        app_->offer_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

        std::set<vsomeip::eventgroup_t> egs{VEH_STATUS_EVENTGROUP_ID};
        app_->offer_event(VEH_STATUS_SERVICE_ID,
                          VEH_STATUS_INSTANCE_ID,
                          VEH_STATUS_EVENT_ID,
                          egs,
                          vsomeip::event_type_e::ET_EVENT,
                          std::chrono::milliseconds::zero(),
                          true /* is_field */);

        LOG_INFO(status_logger, "veh_status_service offered. event=0x" + to_hex(VEH_STATUS_EVENT_ID));
    }

    // 주기 송신 루프
    void run_loop() {
        using namespace std::chrono_literals;

        if (!g_test_mode) {
            LOG_INFO(status_logger, "Running in NORMAL mode (no dummy publishing).");
            while (g_running.load()) {
                std::this_thread::sleep_for(500ms);
            }
            return;
        }

        LOG_WARN(status_logger, "Running in TEST mode: dummy status publishing enabled.");

        std::mt19937 rng(static_cast<unsigned>(std::random_device{}()));
        std::uniform_int_distribution<uint16_t> dist_cm(30, 150);

        bool aeb = false;
        int park = 0;

        auto last_aeb = std::chrono::steady_clock::now();
        auto last_tof = std::chrono::steady_clock::now();
        auto last_park = std::chrono::steady_clock::now();

        while (g_running.load()) {
            auto now = std::chrono::steady_clock::now();

            // ToF (500ms)
            if (now - last_tof >= 500ms) {
                last_tof = now;
                uint16_t cm = dist_cm(rng);
                auto pl = make_payload(0x04, u16_le(cm));
                notify(pl);
            }

            // AEB (3s)
            if (now - last_aeb >= 3s) {
                last_aeb = now;
                aeb = !aeb;
                auto pl = make_payload(0x02, { static_cast<uint8_t>(aeb ? 1 : 0) });
                notify(pl);
            }

            // AutoPark (200ms)
            if (now - last_park >= 200ms) {
                last_park = now;
                if (park < 100) {
                    park += 5;
                    if (park > 100)
                        park = 100;
                    auto pl = make_payload(0x03, { static_cast<uint8_t>(park) });
                    notify(pl);

                    if (park == 100) {
                        auto done = make_payload(0x03, { 100 });
                        notify(done);
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

// ======================================
// main()
// ======================================
int main(int argc, char *argv[]) {
    // 명령행 인자 확인
    if (argc > 1 && std::string(argv[1]) == "--test") {
        g_test_mode = true;
    }

    VehStatusPublisher pub;
    if (!pub.init()) {
        LOG_ERROR(status_logger, "Publisher init failed!");
        return 1;
    }

    pub.start();
    pub.stop();
    return 0;
}
