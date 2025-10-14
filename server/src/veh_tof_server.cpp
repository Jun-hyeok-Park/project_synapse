#include "veh_tof_server.hpp"
#include <set>

VehTofServer::VehTofServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehTofServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("ToF", "vehcan init failed");
        return;
    }

    // 1) 이벤트 오퍼 (주의: offer_event의 3번째 인자는 '이벤트ID'!)
    std::set<vsomeip::eventgroup_t> eg{TOF_EG_ID};
    app_->offer_event(VEH_TOF_SERVICE_ID, VEH_INSTANCE_ID, TOF_DISTANCE_EVENT_ID,
                      eg, vsomeip::event_type_e::ET_EVENT,
                      std::chrono::milliseconds::zero(),
                      false, true, nullptr,
                      vsomeip::reliability_type_e::RT_UNRELIABLE);

    // 2) 구독 핸들러 (경고는 뜨지만 호환용으로 OK)
    app_->register_subscription_handler(
        VEH_TOF_SERVICE_ID, VEH_INSTANCE_ID, TOF_EG_ID,
        [this](vsomeip::service_t sid, vsomeip::instance_t iid,
               vsomeip::eventgroup_t eg, bool subscribed) -> bool {
            return on_subscription(sid, iid, eg, subscribed);
        });

    // 3) CAN 이벤트 리스너 등록
    //    설계: E5 [MSB][LSB]  (거리 cm)
    tof_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == 0xE5 && n >= 2) {
                const uint16_t cm = (uint16_t(p[0]) << 8) | uint16_t(p[1]);
                {
                    std::lock_guard<std::mutex> lk(m_);
                    last_distance_ = cm;
                }
                // 구독자 있으면 즉시 notify
                if (client_subscribed_.load()) {
                    app_->notify(VEH_TOF_SERVICE_ID, VEH_INSTANCE_ID,
                                 TOF_DISTANCE_EVENT_ID, make_payload(cm));
                    LOG("ToF", "CAN→SOMEIP distance=" << cm << " cm");
                }
            }
        }
    );

    // 4) 주기 리퍼블리시(옵션): 구독자가 있으면 최근 거리 1초마다 재전송
    pub_ = std::thread(&VehTofServer::publish_loop, this);

    LOG("ToF", "veh.tof initialized");
}

bool VehTofServer::on_subscription(vsomeip::service_t, vsomeip::instance_t,
                                   vsomeip::eventgroup_t, bool subscribed) {
    client_subscribed_.store(subscribed);
    LOG("ToF", (subscribed ? "Client subscribed" : "Client unsubscribed"));

    // 방금 구독했다면 마지막 측정값 한 번 쏴줌(있을 때)
    if (subscribed) {
        uint16_t cm;
        {
            std::lock_guard<std::mutex> lk(m_);
            cm = last_distance_;
        }
        if (cm != 0) {
            app_->notify(VEH_TOF_SERVICE_ID, VEH_INSTANCE_ID,
                         TOF_DISTANCE_EVENT_ID, make_payload(cm));
        }
    }
    return true;
}

void VehTofServer::publish_loop() {
    while (running_.load()) {
        if (client_subscribed_.load()) {
            uint16_t cm;
            {
                std::lock_guard<std::mutex> lk(m_);
                cm = last_distance_;
            }
            if (cm != 0) {
                app_->notify(VEH_TOF_SERVICE_ID, VEH_INSTANCE_ID,
                             TOF_DISTANCE_EVENT_ID, make_payload(cm));
                LOG("ToF", "Republish distance=" << cm << " cm");
            }
        }
        delay_ms(1000);
    }
}

void VehTofServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    if (pub_.joinable())
        pub_.join();

    if (tof_listener_id_ != 0) {
        vehcan::remove_event_listener(tof_listener_id_);
        tof_listener_id_ = 0;
    }
}
