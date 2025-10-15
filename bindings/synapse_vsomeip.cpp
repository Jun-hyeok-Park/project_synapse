#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <sstream>

#include <vsomeip/vsomeip.hpp>

#include "common/veh_control_service.hpp"
#include "common/veh_status_service.hpp"
#include "common/veh_logger.hpp"
#include "common/config.hpp"

namespace py = pybind11;

class VsClient {
public:
    VsClient()
    : app_(vsomeip::runtime::get()->create_application(veh::VSOMEIP_CLIENT_NAME)),
      logger_(veh::LOG_PATH_CLIENT), running_(false) {}

    bool init() {
        LOG_INFO(logger_, "synapse_vsomeip: init()");
        if (!app_->init()) {
            LOG_ERROR(logger_, "vsomeip init failed");
            return false;
        }

        app_->register_availability_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            std::bind(&VsClient::on_availability, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        app_->register_message_handler(
            VEH_CONTROL_SERVICE_ID,
            VEH_CONTROL_INSTANCE_ID,
            VEH_CONTROL_METHOD_ID,
            std::bind(&VsClient::on_response, this, std::placeholders::_1));

        // (선택) status 이벤트 구독을 원하면 여기에서 request_event/subscribe 추가

        return true;
    }

    void start() {
        if (running_) return;
        running_ = true;
        th_ = std::thread([this](){
            LOG_INFO(logger_, "synapse_vsomeip: start() -> app->start()");
            app_->start();
            LOG_INFO(logger_, "synapse_vsomeip: app->start() returned");
        });
    }

    void stop() {
        if (!running_) return;
        LOG_INFO(logger_, "synapse_vsomeip: stop()");
        app_->stop();
        running_ = false;
        if (th_.joinable()) th_.join();
    }

    ~VsClient() {
        try { stop(); } catch (...) {}
    }

    // Python에서 호출: cmd_type, values(list[int]) -> 8B payload
    void send_command(uint8_t cmd_type, const std::vector<uint8_t>& values) {
        std::vector<vsomeip::byte_t> data(8, 0x00);
        data[0] = cmd_type;
        for (size_t i = 0; i < values.size() && i < 7; ++i)
            data[i + 1] = values[i];

        auto req = vsomeip::runtime::get()->create_request();
        req->set_service(VEH_CONTROL_SERVICE_ID);
        req->set_instance(VEH_CONTROL_INSTANCE_ID);
        req->set_method(VEH_CONTROL_METHOD_ID);
        auto pl = vsomeip::runtime::get()->create_payload(data);
        req->set_payload(pl);
        app_->send(req);

        std::ostringstream oss;
        oss << "Python → Sent [0x" << std::hex << (int)cmd_type << "] : ";
        for (auto b : data) oss << std::uppercase << std::setw(2) << std::setfill('0') << std::hex << (int)b << " ";
        LOG_INFO(logger_, oss.str());
    }

    // (선택) Python 콜백 등록: 서버 응답 OK/ERR
    void set_response_callback(py::function fn) {
        response_cb_ = fn; // GIL은 pybind11이 호출 시점에 획득
    }

    // (선택) Python 콜백 등록: 상태 이벤트 수신
    void set_event_callback(py::function fn) {
        event_cb_ = fn;
        // 원하면 여기서 request_event/subscribe 구현 추가
    }

private:
    void on_availability(vsomeip::service_t, vsomeip::instance_t, bool avail) {
        LOG_INFO(logger_, std::string("veh_control_service ") + (avail ? "available" : "unavailable"));
    }

    void on_response(const std::shared_ptr<vsomeip::message>& resp) {
        auto pl = resp->get_payload();
        if (pl && pl->get_length() > 0) {
            uint8_t code = pl->get_data()[0];
            LOG_INFO(logger_, std::string("Server response: ") + (code == VEH_RESP_OK ? "OK" : "ERROR"));
            if (response_cb_) {
                // Python 콜백 호출
                py::gil_scoped_acquire gil;
                try { response_cb_(code); } catch (...) {}
            }
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    veh::Logger logger_;
    std::thread th_;
    std::atomic<bool> running_;
    py::function response_cb_;
    py::function event_cb_;
};

// pybind11 모듈 정의
PYBIND11_MODULE(synapse_vsomeip, m) {
    m.doc() = "Project SYNAPSE vsomeip Python bindings";

    py::class_<VsClient>(m, "Client")
        .def(py::init<>())
        .def("init", &VsClient::init)
        .def("start", &VsClient::start)
        .def("stop", &VsClient::stop)
        .def("send_command", &VsClient::send_command,
             py::arg("cmd_type"), py::arg("values"))
        .def("set_response_callback", &VsClient::set_response_callback)
        .def("set_event_callback", &VsClient::set_event_callback);
}
