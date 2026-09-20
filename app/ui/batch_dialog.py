"""Batch processing dialog for multi-file metric extraction."""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional

import pandas as pd
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
)

from app.config import BEAM_IDS
from app.data.gedi_loader import GediLoader
from app.processing.batch import BatchConfig, run_batch
from app.processing.waveform_ops import ProcessParams


class BatchWorker(QThread):
    progress = Signal(int, int, str)
    finished_ok = Signal(object)
    failed = Signal(str)

    def __init__(self, loader: GediLoader, config: BatchConfig, parent=None):
        super().__init__(parent)
        self.loader = loader
        self.config = config

    def run(self):
        try:
            def cb(cur, total, msg):
                self.progress.emit(cur, total, msg)

            result = run_batch(self.loader, self.config, progress=cb)
            self.finished_ok.emit(result)
        except Exception as e:
            self.failed.emit(str(e))


class BatchDialog(QDialog):
    def __init__(
        self,
        loader: GediLoader,
        process_params: Optional[ProcessParams],
        parent=None,
        visible_ids: Optional[set] = None,
    ):
        super().__init__(parent)
        self.loader = loader
        self.process_params = process_params
        self.visible_ids = set(visible_ids) if visible_ids else None
        self.worker: Optional[BatchWorker] = None
        self.result_df: Optional[pd.DataFrame] = None
        self.setWindowTitle("批处理 — 多文件波形/光斑指标")
        self.resize(640, 560)
        self._build_ui()
        self._reload_files()

    def _build_ui(self):
        root = QVBoxLayout(self)

        grp_f = QGroupBox("1. 选择数据文件")
        fl = QVBoxLayout(grp_f)
        self.file_list = QListWidget()
        self.file_list.setSelectionMode(QListWidget.ExtendedSelection)
        fl.addWidget(self.file_list)
        btn_row = QHBoxLayout()
        self.btn_all = QPushButton("全选")
        self.btn_none = QPushButton("全不选")
        self.btn_all.clicked.connect(lambda: self.file_list.selectAll())
        self.btn_none.clicked.connect(lambda: self.file_list.clearSelection())
        btn_row.addWidget(self.btn_all)
        btn_row.addWidget(self.btn_none)
        btn_row.addStretch(1)
        fl.addLayout(btn_row)

        grp_o = QGroupBox("2. 抽样与质量控制")
        form = QFormLayout(grp_o)
        self.cmb_beam = QComboBox()
        self.cmb_beam.addItem("全部波束", None)
        for b in BEAM_IDS:
            self.cmb_beam.addItem(b, b)
        self.chk_quality = QCheckBox("仅使用质量标志=1 的光斑")
        self.chk_quality.setChecked(True)
        self.spin_stride = QSpinBox()
        self.spin_stride.setRange(1, 200)
        self.spin_stride.setValue(20)
        self.spin_stride.setToolTip("步长=1 表示全部；增大可加速预览批处理")
        self.spin_max = QSpinBox()
        self.spin_max.setRange(0, 100000)
        self.spin_max.setValue(200)
        self.spin_max.setSpecialValueText("全部")
        self.spin_max.setToolTip("每个波束最多计算的光斑数；0=全部")
        form.addRow("波束", self.cmb_beam)
        form.addRow(self.chk_quality)
        form.addRow("抽样步长", self.spin_stride)
        form.addRow("每波束上限", self.spin_max)

        grp_out = QGroupBox("3. 输出")
        ol = QFormLayout(grp_out)
        self.ed_out = QLineEdit()
        self.ed_out.setText(str(Path.cwd() / "output" / "gedi_batch_metrics.csv"))
        btn_browse = QPushButton("浏览…")
        btn_browse.clicked.connect(self._browse_out)
        row = QHBoxLayout()
        row.addWidget(self.ed_out)
        row.addWidget(btn_browse)
        ol.addRow("CSV 路径", row)
        self.lbl_info = QLabel("计算指标将使用右侧当前处理参数（滤波等会作用于 L1B 波形指标）。")
        self.lbl_info.setWordWrap(True)
        ol.addRow(self.lbl_info)

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.lbl_status = QLabel("就绪")

        self.btn_run = QPushButton("开始批处理")
        self.btn_run.setObjectName("primary")
        self.btn_run.clicked.connect(self._start)
        self.btn_export_preview = QPushButton("导出当前结果")
        self.btn_export_preview.setEnabled(False)
        self.btn_export_preview.clicked.connect(self._export_now)
        self.btn_close = QPushButton("关闭")
        self.btn_close.clicked.connect(self.close)

        bottom = QHBoxLayout()
        bottom.addWidget(self.btn_run)
        bottom.addWidget(self.btn_export_preview)
        bottom.addStretch(1)
        bottom.addWidget(self.btn_close)

        self.preview = QListWidget()
        self.preview.setMaximumHeight(120)

        root.addWidget(grp_f)
        root.addWidget(grp_o)
        root.addWidget(grp_out)
        root.addWidget(self.progress)
        root.addWidget(self.lbl_status)
        root.addWidget(QLabel("最近结果预览:"))
        root.addWidget(self.preview)
        root.addLayout(bottom)

    def _reload_files(self):
        self.file_list.clear()
        for gf in self.loader.files:
            item = QListWidgetItem(gf.display_name)
            item.setData(Qt.UserRole, gf.file_id)
            self.file_list.addItem(item)
            # default: visible files in tree, else all
            if self.visible_ids is None:
                item.setSelected(True)
            else:
                item.setSelected(gf.file_id in self.visible_ids)

    def _browse_out(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "导出 CSV", self.ed_out.text(), "CSV Files (*.csv)"
        )
        if path:
            self.ed_out.setText(path)

    def _selected_file_ids(self) -> List[int]:
        ids = []
        for item in self.file_list.selectedItems():
            fid = item.data(Qt.UserRole)
            if fid is not None:
                ids.append(int(fid))
        return ids

    def _start(self):
        fids = self._selected_file_ids()
        if not fids:
            QMessageBox.warning(self, "批处理", "请至少选择一个数据文件。")
            return
        beam = self.cmb_beam.currentData()
        beams = [beam] if beam else None
        max_shots = int(self.spin_max.value())
        config = BatchConfig(
            file_ids=fids,
            beams=beams,
            max_shots_per_beam=None if max_shots == 0 else max_shots,
            quality_only=self.chk_quality.isChecked(),
            stride=int(self.spin_stride.value()),
            process_params=self.process_params,
            output_csv=Path(self.ed_out.text()),
        )
        self.btn_run.setEnabled(False)
        self.preview.clear()
        self.worker = BatchWorker(self.loader, config, self)
        self.worker.progress.connect(self._on_progress)
        self.worker.finished_ok.connect(self._on_done)
        self.worker.failed.connect(self._on_fail)
        self.worker.start()

    def _on_progress(self, cur, total, msg):
        if total > 0:
            self.progress.setValue(int(100 * cur / total))
        self.lbl_status.setText(msg)

    def _on_done(self, result):
        self.btn_run.setEnabled(True)
        self.result_df = result.to_dataframe()
        self.btn_export_preview.setEnabled(not self.result_df.empty)
        self.lbl_status.setText(
            f"完成: 成功 {result.n_ok} 条, 跳过 {result.n_skip} 条"
            + (f", 已保存 {result.output_path}" if result.output_path else "")
        )
        self.preview.clear()
        if not self.result_df.empty:
            head = self.result_df.head(8)
            for _, row in head.iterrows():
                text = f"{row.get('file','?')} | {row.get('beam','?')} | shot={row.get('shot_number','?')} | rh98={row.get('rh98')} | fwhm={row.get('wf_fwhm')}"
                self.preview.addItem(str(text))
            QMessageBox.information(
                self,
                "批处理完成",
                f"共计算 {result.n_ok} 条光斑指标。\n"
                + (f"结果已写入:\n{result.output_path}" if result.output_path else "可点击「导出当前结果」保存 CSV。"),
            )
        else:
            QMessageBox.information(self, "批处理完成", "未得到有效结果，请检查筛选条件。")

    def _on_fail(self, msg):
        self.btn_run.setEnabled(True)
        self.lbl_status.setText("失败")
        QMessageBox.critical(self, "批处理失败", msg)

    def _export_now(self):
        if self.result_df is None or self.result_df.empty:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "导出 CSV", self.ed_out.text(), "CSV Files (*.csv)"
        )
        if not path:
            return
        self.result_df.to_csv(path, index=False, encoding="utf-8-sig")
        self.ed_out.setText(path)
        self.lbl_status.setText(f"已导出: {path}")
