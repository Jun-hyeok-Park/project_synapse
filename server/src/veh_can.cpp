#include "veh_can.hpp"
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace vehcan {
namespace {

int g_fd = -1;
std::mutex g_mtx;
uint32_t g_can_id = 0x123;  // TX ID

// RX 스레드 관련
std::thread g_rx_th;
std::atomic<bool> g_running{false};

// 멀티 리스너
std::mutex g_ls_mtx;
std::vector<std::pair<ListenerId, EventHandler>> g_listeners;
std::atomic<ListenerId> g_next_id{1};

// 소켓 필터 (옵션)
bool      g_use_filter = false;
can_filter g_filter{};

// env 헬퍼
std::string env_or(const char* key, const std::string& defv) {
    if (const char* v = std::getenv(key)) return std::string(v);
    return defv;
}
uint32_t env_or_u32(const char* key, uint32_t defv) {
    if (const char* v = std::getenv(key)) {
        char* end=nullptr;
        unsigned long x = std::strtoul(v, &end, 0);
        if (end && *end=='\0') return static_cast<uint32_t>(x);
    }
    return defv;
}

// RX 루프
void rx_loop(int fd) {
    struct pollfd pfd{fd, POLLIN, 0};
    while (g_running.load(std::memory_order_relaxed)) {
        int r = ::poll(&pfd, 1, 500); // 500ms 타임아웃
        if (r <= 0) continue;
        if (!(pfd.revents & POLLIN)) continue;

        struct can_frame fr{};
        ssize_t n = ::read(fd, &fr, sizeof(fr));
        if (n != sizeof(fr)) continue;
        if (fr.can_dlc < 1)   continue;

        const uint8_t eid = fr.data[0];
        if (eid >= 0xE0 && eid <= 0xE5) {
            // 리스너 목록 스냅샷 후, 락 해제 상태에서 콜백 호출
            std::vector<std::pair<ListenerId, EventHandler>> snapshot;
            {
                std::lock_guard<std::mutex> lk(g_ls_mtx);
                snapshot = g_listeners;
            }
            for (auto &p : snapshot) {
                if (p.second)
                    p.second(eid, &fr.data[1], fr.can_dlc - 1, fr.can_id & CAN_EFF_MASK);
            }
        }
    }
}

} // anon

bool init(const Config& cfg_) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_fd >= 0) return true;

    Config cfg = cfg_;
    cfg.ifname = env_or("VEH_CAN_IF", cfg.ifname);
    g_can_id   = env_or_u32("VEH_CAN_ID", cfg.can_id);

    int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) { perror("socket(PF_CAN)"); return false; }

    // Loopback 토글: 환경변수로 제어 (있으면 ON)
    int loopback = std::getenv("VEH_CAN_LOOPBACK") ? 1 : 0;
    ::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));

    // Own msgs 수신 안함(기본 OFF지만 명시)
    int recv_own = 0;
    ::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own, sizeof(recv_own));

    // (옵션) 수신 필터 적용
    if (g_use_filter) {
        ::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &g_filter, sizeof(g_filter));
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, cfg.ifname.c_str(), IFNAMSIZ-1);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        ::close(fd);
        return false;
    }

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind(can)");
        ::close(fd);
        return false;
    }

    g_fd = fd;
    std::cerr << "[vehcan] init OK if=" << cfg.ifname
              << " tx_can_id=0x" << std::hex << g_can_id << std::dec << "\n";

    // ★ RX 스레드 시작
    g_running = true;
    g_rx_th = std::thread(rx_loop, g_fd);

    return true;
}

void close() {
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_fd >= 0) {
            g_running = false;
            ::shutdown(g_fd, SHUT_RD); // poll 깨우기용 (무시될 수도 있음)
            ::close(g_fd);
            g_fd = -1;
        } else {
            g_running = false;
        }
    }
    if (g_rx_th.joinable())
        g_rx_th.join();
}

bool send_cmd(uint8_t cmd_id, const std::vector<uint8_t>& args) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_fd < 0) {
        if (!init({})) return false; // lazy-init
    }
    struct can_frame fr{};
    fr.can_id  = g_can_id; // 표준 11-bit. (29-bit 쓰면 외부에서 CAN_EFF_FLAG 포함해서 설정)
    fr.can_dlc = static_cast<__u8>(1 + (args.size() > 7 ? 7 : args.size()));
    fr.data[0] = cmd_id;
    for (size_t i = 0; i < fr.can_dlc - 1; ++i)
        fr.data[1 + i] = args[i];

    ssize_t n = ::write(g_fd, &fr, sizeof(fr));
    if (n != sizeof(fr)) {
        perror("write(can)");
        return false;
    }
    return true;
}

// ===== 리스너 관리 & 필터 =====
ListenerId add_event_listener(EventHandler cb) {
    std::lock_guard<std::mutex> lk(g_ls_mtx);
    ListenerId id = g_next_id++;
    g_listeners.emplace_back(id, std::move(cb));
    return id;
}

void remove_event_listener(ListenerId id) {
    std::lock_guard<std::mutex> lk(g_ls_mtx);
    for (auto it = g_listeners.begin(); it != g_listeners.end(); ++it) {
        if (it->first == id) { g_listeners.erase(it); break; }
    }
}

void set_rx_filter(uint32_t mask, uint32_t val, bool ext_id) {
    std::lock_guard<std::mutex> lk(g_mtx);
    g_use_filter = true;
    if (ext_id) {
        // 29bit ID + 확장 플래그 비교
        g_filter.can_id   = (val  & CAN_EFF_MASK) | CAN_EFF_FLAG;
        g_filter.can_mask = (mask & CAN_EFF_MASK) | CAN_EFF_FLAG;
    } else {
        // 11bit ID + "확장=0" 비교
        g_filter.can_id   = (val  & CAN_SFF_MASK);      // 확장 플래그는 0
        g_filter.can_mask = (mask & CAN_SFF_MASK) | CAN_EFF_FLAG; // 확장비트도 비교대상에 포함(=0)
    }
    // 이미 열려 있으면 즉시 적용
    if (g_fd >= 0) {
        ::setsockopt(g_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &g_filter, sizeof(g_filter));
    }
}

} // namespace vehcan
