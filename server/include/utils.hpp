#pragma once
#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

// ============================
// 🧩 Timestamp & Logging
// ============================
inline std::string timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

// 안전한 매크로 패턴 (세미콜론은 호출부에서)
#define LOG(tag, msg) \
    do { std::cout << "[" << timestamp() << "] [" << tag << "] " << msg << std::endl; } while (0)

// ============================
// 🧩 Payload Helpers
// ============================
inline std::shared_ptr<vsomeip::payload> make_payload(const std::string &data) {
    auto pl = vsomeip::runtime::get()->create_payload();
    pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(data.data()), data.size());
    return pl;
}

template <typename T>
inline std::shared_ptr<vsomeip::payload> make_payload(const T &value) {
    auto pl = vsomeip::runtime::get()->create_payload();
    pl->set_data(reinterpret_cast<const vsomeip::byte_t*>(&value), sizeof(T));
    return pl;
}

template <typename T>
inline T payload_to_value(const std::shared_ptr<vsomeip::payload> &pl) {
    T val{};
    if (!pl || pl->get_length() == 0) return val;
    std::memcpy(&val, pl->get_data(),
        std::min<std::size_t>(sizeof(T), static_cast<std::size_t>(pl->get_length())));
    return val;
}

inline std::string payload_to_string(const std::shared_ptr<vsomeip::payload> &pl) {
    if (!pl || pl->get_length() == 0) return {};
    return std::string(reinterpret_cast<const char*>(pl->get_data()), pl->get_length());
}

// ============================
// 🧩 Misc
// ============================
inline void delay_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
