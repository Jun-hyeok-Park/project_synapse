#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <poll.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "veh_logger.hpp"
#include "veh_control_service.hpp"
#include "veh_status_service.hpp"

using namespace std::chrono_literals;

/* ──────────────────────────────────────────────────────────────
 *  Global logger & signal handler
 * ────────────────────────────────────────────────────────────── */
veh::Logger g_logger("logs/veh_unified_server.log");
std::atomic<bool> g_running{true};

/* SIGINT(SIGTERM) 수신 시 플래그를 내려 서버를 안전하게 종료 */
void on_signal(int) { 
    std::cout << "\n[INFO] SIGINT received. Graceful shutdown..." << std::endl;
    g_running = false; 
}

/* ──────────────────────────────────────────────────────────────
 *  Unified Server Class (SOME/IP + CAN Bridge)
 *  - 역할: vsomeip 요청 수신 → CAN 송신
 *          CAN 수신 → vsomeip Event 전송
 * ────────────────────────────────────────────────────────────── */
class VehUnifiedServer {
public:
    VehUnifiedServer()
    : app_(vsomeip::runtime::get()->create_application("veh_unified_server")) {}

    /* ─────────────── 초기화 ─────────────── */
    bool init() {
        /* vsomeip 초기화 */
        if (!app_->init()) {
            LOG_ERROR(g_logger, "vsomeip init failed");
            return false;
        }

        /* vsomeip 상태 핸들러 등록 */
        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                offer_services(); // 등록 완료 후 서비스 제공
        });

        /* 제어 요청 메시지 핸들러 등록 */
        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            [this](const std::shared_ptr<vsomeip::message> &req) {
                on_control_request(req);
            });

        /* CAN 송신 소켓 열기 */
        can_tx_fd_ = open_can("can0");
        if (can_tx_fd_ < 0) {
            LOG_ERROR(g_logger, "CAN TX socket open failed");
            return false;
        }

        return true;
    }

    /* ─────────────── 실행 ─────────────── */
    void start() {
        LOG_INFO(g_logger, "Starting veh_unified_server...");

        /* vsomeip 런타임과 CAN 리스너를 각각 별도 스레드로 실행 */
        vsomeip_thread_ = std::thread([&]() { app_->start(); });
        can_rx_thread_  = std::thread([&]() { can_listener_loop(); });

        /* 메인 루프는 SIGINT 감시만 수행 */
        while (g_running)
            std::this_thread::sleep_for(200ms);

        shutdown();  // 종료 시 안전하게 정리
    }

    /* ─────────────── 안전 종료 ─────────────── */
    void shutdown() {
        LOG_INFO(g_logger, "Shutting down services...");

        /* vsomeip 서비스 중단 */
        app_->stop_offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        app_->stop_offer_service(VEH_STATUS_SERVICE_ID,  VEH_STATUS_INSTANCE_ID);

        /* CAN 수신 스레드 종료 */
        g_running = false;
        if (can_rx_thread_.joinable()) {
            LOG_INFO(g_logger, "Waiting for CAN thread...");
            can_rx_thread_.join();
        }

        /* vsomeip 종료 */
        LOG_INFO(g_logger, "Stopping vsomeip app...");
        app_->stop();
        if (vsomeip_thread_.joinable()) {
            vsomeip_thread_.join();
        }

        /* CAN 송신 소켓 닫기 */
        if (can_tx_fd_ >= 0) {
            close(can_tx_fd_);
            can_tx_fd_ = -1;
        }

        LOG_INFO(g_logger, "Server shutdown complete.");
    }

