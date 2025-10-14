#include "veh_fcw_server.hpp"
#include <set>

VehFcwServer::VehFcwServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehFcwServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("FCW", "vehcan init failed");
        return;
    }

    // ===== SOME/IP 이벤트 오퍼 =====
    //  - Alert: 이벤트(필드 이벤트) / unreliable
    //  - Fault: 알림(필드 이벤트) / reliable
    std::set<vsomeip::eventgroup_t> eg{FCW_EG_ID};

    app_->offer_event(
        VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID, FCW_ALERT_EVENT_ID,
        eg, vsomeip::event_type_e::ET_FIELD,
        std::chrono::milliseconds::zero(),
        /*is_field*/ true, /*is_cyclic*/ true, nullptr,
        vsomeip::reliability_type_e::RT_UNRELIABLE
    );

    app_->offer_event(
        VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID, FCW_FAULT_NOTIFY_ID,
        eg, vsomeip::event_type_e::ET_FIELD,
        std::chrono::milliseconds::zero(),
        /*is_field*/ true, /*is_cyclic*/ true, nullptr,
        vsomeip::reliability_type_e::RT_RELIABLE
    );

    // ===== 메서드 핸들러 등록 (warn) =====
    app_->register_message_handler(
        VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID, FCW_EG_ID,
        std::bind(&VehFcwServer::on_warn, this, std::placeholders::_1)
    );

    // ===== CAN 이벤트 구독 =====
    // 규격: E2 <alert>  (0:OFF, 1:ON …)
    // (옵션) Fault가 따로 있으면 E? <code>로 확장 가능
    fcw_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == 0xE2 && n >= 1) {
                const uint8_t alert = p[0];
                {
                    std::lock_guard<std::mutex> lk(m_);
                    last_alert_ = alert;
                }
                app_->notify(VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID,
                             FCW_ALERT_EVENT_ID, make_payload(alert));
                LOG("FCW", "CAN→SOMEIP alert=" << unsigned(alert));
            }

            // 필요 시 Fault 이벤트도 매핑:
            // else if (eid == 0xE7 && n >= 1) {
            //     const uint8_t fault = p[0];
            //     {
            //         std::lock_guard<std::mutex> lk(m_);
            //         last_fault_ = fault;
            //     }
            //     app_->notify(VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID,
            //                  FCW_FAULT_NOTIFY_ID, make_payload(fault));
            //     LOG("FCW", "CAN→SOMEIP fault=" << unsigned(fault));
            // }
        }
    );

    // ===== 주기 리퍼블리시(선택): 최근 alert 값을 2초마다 재전송 =====
    // 구독/비구독을 따로 캐시하지 않고 항상 주기 재전송(필드값 최신화)을 원하면 유지,
    // 불필요하면 이 스레드를 제거해도 됨.
    pub_ = std::thread(&VehFcwServer::publish_loop, this);

    LOG("FCW", "veh.fcw initialized (0x1102/0x0001)");
}

void VehFcwServer::on_warn(const std::shared_ptr<vsomeip::message> &req) {
    // vsomeip 메서드: warn() → CAN 0x30 전송
    vehcan::fcw_warn();

    auto resp = vsomeip::runtime::get()->create_response(req);
    resp->set_payload(make_payload(std::string("ACK: FCW WARN")));
    app_->send(resp);

    LOG("FCW", "WARN command sent via CAN (0x30)");
}

void VehFcwServer::publish_loop() {
    while (running_.load()) {
        uint8_t alert;
        {
            std::lock_guard<std::mutex> lk(m_);
            alert = last_alert_;
        }
        // 마지막 값이 있으면 재전송(필드 최신화 용도)
        app_->notify(VEH_FCW_SERVICE_ID, VEH_INSTANCE_ID,
                     FCW_ALERT_EVENT_ID, make_payload(alert));
        delay_ms(2000);
    }
}

void VehFcwServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    if (pub_.joinable())
        pub_.join();

    if (fcw_listener_id_ != 0) {
        vehcan::remove_event_listener(fcw_listener_id_);
        fcw_listener_id_ = 0;
    }
}
