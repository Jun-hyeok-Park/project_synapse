#pragma once
#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include <vector>

#include <vsomeip/vsomeip.hpp>
#include "veh_control_service.hpp"
#include "veh_status_service.hpp"
#include "veh_logger.hpp"

// Qt 스레드: vSomeIP 앱을 이 스레드에서 실행
class VsClientThread : public QThread {
    Q_OBJECT
public:
    explicit VsClientThread(QObject *parent = nullptr);
    ~VsClientThread() override;

    // GUI -> vSomeIP (명령 전송)
public slots:
    void sendDriveDirection(uint8_t dir);          // 0x01 + [dir]
    void sendSpeedDuty(uint16_t duty_0_1000);      // 0x02 + [duty/10] (예: 0~100)
    void sendAebControl(bool on);                  // 0x03 + [0x01/0x00]
    void sendAutoparkStart();                      // 0x04 + [0x01]

signals:
    // vSomeIP 이벤트 -> GUI (상태 표시)
    void aebStateChanged(bool active);
    void autoparkStateChanged(uint8_t state);
    void tofChanged(uint32_t mm);
    void authStateChanged(bool ok);

    void logLine(QString line); // 상태/로그 출력용(선택)

protected:
    void run() override;        // QThread 엔트리

private:
    void init_vsomeip();
    void start_vsomeip();
    void stop_vsomeip();

    void onEvent(const std::shared_ptr<vsomeip::message> &msg);
    void sendCommand(uint8_t cmdType, const std::vector<uint8_t> &val);

private:
    std::shared_ptr<vsomeip::application> app_;
    std::atomic<bool> running_{true};
    veh::Logger logger_{"logs/veh_unified_client_qt.log"};
};
