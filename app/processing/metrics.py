"""Basic waveform / RH metrics for single-shot and batch computation."""

from __future__ import annotations

from typing import Dict, List, Optional

import numpy as np
from scipy import stats
from scipy.signal import find_peaks

from app.data.gedi_loader import WaveformResult
from app.processing.waveform_ops import ProcessParams, process_waveform


def _finite(y: np.ndarray) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    return y[np.isfinite(y)]


def waveform_energy_centroid(y: np.ndarray) -> float:
    y = np.asarray(y, dtype=np.float64)
    w = np.clip(y - np.nanmin(y), 0, None)
    s = np.nansum(w)
    if s <= 0:
        return float("nan")
    idx = np.arange(y.size, dtype=np.float64)
    return float(np.nansum(idx * w) / s)


def waveform_fwhm(y: np.ndarray) -> float:
    y = np.asarray(y, dtype=np.float64)
    if y.size < 3:
        return float("nan")
    peak = np.nanmax(y)
    base = np.nanmin(y)
    half = base + 0.5 * (peak - base)
    above = np.where(y >= half)[0]
    if above.size == 0:
        return float("nan")
    return float(above[-1] - above[0] + 1)


def count_peaks(y: np.ndarray, prominence_ratio: float = 0.15) -> int:
    y = np.asarray(y, dtype=np.float64)
    if y.size < 5:
        return 0
    span = np.nanmax(y) - np.nanmin(y)
    if span <= 0:
        return 0
    peaks, _ = find_peaks(y, prominence=prominence_ratio * span)
    return int(peaks.size)


def ground_canopy_ratio(y: np.ndarray, ground_frac: float = 0.25) -> Dict[str, float]:
    """Split waveform into upper canopy vs lower (ground-like) energy.

    GEDI waveforms typically record from canopy top downward; last samples
    often contain ground return.
    """
    y = np.asarray(y, dtype=np.float64)
    if y.size < 4:
        return {"canopy_energy": float("nan"), "ground_energy": float("nan"), "gcr": float("nan")}
    y_pos = np.clip(y - np.nanmin(y), 0, None)
    n_g = max(1, int(round(y.size * ground_frac)))
    ground = float(np.nansum(y_pos[-n_g:]))
    canopy = float(np.nansum(y_pos[:-n_g]))
    gcr = ground / canopy if canopy > 0 else float("nan")
    return {"canopy_energy": canopy, "ground_energy": ground, "gcr": gcr}


