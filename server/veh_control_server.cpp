#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>

#include "veh_control_service.hpp"  // SOME/IP + CAN 통합 프로토콜 정의
#include "veh_logger.hpp"           // 로깅 유틸리티
#include "veh_can.hpp"              // SocketCAN 인터페이스

// ───────────────────────────────────────────────
// 전역 객체: 로거 및 CAN 인터페이스
// ───────────────────────────────────────────────
veh::Logger server_logger("logs/veh_control_server.log");
CanInterface can("can0"); // CAN0 인터페이스 사용

// ───────────────────────────────────────────────
// CAN 송신 함수
//  - SocketCAN을 이용해 특정 ID로 프레임 송신
//  - 필요 시 자동으로 초기화(init) 수행
// ───────────────────────────────────────────────
void veh_can_send(uint16_t can_id, const uint8_t* data, size_t len) {
    if (!can.isOpen()) {
        if (!can.init()) {
            LOG_ERROR(server_logger, "CAN init failed");
            return;
        }
    }

    CanFrame frame;
    frame.id = can_id;
    frame.dlc = len;
    memcpy(frame.data.data(), data, len);

    if (!can.sendFrame(frame))
        LOG_ERROR(server_logger, "CAN TX failed");
}

// ───────────────────────────────────────────────
// VehControlServer 클래스
//  - SOME/IP 기반 차량 제어 서버
//  - Client로부터 Command 수신 후 CAN으로 변환 송신
// ───────────────────────────────────────────────
class VehControlServer {
public:
    VehControlServer()
        : app_(vsomeip::runtime::get()->create_application("veh_server")) {}

    // vsomeip 초기화 및 핸들러 등록
    bool init() {
        if (!app_->init()) {
            LOG_ERROR(server_logger, "vsomeip init failed!");
            return false;
        }

        // SOME/IP 상태 핸들러 (REGISTERED 상태 시 서비스 제공)
        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                offer_service();
        });

        // 요청(Request) 핸들러 등록
        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            [this](const std::shared_ptr<vsomeip::message> &request) {
                on_request(request);
            });

        return true;
    }

    // vsomeip 이벤트 루프 시작
    void start() {
        LOG_INFO(server_logger, "Starting veh_control_server...");
        app_->start();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    // ───────────────────────────────────────────────
    // SOME/IP 서비스 오퍼 (클라이언트에게 서비스 알림)
    // ───────────────────────────────────────────────
    void offer_service() {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "Offered veh_control_service (0x%04X, inst 0x%04X)",
                 VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
        LOG_INFO(server_logger, buf);

        // 서비스 등록
        app_->offer_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
    }

    // ───────────────────────────────────────────────
    // SOME/IP 요청 수신 핸들러
    //  - 클라이언트 → 서버 방향 메시지 수신
    //  - payload에서 cmd_type, cmd_value 파싱 후 handle_command() 호출
    // ───────────────────────────────────────────────
    void on_request(const std::shared_ptr<vsomeip::message> &request) {
        auto payload = request->get_payload();
        auto data = payload->get_data();
        auto len = payload->get_length();

        if (len < 1) {
            LOG_WARN(server_logger, "Received empty payload.");
            return;
        }

        // [0]: 명령 타입(cmd_type), [1~]: 값(cmd_value)
        uint8_t cmd_type = data[0];
        std::vector<uint8_t> cmd_value(data + 1, data + len);

        char rxbuf[64];
        snprintf(rxbuf, sizeof(rxbuf),
                 "RX cmd_type=0x%02X len=%zu", cmd_type, len - 1);
        LOG_INFO(server_logger, rxbuf);

        // 실제 명령 처리
        handle_command(cmd_type, cmd_value);

        // 응답 (Response) 생성 및 전송
        auto resp = vsomeip::runtime::get()->create_response(request);
        std::vector<uint8_t> resp_data = { VEH_RESP_OK };
        resp->set_payload(vsomeip::runtime::get()->create_payload(resp_data));
        app_->send(resp);
    }

    // ───────────────────────────────────────────────
    // 명령 처리 함수
    //  - cmd_type, cmd_value 기반으로 CAN 프레임 구성 후 송신
    //  - 모든 제어 명령은 공통 ID(0x200)로 전송
    // ───────────────────────────────────────────────
    void handle_command(uint8_t cmd_type, const std::vector<uint8_t> &cmd_value) {
        char msgbuf[80];

        // 명령 로그 출력
        snprintf(msgbuf, sizeof(msgbuf),
                 "→ CMD[0x%02X], Len=%zu", cmd_type, cmd_value.size());
        LOG_INFO(server_logger, msgbuf);

        // CAN 프레임 구성: [0]=cmd_type, [1~]=cmd_value
        std::vector<uint8_t> frame_data;
        frame_data.push_back(cmd_type);
        frame_data.insert(frame_data.end(), cmd_value.begin(), cmd_value.end());

        // 단일 CAN ID(0x200)로 송신
        veh_can_send(VEH_CONTROL_CAN_ID, frame_data.data(), frame_data.size());
    }
};

// ───────────────────────────────────────────────
// main()
//  - 서버 인스턴스 생성 및 실행
// ───────────────────────────────────────────────
int main() {
    VehControlServer server;

    // 초기화 실패 시 종료
    if (!server.init()) {
        LOG_ERROR(server_logger, "Server init failed.");
        return -1;
    }

    // SOME/IP 서버 실행 (이 루프는 블로킹 상태로 동작)
    server.start();
    return 0;
}
