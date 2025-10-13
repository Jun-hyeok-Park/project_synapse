#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <cstddef>

// 간단 SocketCAN 송신 모듈 (thread-safe X, 단일 fd 사용)
namespace vehcan {

// 환경변수로 바꿀 수 있음: VEH_CAN_IF, VEH_CAN_ID
struct Config {
    std::string ifname = "can0";
    uint32_t can_id = 0x123; // 표준 ID(11-bit). 필요시 바꿔도 됨.
};

bool init(const Config& cfg = {});
void close();

// 명령 전송: CMD_ID + args… 를 1프레임(최대 8바이트)로 보냄
bool send_cmd(uint8_t cmd_id, const std::vector<uint8_t>& args);

// 편의 래퍼 (네가 준 PDU 표에 맞춤)
inline bool set_speed(uint8_t pct)            { return send_cmd(0x10, {pct}); }
inline bool set_direction(uint8_t code)       { return send_cmd(0x11, {code}); }
inline bool soft_stop()                        { return send_cmd(0x12, {}); }
inline bool aeb_enable(uint8_t onoff)          { return send_cmd(0x20, {onoff}); }
inline bool aeb_status_req()                   { return send_cmd(0x21, {}); }
inline bool fcw_warn()                          { return send_cmd(0x30, {}); }
inline bool autopark_start()                    { return send_cmd(0x40, {}); }
inline bool autopark_cancel()                   { return send_cmd(0x41, {}); }
inline bool autopark_status()                   { return send_cmd(0x42, {}); }
inline bool auth_login(const std::string &pwd_ascii) {
    std::vector<uint8_t> v(pwd_ascii.begin(), pwd_ascii.end());
    // 길이 4로 제한하고 싶으면 아래처럼 슬라이스:
    if (v.size() > 4) v.resize(4);
    return send_cmd(0x50, v);
}
inline bool sys_heartbeat()                     { return send_cmd(0x60, {}); }

// ===== ★ 이벤트 구독 API 추가 ★ =====
//  - ECU가 보내는 Event 프레임(E0~E5)을 수신하면 호출됨
//  - eid: 0xE0..0xE5, data: payload(최대 7B), len: 길이, rx_can_id: 수신 CAN ID
using EventHandler = std::function<void(uint8_t eid, const uint8_t* data, size_t len, uint32_t rx_can_id)>;

// 여러 서버가 동시에 들을 수 있도록 리스너를 등록/삭제하는 형태
using ListenerId = uint32_t;
ListenerId add_event_listener(EventHandler cb);
void       remove_event_listener(ListenerId id);

// (선택) 수신 필터 설정: mask/val는 표준 CAN ID 기준 (29bit 쓰면 CAN_EFF_FLAG 포함해서 넣기)
void set_rx_filter(uint32_t mask, uint32_t val, bool ext_id = false);

} // namespace vehcan
