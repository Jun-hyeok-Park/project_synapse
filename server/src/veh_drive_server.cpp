#include "veh_drive_server.hpp"
#include "veh_can.hpp"

VehDriveServer::VehDriveServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehDriveServer::init() {    
    // 이미 돌고 있으면 재초기화 방지
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    vehcan::init();
    if (!vehcan::init()) {
        running_.store(false);
        LOG("DRIVE", "vehcan init failed");
        return;
    }

    drive_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == 0xE0 && n >= 2) { // data: state, dir
                update_payload(p[0], p[1]);
                publish_event();
            }
        }
    );

    // message handlers
    app_->register_message_handler(
        VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID, DRIVE_SET_SPEED_ID,
        std::bind(&VehDriveServer::on_set_speed, this, std::placeholders::_1));

    app_->register_message_handler(
        VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID, DRIVE_SET_DIR_ID,
        std::bind(&VehDriveServer::on_set_dir, this, std::placeholders::_1));

    app_->register_message_handler(
        VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID, DRIVE_SOFT_STOP_ID,
        std::bind(&VehDriveServer::on_soft_stop, this, std::placeholders::_1));

    // event offer (UDP / eventgroup)
    std::set<vsomeip::eventgroup_t> eg{DRIVE_EG_ID};
    app_->offer_event(VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID, DRIVE_STATE_EVENT_ID,
                      eg, vsomeip::event_type_e::ET_EVENT,
                      std::chrono::milliseconds::zero(),
                      false, true, nullptr,
                      vsomeip::reliability_type_e::RT_UNRELIABLE);

    // 서비스 offer는 main에서 수행하지만, 중복 호출 무해 → 여기선 생략 가능
    // offer_service(app_, VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID);
    payload_ = vsomeip::runtime::get()->create_payload();
    update_payload(0, 5);

    publish_thread_ = std::thread([this]() {
        while (running_.load()) {
            publish_event();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    LOG("DRIVE", "veh.drive initialized");
}

void VehDriveServer::on_set_speed(const std::shared_ptr<vsomeip::message>& msg) {
    auto pl = msg->get_payload();
    if (!pl || pl->get_length() < 1) {
        auto res = vsomeip::runtime::get()->create_response(msg);
        res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
        app_->send(res);
        return;
    }

    uint8_t spd = pl->get_data()[0];
    vehcan::set_speed(spd);
    update_payload(spd, direction_);
    publish_event();
    // ACK 응답
    auto res = vsomeip::runtime::get()->create_response(msg);
    auto ack = make_payload(std::string("ACK: SPEED SET"));
    res->set_payload(ack);
    app_->send(res);
    LOG("DRIVE", "SPEED SET: " << unsigned(spd));
}

void VehDriveServer::on_set_dir(const std::shared_ptr<vsomeip::message>& msg) {
    auto pl = msg->get_payload();
    if (!pl || pl->get_length() < 1) {
        auto res = vsomeip::runtime::get()->create_response(msg);
        res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
        app_->send(res);
        return;
    }

    uint8_t dir = pl->get_data()[0];
    vehcan::set_direction(dir);
    update_payload(speed_, dir);
    publish_event();
    // ACK 응답
    auto res = vsomeip::runtime::get()->create_response(msg);
    auto ack = make_payload(std::string("ACK: DIR SET"));
    res->set_payload(ack);
    app_->send(res);
    LOG("DRIVE", "DIR SET: " << unsigned(dir));
}

void VehDriveServer::on_soft_stop(const std::shared_ptr<vsomeip::message>& msg) {
    vehcan::soft_stop();
    update_payload(0, direction_);
    auto res = vsomeip::runtime::get()->create_response(msg);
    auto ack_payload = make_payload(std::string("ACK: SOFT STOP"));
    res->set_payload(ack_payload);
    app_->send(res);
    LOG("DRIVE", "SOFT STOP");
}

void VehDriveServer::publish_event() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 같으면 전송 스킵
    if (speed_ == last_sent_spd_ && direction_ == last_sent_dir_)
        return;
    app_->notify(VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID, DRIVE_STATE_EVENT_ID, payload_);
    last_sent_spd_  = speed_;
    last_sent_dir_  = direction_;
    LOG("DRIVE", "Event published → SPD:" << unsigned(speed_) << " DIR:" << unsigned(direction_));
}

void VehDriveServer::update_payload(uint8_t spd, uint8_t dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    speed_ = spd;
    direction_ = dir;
    uint8_t data[2] = {spd, dir};
    payload_->set_data(data, sizeof(data));
}

void VehDriveServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return; // 이미 정지
    }
    if (publish_thread_.joinable())
        publish_thread_.join();
    if (drive_listener_id_ != 0) {
        vehcan::remove_event_listener(drive_listener_id_);
        drive_listener_id_ = 0;
    }
}