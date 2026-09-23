"""Smoke: pan/zoom bounds + right-click delete."""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

ROOT = Path(r"F:\GEDI_process\gedi_desktop")
sys.path.insert(0, str(ROOT))


def main() -> int:
    from PySide6.QtCore import Qt, QTimer
    from PySide6.QtWidgets import QApplication
    import numpy as np
    from app.ui.main_window import MainWindow
    from app.ui.pan_zoom import MAP_MIN_SPAN_LAT, MAP_MIN_SPAN_LON
    from app.i18n import t

    app = QApplication.instance() or QApplication(sys.argv)
    win = MainWindow()
    win.resize(1500, 900)
    win.show()
    paths = list(Path(r"F:\GEDI_process\data_sample").rglob("*.h5"))
    win.load_paths(paths)

    mp = win.map_panel.canvas
    print("data_bounds", mp._data_bounds)
    assert mp._data_bounds is not None

    # --- wheel zoom in many times: should not go below min span ---
    class E:
        pass

    def wheel(ev_button):
        ev = E()
        ev.inaxes = mp.ax
        x0, x1 = mp.ax.get_xlim()
        y0, y1 = mp.ax.get_ylim()
        ev.xdata = 0.5 * (x0 + x1)
        ev.ydata = 0.5 * (y0 + y1)
        ev.button = ev_button
        mp._pz_scroll(ev)

    for _ in range(30):
        wheel("up")
    span_x = mp.ax.get_xlim()[1] - mp.ax.get_xlim()[0]
    span_y = mp.ax.get_ylim()[1] - mp.ax.get_ylim()[0]
    print("after zoom-in spans", span_x, span_y)
    assert span_x >= MAP_MIN_SPAN_LON - 1e-9
    assert span_y >= MAP_MIN_SPAN_LAT - 1e-9
    print("min span clamp OK")

    # --- zoom out many times: should not exceed data extent * max_zoom_out ---
    for _ in range(40):
        wheel("down")
    x0, x1 = mp.ax.get_xlim()
    y0, y1 = mp.ax.get_ylim()
    db = mp._data_bounds
    max_sx = max((db[1] - db[0]) * mp._max_zoom_out, MAP_MIN_SPAN_LON)
    max_sy = max((db[3] - db[2]) * mp._max_zoom_out, MAP_MIN_SPAN_LAT)
    sx, sy = x1 - x0, y1 - y0
    print("after zoom-out spans", sx, sy, "max", max_sx, max_sy)
    assert sx <= max_sx + 1e-6
    assert sy <= max_sy + 1e-6
    # view should stay near data
    assert x0 <= db[1] and x1 >= db[0]
    print("max zoom-out clamp OK")

    # --- drag pan ---
    x_before = mp.ax.get_xlim()
    press = E()
    press.inaxes = mp.ax
    press.button = 1
    press.x, press.y = 100, 100
    press.xdata = 0.5 * (x_before[0] + x_before[1])
    press.ydata = 0.5 * (mp.ax.get_ylim()[0] + mp.ax.get_ylim()[1])
    mp._pz_press(press)
    move = E()
    move.inaxes = mp.ax
    move.x, move.y = 140, 100  # 40px move
    move.xdata = press.xdata - 0.05  # pan
    move.ydata = press.ydata
    mp._pz_motion(move)
    rel = E()
    rel.inaxes = mp.ax
    rel.x, rel.y = 140, 100
    rel.button = 1
    mp._pz_release(rel)
    x_after = mp.ax.get_xlim()
    print("pan xlim", x_before, "->", x_after)
    assert x_after != x_before
    assert getattr(mp, "_pz_was_drag", False) is True
    print("drag pan OK, was_drag=", mp._pz_was_drag)

    # small move → not drag → would select
    mp._pz_was_drag = False
    press.x, press.y = 100, 100
    press.xdata = x_after[0]
    press.ydata = mp.ax.get_ylim()[0]
    mp._pz_press(press)
    move.x, move.y = 102, 100
    move.xdata = press.xdata
    move.ydata = press.ydata
    mp._pz_motion(move)
    mp._pz_release(rel)
    print("click not drag:", mp._pz_was_drag)
    assert mp._pz_was_drag is False

    # --- waveform pan/zoom bounds ---
    if win._map_points:
        win.on_footprint_selected(win._map_points[0])
    wc = win.waveform_panel.canvas
    print("wave bounds", wc._data_bounds)
    if wc._data_bounds is not None:
        for _ in range(20):
            class E2:
                pass
            e = E2()
            e.inaxes = wc.ax
            e.xdata = 0.5 * sum(wc.ax.get_xlim())
            e.ydata = 0.5 * sum(wc.ax.get_ylim())
            e.button = "up"
            wc._pz_scroll(e)
        print("wave after zoom-in", wc.ax.get_xlim(), wc.ax.get_ylim())
        b = wc._data_bounds
        assert (wc.ax.get_xlim()[1] - wc.ax.get_xlim()[0]) >= wc._min_span_x - 1e-9
        assert (wc.ax.get_ylim()[1] - wc.ax.get_ylim()[0]) >= wc._min_span_y - 1e-9

    # --- right-click delete ---
    n0 = win.file_tree.tree.topLevelItemCount()
    assert n0 >= 1
    fid = win.loader.files[0].file_id
    print("removing file_id", fid, "count", n0)
    win.remove_file_by_id(fid)
    n1 = win.file_tree.tree.topLevelItemCount()
    print("after remove count", n1)
    assert n1 == n0 - 1
    assert win.loader.get_file(fid) is None
    print("i18n delete label:", t("ctx_delete"))

    # canvas cursors open hand
    print("map cursor", mp.cursor().shape())
    print("hint", win.file_tree.hint_label.text())

    def _done():
        print("PAN_ZOOM_DELETE_OK")
        win.close()
        app.quit()

    QTimer.singleShot(800, _done)
    rc = app.exec()
    print("exit", rc)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        traceback.print_exc()
        raise SystemExit(2)
