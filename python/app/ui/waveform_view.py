"""Waveform panel — placed to the RIGHT of the map.

Orientation: height on Y (vertical), intensity on X (horizontal).
FFT/wavelet appear only when invoked from the Transform menu.
"""

from __future__ import annotations

from typing import Any, Dict, Optional

import numpy as np
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qtagg import NavigationToolbar2QT
from matplotlib.figure import Figure
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QLabel, QVBoxLayout, QWidget

from app.config import CYAN, GRID, INK, MUTED, NASA, PAPER
from app.data.gedi_loader import WaveformResult
from app.i18n import get_language, t
from app.ui import mpl_fonts  # noqa: F401
from app.ui.pan_zoom import WAVE_MAX_ZOOM_OUT, WAVE_MIN_SPAN_X, WAVE_MIN_SPAN_Y, PanZoomMixin


class WaveformCanvas(PanZoomMixin, FigureCanvas):
    def __init__(self, parent=None):
        self.fig = Figure(figsize=(4.2, 6.0), dpi=100, facecolor=PAPER)
        FigureCanvas.__init__(self, self.fig)
        self.setParent(parent)
        self.ax = self.fig.add_subplot(111)
        self._style(self.ax)
        self.setMinimumSize(280, 280)
        self._result: Optional[WaveformResult] = None
        self._display_y: Optional[np.ndarray] = None
        self._display_label = ""
        self._aux = None
        self._pz_was_drag = False
        self.enable_pan_zoom(WAVE_MIN_SPAN_X, WAVE_MIN_SPAN_Y, WAVE_MAX_ZOOM_OUT)
        try:
            self.setCursor(Qt.OpenHandCursor)
        except Exception:
            pass

    def _style(self, ax):
        ax.set_facecolor("#FBFCFE")
        ax.tick_params(colors=INK, labelsize=8)
        for spine in ax.spines.values():
            spine.set_color(GRID)
        ax.grid(True, color=GRID, linewidth=0.5, alpha=0.85)

    def wheelEvent(self, event):
        delta = event.angleDelta().y()
        if delta == 0:
            super().wheelEvent(event)
            return
        pos = event.position()
        try:
            inv = self.ax.transData.inverted()
            fig_h = self.figure.bbox.height
            xdata, ydata = inv.transform((pos.x(), fig_h - pos.y()))
        except Exception:
            super().wheelEvent(event)
            return

        class _Ev:
            pass

        ev = _Ev()
        ev.inaxes = self.ax
        ev.xdata = xdata
        ev.ydata = ydata
        ev.button = "up" if delta > 0 else "down"
        self._pz_scroll(ev)
        event.accept()

    def clear_message(self, msg: str):
        self._result = None
        self._display_y = None
        self._aux = None
        self.fig.clf()
        self.ax = self.fig.add_subplot(111)
        self._style(self.ax)
        self.set_data_bounds(None)
        self.ax.text(
            0.5,
            0.5,
            msg,
            transform=self.ax.transAxes,
            ha="center",
            va="center",
            color=MUTED,
            fontsize=10,
        )
        self.ax.set_xticks([])
        self.ax.set_yticks([])
        self.draw_idle()

    def _profile_xy(self, result: WaveformResult, y_series: np.ndarray):
        """Return x=intensity, y=height (vertical), plus axis labels."""
        if result.has_waveform():
            intensity = np.asarray(y_series, dtype=np.float64)
            if result.heights is not None and result.heights.size == intensity.size:
                height = np.asarray(result.heights, dtype=np.float64)
            else:
                # fallback: monotonic synthetic height
                height = np.linspace(0.0, float(intensity.size), intensity.size)
            return intensity, height, t("axis_intensity"), t("axis_height")
        if result.has_rh():
            # L2A RH: height = RH values; intensity proxy = energy percentile
            rh = np.asarray(y_series, dtype=np.float64)
            n = rh.size
            percentile = np.arange(n, dtype=np.float64)
            if n == 101:
                # standard 0-100
                pass
            return percentile, rh, t("axis_percentile"), t("axis_height_rel")
        arr = np.asarray(y_series, dtype=np.float64)
        return arr, np.arange(arr.size, dtype=np.float64), t("axis_intensity"), t("axis_height")

    def show_base(self, result: WaveformResult):
        self._result = result
        self._aux = None
        if result.has_waveform():
            y = np.asarray(result.waveform, dtype=np.float64)
        elif result.has_rh():
            y = np.asarray(result.rh, dtype=np.float64)
        else:
            self.clear_message(t("hint_no_wave"))
            return
        self._display_y = y
        self._display_label = t("wave_original")
        self._redraw()

    def apply_transform_result(self, result: WaveformResult, out: Dict[str, Any]):
        self._result = result
        y = out.get("y")
        if y is None:
            if result.has_waveform():
                y = result.waveform
            elif result.has_rh():
                y = result.rh
            else:
                return
        self._display_y = np.asarray(y, dtype=np.float64)
        self._display_label = str(out.get("label", t("wave_current")))
        if out.get("kind") == "spectrum":
            image = out.get("image")
            aux_y = out.get("aux_y")
            if image is not None:
                self._aux = {
                    "mode": "image",
                    "data": image,
                    "xlabel": out.get("image_xlabel", ""),
                    "ylabel": out.get("image_ylabel", ""),
                    "title": out.get("aux_label", "Spectrum"),
                }
            elif aux_y is not None:
                self._aux = {
                    "mode": "line",
                    "x": out.get("aux_x"),
                    "y": aux_y,
                    "xlabel": out.get("aux_xlabel", "Freq"),
                    "ylabel": out.get("aux_ylabel", "Mag"),
                    "title": out.get("aux_label", "Spectrum"),
                }
            else:
                self._aux = None
        else:
            self._aux = None
        self._redraw()

    def _raw_series(self, res: WaveformResult) -> Optional[np.ndarray]:
        if res.has_waveform():
            return np.asarray(res.waveform, dtype=np.float64)
        if res.has_rh():
            return np.asarray(res.rh, dtype=np.float64)
        return None

    def _redraw(self):
        res = self._result
        y = self._display_y
        self.fig.clf()
        if res is None or y is None:
            self.clear_message(t("hint_no_wave"))
            return

        if self._aux is not None:
            self.ax = self.fig.add_subplot(2, 1, 1)
            self.ax_aux = self.fig.add_subplot(2, 1, 2)
        else:
            self.ax = self.fig.add_subplot(111)
            self.ax_aux = None
        self._style(self.ax)

        x, y_h, xlab, ylab = self._profile_xy(res, y)
        y_raw = self._raw_series(res)
        if y_raw is not None and y_raw.size == y.size:
            xr, yr, _, _ = self._profile_xy(res, y_raw)
            self.ax.plot(xr, yr, color=MUTED, linewidth=1.0, alpha=0.65, label=t("wave_original"))
        self.ax.plot(x, y_h, color=NASA, linewidth=1.5, label=self._display_label or t("wave_current"))

        title = f"{res.product} · {res.beam} · {res.shot_number}"
        sub = f"({res.lon:.5f}, {res.lat:.5f})"
        if res.elev_top is not None and res.elev_bot is not None:
            sub += f"\nΔH={res.elev_top - res.elev_bot:.2f} m"
        self.ax.set_title(f"{title}\n{sub}", color=NASA, fontsize=9, fontweight="bold")
        self.ax.set_xlabel(xlab, color=INK, fontsize=9)
        self.ax.set_ylabel(ylab, color=INK, fontsize=9)
        self.ax.legend(loc="best", fontsize=8, framealpha=0.9)

        # For waveform: ensure higher elevation is at the top of the plot
        if res.has_waveform() and res.heights is not None:
            h = np.asarray(res.heights, dtype=np.float64)
            h0, h1 = float(np.nanmin(h)), float(np.nanmax(h))
            if h1 - h0 < WAVE_MIN_SPAN_Y:
                h1 = h0 + WAVE_MIN_SPAN_Y
            i0, i1 = float(np.nanmin(x)), float(np.nanmax(x))
            if i1 - i0 < WAVE_MIN_SPAN_X:
                i1 = i0 + WAVE_MIN_SPAN_X
            self.set_data_bounds((i0, i1, h0, h1))
            self.clamp_view(i0, i1, h0 - 1, h1 + 1)
        elif res.has_rh() and y_h.size >= 2:
            y0, y1 = float(np.nanmin(y_h)), float(np.nanmax(y_h))
            if y1 - y0 < WAVE_MIN_SPAN_Y:
                y1 = y0 + WAVE_MIN_SPAN_Y
            x0, x1 = float(np.nanmin(x)), float(np.nanmax(x))
            if x1 - x0 < WAVE_MIN_SPAN_X:
                x1 = x0 + WAVE_MIN_SPAN_X
            self.set_data_bounds((x0, x1, y0, y1))
            self.clamp_view(x0 - 2, x1 + 2, y0 - 0.5, y1 + 0.5)
        else:
            if np.size(x) and np.size(y_h):
                self.set_data_bounds(
                    (
                        float(np.nanmin(x)),
                        float(np.nanmax(x)),
                        float(np.nanmin(y_h)),
                        float(np.nanmax(y_h)),
                    )
                )

        if self.ax_aux is not None and self._aux is not None:
            self._style(self.ax_aux)
            if self._aux["mode"] == "image":
                data = self._aux["data"]
                im = self.ax_aux.imshow(data, aspect="auto", origin="lower", cmap="magma")
                self.fig.colorbar(im, ax=self.ax_aux, fraction=0.046, pad=0.04)
                self.ax_aux.set_xlabel(self._aux["xlabel"] or "X", fontsize=8)
                self.ax_aux.set_ylabel(self._aux["ylabel"] or "Y", fontsize=8)
            else:
                ax_x = self._aux.get("x")
                ay = self._aux.get("y")
                if ax_x is None or np.ndim(ay) != 1:
                    ax_x = np.arange(np.asarray(ay).ravel().size)
                    ay = np.asarray(ay).ravel()
                self.ax_aux.plot(ax_x, np.asarray(ay).ravel(), color=CYAN, linewidth=1.0)
                self.ax_aux.set_xlabel(self._aux["xlabel"], fontsize=8)
                self.ax_aux.set_ylabel(self._aux["ylabel"], fontsize=8)
            self.ax_aux.set_title(self._aux["title"], color=CYAN, fontsize=8)

        try:
            if self.ax_aux is not None:
                self.fig.subplots_adjust(left=0.16, right=0.96, top=0.90, bottom=0.08, hspace=0.35)
            else:
                self.fig.subplots_adjust(left=0.16, right=0.96, top=0.88, bottom=0.10)
        except Exception:
            pass
        self.draw_idle()

    @property
    def result(self) -> Optional[WaveformResult]:
        return self._result


