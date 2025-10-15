import sys
import time
import threading
from PyQt5.QtWidgets import (
    QApplication, QWidget, QPushButton, QVBoxLayout, QLabel, QHBoxLayout, QMessageBox
)
from PyQt5.QtCore import Qt, QTimer
import synapse_vsomeip as sv


class VehGui(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Project SYNAPSE - Vehicle Control GUI")
        self.setGeometry(200, 200, 420, 300)

        # vsomeip client
        self.client = sv.VsomeipClient()
        self.client.init()
        self.client.register_status_callback(self.on_status_event)

        # 내부 상태
        self.last_event = "No event received"
        self.running = True

        # UI 구성
        self.setup_ui()

        # vsomeip 스레드 시작
        self.thread = threading.Thread(target=self.client.start, daemon=True)
        self.thread.start()

        # 주기적 상태 갱신
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_status)
        self.timer.start(1000)

    def setup_ui(self):
        # 상태 표시
        self.label_status = QLabel("Waiting for server...")
        self.label_status.setAlignment(Qt.AlignCenter)
        self.label_status.setStyleSheet("font-size: 15px; font-weight: bold;")

        # 제어 버튼
        self.btn_forward = QPushButton("↑ Forward")
        self.btn_backward = QPushButton("↓ Backward")
        self.btn_stop = QPushButton("■ Stop")
        self.btn_exit = QPushButton("Exit")

        for btn in [self.btn_forward, self.btn_backward, self.btn_stop, self.btn_exit]:
            btn.setFixedHeight(45)
            btn.setStyleSheet("font-size: 16px;")

        # 버튼 동작 연결
        self.btn_forward.clicked.connect(lambda: self.send_cmd(0x01, [0x08]))
        self.btn_backward.clicked.connect(lambda: self.send_cmd(0x01, [0x02]))
        self.btn_stop.clicked.connect(lambda: self.send_cmd(0x01, [0x05]))
        self.btn_exit.clicked.connect(self.close_app)

        # 레이아웃
        vbox = QVBoxLayout()
        vbox.addWidget(self.label_status)

        hbox1 = QHBoxLayout()
        hbox1.addWidget(self.btn_forward)
        hbox1.addWidget(self.btn_backward)

        hbox2 = QHBoxLayout()
        hbox2.addWidget(self.btn_stop)
        hbox2.addWidget(self.btn_exit)

        vbox.addLayout(hbox1)
        vbox.addLayout(hbox2)

        self.setLayout(vbox)

    def send_cmd(self, cmd_type, val):
        try:
            self.client.send_command(cmd_type, val)
            self.label_status.setText(f"📡 Sent: cmd=0x{cmd_type:02X}, val={val}")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to send command:\n{e}")

    def on_status_event(self, data):
        """C++ vsomeip 이벤트 수신 → Python 콜백"""
        if not data:
            return
        status_type = data[0]
        value = data[1] if len(data) > 1 else None

        text = f"Event → type=0x{status_type:02X}, val=0x{value:02X}" if value else f"Event type={status_type}"
        self.last_event = text

    def update_status(self):
        self.label_status.setText(self.last_event)

    def close_app(self):
        self.running = False
        self.client.stop()
        self.close()


def main():
    app = QApplication(sys.argv)
    gui = VehGui()
    gui.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
