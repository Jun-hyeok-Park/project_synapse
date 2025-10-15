/*
    목적: 각 노드(클라이언트/서버)가 자신의 로그 파일에 자동으로 기록하도록 지원
    특징: LOG_INFO, LOG_WARN, LOG_ERROR 매크로로 간단히 사용
*/
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <mutex>

namespace veh {

enum class LogLevel {
    INFO,
    WARN,
    ERROR
};

class Logger {
private:
    std::ofstream file;
    std::mutex lock;
    std::string log_name;

    // 현재 시각 문자열 반환
    std::string timestamp() {
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string level_to_str(LogLevel lv) {
        switch (lv) {
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default:              return "LOG";
        }
    }

public:
    explicit Logger(const std::string& filename = "veh_default.log") {
        file.open(filename, std::ios::out | std::ios::app);
        log_name = filename;
        if (!file.is_open())
            std::cerr << "[veh_logger] Failed to open log file: " << filename << std::endl;
    }

    ~Logger() {
        if (file.is_open()) file.close();
    }

    void write(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> guard(lock);
        if (!file.is_open()) return;

        file << "[" << timestamp() << "]"
             << "[" << level_to_str(level) << "] "
             << msg << std::endl;

        // 콘솔에도 출력
        std::cout << "[" << level_to_str(level) << "] " << msg << std::endl;
    }
};

// ────────────────────────────────
// 전역 매크로 (간편 사용용)
// ────────────────────────────────
#define LOG_INFO(logger, msg)  (logger).write(veh::LogLevel::INFO,  msg)
#define LOG_WARN(logger, msg)  (logger).write(veh::LogLevel::WARN,  msg)
#define LOG_ERROR(logger, msg) (logger).write(veh::LogLevel::ERROR, msg)

} // namespace veh
