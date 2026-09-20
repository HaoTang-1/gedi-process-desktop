"""Headless smoke test: import modules, open sample data, process a shot, batch."""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

ROOT = Path(r"F:\GEDI_process\gedi_desktop")
sys.path.insert(0, str(ROOT))

def main() -> int:
    print("== 1. imports ==")
    from app.config import APP_NAME, sample_data_dir
    from app.data.gedi_loader import GediLoader
    from app.processing.batch import BatchConfig, run_batch
    from app.processing.metrics import compute_waveform_metrics
    from app.processing.waveform_ops import ProcessParams, process_waveform

    print("APP:", APP_NAME)

    print("== 2. discover sample files ==")
    root = sample_data_dir()
    paths = sorted(root.rglob("*.h5"))
    print("files:", len(paths))
    for p in paths:
        print(" -", p.name, f"{p.stat().st_size/1e6:.1f} MB")
    if not paths:
        print("NO SAMPLE DATA")
        return 1

    print("== 3. open files (metadata only) ==")
    loader = GediLoader()
    opened = loader.open_files(paths)
    for gf in opened:
        print(f"  {gf.product} {gf.path.name}: shots={gf.n_shots}, beams={list(gf.beams)}")

    print("== 4. map points ==")
    pts = loader.collect_map_points(max_points=500)
    print("map points:", len(pts))
    if pts:
        sm = pts[len(pts) // 2]
        print(f"  sample point: {sm.beam} shot={sm.shot_number} lon={sm.lon:.5f} lat={sm.lat:.5f}")

        print("== 5. load one shot from each product ==")
        for product in ("L1B", "L2A"):
            gf = next((f for f in loader.files if f.product == product), None)
            if gf is None:
                print("  missing", product)
                continue
            beam = next(iter(gf.beams))
            # pick a mid index
            n = gf.beams[beam]["shot_number"].shape[0]
            idx = n // 2
            res = loader.load_waveform_result(gf.file_id, beam, idx)
            print(f"  {product} {beam} idx={idx}: shot={res.shot_number}")
            print(f"    lon/lat={res.lon:.5f}/{res.lat:.5f}")
            print(f"    elev_top/bot={res.elev_top}/{res.elev_bot} sens={res.sensitivity} q={res.quality}")
            if res.has_waveform():
                y = res.waveform
                print(f"    waveform n={y.size} min={y.min():.2f} max={y.max():.2f} mean={y.mean():.2f}")
                out = process_waveform(
                    y,
                    ProcessParams(fft=True, lowpass=True, baseline=True, normalize=True, cutoff_low=0.2),
                )
                print(f"    processed n={out['processed'].size} fft_n={None if out['fft_mag'] is None else out['fft_mag'].size}")
            if res.has_rh():
                print(f"    rh n={res.rh.size} rh50={res.rh[50]:.2f} rh98={res.rh[98]:.2f}")
            m = compute_waveform_metrics(res, ProcessParams(lowpass=True))
            keys = ["wf_mean", "wf_energy", "wf_centroid", "wf_fwhm", "rh50", "rh98", "canopy_height", "gcr"]
            print("    metrics:", {k: m.get(k) for k in keys})

    print("== 6. batch (small) ==")
    fids = [f.file_id for f in loader.files]
    out_csv = ROOT / "output" / "_smoke_batch.csv"
    cfg = BatchConfig(
        file_ids=fids,
        max_shots_per_beam=5,
        quality_only=True,
        stride=1,
        process_params=ProcessParams(baseline=True, savgol=True),
        output_csv=out_csv,
    )
    result = run_batch(loader, cfg, progress=lambda c, t, m: None)
    print(f"  ok={result.n_ok} skip={result.n_skip} csv={result.output_path}")
    df = result.to_dataframe()
    print("  columns:", list(df.columns)[:12], "...")
    print("  rows:", len(df))
    if len(df):
        print(df.head(2).to_string())

    print("== 7. Qt import (no show) ==")
    from PySide6.QtWidgets import QApplication
    from app.ui.main_window import MainWindow

    app = QApplication(sys.argv)
    win = MainWindow()
    win.load_paths(paths)
    print("  files in UI list:", win.file_list.count())
    print("  map points:", len(win._map_points))
    if win._map_points:
        win.on_footprint_selected(win._map_points[0])
        print("  selected shot:", win._current_result.shot_number if win._current_result else None)
        print("  detail lines:", len(win.detail_text.toPlainText().splitlines()))
        win.apply_processing()
        print("  processing applied, metrics rows:", win.process_panel.metrics_table.rowCount())
    win.close()
    print("== ALL OK ==")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        traceback.print_exc()
        raise SystemExit(2)