private:
    /* ─────────────── 멤버 변수 ─────────────── */
    std::shared_ptr<vsomeip::application> app_;
    std::thread vsomeip_thread_;  // vsomeip 실행 스레드
    std::thread can_rx_thread_;   // CAN 수신 스레드
    int can_tx_fd_ = -1;          // CAN 송신 소켓
    int can_rx_fd_ = -1;          // CAN 수신 소켓

    /* ─────────────── 서비스 제공 등록 ─────────────── */
    void offer_services() {
        LOG_INFO(g_logger, "Offering veh_control_service + veh_status_service");

        // ① 제어 서비스 제공
        app_->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);

        // ② 상태 이벤트 서비스 제공
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

    /* ─────────────── 제어 요청 수신 (vsomeip → CAN) ─────────────── */
    void on_control_request(const std::shared_ptr<vsomeip::message> &req) {
        auto payload = req->get_payload();
        auto data = payload->get_data();
        auto len = payload->get_length();
        if (len < 1) return;

        uint8_t cmd_type = data[0];
        std::vector<uint8_t> cmd_value(data + 1, data + len);

        {
            std::ostringstream oss;
            oss << "[REQ] cmd_type=0x" << std::hex << (int)cmd_type
                << " len=" << std::dec << (len - 1);
            LOG_INFO(g_logger, oss.str());
        }

        /* CAN Frame 생성 */
        can_frame f{};
        f.can_id  = VEH_CONTROL_CAN_ID;
        f.can_dlc = (len > 8 ? 8 : len);
        f.data[0] = cmd_type;
        for (uint8_t i = 1; i < f.can_dlc; ++i)
            f.data[i] = data[i];

        /* CAN 송신 */
        if (can_tx_fd_ >= 0) {
            int r = write(can_tx_fd_, &f, sizeof(f));
            if (r < 0) perror("CAN TX write");
        }

        /* 로그 출력 */
        {
            std::ostringstream oss;
            oss << "[CAN TX] ID=0x" << std::hex << VEH_CONTROL_CAN_ID << " DATA=[";
            for (int i = 0; i < f.can_dlc; ++i)
                oss << std::setw(2) << std::setfill('0') << std::hex << (int)f.data[i] << " ";
            oss << "]";
            LOG_INFO(g_logger, oss.str());
        }

        /* 클라이언트 응답 송신 (ACK 역할) */
        auto resp = vsomeip::runtime::get()->create_response(req);
        resp->set_payload(vsomeip::runtime::get()->create_payload({ VEH_RESP_OK }));
        app_->send(resp);
    }

    /* ─────────────── CAN 수신 루프 (CAN → vsomeip Event) ─────────────── */
    void can_listener_loop() {
        can_rx_fd_ = open_can("can0");
        if (can_rx_fd_ < 0) {
            LOG_ERROR(g_logger, "CAN RX socket open failed");
            return;
        }

        /* 특정 CAN ID(0x310) 필터링 */
        struct can_filter flt{};
        flt.can_id   = VEH_STATUS_CAN_ID;
        flt.can_mask = CAN_SFF_MASK;
        if (setsockopt(can_rx_fd_, SOL_CAN_RAW, CAN_RAW_FILTER, &flt, sizeof(flt)) < 0)
            perror("setsockopt filter");

        LOG_INFO(g_logger, "[CAN] Listening on can0 (ID=0x310)");

        /* poll() 기반 비차단 수신 루프 */
        struct pollfd pfd{ .fd = can_rx_fd_, .events = POLLIN, .revents = 0 };
        while (g_running) {
            int pr = poll(&pfd, 1, 100);
            if (pr <= 0) continue;
            if (!(pfd.revents & POLLIN)) continue;

            struct can_frame frame{};
            int nbytes = read(can_rx_fd_, &frame, sizeof(frame));
            if (nbytes < 0) continue;

            /* 상태 ID(0x310) + 데이터 최소 2바이트 */
            if ((frame.can_id & CAN_EFF_FLAG) == 0 && 
                frame.can_id == VEH_STATUS_CAN_ID && frame.can_dlc >= 2) {
                
                const uint8_t status_type = frame.data[0];
                std::vector<uint8_t> val;

                /* ToF 거리 데이터(0x03)는 3바이트 값 (예: 03 00 02 28 → 0x0228 = 552mm) */
                if (status_type == 0x03 && frame.can_dlc >= 4) {
                    val.assign(frame.data + 1, frame.data + 4);
                } else {
                    val.assign(frame.data + 1, frame.data + frame.can_dlc);
                }

                /* SOME/IP Event Publish */
                publish_status(status_type, val);
            }
        }

        /* 종료 시 소켓 닫기 */
        close(can_rx_fd_); 
        can_rx_fd_ = -1;
        LOG_INFO(g_logger, "CAN listener stopped.");
    }

    /* ─────────────── 상태 이벤트 송신 ─────────────── */
    void publish_status(uint8_t type, const std::vector<uint8_t>& val) {
        std::vector<uint8_t> payload;
        payload.reserve(1 + val.size());
        payload.push_back(type);
        payload.insert(payload.end(), val.begin(), val.end());

        auto pl = vsomeip::runtime::get()->create_payload(payload);
        app_->notify(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENT_ID, pl);

        std::ostringstream oss;
        oss << "[EVT] TYPE=0x" << std::hex << (int)type << " DATA=[";
        for (auto b : val)
            oss << std::setw(2) << std::setfill('0') << std::hex << (int)b << " ";
        oss << "]";
        LOG_INFO(g_logger, oss.str());
    }

    /* ─────────────── CAN 소켓 오픈 함수 ─────────────── */
    static int open_can(const char* ifname) {
        int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s < 0) { perror("socket"); return -1; }

        struct ifreq ifr{};
        std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
        if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { perror("ioctl"); close(s); return -1; }

        struct sockaddr_can addr{};
        addr.can_family  = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { 
            perror("bind"); close(s); return -1; 
        }
        return s;
    }
};

/* ─────────────── 메인 함수 ─────────────── */
int main() {
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    VehUnifiedServer server;
    if (!server.init()) return -1;

    server.start();
    return 0;
}
