#pragma once
#include <vsomeip/vsomeip.hpp>
#include <memory>
#include <iostream>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>

// 공용 유틸/상수
#include "utils.hpp"
#include "veh_common.hpp"

// CAN 유틸
#include "veh_can.hpp"

// 간단 서버 로깅
#define LOG_SERVER(tag, msg) \
    do { std::cout << "[SERVER][" << tag << "] " << msg << std::endl; } while (0)

// 공용 helper: 서비스 offer 로그 포함
inline void offer_service(std::shared_ptr<vsomeip::application> app,
                          vsomeip::service_t sid,
                          vsomeip::instance_t iid) {
    app->offer_service(sid, iid);
    LOG_SERVER("Offer", "Service offered → SID=0x" << std::hex << sid << " IID=0x" << iid);
}