class WaveformPanel(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.title = QLabel(t("panel_wave"))
        self.title.setStyleSheet(f"color:{NASA}; font-weight:600;")
        self.hint = QLabel("Y: 高度 ↑    X: 强度 →")
        self.hint.setStyleSheet(f"color:{MUTED}; font-size:11px;")
        self.canvas = WaveformCanvas(self)
        self.toolbar = NavigationToolbar2QT(self.canvas, self)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addWidget(self.title)
        lay.addWidget(self.hint)
        lay.addWidget(self.toolbar)
        lay.addWidget(self.canvas, 1)
        try:
            self.setCursor(Qt.OpenHandCursor)
            self.canvas.setCursor(Qt.OpenHandCursor)
        except Exception:
            pass

    def retranslate(self):
        self.title.setText(t("panel_wave"))
        self.hint.setText(
            "Y: Height ↑    X: Intensity →"
            if get_language() == "en"
            else "Y: 高度 ↑    X: 强度 →"
        )
        if self.canvas.result is not None:
            self.show_base(self.canvas.result)
        else:
            self.clear_message(t("hint_wave"))

    def show_base(self, result: WaveformResult):
        self.canvas.show_base(result)
        self.title.setText(f"{t('panel_wave')} · {result.product} {result.beam} · {result.shot_number}")

    def apply_transform_result(self, result: WaveformResult, out: Dict[str, Any]):
        self.canvas.apply_transform_result(result, out)
        label = out.get("label", "")
        self.title.setText(f"{t('panel_wave')} · {result.product} {result.beam} · {result.shot_number} · {label}")

    def clear_message(self, msg: str):
        self.title.setText(t("panel_wave"))
        self.canvas.clear_message(msg)

    @property
    def current_result(self) -> Optional[WaveformResult]:
        return self.canvas.result
