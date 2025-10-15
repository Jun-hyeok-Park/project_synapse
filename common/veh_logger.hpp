/*
    목적: 각 노드(클라이언트/서버)가 자신의 로그 파일에 자동으로 기록하도록 지원
    특징: LOG_INFO, LOG_WARN, LOG_ERROR 매크로로 간단히 사용
*/
#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>

namespace veh {

enum class LogLevel { INFO, WARN, ERROR };

class Logger {
public:
    explicit Logger(const std::string& filename = "veh.log")
        : file_(filename, std::ios::app), name_(filename) {
        if (!file_.is_open())
            std::cerr << "[veh_logger] Failed to open: " << filename << std::endl;
    }

    void write(LogLevel lv, const std::string& msg) {
        std::lock_guard<std::mutex> g(m_);
        if (!file_.is_open()) return;

        file_ << "[" << now() << "][" << to_str(lv) << "] " << msg << '\n';
        file_.flush();
        std::cout << "[" << to_str(lv) << "] " << msg << std::endl;
    }

private:
    std::ofstream file_;
    std::mutex m_;
    std::string name_;

    static std::string now() {
        std::time_t t = std::time(nullptr);
        std::tm tm{}; localtime_r(&t, &tm);
        std::ostringstream os;
        os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return os.str();
    }

    static const char* to_str(LogLevel lv) {
        switch (lv) {
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
        }
        return "LOG";
    }
};

#define LOG_INFO(lg, msg)  (lg).write(::veh::LogLevel::INFO,  (msg))
#define LOG_WARN(lg, msg)  (lg).write(::veh::LogLevel::WARN,  (msg))
#define LOG_ERROR(lg, msg) (lg).write(::veh::LogLevel::ERROR, (msg))

} // namespace veh

