#include <iostream>
#include <thread>
#include <chrono>
#include <set>
#include <iomanip>
#include <sstream>
#include <vsomeip/vsomeip.hpp>
#include "veh_status_service.hpp"
#include "veh_logger.hpp"

// 주기적으로 상태 Event 송신

veh::Logger status_logger("logs/veh_status.log");

class VehStatusPublisher {
public:
    VehStatusPublisher()
        : app_(vsomeip::runtime::get()->create_application("veh_status_publisher")) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(status_logger, "vsomeip init failed!");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
                offer_service();
        });

        return true;
    }

    void start() {
        LOG_INFO(status_logger, "Starting veh_status_publisher...");
        std::thread pub_thread([this]() {
            while (true) {
                // 예: ToF 거리 100cm (status_type=0x04, value=0x00 0x64)
                publish_status(0x04, {0x00, 0x64});
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
        pub_thread.detach();
        app_->start();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    void offer_service() {
        std::set<vsomeip::eventgroup_t> groups = { VEH_STATUS_EVENTGROUP_ID };

        app_->offer_event(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            groups,
            vsomeip::event_type_e::ET_FIELD);

        app_->offer_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

        LOG_INFO(status_logger, "veh_status_service offered.");
    }

    static std::string to_hex(uint8_t v) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << int(v);
        return oss.str();
    }

    void publish_status(uint8_t type, std::vector<uint8_t> value) {
        std::vector<uint8_t> payload_data;
        payload_data.push_back(type);
        payload_data.insert(payload_data.end(), value.begin(), value.end());

        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(payload_data);

        app_->notify(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENT_ID, payload);

        LOG_INFO(status_logger, "Published status event type=0x" + to_hex(type));
    }
};

int main() {
    VehStatusPublisher pub;
    if (!pub.init()) {
        LOG_ERROR(status_logger, "Publisher init failed!");
        return 1;
    }
    pub.start();
    return 0;
}
