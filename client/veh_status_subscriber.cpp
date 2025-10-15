#include <iostream>
#include <iomanip>
#include <thread>
#include <vsomeip/vsomeip.hpp>
#include "veh_status_service.hpp"
#include "veh_logger.hpp"

// 상태 이벤트 수신 클라이언트

veh::Logger subscriber_logger("logs/veh_subscriber.log");

class VehStatusSubscriber {
public:
    VehStatusSubscriber()
        : app_(vsomeip::runtime::get()->create_application("veh_status_subscriber")) {}

    bool init() {
        if (!app_->init()) {
            LOG_ERROR(subscriber_logger, "vsomeip init failed!");
            return false;
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                LOG_INFO(subscriber_logger, "Client registered → requesting service...");
                app_->request_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);

                // ✅ vsomeip 3.5: subscribe(service, instance, eventgroup, major, event)
                app_->subscribe(VEH_STATUS_SERVICE_ID,
                                VEH_STATUS_INSTANCE_ID,
                                VEH_STATUS_EVENTGROUP_ID,
                                vsomeip::DEFAULT_MAJOR,
                                VEH_STATUS_EVENT_ID);
            }
        });

        app_->register_message_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            std::bind(&VehStatusSubscriber::on_status_event, this, std::placeholders::_1));

        return true;
    }

    void start() {
        LOG_INFO(subscriber_logger, "Starting veh_status_subscriber...");
        app_->start();
    }

private:
    std::shared_ptr<vsomeip::application> app_;

    void on_status_event(const std::shared_ptr<vsomeip::message>& msg) {
        auto payload = msg->get_payload();
        auto data_ptr = payload->get_data();
        auto len = payload->get_length();

        if (!data_ptr || len < 2) return;

        std::vector<uint8_t> data(data_ptr, data_ptr + len);
        uint8_t type = data[0];
        uint8_t val  = data[1];

        LOG_INFO(subscriber_logger,
                 "Received status event → Type: 0x" + to_hex(type) +
                 "  Value: 0x" + to_hex(val));
    }

    static std::string to_hex(uint8_t v) {
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setw(2)
            << std::setfill('0') << int(v);
        return oss.str();
    }
};

int main() {
    VehStatusSubscriber sub;
    if (!sub.init()) {
        LOG_ERROR(subscriber_logger, "Subscriber init failed!");
        return 1;
    }
    sub.start();
    return 0;
}
