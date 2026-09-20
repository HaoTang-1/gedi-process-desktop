"""Footprint map canvas — lon/lat scatter, scalar coloring, visibility from file tree."""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple

import numpy as np
import matplotlib
from matplotlib import cm
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qtagg import NavigationToolbar2QT
from matplotlib.colors import Normalize
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import QVBoxLayout, QWidget

from app.config import BEAM_COLORS, GRID, INK, MAP_MAX_POINTS, MUTED, NASA, PAPER, footprint_deg_radius
from app.data.gedi_loader import ShotMeta
from app.i18n import t
from app.ui import mpl_fonts  # noqa: F401
from app.ui.pan_zoom import (
    CLICK_PIXEL_THRESHOLD,
    MAP_MAX_ZOOM_OUT,
    MAP_MIN_SPAN_LAT,
    MAP_MIN_SPAN_LON,
    PanZoomMixin,
)


class FootprintMapCanvas(PanZoomMixin, FigureCanvas):
    footprint_clicked = Signal(object)

    def __init__(self, parent=None):
        self.fig = Figure(figsize=(5, 4), dpi=100, facecolor=PAPER)
        FigureCanvas.__init__(self, self.fig)
        self.setParent(parent)
        self.ax = self.fig.add_subplot(111)
        self._style_axes()
        self._points: List[ShotMeta] = []
        self._scalar_key = "beam"
        self._point_size = 6
        self._show_circles = True
        self._selected_artist = None
        self._footprint_artist = None
        self._title_suffix = ""
        self._pz_was_drag = False
        self.enable_pan_zoom(MAP_MIN_SPAN_LON, MAP_MIN_SPAN_LAT, MAP_MAX_ZOOM_OUT)
        self.mpl_connect("button_release_event", self._on_release_select)
        self.setMinimumSize(280, 240)
        try:
            self.setFocusPolicy(Qt.StrongFocus)
            self.setCursor(Qt.OpenHandCursor)
        except Exception:
            pass

    def _on_release_select(self, event):
        """Select footprint only if release was a click, not a drag-pan."""
        if getattr(self, "_pz_was_drag", False):
            return
        if event.inaxes != self.ax or not self._points:
            return
        if event.xdata is None or event.ydata is None:
            return
        pts = np.array([[p.lon, p.lat] for p in self._points], dtype=float)
        if pts.size == 0:
            return
        x0, x1 = self.ax.get_xlim()
        y0, y1 = self.ax.get_ylim()
        dx = (pts[:, 0] - event.xdata) / max(1e-9, abs(x1 - x0))
        dy = (pts[:, 1] - event.ydata) / max(1e-9, abs(y1 - y0))
        dist = np.hypot(dx, dy)
        i = int(np.argmin(dist))
        if dist[i] > 0.04:
            return
        sm = self._points[i]
        self.highlight(sm)
        self.footprint_clicked.emit(sm)

    def _style_axes(self):
        self.ax.set_facecolor("#F8FAFC")
        self.ax.tick_params(colors=INK, labelsize=8)
        for spine in self.ax.spines.values():
            spine.set_color(GRID)
        self.ax.grid(True, color=GRID, linewidth=0.5, alpha=0.8)
        self.ax.set_xlabel(t("axis_lon"), color=INK, fontsize=9)
        self.ax.set_ylabel(t("axis_lat"), color=INK, fontsize=9)
        self.ax.set_title(t("tb_map"), color=NASA, fontsize=10, fontweight="bold")

    def set_display_options(self, scalar: str = "beam", point_size: int = 6, show_circles: bool = True):
        self._scalar_key = scalar
        self._point_size = int(point_size)
        self._show_circles = bool(show_circles)

    def set_points(self, points: List[ShotMeta], title_suffix: str = "", extras: Optional[Dict[int, Dict]] = None):
        """extras: file_id -> optional per-shot arrays not stored on ShotMeta (unused for now)."""
        self._title_suffix = title_suffix
        self._points = list(points)
        self.ax.clear()
        self._style_axes()
        if title_suffix:
            self.ax.set_title(f"{t('tb_map')} · {title_suffix}", color=NASA, fontsize=10, fontweight="bold")
        self._selected_artist = None
        self._footprint_artist = None

        if not points:
            self.ax.text(
                0.5,
                0.5,
                t("hint_map_empty"),
                transform=self.ax.transAxes,
                ha="center",
                va="center",
                color=MUTED,
                fontsize=10,
            )
            self.draw_idle()
            return

        pts = points
        if len(pts) > MAP_MAX_POINTS:
            stride = max(1, len(pts) // MAP_MAX_POINTS)
            pts = pts[::stride]
            self._points = pts

        lons = np.array([p.lon for p in pts], dtype=float)
        lats = np.array([p.lat for p in pts], dtype=float)
        # data bounds for zoom clamp + adaptive min view
        lon0, lon1 = float(np.nanmin(lons)), float(np.nanmax(lons))
        lat0, lat1 = float(np.nanmin(lats)), float(np.nanmax(lats))
        if lon1 - lon0 < MAP_MIN_SPAN_LON:
            cx = 0.5 * (lon0 + lon1)
            lon0, lon1 = cx - MAP_MIN_SPAN_LON / 2, cx + MAP_MIN_SPAN_LON / 2
        if lat1 - lat0 < MAP_MIN_SPAN_LAT:
            cy = 0.5 * (lat0 + lat1)
            lat0, lat1 = cy - MAP_MIN_SPAN_LAT / 2, cy + MAP_MIN_SPAN_LAT / 2
        self.set_data_bounds((lon0, lon1, lat0, lat1))
        colors, legend_handles, legend_title = self._colors_for(pts)

        self.ax.scatter(
            lons,
            lats,
            c=colors,
            s=self._point_size,
            alpha=0.8,
            linewidths=0,
            zorder=3,
        )
        lon_pad = max(MAP_MIN_SPAN_LON * 2.5, (lon1 - lon0) * 0.05 + 1e-4)
        lat_pad = max(MAP_MIN_SPAN_LAT * 2.5, (lat1 - lat0) * 0.05 + 1e-4)
        # adaptive initial view: at least min spans, cover data
        self.clamp_view(lon0 - lon_pad, lon1 + lon_pad, lat0 - lat_pad, lat1 + lat_pad)
        self.ax.set_aspect(1.0 / max(0.2, np.cos(np.radians(float(np.nanmean(lats))))))
        if legend_handles:
            self.ax.legend(
                handles=legend_handles,
                loc="upper right",
                fontsize=7,
                framealpha=0.9,
                title=legend_title,
                title_fontsize=8,
            )
        try:
            self.fig.subplots_adjust(left=0.10, right=0.97, top=0.88, bottom=0.12)
        except Exception:
            pass
        self.draw_idle()

    def _colors_for(self, pts: List[ShotMeta]) -> Tuple[np.ndarray, list, str]:
        key = self._scalar_key
        if key == "beam":
            colors = [BEAM_COLORS.get(p.beam, NASA) for p in pts]
            seen = []
            handles = []
            for p in pts:
                if p.beam not in seen:
                    seen.append(p.beam)
                    handles.append(
                        Line2D(
                            [0],
                            [0],
                            marker="o",
                            color="w",
                            markerfacecolor=BEAM_COLORS.get(p.beam, NASA),
                            markersize=6,
                            label=p.beam,
                        )
                    )
            return colors, handles, "波束"

        if key == "file":
            fids = []
            for p in pts:
                if p.file_id not in fids:
                    fids.append(p.file_id)
            palette = ["#0B3D91", "#1AA3C8", "#2E7D32", "#C45C26", "#6A1B9A", "#AD1457"]
            cmap = {fid: palette[i % len(palette)] for i, fid in enumerate(fids)}
            colors = [cmap[p.file_id] for p in pts]
            handles = [
                Line2D([0], [0], marker="o", color="w", markerfacecolor=cmap[fid], markersize=6, label=f"file#{fid}")
                for fid in fids
            ]
            return colors, handles, "文件"

        # continuous fields
        vals = []
        for p in pts:
            if key == "sensitivity":
                vals.append(np.nan if p.sensitivity is None else p.sensitivity)
            elif key == "quality":
                vals.append(np.nan if p.quality is None else p.quality)
            elif key == "canopy_proxy":
                if p.elev_top is None or p.elev_bot is None:
                    vals.append(np.nan)
                else:
                    vals.append(p.elev_top - p.elev_bot)
            elif key == "elev_top":
                vals.append(np.nan if p.elev_top is None else p.elev_top)
            else:
                vals.append(0.0)
        vals = np.asarray(vals, dtype=float)
        finite = vals[np.isfinite(vals)]
        if finite.size == 0:
            return np.array([MUTED] * len(pts)), [], key
        vmin, vmax = float(np.nanpercentile(finite, 2)), float(np.nanpercentile(finite, 98))
        if vmax <= vmin:
            vmax = vmin + 1.0
        norm = Normalize(vmin=vmin, vmax=vmax)
        try:
            cmap = matplotlib.colormaps["viridis"]
        except Exception:
            cmap = cm.get_cmap("viridis")
        colors = cmap(norm(np.nan_to_num(vals, nan=vmin)))
        # legend as scalar bar proxy
        handles = [
            Line2D([0], [0], marker="o", color="w", markerfacecolor=cmap(0.0), markersize=6, label=f"{vmin:.3g}"),
            Line2D([0], [0], marker="o", color="w", markerfacecolor=cmap(1.0), markersize=6, label=f"{vmax:.3g}"),
        ]
        label_map = {
            "sensitivity": "Sensitivity",
            "quality": "Quality",
            "canopy_proxy": "ΔH (m)",
            "elev_top": "Elev top (m)",
        }
        return colors, handles, label_map.get(key, key)

    def _on_click(self, event):
        # kept for API compatibility; selection handled on release after pan check
        self._on_release_select(event)

    def highlight(self, sm: ShotMeta):
        if self._selected_artist is not None:
            try:
                self._selected_artist.remove()
            except Exception:
                pass
        if self._footprint_artist is not None:
            try:
                self._footprint_artist.remove()
            except Exception:
                pass
        color = BEAM_COLORS.get(sm.beam, NASA)
        self._selected_artist = self.ax.scatter(
            [sm.lon],
            [sm.lat],
            s=140,
            facecolors="none",
            edgecolors="#D97706",
            linewidths=2.0,
            zorder=5,
        )
        if self._show_circles:
            x0, x1 = self.ax.get_xlim()
            y0, y1 = self.ax.get_ylim()
            r = footprint_deg_radius(sm.lat)
            circ_r = max(r, (x1 - x0) * 0.004)
            theta = np.linspace(0, 2 * np.pi, 48)
            xs = sm.lon + circ_r * np.cos(theta)
            ys = sm.lat + circ_r * np.sin(theta) * (x1 - x0) / max(1e-9, y1 - y0)
            self._footprint_artist = self.ax.plot(xs, ys, color=color, linewidth=1.4, alpha=0.95, zorder=4)[0]
        self.ax.set_title(
            f"{t('tb_map_sel')} · {sm.beam} · shot {sm.shot_number}",
            color=NASA,
            fontsize=10,
            fontweight="bold",
        )
        self.draw_idle()

    def reset_view(self):
        if not self._points:
            return
        if self._data_bounds is not None:
            x0, x1, y0, y1 = self._data_bounds
        else:
            lons = [p.lon for p in self._points]
            lats = [p.lat for p in self._points]
            x0, x1, y0, y1 = min(lons), max(lons), min(lats), max(lats)
        lon_pad = max(MAP_MIN_SPAN_LON * 2.5, (x1 - x0) * 0.05 + 1e-4)
        lat_pad = max(MAP_MIN_SPAN_LAT * 2.5, (y1 - y0) * 0.05 + 1e-4)
        self.clamp_view(x0 - lon_pad, x1 + lon_pad, y0 - lat_pad, y1 + lat_pad)
        self.draw_idle()

    def wheelEvent(self, event):
        """Qt wheel → clamped zoom at cursor."""
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

class FootprintMapPanel(QWidget):
    footprint_selected = Signal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.canvas = FootprintMapCanvas(self)
        self.toolbar = NavigationToolbar2QT(self.canvas, self)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addWidget(self.toolbar)
        lay.addWidget(self.canvas)
        self.canvas.footprint_clicked.connect(self.footprint_selected.emit)
        try:
            self.setCursor(Qt.OpenHandCursor)
            self.canvas.setCursor(Qt.OpenHandCursor)
        except Exception:
            pass

    def set_points(self, points: List[ShotMeta], title_suffix: str = ""):
        self.canvas.set_points(points, title_suffix)

    def set_display_options(self, scalar: str = "beam", point_size: int = 6, show_circles: bool = True):
        self.canvas.set_display_options(scalar, point_size, show_circles)

    def highlight(self, sm: ShotMeta):
        self.canvas.highlight(sm)

    def reset_view(self):
        self.canvas.reset_view()
