"""Smoke test for CloudCompare-like UI + transform registry."""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

ROOT = Path(r"F:\GEDI_process\gedi_desktop")
sys.path.insert(0, str(ROOT))


def main() -> int:
    print("== imports ==")
    from app.core.registry import METRICS, TRANSFORMS
    from app.processing import builtin_metrics, transforms  # noqa
    from app.processing.transforms import apply_transform_by_id
    import numpy as np
    from app.data.gedi_loader import GediLoader
    from pathlib import Path as P

    print("transforms:", [(s.id, s.category, s.result_kind) for s in TRANSFORMS.all()])
    print("metrics:", [(s.id, s.category) for s in METRICS.all()])
    assert len(TRANSFORMS.all()) >= 8
    assert len(METRICS.all()) >= 3

    print("== data + transform pipeline ==")
    loader = GediLoader()
    paths = list(P(r"F:\GEDI_process\data_sample").rglob("*.h5"))
    loader.open_files(paths)
    # pick L1B shot with long waveform
    gf = next(f for f in loader.files if f.product == "L1B")
    beam = "BEAM0000"
    res = None
    for i in range(0, 200):
        r = loader.load_waveform_result(gf.file_id, beam, i)
        if r and r.has_waveform() and r.waveform.size > 100:
            res = r
            break
    assert res is not None
    y = res.waveform
    print("waveform n", y.size)
    for sid in ("baseline", "normalize", "savgol", "lowpass", "fft", "dwt_denoise", "cwt_scalogram"):
        spec = TRANSFORMS.get(sid)
        out = apply_transform_by_id(y, sid, {})
        print(f"  {sid}: kind={out.get('kind')} label={out.get('label')} ylen={np.asarray(out.get('y')).size}")

    print("== Qt UI ==")
    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication
    from app.ui.main_window import MainWindow

    app = QApplication(sys.argv)
    win = MainWindow()
    win.resize(1500, 940)
    win.show()
    win.load_paths(paths)
    print("tree top items:", win.file_tree.tree.topLevelItemCount())
    print("visible checked:", win.file_tree.checked_file_ids())
    print("map points:", len(win._map_points))

    # uncheck all then check one file
    win.file_tree.set_all_checked(False)
    print("after uncheck map:", len(win._map_points))
    win.file_tree.set_all_checked(True)

    if win._map_points:
        win.on_footprint_selected(win._map_points[5])
        print("selected:", win.session.selected_result.shot_number)
        print("waveform title:", win.waveform_panel.title.text())
        # run FFT from menu path
        win.run_transform("fft")
        print("after fft title:", win.waveform_panel.title.text())
        win.run_transform("dwt_denoise")
        print("after dwt title:", win.waveform_panel.title.text())
        win.run_metric("rh_profile")
        print("metrics rows:", win.info_panel.table.rowCount())

    # visibility toggle
    win.file_tree.set_all_checked(False)
    first_id = next(iter(win.loader.files)).file_id
    # check only first
    from PySide6.QtCore import Qt
    it = win.file_tree.tree.topLevelItem(0)
    win.file_tree._block = True
    it.setCheckState(0, Qt.Checked)
    win.file_tree._block = False
    win.file_tree.visibility_changed.emit()
    print("map after single file visible:", len(win._map_points))

    def _done():
        print("UI_EXTENSIBILITY_OK")
        win.close()
        app.quit()

    QTimer.singleShot(1500, _done)
    rc = app.exec()
    print("exit", rc)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        traceback.print_exc()
        raise SystemExit(2)
