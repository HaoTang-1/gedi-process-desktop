"""Bottom-left panel: file info and footprint/waveform metrics (mutually exclusive tabs)."""

from __future__ import annotations

from typing import Any, Dict, Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from app.config import MUTED, NASA
from app.core.registry import METRICS
from app.data.gedi_loader import GediFile, WaveformResult
from app.i18n import get_language, t
from app.processing.metrics import compute_waveform_metrics


class InfoPanel(QWidget):
    """左下：文件信息 / 波形基础计算"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build()

    def _build(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(4)
        title = QLabel(t("panel_info"))
        self.title_label = title
        title.setStyleSheet(f"color:{NASA}; font-weight:600; padding:2px 4px;")
        lay.addWidget(title)

        self.tabs = QTabWidget()
        # --- file info tab ---
        w_info = QWidget()
        fl = QVBoxLayout(w_info)
        fl.setContentsMargins(4, 4, 4, 4)
        self.info_text = QLabel(t("hint_info"))
        self.info_text.setWordWrap(True)
        self.info_text.setAlignment(Qt.AlignLeft | Qt.AlignTop)
        self.info_text.setStyleSheet(f"color:{MUTED};")
        fl.addWidget(self.info_text)
        fl.addStretch(1)
        self.tabs.addTab(w_info, t("tab_file_info"))

        # --- metrics tab ---
        w_m = QWidget()
        ml = QVBoxLayout(w_m)
        ml.setContentsMargins(4, 4, 4, 4)
        row = QHBoxLayout()
        self.cmb_metric = QLabel(t("metric_module"))
        self.btn_recompute = QPushButton(t("act_recompute"))
        self.btn_recompute.clicked.connect(self._recompute_clicked)
        row.addWidget(self.cmb_metric)
        row.addStretch(1)
        row.addWidget(self.btn_recompute)
        ml.addLayout(row)
        self.table = QTableWidget(0, 2)
        self.table.setHorizontalHeaderLabels([t("col_metric"), t("col_value")])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.verticalHeader().setVisible(False)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        ml.addWidget(self.table)
        self.tabs.addTab(w_m, t("tab_metrics"))

        lay.addWidget(self.tabs, 1)
        self._last_result: Optional[WaveformResult] = None
        self._on_recompute = None
        self._metric_order_labels = None

    def retranslate(self):
        self.title_label.setText(t("panel_info"))
        self.tabs.setTabText(0, t("tab_file_info"))
        self.tabs.setTabText(1, t("tab_metrics"))
        self.btn_recompute.setText(t("act_recompute"))
        self.table.setHorizontalHeaderLabels([t("col_metric"), t("col_value")])
        if self._last_result is not None:
            self.show_metrics(self._last_result)
        else:
            self.info_text.setText(t("hint_info"))

    def set_recompute_handler(self, fn):
        self._on_recompute = fn

    def _recompute_clicked(self):
        if self._on_recompute:
            self._on_recompute()

    def show_file(self, gf: Optional[GediFile]):
        self.tabs.setCurrentIndex(0)
        if gf is None:
            self.info_text.setText(t("no_file_info"))
            return
        lines = [
            f"<b>{gf.path.name}</b>",
            f"{t('product')}: {gf.product}",
            f"{t('n_shots')}: {gf.n_shots:,}",
            f"{t('path')}: {gf.path}",
            f"{t('beams')}: {', '.join(gf.beams.keys())}",
        ]
        if gf.beams:
            b0 = next(iter(gf.beams))
            meta = gf.beams[b0]
            q = meta.get("quality")
            nq = "" if q is None else f", q=1 ~ {int((q == 1).sum()):,}"
            lines.append(f"{b0}: {meta['shot_number'].shape[0]:,} shots{nq}")
            if meta.get("sensitivity") is not None:
                import numpy as np

                s = np.asarray(meta["sensitivity"], dtype=float)
                s = s[np.isfinite(s)]
                if s.size:
                    lines.append(f"Sensitivity: {float(s.min()):.3f} ~ {float(s.max()):.3f}")
        self.info_text.setText("<br>".join(lines))

    def show_metrics(self, result: Optional[WaveformResult], metrics: Optional[Dict[str, Any]] = None):
        self.tabs.setCurrentIndex(1)
        self._last_result = result
        if result is None:
            self.table.setRowCount(0)
            return
        if metrics is None:
            metrics = compute_waveform_metrics(result, None)
        self.cmb_metric.setText(f"{t('metric_module')} · {result.product} {result.beam}")
        # bilingual labels
        zh = get_language() == "zh"
        order = [
            ("shot_number", "Shot 号" if zh else "Shot"),
            ("lon", "经度" if zh else "Longitude"),
            ("lat", "纬度" if zh else "Latitude"),
            ("product", "产品" if zh else "Product"),
            ("beam", "波束" if zh else "Beam"),
            ("n_samples", "采样点数" if zh else "Samples"),
            ("elev_top", "最高高程 (m)" if zh else "Elev top (m)"),
            ("elev_bot", "最低模式高程 (m)" if zh else "Elev low (m)"),
            ("canopy_height", "冠层高度代理 (m)" if zh else "Canopy H proxy (m)"),
            ("sensitivity", "Sensitivity"),
            ("quality", "质量标志" if zh else "Quality"),
            ("wf_mean", "波形均值" if zh else "Mean"),
            ("wf_std", "波形标准差" if zh else "Std"),
            ("wf_min", "波形最小值" if zh else "Min"),
            ("wf_max", "波形最大值" if zh else "Max"),
            ("wf_energy", "波形能量" if zh else "Energy"),
            ("wf_centroid", "能量质心 (bin)" if zh else "Centroid (bin)"),
            ("wf_fwhm", "FWHM (bin)"),
            ("wf_skew", "偏度" if zh else "Skewness"),
            ("wf_kurtosis", "峰度" if zh else "Kurtosis"),
            ("wf_peak_count", "峰值个数" if zh else "Peak count"),
            ("canopy_energy", "冠层能量" if zh else "Canopy energy"),
            ("ground_energy", "地表能量" if zh else "Ground energy"),
            ("gcr", "地表/冠层能量比" if zh else "GCR"),
            ("rh25", "RH25"),
            ("rh50", "RH50"),
            ("rh75", "RH75"),
            ("rh98", "RH98"),
            ("rh100", "RH100"),
            ("energy_total", "总能量 (L2A)" if zh else "Energy total (L2A)"),
            ("delta_time", "Delta time"),
            ("selected_algorithm", "反演算法" if zh else "Algorithm"),
        ]
        rows = []
        for key, label in order:
            if key not in metrics:
                continue
            val = metrics[key]
            if val is None:
                continue
            if isinstance(val, float):
                if val != val:
                    continue
                if abs(val) >= 1e6 or (abs(val) < 1e-4 and val != 0):
                    text = f"{val:.6g}"
                else:
                    text = f"{val:.4f}"
            else:
                text = str(val)
            rows.append((label, text))
        self.table.setRowCount(len(rows))
        for r, (k, v) in enumerate(rows):
            self.table.setItem(r, 0, QTableWidgetItem(k))
            self.table.setItem(r, 1, QTableWidgetItem(v))

    def show_message(self, msg: str):
        self.tabs.setCurrentIndex(0)
        self.info_text.setText(msg)
