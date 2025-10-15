import sys
import os
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QLabel, QSlider, QGridLayout, QGroupBox, QFrame
)
from PyQt5.QtCore import Qt, QTimer
import synapse_vsomeip as sv  # ← C++ 바인딩 모듈

# ================================================================
#  Vehicle Control GUI
# ================================================================
class VehicleControlGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Project SYNAPSE - Vehicle Control GUI")
        self.setGeometry(600, 300, 500, 550)

        # ----- 상태 변수 -----
        self.speed = 0
        self.direction = "STOP"
        self.aeb = False
        self.autopark = False
        self.tof_distance = 0

        # ----- vsomeip 클라이언트 초기화 -----
        self.client = sv.VsomeipClient()
        self.client.set_event_callback(self.on_status_update)

        # ----- UI 초기화 -----
        self.init_ui()

        # ----- 주기적 이벤트 폴링 -----
        self.timer = QTimer()
        self.timer.timeout.connect(self.client.poll_events)
        self.timer.start(200)

    # ================================================================
    #  UI 초기화
    # ================================================================
    def init_ui(self):
        layout = QVBoxLayout()

        # --- Vehicle Status ---
        status_box = QGroupBox("Vehicle Status")
        grid = QGridLayout()
        self.dir_label = QLabel("Direction: STOP")
        self.spd_label = QLabel("Speed: 0%")
        self.aeb_label = QLabel("AEB: OFF")
        self.park_label = QLabel("AutoPark: OFF")
        self.tof_label = QLabel("ToF: 0 cm")

        for lbl in [self.dir_label, self.spd_label, self.aeb_label, self.park_label, self.tof_label]:
            lbl.setFrameStyle(QFrame.Panel | QFrame.Sunken)
            lbl.setAlignment(Qt.AlignCenter)

        grid.addWidget(self.dir_label, 0, 0)
        grid.addWidget(self.spd_label, 0, 1)
        grid.addWidget(self.aeb_label, 1, 0)
        grid.addWidget(self.park_label, 1, 1)
        grid.addWidget(self.tof_label, 2, 0, 1, 2)
        status_box.setLayout(grid)
        layout.addWidget(status_box)

        # --- Drive Control ---
        drive_box = QGroupBox("Drive Control")
        drive_grid = QGridLayout()
        btns = {
            "↖": (-1, -1), "↑": (0, -1), "↗": (1, -1),
            "←": (-1, 0), "Stop": (0, 0), "→": (1, 0),
            "↙": (-1, 1), "↓": (0, 1), "↘": (1, 1),
        }
        for label, pos in btns.items():
            btn = QPushButton(label)
            btn.clicked.connect(lambda _, d=label: self.on_direction(d))
            drive_grid.addWidget(btn, pos[1]+1, pos[0]+1)
        drive_box.setLayout(drive_grid)
        layout.addWidget(drive_box)

        # --- Speed Control ---
        spd_box = QGroupBox("Speed Control")
        vbox = QVBoxLayout()
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setRange(0, 100)
        self.speed_slider.valueChanged.connect(self.on_speed_change)
        vbox.addWidget(self.speed_slider)
        spd_box.setLayout(vbox)
        layout.addWidget(spd_box)

        # --- Safety Features ---
        safety_box = QGroupBox("Safety Features")
        hbox = QHBoxLayout()
        self.aeb_btn = QPushButton("AEB: OFF")
        self.autopark_btn = QPushButton("AutoPark: OFF")
        self.aeb_btn.clicked.connect(self.toggle_aeb)
        self.autopark_btn.clicked.connect(self.trigger_autopark)
        hbox.addWidget(self.aeb_btn)
        hbox.addWidget(self.autopark_btn)
        safety_box.setLayout(hbox)
        layout.addWidget(safety_box)

        # --- Exit ---
        exit_btn = QPushButton("Exit")
        exit_btn.clicked.connect(self.close)
        layout.addWidget(exit_btn)

        self.setLayout(layout)

    # ================================================================
    #  이벤트 핸들러
    # ================================================================
    def on_direction(self, direction):
        self.direction = direction
        self.dir_label.setText(f"Direction: {direction}")
        cmd_map = {"↑": 8, "↓": 2, "←": 4, "→": 6, "↖": 7, "↗": 9, "↙": 1, "↘": 3, "Stop": 5}
        if direction in cmd_map:
            self.client.send_command(cmd_map[direction], [self.speed])

    def on_speed_change(self, value):
        self.speed = value
        self.spd_label.setText(f"Speed: {value}%")

    def toggle_aeb(self):
        self.aeb = not self.aeb
        state = "ON" if self.aeb else "OFF"
        self.aeb_btn.setText(f"AEB: {state}")
        self.aeb_label.setText(f"AEB: {state}")
        self.client.send_command(0xA0, [1 if self.aeb else 0])

    def trigger_autopark(self):
        self.autopark = True
        self.park_label.setText("AutoPark: RUNNING")
        self.autopark_btn.setText("AutoPark: RUNNING")
        self.client.send_command(0xB0, [1])

    # ================================================================
    #  vsomeip 이벤트 콜백
    # ================================================================
    def on_status_update(self, msg_type, data):
        if not data or len(data) < 2:
            return

        status_type = data[0]
        if status_type == 0x04:  # ToF
            if len(data) >= 3:
                dist = int.from_bytes(bytes(data[1:3]), byteorder='little')
                self.tof_label.setText(f"ToF: {dist} cm")

        elif status_type == 0x02:  # AEB
            self.aeb = bool(data[1])
            state = "ON" if self.aeb else "OFF"
            self.aeb_btn.setText(f"AEB: {state}")
            self.aeb_label.setText(f"AEB: {state}")

        elif status_type == 0x03:  # AutoPark progress
            prog = data[1]
            if prog >= 100:
                self.park_label.setText("AutoPark: COMPLETE")
                self.autopark_btn.setText("AutoPark: OFF")
            else:
                self.park_label.setText(f"AutoPark: {prog}%")
                self.autopark_btn.setText("AutoPark: RUNNING")

    # ================================================================
    #  종료 이벤트
    # ================================================================
    def closeEvent(self, event):
        print("[GUI] Closing vsomeip client...")
        self.timer.stop()
        self.client = None
        event.accept()


# ================================================================
#  실행
# ================================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = VehicleControlGUI()
    gui.show()
    sys.exit(app.exec_())
