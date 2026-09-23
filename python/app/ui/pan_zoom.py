"""Shared 2D plot interaction: mouse-drag pan + adaptive zoom bounds."""

from __future__ import annotations

from typing import Optional, Tuple

# Map geographic minimum spans (degrees) — keep view "reasonably dimensional"
MAP_MIN_SPAN_LON = 0.002   # ~200 m at equator
MAP_MIN_SPAN_LAT = 0.002
MAP_MAX_ZOOM_OUT = 1.35    # allow slight padding beyond data extent

# Waveform/profile minimum spans
WAVE_MIN_SPAN_X = 1e-3
WAVE_MIN_SPAN_Y = 1e-3
WAVE_MAX_ZOOM_OUT = 1.5

CLICK_PIXEL_THRESHOLD = 4.0


class PanZoomMixin:
    """Mixin for FigureCanvas subclasses.

    Requires: self.ax, self.draw_idle(), optional self._data_bounds
    _data_bounds: (x0, x1, y0, y1) full data extent used to clamp zoom-out.
    """

    _drag_press = None
    _drag_axes_lim = None
    _drag_moved_px = 0.0
    _data_bounds: Optional[Tuple[float, float, float, float]] = None
    _min_span_x = MAP_MIN_SPAN_LON
    _min_span_y = MAP_MIN_SPAN_LAT
    _max_zoom_out = MAP_MAX_ZOOM_OUT

    def enable_pan_zoom(self, min_span_x: float, min_span_y: float, max_zoom_out: float = 1.35):
        self._min_span_x = float(min_span_x)
        self._min_span_y = float(min_span_y)
        self._max_zoom_out = float(max_zoom_out)
        self.mpl_connect("button_press_event", self._pz_press)
        self.mpl_connect("motion_notify_event", self._pz_motion)
        self.mpl_connect("button_release_event", self._pz_release)
        # scroll handled via Qt wheelEvent → _pz_scroll (avoids double zoom)

    def set_data_bounds(self, bounds: Optional[Tuple[float, float, float, float]]):
        """bounds = (x0, x1, y0, y1) of full data; None clears constraint."""
        if bounds is None:
            self._data_bounds = None
            return
        x0, x1, y0, y1 = bounds
        if x1 < x0:
            x0, x1 = x1, x0
        if y1 < y0:
            y0, y1 = y1, y0
        # ensure non-degenerate data bounds
        if x1 - x0 < self._min_span_x:
            cx = 0.5 * (x0 + x1)
            x0, x1 = cx - self._min_span_x / 2, cx + self._min_span_x / 2
        if y1 - y0 < self._min_span_y:
            cy = 0.5 * (y0 + y1)
            y0, y1 = cy - self._min_span_y / 2, cy + self._min_span_y / 2
        self._data_bounds = (x0, x1, y0, y1)

    def clamp_view(self, x0=None, x1=None, y0=None, y1=None):
        """Clamp proposed view to min span and max zoom-out around data bounds."""
        if x0 is None or x1 is None:
            x0, x1 = self.ax.get_xlim()
        if y0 is None or y1 is None:
            y0, y1 = self.ax.get_ylim()
        if x1 < x0:
            x0, x1 = x1, x0
        if y1 < y0:
            y0, y1 = y1, y0

        # --- X ---
        span_x = x1 - x0
        if span_x < self._min_span_x:
            cx = 0.5 * (x0 + x1)
            half = self._min_span_x / 2.0
            x0, x1 = cx - half, cx + half
            span_x = self._min_span_x

        # --- Y ---
        span_y = y1 - y0
        if span_y < self._min_span_y:
            cy = 0.5 * (y0 + y1)
            half = self._min_span_y / 2.0
            y0, y1 = cy - half, cy + half
            span_y = self._min_span_y

        db = self._data_bounds
        if db is not None:
            dx0, dx1, dy0, dy1 = db
            max_sx = max((dx1 - dx0) * self._max_zoom_out, self._min_span_x)
            max_sy = max((dy1 - dy0) * self._max_zoom_out, self._min_span_y)
            if span_x > max_sx:
                cx = 0.5 * (x0 + x1)
                x0, x1 = cx - max_sx / 2, cx + max_sx / 2
                span_x = max_sx
            if span_y > max_sy:
                cy = 0.5 * (y0 + y1)
                y0, y1 = cy - max_sy / 2, cy + max_sy / 2
                span_y = max_sy
            # keep view near data: allow max_zoom_out padding outside data extent
            pad_x = (span_x - (dx1 - dx0)) / 2.0
            pad_y = (span_y - (dy1 - dy0)) / 2.0
            pad_x = max(0.0, pad_x)
            pad_y = max(0.0, pad_y)
            # limit how far outside data we can pan
            lim_x0 = dx0 - pad_x if pad_x > 0 else dx0
            lim_x1 = dx1 + pad_x if pad_x > 0 else dx1
            lim_y0 = dy0 - pad_y if pad_y > 0 else dy0
            lim_y1 = dy1 + pad_y if pad_y > 0 else dy1
            # if view still larger than allowed outside, pull back inside
            if x0 < lim_x0 - 1e-12:
                x1 += lim_x0 - x0
                x0 = lim_x0
            if x1 > lim_x1 + 1e-12:
                x0 -= x1 - lim_x1
                x1 = lim_x1
            if y0 < lim_y0 - 1e-12:
                y1 += lim_y0 - y0
                y0 = lim_y0
            if y1 > lim_y1 + 1e-12:
                y0 -= y1 - lim_y1
                y1 = lim_y1

        self.ax.set_xlim(x0, x1)
        self.ax.set_ylim(y0, y1)
        return x0, x1, y0, y1

    def _pz_press(self, event):
        if event.inaxes != self.ax:
            self._drag_press = None
            return
        if event.button not in (1, 2):  # left or middle drag to pan
            self._drag_press = None
            return
        self._drag_press = (event.x, event.y, event.xdata, event.ydata)
        self._drag_axes_lim = self.ax.get_xlim() + self.ax.get_ylim()
        self._drag_moved_px = 0.0

    def _pz_motion(self, event):
        if self._drag_press is None or event.inaxes != self.ax:
            return
        if event.xdata is None or event.ydata is None:
            return
        x_press, y_press, xd_press, yd_press = self._drag_press
        self._drag_moved_px = max(self._drag_moved_px, ((event.x - x_press) ** 2 + (event.y - y_press) ** 2) ** 0.5)
        if self._drag_axes_lim is None:
            return
        x0, x1, y0, y1 = self._drag_axes_lim
        dx = event.xdata - xd_press
        dy = event.ydata - yd_press
        # pan: move view opposite to cursor delta in data space
        self.clamp_view(x0 - dx, x1 - dx, y0 - dy, y1 - dy)
        self.draw_idle()

    def _pz_release(self, event):
        moved = self._drag_moved_px
        self._drag_press = None
        self._drag_axes_lim = None
        self._drag_moved_px = 0.0
        # subclass may treat as click if moved small
        self._pz_was_drag = moved >= CLICK_PIXEL_THRESHOLD

    def _pz_scroll(self, event):
        if event.inaxes != self.ax:
            return
        if event.xdata is None or event.ydata is None:
            return
        if event.button == "up":
            scale = 0.85
        elif event.button == "down":
            scale = 1.18
        else:
            return
        x0, x1 = self.ax.get_xlim()
        y0, y1 = self.ax.get_ylim()
        cx, cy = float(event.xdata), float(event.ydata)
        self.clamp_view(
            cx - (cx - x0) * scale,
            cx + (x1 - cx) * scale,
            cy - (cy - y0) * scale,
            cy + (y1 - cy) * scale,
        )
        self.draw_idle()
