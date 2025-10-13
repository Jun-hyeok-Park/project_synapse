#include "veh_aeb_server.hpp"
#include "veh_can.hpp"
#include <set>

VehAebServer::VehAebServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehAebServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("AEB", "vehcan init failed");
        return;
    }

    // handlers
    app_->register_message_handler(VEH_AEB_SERVICE_ID, VEH_INSTANCE_ID, AEB_ENABLE_ID,
                                   std::bind(&VehAebServer::on_enable, this, std::placeholders::_1));
    app_->register_message_handler(VEH_AEB_SERVICE_ID, VEH_INSTANCE_ID, AEB_GET_STATE_ID,
                                   std::bind(&VehAebServer::on_get_state, this, std::placeholders::_1));

    // event
    std::set<vsomeip::eventgroup_t> eg{AEB_EG_ID};
    app_->offer_event(VEH_AEB_SERVICE_ID, VEH_INSTANCE_ID, AEB_STATE_EVENT_ID,
                      eg, vsomeip::event_type_e::ET_EVENT,
                      std::chrono::milliseconds::zero(),
                      false, true, nullptr,
                      vsomeip::reliability_type_e::RT_UNRELIABLE);

  // ECU → RP4 : E1 xx  (xx = 상태코드)
  aeb_listener_id_ = vehcan::add_event_listener(
      [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
          if (eid == 0xE1 && n >= 1) {
              const uint8_t st = p[0];
              {
                  std::lock_guard<std::mutex> lk(m_);
                  state_ = st;
              }
              auto pl = vsomeip::runtime::get()->create_payload();
              pl->set_data(&st, 1);
              app_->notify(VEH_AEB_SERVICE_ID, VEH_INSTANCE_ID, AEB_STATE_EVENT_ID, pl);
              LOG("AEB", "Event publish from CAN → state=" << unsigned(st));
          }
      }
  );

    LOG("AEB", "veh.aeb initialized");
}

void VehAebServer::on_enable(const std::shared_ptr<vsomeip::message> &req) {
    bool en = payload_to_value<bool>(req->get_payload());
    vehcan::aeb_enable(en ? 1 : 0);      // CAN으로 명령 전송 (0x20)
    {
        std::lock_guard<std::mutex> lk(m_);
        enabled_ = en;
    }
    std::string msg = en ? "AEB Enabled (CMD sent)" : "AEB Disabled (CMD sent)";

    auto resp = vsomeip::runtime::get()->create_response(req);
    resp->set_payload(make_payload(msg));
    app_->send(resp);
    LOG("AEB", msg);
}

void VehAebServer::on_get_state(const std::shared_ptr<vsomeip::message> &req) {
    // 캐시된 마지막 상태를 응답
    uint8_t state;
    {
        std::lock_guard<std::mutex> lk(m_);
        state = state_;
    }
    // (옵션) 실제 ECU 최신 상태를 트리거로 요청
    // vehcan::aeb_status_req();  // 0x21

    auto resp = vsomeip::runtime::get()->create_response(req);
    resp->set_payload(make_payload(state));
    app_->send(resp);
    LOG("AEB", "State requested → " << unsigned(state));
}

void VehAebServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;
    if (aeb_listener_id_ != 0) {
        vehcan::remove_event_listener(aeb_listener_id_);
        aeb_listener_id_ = 0;
    }
}