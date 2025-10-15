#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vsomeip/vsomeip.hpp>
#include "../common/veh_control_service.hpp"
#include "../common/veh_status_service.hpp"

namespace py = pybind11;

class VsomeipClient {
public:
    VsomeipClient() {
        app_ = vsomeip::runtime::get()->create_application("veh_gui_client");
        app_->init();
        app_->start();
    }

    void send_command(uint8_t cmd, const std::vector<uint8_t> &payload) {
        auto msg = vsomeip::runtime::get()->create_request();
        msg->set_service(VEH_CONTROL_SERVICE_ID);
        msg->set_instance(VEH_CONTROL_INSTANCE_ID);
        msg->set_method(VEH_CONTROL_METHOD_ID);

        std::vector<uint8_t> data;
        data.push_back(cmd);
        data.insert(data.end(), payload.begin(), payload.end());

        msg->set_payload(vsomeip::runtime::get()->create_payload(data));
        app_->send(msg);
    }

    void set_event_callback(py::function callback) {
        // Python 콜백 저장
        py_callback_ = callback;

        app_->register_message_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            [this](const std::shared_ptr<vsomeip::message> &msg) {
                auto payload = msg->get_payload();
                auto data = payload->get_data();
                auto len = payload->get_length();

                std::vector<uint8_t> vec(data, data + len);
                uint16_t msg_type = msg->get_method();

                if (py_callback_) {
                    py_callback_(msg_type, vec);
                }
            });
    }

    void poll_events() {
        // vsomeip는 blocking loop 기반이라 별도 poll 불필요하지만,
        // GUI 쪽에서 주기적으로 호출 가능하게 placeholder
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    py::function py_callback_;
};

PYBIND11_MODULE(synapse_vsomeip, m) {
    py::class_<VsomeipClient>(m, "VsomeipClient")
        .def(py::init<>())
        .def("send_command", &VsomeipClient::send_command)
        .def("set_event_callback", &VsomeipClient::set_event_callback)
        .def("poll_events", &VsomeipClient::poll_events);
}
