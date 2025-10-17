/*
    목적: 각 노드(클라이언트/서버)가 자신의 로그 파일에 자동으로 기록하도록 지원
    특징: LOG_INFO, LOG_WARN, LOG_ERROR 매크로로 간단히 사용
          - 컬러 콘솔 출력 지원 (INFO=파랑, WARN=노랑, ERROR=빨강)
          - 로그레벨 필터 설정 가능
          - 기존 인터페이스 100% 호환
*/
#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace veh {

enum class LogLevel { INFO, WARN, ERROR };

class Logger {
public:
    explicit Logger(const std::string &filename = "veh.log",
                    LogLevel min_level = LogLevel::INFO,
                    bool enable_color = true)
        : file_(filename, std::ios::app),
          name_(filename),
          min_level_(min_level),
          use_color_(enable_color) 
          {
            std::filesystem::path p(filename);
            if (p.has_parent_path()) 
            {
                std::filesystem::create_directories(p.parent_path());
            }
            if (!file_.is_open())
            {
                std::cerr << "[veh_logger] Failed to open: " << filename << std::endl;
            }
          }

    void write(LogLevel lv, const std::string &msg) {
        if (lv < min_level_)
            return;

        std::lock_guard<std::mutex> g(m_);
        if (!file_.is_open())
            return;

        // 파일 출력 (색상 없이)
        file_ << "[" << now() << "][" << to_str(lv) << "] " << msg << '\n';
        file_.flush();

        // 콘솔 출력 (색상 지원)
        if (use_color_) {
            std::cout << color_code(lv)
                      << "[" << to_str(lv) << "] "
                      << msg
                      << "\033[0m" << std::endl;
        } else {
            std::cout << "[" << to_str(lv) << "] " << msg << std::endl;
        }
    }

    void setLevel(LogLevel lv) { min_level_ = lv; }
    void enableColor(bool on) { use_color_ = on; }

private:
    std::ofstream file_;
    std::mutex m_;
    std::string name_;
    LogLevel min_level_;
    bool use_color_;

    static std::string now() {
        std::time_t t = std::time(nullptr);
        std::tm tm {};
        localtime_r(&t, &tm);
        std::ostringstream os;
        os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return os.str();
    }

    static const char *to_str(LogLevel lv) {
        switch (lv) {
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
        }
        return "LOG";
    }

    static const char *color_code(LogLevel lv) {
        switch (lv) {
            case LogLevel::INFO:  return "\033[94m"; // 연한 파랑
            case LogLevel::WARN:  return "\033[93m"; // 노랑
            case LogLevel::ERROR: return "\033[91m"; // 빨강
        }
        return "\033[0m"; // 기본색
    }
};

// ==============================
//  편의 매크로
// ==============================
#define LOG_INFO(lg, msg)  (lg).write(::veh::LogLevel::INFO,  (msg))
#define LOG_WARN(lg, msg)  (lg).write(::veh::LogLevel::WARN,  (msg))
#define LOG_ERROR(lg, msg) (lg).write(::veh::LogLevel::ERROR, (msg))

} // namespace veh
