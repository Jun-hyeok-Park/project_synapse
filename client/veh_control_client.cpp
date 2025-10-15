// veh_control_client.cpp
#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

static constexpr vsomeip::service_t  SERVICE_ID  = 0x1100;
static constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;
static constexpr vsomeip::method_t   METHOD_ID   = 0x0100;

class veh_client {
public:
    veh_client()
        : app_(vsomeip::runtime::get()->create_application("veh_control_client")) {}

    bool init() {
        if (!app_->init()) return false;

        app_->register_availability_handler(
            SERVICE_ID, INSTANCE_ID,
            std::bind(&veh_client::on_availability, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        app_->start();
        return true;
    }

    // ====== 빌더 유틸 ======
    static std::shared_ptr<vsomeip::payload> make_payload_dir(uint8_t dir) {
        std::vector<uint8_t> d = { 0x01, dir };
        return vec_to_payload(d);
    }
    static std::shared_ptr<vsomeip::payload> make_payload_duty(uint8_t duty) {
        std::vector<uint8_t> d = { 0x02, duty };
        return vec_to_payload(d);
    }
    static std::shared_ptr<vsomeip::payload> make_payload_aeb(bool on) {
        std::vector<uint8_t> d = { 0x03, static_cast<uint8_t>(on ? 1 : 0) };
        return vec_to_payload(d);
    }
    static std::shared_ptr<vsomeip::payload> make_payload_autopark(uint8_t sub) {
        std::vector<uint8_t> d = { 0x04, sub }; // 0x01 start, 0x00 cancel 등
        return vec_to_payload(d);
    }
    static std::shared_ptr<vsomeip::payload> make_payload_auth(const std::string &pwd) {
        std::vector<uint8_t> d = { 0x05 };
        d.insert(d.end(), pwd.begin(), pwd.end()); // ASCII raw
        return vec_to_payload(d);
    }
    static std::shared_ptr<vsomeip::payload> make_payload_fault(uint8_t sub) {
        std::vector<uint8_t> d = { 0xFE, sub }; // 0x01 E-Stop 등
        return vec_to_payload(d);
    }

    // ====== 테스트 시나리오 ======
    void run_demo_sequence() {
        // 서버가 올라올 시간 약간 대기 (서비스 가용 대기)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        call_and_print(make_payload_auth("1234"), "AUTH 1234");
        call_and_print(make_payload_dir(0x08),    "DIR Forward");
        call_and_print(make_payload_duty(70),     "DUTY 70%");
        call_and_print(make_payload_aeb(true),    "AEB ON");
        call_and_print(make_payload_autopark(0x01), "AP START");
        call_and_print(make_payload_fault(0x01),  "FAULT E-STOP");
    }

private:
    static std::shared_ptr<vsomeip::payload> vec_to_payload(const std::vector<uint8_t> &v) {
        auto pl = vsomeip::runtime::get()->create_payload();
        pl->set_data(v.data(), v.size());
        return pl;
    }

    void on_availability(vsomeip::service_t, vsomeip::instance_t, bool available) {
        std::cout << "[CLI] Service " << (available ? "available" : "not available") << "\n";
        if (available) {
            run_demo_sequence();
        }
    }

    void call_and_print(std::shared_ptr<vsomeip::payload> payload, const char *tag) {
        auto req = vsomeip::runtime::get()->create_request();
        req->set_service(SERVICE_ID);
        req->set_instance(INSTANCE_ID);
        req->set_method(METHOD_ID);
        req->set_payload(payload);

        // 동기식 요청-응답
        auto f = app_->send(req, true); // true: block for response
        const auto &resp = f.get();

        if (resp) {
            auto pl = resp->get_payload();
            const uint8_t *d = pl->get_data();
            std::size_t n = pl->get_length();
            if (n >= 1) {
                std::cout << "[CLI] " << tag << " -> resp: 0x"
                          << std::hex << int(d[0]) << std::dec << "\n";
            } else {
                std::cout << "[CLI] " << tag << " -> (no payload)\n";
            }
        } else {
            std::cout << "[CLI] " << tag << " -> (no response)\n";
        }
    }

    std::shared_ptr<vsomeip::application> app_;
};

int main() {
    veh_client c;
    if (!c.init()) return 1;
    // app_->start() 내부에서 이벤트루프가 돌아가므로 main이 바로 종료되지 않음.
    // 종료는 Ctrl+C 시그널로.
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
