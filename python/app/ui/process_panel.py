"""Right-side processing / metrics controls."""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from app.processing.waveform_ops import ProcessParams, describe_process_stack


class ProcessPanel(QWidget):
    process_requested = Signal()
    metrics_requested = Signal()
    batch_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(6, 6, 6, 6)
        root.setSpacing(8)

        # ---- filters ----
        grp_p = QGroupBox("波形基础处理")
        form = QFormLayout(grp_p)
        self.chk_baseline = QCheckBox("基线去除 (百分位地板)")
        self.chk_norm = QCheckBox("归一化 (0–1)")
        self.chk_savgol = QCheckBox("Savitzky-Golay 平滑")
        self.chk_avg = QCheckBox("滑动平均")
        self.chk_low = QCheckBox("Butterworth 低通滤波")
        self.chk_high = QCheckBox("Butterworth 高通滤波")
        self.chk_fft = QCheckBox("显示傅里叶变换 (FFT)")

        self.spin_savgol_w = QSpinBox()
        self.spin_savgol_w.setRange(5, 101)
        self.spin_savgol_w.setSingleStep(2)
        self.spin_savgol_w.setValue(11)
        self.spin_savgol_p = QSpinBox()
        self.spin_savgol_p.setRange(1, 5)
        self.spin_savgol_p.setValue(3)
        self.spin_avg_w = QSpinBox()
        self.spin_avg_w.setRange(1, 51)
        self.spin_avg_w.setValue(5)
        self.spin_fc_low = QDoubleSpinBox()
        self.spin_fc_low.setRange(0.001, 0.49)
        self.spin_fc_low.setDecimals(3)
        self.spin_fc_low.setSingleStep(0.01)
        self.spin_fc_low.setValue(0.15)
        self.spin_fc_high = QDoubleSpinBox()
        self.spin_fc_high.setRange(0.001, 0.49)
        self.spin_fc_high.setDecimals(3)
        self.spin_fc_high.setSingleStep(0.01)
        self.spin_fc_high.setValue(0.05)
        self.spin_order = QSpinBox()
        self.spin_order.setRange(1, 10)
        self.spin_order.setValue(4)

        for w in (
            self.chk_baseline,
            self.chk_norm,
            self.chk_savgol,
            self.chk_avg,
            self.chk_low,
            self.chk_high,
            self.chk_fft,
        ):
            form.addRow(w)
        form.addRow("SG 窗口", self.spin_savgol_w)
        form.addRow("SG 多项式阶", self.spin_savgol_p)
        form.addRow("滑动平均窗口", self.spin_avg_w)
        form.addRow("低通截止 (×Nyquist)", self.spin_fc_low)
        form.addRow("高通截止 (×Nyquist)", self.spin_fc_high)
        form.addRow("滤波阶数", self.spin_order)

        self.btn_apply = QPushButton("应用处理到当前波形")
        self.btn_apply.setObjectName("primary")
        self.btn_apply.clicked.connect(self.process_requested.emit)
        self.btn_reset = QPushButton("重置处理参数")
        self.btn_reset.clicked.connect(self.reset_params)
        row = QHBoxLayout()
        row.addWidget(self.btn_apply)
        row.addWidget(self.btn_reset)
        form.addRow(row)
        self.lbl_stack = QLabel("处理链: 无处理")
        self.lbl_stack.setWordWrap(True)
        self.lbl_stack.setStyleSheet("color:#5B6B7C;")
        form.addRow(self.lbl_stack)

        for chk in (
            self.chk_baseline,
            self.chk_norm,
            self.chk_savgol,
            self.chk_avg,
            self.chk_low,
            self.chk_high,
            self.chk_fft,
        ):
            chk.toggled.connect(self._update_stack_label)
        for sp in (
            self.spin_savgol_w,
            self.spin_savgol_p,
            self.spin_avg_w,
            self.spin_fc_low,
            self.spin_fc_high,
            self.spin_order,
        ):
            sp.valueChanged.connect(self._update_stack_label)

        # ---- metrics ----
        grp_m = QGroupBox("波形基础计算")
        mlay = QVBoxLayout(grp_m)
        self.btn_metrics = QPushButton("计算当前光斑指标")
        self.btn_metrics.setObjectName("accent")
        self.btn_metrics.clicked.connect(self.metrics_requested.emit)
        self.btn_batch = QPushButton("批处理多文件…")
        self.btn_batch.clicked.connect(self.batch_requested.emit)
        mlay.addWidget(self.btn_metrics)
        mlay.addWidget(self.btn_batch)
        self.metrics_table = QTableWidget(0, 2)
        self.metrics_table.setHorizontalHeaderLabels(["指标", "值"])
        self.metrics_table.horizontalHeader().setStretchLastSection(True)
        self.metrics_table.verticalHeader().setVisible(False)
        self.metrics_table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.metrics_table.setMinimumHeight(180)
        mlay.addWidget(self.metrics_table)

        root.addWidget(grp_p)
        root.addWidget(grp_m)
        root.addStretch(1)

        self.chk_fft.setChecked(True)
        self._update_stack_label()

    def reset_params(self):
        self.chk_baseline.setChecked(False)
        self.chk_norm.setChecked(False)
        self.chk_savgol.setChecked(False)
        self.chk_avg.setChecked(False)
        self.chk_low.setChecked(False)
        self.chk_high.setChecked(False)
        self.chk_fft.setChecked(True)
        self.spin_savgol_w.setValue(11)
        self.spin_savgol_p.setValue(3)
        self.spin_avg_w.setValue(5)
        self.spin_fc_low.setValue(0.15)
        self.spin_fc_high.setValue(0.05)
        self.spin_order.setValue(4)

    def _update_stack_label(self, *_):
        p = self.get_params()
        self.lbl_stack.setText(f"处理链: {describe_process_stack(p)}")

    def get_params(self) -> ProcessParams:
        return ProcessParams(
            fft=self.chk_fft.isChecked(),
            lowpass=self.chk_low.isChecked(),
            highpass=self.chk_high.isChecked(),
            savgol=self.chk_savgol.isChecked(),
            moving_avg=self.chk_avg.isChecked(),
            normalize=self.chk_norm.isChecked(),
            baseline=self.chk_baseline.isChecked(),
            cutoff_low=float(self.spin_fc_low.value()),
            cutoff_high=float(self.spin_fc_high.value()),
            filter_order=int(self.spin_order.value()),
            savgol_window=int(self.spin_savgol_w.value()),
            savgol_poly=int(self.spin_savgol_p.value()),
            avg_window=int(self.spin_avg_w.value()),
        )

    def show_metrics(self, metrics: dict):
        self.metrics_table.setRowCount(0)
        # preferred display order
        order = [
            ("shot_number", "Shot 号"),
            ("lon", "经度"),
            ("lat", "纬度"),
            ("product", "产品"),
            ("beam", "波束"),
            ("n_samples", "采样点数"),
            ("elev_top", "最高高程 (m)"),
            ("elev_bot", "最低模式高程 (m)"),
            ("canopy_height", "冠层高度代理 (m)"),
            ("sensitivity", "Sensitivity"),
            ("quality", "质量标志"),
            ("wf_mean", "波形均值"),
            ("wf_std", "波形标准差"),
            ("wf_min", "波形最小值"),
            ("wf_max", "波形最大值"),
            ("wf_energy", "波形能量"),
            ("wf_centroid", "能量质心 (bin)"),
            ("wf_fwhm", "FWHM (bin)"),
            ("wf_skew", "偏度"),
            ("wf_kurtosis", "峰度"),
            ("wf_peak_count", "峰值个数"),
            ("canopy_energy", "冠层能量"),
            ("ground_energy", "地表能量"),
            ("gcr", "地表/冠层能量比"),
            ("rh25", "RH25"),
            ("rh50", "RH50"),
            ("rh75", "RH75"),
            ("rh98", "RH98"),
            ("rh100", "RH100"),
            ("energy_total", "总能量 (L2A)"),
            ("delta_time", "Delta time"),
            ("selected_algorithm", "反演算法"),
        ]
        rows = []
        for key, label in order:
            if key not in metrics:
                continue
            val = metrics[key]
            if val is None:
                continue
            if isinstance(val, float):
                if val != val:  # NaN
                    continue
                if abs(val) >= 1e6 or (abs(val) < 1e-4 and val != 0):
                    text = f"{val:.6g}"
                else:
                    text = f"{val:.4f}"
            else:
                text = str(val)
            rows.append((label, text))
        self.metrics_table.setRowCount(len(rows))
        for r, (k, v) in enumerate(rows):
            self.metrics_table.setItem(r, 0, QTableWidgetItem(k))
            self.metrics_table.setItem(r, 1, QTableWidgetItem(v))