def compute_waveform_metrics(
    result: WaveformResult,
    params: Optional[ProcessParams] = None,
) -> Dict[str, Optional[float]]:
    """Compute a dictionary of metrics for one shot."""
    out: Dict[str, Optional[float]] = {
        "shot_number": float(result.shot_number),
        "lon": float(result.lon),
        "lat": float(result.lat),
        "elev_top": result.elev_top,
        "elev_bot": result.elev_bot,
        "sensitivity": result.sensitivity,
        "quality": result.quality,
        "degrade": result.degrade,
        "delta_time": result.delta_time,
        "product": result.product,
        "beam": result.beam,
        "energy_total": result.energy_total,
        "selected_algorithm": result.selected_algorithm,
        "n_samples": None,
        "wf_mean": None,
        "wf_std": None,
        "wf_min": None,
        "wf_max": None,
        "wf_energy": None,
        "wf_centroid": None,
        "wf_fwhm": None,
        "wf_skew": None,
        "wf_kurtosis": None,
        "wf_peak_count": None,
        "canopy_energy": None,
        "ground_energy": None,
        "gcr": None,
        "rh25": None,
        "rh50": None,
        "rh75": None,
        "rh98": None,
        "rh100": None,
        "canopy_height": None,
    }

    # Processed waveform preferred for metrics when filters are enabled
    y = None
    if result.has_waveform():
        raw = result.waveform
        if params is not None:
            y = process_waveform(raw, params)["processed"]
        else:
            y = np.asarray(raw, dtype=np.float64)
        yf = _finite(y)
        if yf.size:
            out["n_samples"] = float(y.size)
            out["wf_mean"] = float(np.mean(yf))
            out["wf_std"] = float(np.std(yf))
            out["wf_min"] = float(np.min(yf))
            out["wf_max"] = float(np.max(yf))
            out["wf_energy"] = float(np.nansum(np.clip(y - np.nanmin(y), 0, None)))
            out["wf_centroid"] = waveform_energy_centroid(y)
            out["wf_fwhm"] = waveform_fwhm(y)
            if yf.size >= 3:
                out["wf_skew"] = float(stats.skew(yf))
                out["wf_kurtosis"] = float(stats.kurtosis(yf))
            out["wf_peak_count"] = float(count_peaks(y))
            gcr = ground_canopy_ratio(y)
            out.update(gcr)
        if result.heights is not None and result.elev_top is not None and result.elev_bot is not None:
            # canopy height proxy: highest return - lowest mode elevation
            try:
                out["canopy_height"] = float(result.elev_top - result.elev_bot)
            except Exception:
                out["canopy_height"] = None

    if result.has_rh():
        rh = result.rh
        out["n_samples"] = float(rh.size)
        out["rh25"] = float(np.interp(25, np.arange(rh.size), rh))
        out["rh50"] = float(rh[50]) if rh.size > 50 else float(np.percentile(rh, 50))
        out["rh75"] = float(rh[75]) if rh.size > 75 else float(np.percentile(rh, 75))
        out["rh98"] = float(rh[98]) if rh.size > 98 else float(np.percentile(rh, 98))
        out["rh100"] = float(rh[100]) if rh.size > 100 else float(np.max(rh))
        out["rh_mean"] = float(np.nanmean(rh))
        out["rh_std"] = float(np.nanstd(rh))
        if result.elev_top is not None and result.elev_bot is not None:
            out["canopy_height"] = float(result.elev_top - result.elev_bot)
        # RH profile as pseudo-waveform stats
        out["wf_mean"] = float(np.nanmean(rh))
        out["wf_std"] = float(np.nanstd(rh))
        out["wf_min"] = float(np.nanmin(rh))
        out["wf_max"] = float(np.nanmax(rh))
        out["wf_energy"] = float(np.nansum(np.clip(rh - np.nanmin(rh), 0, None)))
        out["wf_centroid"] = waveform_energy_centroid(rh)
        out["wf_fwhm"] = waveform_fwhm(rh)
        if rh.size >= 3:
            out["wf_skew"] = float(stats.skew(rh))
            out["wf_kurtosis"] = float(stats.kurtosis(rh))
        out["wf_peak_count"] = float(count_peaks(rh))
        gcr = ground_canopy_ratio(rh)
        out.update(gcr)

    return out


METRIC_COLUMNS = [
    "file",
    "product",
    "beam",
    "shot_number",
    "lon",
    "lat",
    "delta_time",
    "sensitivity",
    "quality",
    "degrade",
    "elev_top",
    "elev_bot",
    "canopy_height",
    "n_samples",
    "wf_mean",
    "wf_std",
    "wf_min",
    "wf_max",
    "wf_energy",
    "wf_centroid",
    "wf_fwhm",
    "wf_skew",
    "wf_kurtosis",
    "wf_peak_count",
    "canopy_energy",
    "ground_energy",
    "gcr",
    "rh25",
    "rh50",
    "rh75",
    "rh98",
    "rh100",
    "energy_total",
    "selected_algorithm",
]


def metrics_to_row(file_name: str, metrics: Dict[str, Optional[float]]) -> Dict[str, Optional[float]]:
    row = {c: None for c in METRIC_COLUMNS}
    row["file"] = file_name
    for k, v in metrics.items():
        if k in row:
            row[k] = v
        elif k == "product":
            row["product"] = v
        elif k == "beam":
            row["beam"] = v
    return row
