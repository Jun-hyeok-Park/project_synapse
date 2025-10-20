#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTextEdit>
#include <QGridLayout>
#include <QGroupBox>
#include "vsclientthread.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent=nullptr);
    ~MainWindow() override;

private slots:
    // 버튼 핸들러
    void onDirClicked();                 // 방향 버튼 공용 슬롯
    void onSpeedChanged(int v);          // 슬라이더 변경
    void onAebOn();                      // AEB Enable
    void onAebOff();                     // AEB Disable
    void onAutoparkStart();

    // 상태 갱신
    void onAebState(bool active);
    void onAutoparkState(uint8_t st);
    void onTof(uint32_t mm);
    void onAuth(bool ok);
    void onLog(const QString &line);

private:
    void buildUi();
    QPushButton* makeDirBtn(const QString &txt, uint8_t dirValue);

private:
    // UI
    QLabel *lblAebActive_{};
    QLabel *lblAutopark_{};
    QLabel *lblTof_{};
    QLabel *lblAuth_{};

    QSlider *sldSpeed_{};
    QTextEdit *txtLog_{};

    // dir 버튼과 값 맵핑 저장
    QHash<QPushButton*, uint8_t> dirMap_;

    // vSomeIP 스레드
    VsClientThread *vsThread_{};
};
