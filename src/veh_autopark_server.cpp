#include "veh_autopark_server.hpp"
#include <set>

VehAutoparkServer::VehAutoparkServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehAutoparkServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("AUTOPARK", "vehcan init failed");
        return;
    }

    // ===== CAN 이벤트 구독: E3 <progress(0~100)> =====
    autopark_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == 0xE3 && n >= 1) {
                const uint8_t progress = p[0];
                {
                    std::lock_guard<std::mutex> lk(m_);
                    last_progress_ = progress;
                }
                // SOME/IP parkProgress(uint8) 이벤트 송출
                app_->notify(VEH_AP_SERVICE_ID, VEH_INSTANCE_ID,
                             AP_PROGRESS_EVENT_ID, make_payload(progress));
                LOG("AUTOPARK", "CAN→SOMEIP progress=" << unsigned(progress));
            }
        }
    );

    // ===== 메서드 핸들러 =====
    app_->register_message_handler(
        VEH_AP_SERVICE_ID, VEH_INSTANCE_ID, AP_START_METHOD_ID,
        std::bind(&VehAutoparkServer::on_request, this, std::placeholders::_1));

    app_->register_message_handler(
        VEH_AP_SERVICE_ID, VEH_INSTANCE_ID, AP_CANCEL_METHOD_ID,
        std::bind(&VehAutoparkServer::on_request, this, std::placeholders::_1));

    app_->register_message_handler(
        VEH_AP_SERVICE_ID, VEH_INSTANCE_ID, AP_GET_PROGRESS_ID,
        std::bind(&VehAutoparkServer::on_request, this, std::placeholders::_1));

    // ===== 이벤트 오퍼 =====
    std::set<vsomeip::eventgroup_t> eg{AP_EG_ID};
    app_->offer_event(
        VEH_AP_SERVICE_ID, VEH_INSTANCE_ID, AP_PROGRESS_EVENT_ID,
        eg, vsomeip::event_type_e::ET_FIELD,
        std::chrono::milliseconds::zero(),
        /*is_field*/ true, /*is_cyclic*/ true, nullptr,
        vsomeip::reliability_type_e::RT_UNRELIABLE
    );

    // 초기 값 한 번 브로드캐스트(선택)
    app_->notify(VEH_AP_SERVICE_ID, VEH_INSTANCE_ID,
                 AP_PROGRESS_EVENT_ID, make_payload<uint8_t>(0));

    LOG("AUTOPARK", "veh.autopark initialized");
}

void VehAutoparkServer::on_request(const std::shared_ptr<vsomeip::message>& req) {
    const auto mid = req->get_method();
    auto resp = vsomeip::runtime::get()->create_response(req);

    if (mid == AP_START_METHOD_ID) {
        vehcan::autopark_start();  // CAN 0x40
        resp->set_payload(make_payload(std::string("ACK: START")));
        app_->send(resp);
        LOG("AUTOPARK", "start() → CAN 0x40");

    } else if (mid == AP_CANCEL_METHOD_ID) {
        vehcan::autopark_cancel(); // CAN 0x41
        resp->set_payload(make_payload(std::string("ACK: CANCEL")));
        app_->send(resp);
        LOG("AUTOPARK", "cancel() → CAN 0x41");

    } else if (mid == AP_GET_PROGRESS_ID) {
        // (옵션) 즉시 최신 진행률 캐시 응답
        uint8_t p;
        {
            std::lock_guard<std::mutex> lk(m_);
            p = last_progress_;
        }
        // (선택) ECU에게 진행률 요청 트리거 CAN 0x42
        vehcan::autopark_status();
        resp->set_payload(make_payload(p));
        app_->send(resp);
        LOG("AUTOPARK", "getProgress() → cached=" << unsigned(p) << ", CAN 0x42 sent");
    }
}

void VehAutoparkServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    if (autopark_listener_id_ != 0) {
        vehcan::remove_event_listener(autopark_listener_id_);
        autopark_listener_id_ = 0;
    }
}
