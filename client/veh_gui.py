import sys
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QSlider, QLineEdit, QGroupBox
)
from PyQt5.QtCore import Qt
import synapse_vsomeip as sv

# ===========================================================
# VEH GUI — Project SYNAPSE PyQt Control Interface
# ===========================================================

class VehGui(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Project SYNAPSE — Vehicle Control GUI")
        self.setGeometry(200, 200, 520, 440)

        # C++ vsomeip 클라이언트 직접 초기화
        self.client = sv.Client()
        assert self.client.init(), "vsomeip client init failed"
        self.client.set_response_callback(self.on_response_code)
        # (선택) self.client.set_event_callback(self.on_status_event)
        self.client.start()

        self.init_ui()

    def closeEvent(self, e):
        try:
            self.client.stop()
        except Exception:
            pass
        e.accept()

    def init_ui(self):
        main = QVBoxLayout()

        # ─────────────── 주행 제어 ───────────────
        g_drive = QGroupBox("Drive Control")
        lay = QHBoxLayout()
        for text in ["↑ Forward", "↓ Backward", "← Left", "→ Right", "■ Stop"]:
            btn = QPushButton(text); btn.setFixedHeight(48)
            btn.clicked.connect(self.on_drive); lay.addWidget(btn)
        g_drive.setLayout(lay); main.addWidget(g_drive)

        # ─────────────── 속도 제어 ───────────────
        g_speed = QGroupBox("Speed (Duty %)")
        lay = QVBoxLayout()
        self.lb_speed = QLabel("0%")
        sld = QSlider(Qt.Horizontal); sld.setRange(0, 100)
        sld.valueChanged.connect(lambda v: self.lb_speed.setText(f"{v}%"))
        sld.sliderReleased.connect(lambda: self.client.send_command(0x02, [sld.value()]))
        lay.addWidget(sld); lay.addWidget(self.lb_speed)
        g_speed.setLayout(lay); main.addWidget(g_speed)

        # ─────────────── AEB / AutoPark ───────────────
        g_auto = QGroupBox("AEB / AutoPark")
        lay = QHBoxLayout()
        btn_aeb = QPushButton("AEB ON")
        btn_ap  = QPushButton("AutoPark START")
        btn_aeb.clicked.connect(lambda: self.client.send_command(0x03, [0x01]))
        btn_ap.clicked.connect(lambda: self.client.send_command(0x04, [0x01]))
        lay.addWidget(btn_aeb); lay.addWidget(btn_ap)
        g_auto.setLayout(lay); main.addWidget(g_auto)

        # ─────────────── Auth ───────────────
        g_auth = QGroupBox("Auth")
        lay = QHBoxLayout()
        self.ed_pw = QLineEdit(); self.ed_pw.setPlaceholderText("Enter password (e.g., 1234)")
        btn_login = QPushButton("Login")
        btn_login.clicked.connect(self.on_auth)
        lay.addWidget(self.ed_pw); lay.addWidget(btn_login)
        g_auth.setLayout(lay); main.addWidget(g_auth)

        # ─────────────── Fault / Emergency ───────────────
        g_fault = QGroupBox("Fault")
        lay = QHBoxLayout()
        QPushButton.setAutoDefault
        btn_reset = QPushButton("Reset Fault")
        btn_estop = QPushButton("Emergency Stop")
        btn_reset.clicked.connect(lambda: self.client.send_command(0xFE, [0x00]))
        btn_estop.clicked.connect(lambda: self.client.send_command(0xFE, [0x01]))
        lay.addWidget(btn_reset); lay.addWidget(btn_estop)
        g_fault.setLayout(lay); main.addWidget(g_fault)

        # ─────────────── 상태 표시 ───────────────
        self.lb_status = QLabel("Status: Ready"); self.lb_status.setAlignment(Qt.AlignCenter)
        main.addWidget(self.lb_status)

        self.setLayout(main)

    # ========== UI Handlers ==========
    def on_drive(self):
        sender = self.sender().text()
        mapping = {"↑ Forward":0x08, "↓ Backward":0x02, "← Left":0x04, "→ Right":0x06, "■ Stop":0x05}
        v = mapping.get(sender, 0x05)
        self.client.send_command(0x01, [v])
        self.lb_status.setText(f"Drive: {sender}")

    def on_auth(self):
        pw = self.ed_pw.text()
        data = [ord(c) for c in pw][:7]
        self.client.send_command(0x05, data)
        self.lb_status.setText(f"Auth sent: {pw}")

    # ========== Callbacks from C++ ==========
    def on_response_code(self, code:int):
        self.lb_status.setText("Server Response: OK" if code == 0x00 else "Server Response: ERROR")

    # def on_status_event(self, status_type:int, payload:bytes):
    #     # 필요 시 이벤트 표시 로직 추가 (pybind에서 콜백 호출하도록 확장)
    #     pass

# ===========================================================
# MAIN
# ===========================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = VehGui()
    gui.show()
    sys.exit(app.exec_())
