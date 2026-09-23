"""Right panel: display / scalar-color options."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QGroupBox,
    QLabel,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from app.config import MUTED, NASA
from app.i18n import get_language, t


class DisplayPanel(QWidget):
    scalar_changed = Signal(str)
    point_size_changed = Signal(int)
    show_circles_changed = Signal(bool)
    beam_filter_changed = Signal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build()

    def _scalar_items(self):
        return [
            ("beam", t("scalar_beam")),
            ("file", t("scalar_file")),
            ("sensitivity", t("scalar_sens")),
            ("quality", t("scalar_quality")),
            ("canopy_proxy", t("scalar_dh")),
            ("elev_top", t("scalar_elev")),
        ]

    def _build(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(6, 6, 6, 6)
        lay.setSpacing(8)

        g1 = QGroupBox(t("color_by"))
        self.g1 = g1
        v1 = QVBoxLayout(g1)
        self.cmb_scalar = QComboBox()
        for key, label in self._scalar_items():
            self.cmb_scalar.addItem(label, key)
        self.cmb_scalar.currentIndexChanged.connect(
            lambda _: self.scalar_changed.emit(self.cmb_scalar.currentData())
        )
        self.lbl_scalar_cap = QLabel(t("scalar_field"))
        v1.addWidget(self.lbl_scalar_cap)
        v1.addWidget(self.cmb_scalar)
        self.lbl_legend = QLabel(t("modules_hint"))
        self.lbl_legend.setWordWrap(True)
        self.lbl_legend.setStyleSheet(f"color:{MUTED};")
        # use map legend note instead
        self.lbl_legend.setText("Legend on map")
        v1.addWidget(self.lbl_legend)

        g2 = QGroupBox(t("display"))
        self.g2 = g2
        v2 = QVBoxLayout(g2)
        self.spin_size = QSpinBox()
        self.spin_size.setRange(2, 30)
        self.spin_size.setValue(6)
        self.spin_size.valueChanged.connect(self.point_size_changed.emit)
        self.lbl_size = QLabel(t("point_size"))
        v2.addWidget(self.lbl_size)
        v2.addWidget(self.spin_size)
        self.chk_circle = QCheckBox(t("show_circle"))
        self.chk_circle.setChecked(True)
        self.chk_circle.toggled.connect(self.show_circles_changed.emit)
        v2.addWidget(self.chk_circle)

        g3 = QGroupBox(t("beam_filter"))
        self.g3 = g3
        v3 = QVBoxLayout(g3)
        self.cmb_beam = QComboBox()
        self._fill_beams()
        self.cmb_beam.currentIndexChanged.connect(
            lambda _: self.beam_filter_changed.emit(self.cmb_beam.currentData())
        )
        v3.addWidget(self.cmb_beam)

        g4 = QGroupBox(t("modules"))
        self.g4 = g4
        v4 = QVBoxLayout(g4)
        self.lbl_modules = QLabel(t("modules_hint"))
        self.lbl_modules.setWordWrap(True)
        self.lbl_modules.setStyleSheet(f"color:{MUTED};")
        v4.addWidget(self.lbl_modules)

        lay.addWidget(g1)
        lay.addWidget(g2)
        lay.addWidget(g3)
        lay.addWidget(g4)
        lay.addStretch(1)
        self._module_names = ([], [])

    def _fill_beams(self):
        current = self.cmb_beam.currentData() if self.cmb_beam.count() else None
        self.cmb_beam.blockSignals(True)
        self.cmb_beam.clear()
        self.cmb_beam.addItem(t("all_beams"), None)
        for b in ["BEAM0000", "BEAM0001", "BEAM0010", "BEAM0011", "BEAM0101", "BEAM0110", "BEAM1000", "BEAM1011"]:
            self.cmb_beam.addItem(b, b)
        if current is not None:
            idx = self.cmb_beam.findData(current)
            if idx >= 0:
                self.cmb_beam.setCurrentIndex(idx)
        self.cmb_beam.blockSignals(False)

    def retranslate(self):
        self.g1.setTitle(t("color_by"))
        self.g2.setTitle(t("display"))
        self.g3.setTitle(t("beam_filter"))
        self.g4.setTitle(t("modules"))
        self.lbl_scalar_cap.setText(t("scalar_field"))
        self.lbl_size.setText(t("point_size"))
        self.chk_circle.setText(t("show_circle"))
        self.lbl_modules.setText(t("modules_hint"))
        # keep scalar key
        key = self.cmb_scalar.currentData() or "beam"
        self.cmb_scalar.blockSignals(True)
        self.cmb_scalar.clear()
        for k, label in self._scalar_items():
            self.cmb_scalar.addItem(label, k)
        idx = self.cmb_scalar.findData(key)
        if idx >= 0:
            self.cmb_scalar.setCurrentIndex(idx)
        self.cmb_scalar.blockSignals(False)
        self._fill_beams()
        if self._module_names[0] or self._module_names[1]:
            self.update_module_list(*self._module_names)

    def current_scalar(self) -> str:
        return self.cmb_scalar.currentData() or "beam"

    def current_beam(self):
        return self.cmb_beam.currentData()

    def point_size(self) -> int:
        return int(self.spin_size.value())

    def update_module_list(self, transform_names, metric_names):
        self._module_names = (list(transform_names), list(metric_names))
        zh = get_language() == "zh"
        head_t = "已注册变换:" if zh else "Registered transforms:"
        head_m = "已注册计算:" if zh else "Registered metrics:"
        self.lbl_modules.setText(
            f"{head_t}\n- "
            + "\n- ".join(transform_names[:12])
            + ("\n…" if len(transform_names) > 12 else "")
            + f"\n\n{head_m}\n- "
            + "\n- ".join(metric_names)
        )
