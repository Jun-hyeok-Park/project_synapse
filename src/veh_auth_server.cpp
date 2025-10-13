#include "veh_auth_server.hpp"
#include <set>

VehAuthServer::VehAuthServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehAuthServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("AUTH", "vehcan init failed");
        return;
    }

    // ECU → RP4 : E4 <result> (0:Locked, 1:Unlocked)
    auth_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == 0xE4 && n >= 1) {
                const uint8_t result = p[0];
                authorized_.store(result == 1);
                app_->notify(VEH_AUTH_SERVICE_ID, VEH_INSTANCE_ID,
                             AUTH_STATE_NOTIFY_ID, make_payload(result));
                LOG("AUTH", "CAN→SOMEIP authState=" << unsigned(result));
            }
        }
    );

    // 이벤트 offer (Notification/Field)
    const std::set<vsomeip::eventgroup_t> groups{ AUTH_EG_ID };
    app_->offer_event(
        VEH_AUTH_SERVICE_ID, VEH_INSTANCE_ID, AUTH_STATE_NOTIFY_ID,
        groups,
        vsomeip::event_type_e::ET_FIELD,
        std::chrono::milliseconds::zero(),
        /*is_provided*/ true,
        /*is_change_resets*/ true,
        nullptr,
        vsomeip::reliability_type_e::RT_UNRELIABLE
    );

    // 초기 상태 브로드캐스트(선택): READY(=아직 잠김 상태를 의미하는 안내 문자열)
    app_->notify(VEH_AUTH_SERVICE_ID, VEH_INSTANCE_ID,
                 AUTH_STATE_NOTIFY_ID, make_payload(std::string("AUTH READY")),
                 /*force*/ false);

    // 메서드 핸들러
    app_->register_message_handler(
        VEH_AUTH_SERVICE_ID, VEH_INSTANCE_ID, AUTH_LOGIN_METHOD_ID,
        std::bind(&VehAuthServer::on_login, this, std::placeholders::_1));

    LOG("AUTH", "veh.auth initialized");
}

void VehAuthServer::on_login(const std::shared_ptr<vsomeip::message> &req) {
    using namespace vsomeip;

    const auto pl = req ? req->get_payload() : nullptr;
    const std::string input = pl ? payload_to_string(pl) : std::string();

    // CAN으로 로그인 요청 전송 (최대 4바이트 전송하도록 vehcan에서 잘라줌)
    vehcan::auth_login(input);

    // 응답(Method Response) — 즉시 ACK
    auto resp = runtime::get()->create_response(req);
    resp->set_payload(make_payload(std::string("LOGIN SENT")));
    app_->send(resp);

    // 단발 알림: 요청 보낸 클라이언트에만 “로그인 시도됨” 통지 (실제 결과는 E4 이벤트로 별도 전송됨)
    const client_t target = req->get_client();
    app_->notify_one(
        VEH_AUTH_SERVICE_ID, VEH_INSTANCE_ID, AUTH_STATE_NOTIFY_ID,
        make_payload(std::string("PENDING")),
        target,
        /*use_reliable*/ false
    );

    LOG("AUTH", "Login attempt payload=\"" << input << "\" (CAN 0x50 sent)");
}

void VehAuthServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    if (auth_listener_id_ != 0) {
        vehcan::remove_event_listener(auth_listener_id_);
        auth_listener_id_ = 0;
    }
}
