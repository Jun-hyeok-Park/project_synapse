#include "veh_fault_server.hpp"
#include <set>
#include <cstdlib>

// 사양에 Fault용 CAN Event ID가 명시되지 않아 기본 0xEE로 둠.
// 필요 시 컴파일 옵션으로 덮어쓰기: -DVEH_FAULT_CAN_EID=0xE9
#ifndef VEH_FAULT_CAN_EID
#define VEH_FAULT_CAN_EID 0xEE
#endif

VehFaultServer::VehFaultServer(std::shared_ptr<vsomeip::application> app)
    : app_(std::move(app)) {}

void VehFaultServer::init() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    if (!vehcan::init()) {
        running_.store(false);
        LOG("FAULT", "vehcan init failed");
        return;
    }

    using namespace vsomeip;

    const std::set<eventgroup_t> egs{ FAULT_EG_ID };
    app_->offer_event(
        VEH_FAULT_SERVICE_ID, VEH_INSTANCE_ID, FAULT_CODE_NOTIFY_ID,
        egs, event_type_e::ET_FIELD, std::chrono::milliseconds::zero(),
        true, false, nullptr, reliability_type_e::RT_RELIABLE
    );

    fault_listener_id_ = vehcan::add_event_listener(
        [this](uint8_t eid, const uint8_t* p, size_t n, uint32_t /*rxid*/) {
            if (eid == VEH_FAULT_CAN_EID && n >= 1) {
                const uint8_t code = p[0];
                app_->notify(VEH_FAULT_SERVICE_ID, VEH_INSTANCE_ID,
                             FAULT_CODE_NOTIFY_ID, make_payload<uint8_t>(code));
                LOG("FAULT", "CAN→SOMEIP Fault code=" << unsigned(code));
            }
        }
    );

    // 초기 상태 브로드캐스트(선택)
    app_->notify(VEH_FAULT_SERVICE_ID, VEH_INSTANCE_ID,
                 FAULT_CODE_NOTIFY_ID, make_payload<uint8_t>(0), false);

    // === 데모 ON/OFF 스위치 ===
    // 존재하고 값이 "1"이면 데모 ON, 그 외는 OFF
    const char* v = std::getenv("VEH_FAULT_DEMO");
    const bool demo_on = (v && std::string(v) == "1");

    if (demo_on) {
        f_ = std::thread(&VehFaultServer::fault_loop, this);
        LOG("FAULT", "veh.fault initialized (demo=ON)");
    } else {
        LOG("FAULT", "veh.fault initialized (demo=OFF)");
    }
}

void VehFaultServer::fault_loop() {
    while (running_.load()) {
        // 데모: 가끔 Fault 코드(1~4) 송출
        uint8_t fault = static_cast<uint8_t>(rand() % 6); // 0~5
        if (fault != 0) {
            app_->notify(
                VEH_FAULT_SERVICE_ID, VEH_INSTANCE_ID,
                FAULT_CODE_NOTIFY_ID,
                make_payload<uint8_t>(fault)
            );
            LOG("FAULT", "Demo fault → code=" << unsigned(fault));
        }
        delay_ms(3000);
    }
}

void VehFaultServer::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    if (f_.joinable())
        f_.join();

    if (fault_listener_id_ != 0) {
        vehcan::remove_event_listener(fault_listener_id_);
        fault_listener_id_ = 0;
    }
}
