#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vsomeip/vsomeip.hpp>
#include "../common/veh_control_service.hpp"
#include "../common/veh_status_service.hpp"
#include <thread>
#include <iostream>

namespace py = pybind11;

class VsomeipClient {
public:
    VsomeipClient() {
        app_ = vsomeip::runtime::get()->create_application("veh_gui_client");
        if (!app_->init()) {
            std::cerr << "[vsomeip] init failed!\n";
            return;
        }

        // 이벤트 핸들러 등록
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

                if (py_callback_)
                    py_callback_(msg_type, vec);
            });

        // 별도 스레드에서 vsomeip 실행
        worker_ = std::thread([this]() {
            app_->start();
        });

        // 서비스 및 이벤트 구독 요청
        app_->request_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);
        app_->subscribe(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENT_ID);

        std::cout << "[vsomeip] Client started successfully.\n";
    }

    ~VsomeipClient() {
        try {
            app_->unsubscribe(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID, VEH_STATUS_EVENT_ID);
            app_->release_service(VEH_STATUS_SERVICE_ID, VEH_STATUS_INSTANCE_ID);
            app_->stop();
            if (worker_.joinable()) worker_.join();
            std::cout << "[vsomeip] Client stopped.\n";
        } catch (...) {
            std::cerr << "[vsomeip] Exception on shutdown.\n";
        }
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
        py_callback_ = callback;
    }

    void poll_events() {
        static int cnt = 0;
        if (++cnt % 50 == 0)
            std::cout << "[poll] vsomeip alive\n";
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    py::function py_callback_;
    std::thread worker_;
};

PYBIND11_MODULE(synapse_vsomeip, m) {
    py::class_<VsomeipClient>(m, "VsomeipClient")
        .def(py::init<>())
        .def("send_command", &VsomeipClient::send_command)
        .def("set_event_callback", &VsomeipClient::set_event_callback)
        .def("poll_events", &VsomeipClient::poll_events);
}
