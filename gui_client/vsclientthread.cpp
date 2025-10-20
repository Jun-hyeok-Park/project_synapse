#include "vsclientthread.h"
#include <sstream>
#include <iomanip>
#include <chrono>

#include "veh_control_service.hpp"
#include "veh_status_service.hpp"

using namespace std::chrono_literals;

VsClientThread::VsClientThread(QObject *parent)
    : QThread(parent)
{
}

VsClientThread::~VsClientThread() {
    running_ = false;
    requestInterruption();
    quit();
    wait();
}

void VsClientThread::run() {
    init_vsomeip();
    start_vsomeip();

    // vSomeIP는 내부 스레드가 돌며 콜백 호출
    // 이 QThread는 종료 신호를 기다리다가 stop_vsomeip 호출
    while (running_ && !isInterruptionRequested()) {
        msleep(100);
    }
    stop_vsomeip();
}

void VsClientThread::init_vsomeip() {
    app_ = vsomeip::runtime::get()->create_application("veh_unified_client_qt");
    if (!app_ || !app_->init()) {
        emit logLine("[ERR] vsomeip init failed");
        running_ = false;
        return;
    }

    // 상태 핸들러: REGISTERED시 서비스 요청 + 이벤트 등록/구독
    app_->register_state_handler([this](vsomeip::state_type_e state) {
        if (state == vsomeip::state_type_e::ST_REGISTERED) {
            emit logLine("[INFO] Client registered.");

            app_->request_service(VEH_CONTROL_SERVICE_ID, VEH_CONTROL_INSTANCE_ID);
            app_->request_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

            app_->request_event(
                VEH_STATUS_SERVICE_ID,
                VEH_STATUS_INSTANCE_ID,
                VEH_STATUS_EVENT_ID,
                { VEH_STATUS_EVENTGROUP_ID },
                vsomeip::event_type_e::ET_EVENT,
                vsomeip::reliability_type_e::RT_UNRELIABLE);

            app_->subscribe(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENTGROUP_ID);
        }
    });

    // 이벤트 핸들러: 상태 수신 시 파싱 후 GUI로 시그널
    app_->register_message_handler(
        VEH_STATUS_SERVICE_ID,
        VEH_STATUS_INSTANCE_ID,
        VEH_STATUS_EVENT_ID,
        [this](const std::shared_ptr<vsomeip::message> &msg) {
            onEvent(msg);
        });
}

void VsClientThread::start_vsomeip() {
    // vSomeIP 앱은 내부에서 자체 스레드를 사용 -> 별도 detach
    std::thread([this](){
        app_->start();
    }).detach();
    emit logLine("[INFO] vSomeIP app started");
}

void VsClientThread::stop_vsomeip() {
    if (app_) {
        emit logLine("[INFO] Stopping vSomeIP...");
        app_->stop();
        emit logLine("[INFO] Stopped.");
    }
}

void VsClientThread::onEvent(const std::shared_ptr<vsomeip::message> &msg) {
    auto pl = msg->get_payload();
    if (!pl || pl->get_length() < 2) return;
    const auto *data = pl->get_data();
    uint8_t type = data[0];

    // 로깅
    {
        std::ostringstream oss;
        oss << "[EVT] TYPE=0x" << std::hex << (int)type << " DATA=[";
        for (size_t i=1; i<pl->get_length(); ++i) {
            oss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i] << " ";
        }
        oss << "]";
        emit logLine(QString::fromStdString(oss.str()));
    }

    switch (static_cast<StatusType>(type)) {
        case StatusType::AEB_STATE: {
            if (pl->get_length() >= 2) {
                bool active = data[1] != 0x00;
                emit aebStateChanged(active);
            }
            break;
        }
        case StatusType::AUTOPARK_STATE: {
            if (pl->get_length() >= 2) {
                emit autoparkStateChanged(data[1]);
            }
            break;
        }
        case StatusType::TOF_DISTANCE: {
            if (pl->get_length() >= 4) {
                uint32_t mm = (static_cast<uint32_t>(data[1]) << 16) |
                            (static_cast<uint32_t>(data[2]) << 8)  |
                            (static_cast<uint32_t>(data[3]));
                emit tofChanged(mm);
            }
            break;
        }
        case StatusType::AUTH_STATE: {
            if (pl->get_length() >= 2) {
                bool ok = data[1] != 0x00;
                emit authStateChanged(ok);
            }
            break;
        }
        default:
            break;
    }
}

void VsClientThread::sendCommand(uint8_t cmdType, const std::vector<uint8_t> &val) {
    if (!app_) return;

    auto msg = vsomeip::runtime::get()->create_request();
    msg->set_service(VEH_CONTROL_SERVICE_ID);
    msg->set_instance(VEH_CONTROL_INSTANCE_ID);
    msg->set_method(VEH_CONTROL_METHOD_ID);

    std::vector<uint8_t> payload;
    payload.reserve(1 + val.size());
    payload.push_back(cmdType);
    payload.insert(payload.end(), val.begin(), val.end());
    msg->set_payload(vsomeip::runtime::get()->create_payload(payload));
    app_->send(msg);

    std::ostringstream oss;
    oss << "[REQ] TYPE=0x" << std::hex << (int)cmdType << " DATA=[";
    for (auto b : val) oss << std::setw(2) << std::setfill('0') << (int)b << " ";
    oss << "]";
    emit logLine(QString::fromStdString(oss.str()));
}

void VsClientThread::sendDriveDirection(uint8_t dir) {
    // 기존과 동일: CmdType::DRIVE_DIRECTION == 0x01
    sendCommand(static_cast<uint8_t>(CmdType::DRIVE_DIRECTION), { dir });
}

void VsClientThread::sendSpeedDuty(uint16_t duty_0_1000) {
    // 서버 핸들러는 payload[0] * 10 을 duty로 사용(0~1000) → 여기서는 0~100 값을 보냄
    // GUI 슬라이더를 0~1000로 하고 /10 해서 0~100 전송
    uint8_t scaled = static_cast<uint8_t>(std::clamp<int>(duty_0_1000 / 10, 0, 100));
    sendCommand(static_cast<uint8_t>(CmdType::DRIVE_SPEED), { scaled });
}

void VsClientThread::sendAebControl(bool on) {
    sendCommand(static_cast<uint8_t>(CmdType::AEB_CONTROL), { on ? 0x01 : 0x00 });
}

void VsClientThread::sendAutoparkStart() {
    sendCommand(static_cast<uint8_t>(CmdType::AUTOPARK_CONTROL), { 0x01 });
}
