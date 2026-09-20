"""Smoke: waveform right-of-map orientation + i18n + wheel zoom API."""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

ROOT = Path(r"F:\GEDI_process\gedi_desktop")
sys.path.insert(0, str(ROOT))


def main() -> int:
    print("== i18n ==")
    from app.i18n import get_language, set_language, t
    assert get_language() == "zh"
    assert "文件" in t("menu_file") or t("menu_file").startswith("文件")
    set_language("en")
    assert get_language() == "en"
    assert t("menu_file").startswith("&File") or "File" in t("menu_file")
    set_language("zh")
    print("i18n ok", t("menu_file"), "->", t("panel_wave"))

    print("== data + waveform orientation ==")
    from app.data.gedi_loader import GediLoader
    from app.ui.waveform_view import WaveformCanvas
    import numpy as np
    from PySide6.QtWidgets import QApplication
    from PySide6.QtCore import QTimer

    app = QApplication.instance() or QApplication(sys.argv)

    loader = GediLoader()
    paths = list(Path(r"F:\GEDI_process\data_sample").rglob("*.h5"))
    loader.open_files(paths)
    gf = next(f for f in loader.files if f.product == "L1B")
    res = None
    for i in range(0, 300):
        r = loader.load_waveform_result(gf.file_id, "BEAM0000", i)
        if r and r.has_waveform() and r.waveform.size > 200 and r.heights is not None:
            res = r
            break
    assert res is not None
    print("L1B n", res.waveform.size, "elev", res.elev_top, "->", res.elev_bot)

    wc = WaveformCanvas()
    wc.show_base(res)
    # check axes: x is intensity range, y is height range
    x0, x1 = wc.ax.get_xlim()
    y0, y1 = wc.ax.get_ylim()
    print("axes xlim", x0, x1, "ylim", y0, y1)
    xl = wc.ax.get_xlabel()
    yl = wc.ax.get_ylabel()
    print("labels", xl, "|", yl)
    assert "强度" in xl or "Intensity" in xl
    assert "高度" in yl or "Height" in yl
    # height axis should span elev
    if res.elev_top and res.elev_bot:
        assert y0 <= min(res.elev_top, res.elev_bot) + 5
        assert y1 >= max(res.elev_top, res.elev_bot) - 5

    # L2A RH orientation
    gf2 = next(f for f in loader.files if f.product == "L2A")
    res2 = None
    for i in range(0, 500):
        r = loader.load_waveform_result(gf2.file_id, "BEAM0000", i)
        if r and r.has_rh() and np.nanmax(r.rh) > 1:
            res2 = r
            break
    if res2:
        wc.show_base(res2)
        print("L2A labels", wc.ax.get_xlabel(), "|", wc.ax.get_ylabel())
        print("L2A ylim", wc.ax.get_ylim(), "rh max", np.nanmax(res2.rh))

    print("== main window layout + language + map zoom ==")
    from app.ui.main_window import MainWindow
    from app.i18n import get_language as gl

    win = MainWindow()
    win.resize(1560, 940)
    win.show()
    win.load_paths(paths)

    # layout: waveform is sibling of map inside mid splitter, map at index 0
    mid = win._mid_split
    assert mid.indexOf(win.map_panel) == 0
    assert mid.indexOf(win.waveform_panel) == 1
    print("layout: map then waveform OK")

    # map wheel zoom
    mp = win.map_panel.canvas
    assert len(win._map_points) > 0
    xlim0 = mp.ax.get_xlim()
    # simulate scroll zoom in at center
    class E:
        pass
    ev = E()
    ev.inaxes = mp.ax
    ev.xdata = 0.5 * (xlim0[0] + xlim0[1])
    ev.ydata = 0.5 * (mp.ax.get_ylim()[0] + mp.ax.get_ylim()[1])
    ev.button = "up"
    mp._on_scroll(ev)
    xlim1 = mp.ax.get_xlim()
    w0 = xlim0[1] - xlim0[0]
    w1 = xlim1[1] - xlim0[0] if False else (xlim1[1] - xlim1[0])
    print("wheel zoom width", w0, "->", w1)
    assert w1 < w0
    ev.button = "down"
    mp._on_scroll(ev)
    print("wheel zoom out ok", mp.ax.get_xlim())

    # language switch without dialog: call apply_language directly after set
    set_language("en")
    win.apply_language()
    print("title en:", win.windowTitle())
    assert "GEDI" in win.windowTitle()
    print("file tree header:", win.file_tree.tree.headerItem().text(0))
    assert win.file_tree.tree.headerItem().text(0) == "Name"
    assert win.waveform_panel.hint.text().find("Height") >= 0

    set_language("zh")
    win.apply_language()
    print("title zh:", win.windowTitle())
    assert win.file_tree.tree.headerItem().text(0) == "名称"
    assert "高度" in win.waveform_panel.hint.text()

    # select footprint, check waveform panel is to the right of map and orientation
    win.on_footprint_selected(win._map_points[0])
    wc2 = win.waveform_panel.canvas
    print("selected labels", wc2.ax.get_xlabel(), "|", wc2.ax.get_ylabel())

    # Help menu has language actions
    assert win.act_lang_zh is not None and win.act_lang_en is not None
    print("help language actions OK")

    def _done():
        print("LAYOUT_I18N_ZOOM_OK")
        win.close()
        app.quit()

    QTimer.singleShot(1200, _done)
    rc = app.exec()
    print("exit", rc)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        traceback.print_exc()
        raise SystemExit(2)
