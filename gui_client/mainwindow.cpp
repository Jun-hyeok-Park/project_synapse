#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QStyle>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();

    vsThread_ = new VsClientThread(this);
    connect(vsThread_, &VsClientThread::aebStateChanged,    this, &MainWindow::onAebState);
    connect(vsThread_, &VsClientThread::autoparkStateChanged,this, &MainWindow::onAutoparkState);
    connect(vsThread_, &VsClientThread::tofChanged,         this, &MainWindow::onTof);
    connect(vsThread_, &VsClientThread::authStateChanged,   this, &MainWindow::onAuth);
    connect(vsThread_, &VsClientThread::logLine,            this, &MainWindow::onLog);

    vsThread_->start();
}

MainWindow::~MainWindow() {
    if (vsThread_) {
        vsThread_->requestInterruption();
        vsThread_->quit();
        vsThread_->wait();
    }
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // ─── 상태 그룹 ─────────────────────────────────────────────
    auto *grpStatus = new QGroupBox("Vehicle Status", central);
    auto *gStatus = new QGridLayout(grpStatus);
    lblAebActive_ = new QLabel("AEB Active: -", grpStatus);
    lblAutopark_  = new QLabel("AutoPark: 0x00", grpStatus);
    lblTof_       = new QLabel("ToF: 0 mm", grpStatus);
    lblAuth_      = new QLabel("Auth: -", grpStatus);
    auto *lblPw = new QLabel("Password:", grpStatus);
    txtPw_ = new QLineEdit(grpStatus);
    txtPw_->setPlaceholderText("Enter password...");
    txtPw_->setEchoMode(QLineEdit::Password);
    auto *btnLogin = new QPushButton("LOGIN", grpStatus);
    connect(btnLogin, &QPushButton::clicked, this, [this]() {
        QString pw = txtPw_->text();
        if (!pw.isEmpty() && vsThread_)
            vsThread_->sendAuthPassword(pw.toStdString());
    });


    gStatus->addWidget(lblAebActive_, 0, 0);
    gStatus->addWidget(lblAutopark_,  0, 1);
    gStatus->addWidget(lblTof_,       1, 0);
    gStatus->addWidget(lblAuth_,      1, 1);
    gStatus->addWidget(lblPw,         0, 2);
    gStatus->addWidget(txtPw_,        0, 3);
    gStatus->addWidget(btnLogin,      0, 4);
    root->addWidget(grpStatus);

    // ─── 방향 버튼 그리드 (8방향 + 가운데 STOP) ────────────────
    auto *grpDir = new QGroupBox("Direction", central);
    auto *gDir = new QGridLayout(grpDir);
    // 숫자패드 느낌: ↖7  ↑8  ↗9 / ←4 ●5 →6 / ↙1 ↓2 ↘3
    struct DirBtn { const char* text; int r; int c; uint8_t val; };
    std::vector<DirBtn> btns = {
        {"↖",0,0,0x07}, {"↑",0,1,0x08}, {"↗",0,2,0x09},
        {"←",1,0,0x04}, {"■",1,1,0x05}, {"→",1,2,0x06},
        {"↙",2,0,0x01}, {"↓",2,1,0x02}, {"↘",2,2,0x03},
    };
    for (auto &b: btns) {
        auto *btn = makeDirBtn(b.text, b.val);
        gDir->addWidget(btn, b.r, b.c);
    }
    root->addWidget(grpDir);

    // ─── 속도 슬라이더 ─────────────────────────────────────────
    auto *rowSpeed = new QHBoxLayout();
    auto *lbl = new QLabel("Speed:", central);
    sldSpeed_ = new QSlider(Qt::Horizontal, central);
    sldSpeed_->setRange(0, 1000);       // 0~1000 (duty)
    sldSpeed_->setValue(0);
    connect(sldSpeed_, &QSlider::valueChanged, this, &MainWindow::onSpeedChanged);
    rowSpeed->addWidget(lbl);
    rowSpeed->addWidget(sldSpeed_);
    root->addLayout(rowSpeed);

    // ─── 제어 버튼: AEB ON/OFF, AUTOPARK ──────────────────────
    auto *rowCtrl = new QHBoxLayout();
    auto *btnAebOn = new QPushButton("AEB ENABLE", central);
    auto *btnAebOff= new QPushButton("AEB DISABLE", central);
    auto *btnApStart= new QPushButton("AUTOPARK START", central);
    connect(btnAebOn,  &QPushButton::clicked, this, &MainWindow::onAebOn);
    connect(btnAebOff, &QPushButton::clicked, this, &MainWindow::onAebOff);
    connect(btnApStart,&QPushButton::clicked, this, &MainWindow::onAutoparkStart);
    rowCtrl->addWidget(btnAebOn);
    rowCtrl->addWidget(btnAebOff);
    rowCtrl->addWidget(btnApStart);
    root->addLayout(rowCtrl);

    // ─── 로그 ─────────────────────────────────────────────────
    txtLog_ = new QTextEdit(central);
    txtLog_->setReadOnly(true);
    txtLog_->setMinimumHeight(160);
    root->addWidget(txtLog_);

    setCentralWidget(central);
    setWindowTitle("veh_unified_client - Qt GUI");
    resize(680, 520);
}

QPushButton* MainWindow::makeDirBtn(const QString &txt, uint8_t dirValue) {
    auto *btn = new QPushButton(txt, this);
    btn->setFixedSize(72, 48);
    btn->setProperty("dirVal", dirValue);
    dirMap_.insert(btn, dirValue);
    connect(btn, &QPushButton::clicked, this, &MainWindow::onDirClicked);
    return btn;
}

void MainWindow::onDirClicked() {
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    uint8_t dir = dirMap_.value(btn, 0x05);
    if (vsThread_) vsThread_->sendDriveDirection(dir);
}

void MainWindow::onSpeedChanged(int v) {
    int step = (v / 10) * 10;   // 10 단위로 반올림
    sldSpeed_->setValue(step);
    if (vsThread_) vsThread_->sendSpeedDuty(static_cast<uint16_t>(step));
}

void MainWindow::onAebOn() {
    if (vsThread_) vsThread_->sendAebControl(true);
}

void MainWindow::onAebOff() {
    if (vsThread_) vsThread_->sendAebControl(false);
}

void MainWindow::onAutoparkStart() {
    if (vsThread_) vsThread_->sendAutoparkStart();
}

// ── 상태 UI 업데이트 ───────────────────────────────────────────
void MainWindow::onAebState(bool active) {
    lblAebActive_->setText(QString("AEB Active: %1").arg(active ? "ON" : "OFF"));
}
void MainWindow::onAutoparkState(uint8_t st) {
    lblAutopark_->setText(QString("AutoPark: 0x%1").arg(QString::number(st, 16).rightJustified(2, '0')));
}
void MainWindow::onTof(uint32_t mm) {
    lblTof_->setText(QString("ToF: %1 mm").arg(mm));
}
void MainWindow::onAuth(bool ok) {
    lblAuth_->setText(QString("Auth: %1").arg(ok ? "OK" : "FAIL"));
}
void MainWindow::onLog(const QString &line) {
    txtLog_->append(line);
}
