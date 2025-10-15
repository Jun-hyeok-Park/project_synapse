#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>
#include <vsomeip/vsomeip.hpp>
#include "veh_control_service.hpp"
#include "veh_status_service.hpp"

namespace py = pybind11;

class PyVsomeipClient {
public:
    PyVsomeipClient()
        : app_(vsomeip::runtime::get()->create_application("py_client")) {}

    bool init() {
        if (!app_->init()) return false;

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                subscribe_status();
            }
        });

        app_->register_message_handler(
            VEH_STATUS_SERVICE_ID,
            VEH_STATUS_INSTANCE_ID,
            VEH_STATUS_EVENT_ID,
            std::bind(&PyVsomeipClient::on_status_event, this, std::placeholders::_1));

        return true;
    }

    void start() { th_ = std::thread([this]{ app_->start(); }); }
    void stop()  { app_->stop(); if (th_.joinable()) th_.join(); }

    void send_command(uint8_t cmd_type, const std::vector<uint8_t>& values) {
        auto req = vsomeip::runtime::get()->create_request();
        req->set_service(VEH_CONTROL_SERVICE_ID);
        req->set_instance(VEH_CONTROL_INSTANCE_ID);
        req->set_method(VEH_CONTROL_METHOD_ID);

        std::vector<uint8_t> data; data.reserve(1 + values.size());
        data.push_back(cmd_type);
        data.insert(data.end(), values.begin(), values.end());

        auto pl = vsomeip::runtime::get()->create_payload();
        pl->set_data(data);
        req->set_payload(pl);

        app_->send(req);
    }

    void register_status_callback(py::function cb) { cb_ = cb; }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread th_;
    py::function cb_;

    void subscribe_status() {
        app_->request_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);
        app_->subscribe(VEH_STATUS_SERVICE_ID,
                        VEH_STATUS_INSTANCE_ID,
                        VEH_STATUS_EVENTGROUP_ID,
                        vsomeip::DEFAULT_MAJOR,
                        VEH_STATUS_EVENT_ID);
    }

    void on_status_event(const std::shared_ptr<vsomeip::message>& msg) {
        auto pl  = msg->get_payload();
        auto ptr = pl->get_data();
        auto len = pl->get_length();
        if (!ptr || len == 0) return;

        std::vector<uint8_t> data(ptr, ptr + len);
        if (cb_) cb_(data); // Python 콜백 호출
    }
};

PYBIND11_MODULE(synapse_vsomeip, m) {
    py::class_<PyVsomeipClient>(m, "VsomeipClient")
        .def(py::init<>())
        .def("init",  &PyVsomeipClient::init)
        .def("start", &PyVsomeipClient::start)
        .def("stop",  &PyVsomeipClient::stop)
        .def("send_command", &PyVsomeipClient::send_command)
        .def("register_status_callback", &PyVsomeipClient::register_status_callback);
}
