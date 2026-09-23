"""Waveform signal processing: filters, FFT, and basic transforms."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Optional, Tuple

import numpy as np
from scipy import signal


@dataclass
class ProcessParams:
    fft: bool = False
    lowpass: bool = False
    highpass: bool = False
    savgol: bool = False
    moving_avg: bool = False
    normalize: bool = False
    baseline: bool = False
    cutoff_low: float = 0.15  # normalized 0-0.5 (fraction of Nyquist)
    cutoff_high: float = 0.05
    filter_order: int = 4
    savgol_window: int = 11
    savgol_poly: int = 3
    avg_window: int = 5


def _ensure_odd(n: int, minimum: int = 3) -> int:
    n = max(minimum, int(n))
    return n if n % 2 == 1 else n + 1


def apply_normalize(y: np.ndarray) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    y_min = np.nanmin(y)
    y_max = np.nanmax(y)
    span = y_max - y_min
    if not np.isfinite(span) or span <= 0:
        return y - y_min
    return (y - y_min) / span


def apply_baseline(y: np.ndarray, quantile: float = 5.0) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    floor = np.nanpercentile(y, quantile)
    return y - floor


def apply_moving_avg(y: np.ndarray, window: int = 5) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    window = max(1, int(window))
    if window <= 1 or y.size < window:
        return y
    kernel = np.ones(window, dtype=np.float64) / window
    return np.convolve(y, kernel, mode="same")


def apply_savgol(y: np.ndarray, window: int = 11, poly: int = 3) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    if y.size < 5:
        return y
    window = _ensure_odd(min(window, y.size if y.size % 2 == 1 else y.size - 1))
    poly = max(1, min(poly, window - 2))
    return signal.savgol_filter(y, window_length=window, polyorder=poly)


def apply_lowpass(y: np.ndarray, cutoff: float = 0.15, order: int = 4) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    if y.size < 8:
        return y
    wn = float(np.clip(cutoff, 1e-4, 0.49))
    b, a = signal.butter(order, wn, btype="low", output="ba")
    return signal.filtfilt(b, a, y)


def apply_highpass(y: np.ndarray, cutoff: float = 0.05, order: int = 4) -> np.ndarray:
    y = np.asarray(y, dtype=np.float64)
    if y.size < 8:
        return y
    wn = float(np.clip(cutoff, 1e-4, 0.49))
    b, a = signal.butter(order, wn, btype="high", output="ba")
    return signal.filtfilt(b, a, y)


def apply_fft(
    y: np.ndarray,
    sample_rate: float = 1.0,
) -> Tuple[np.ndarray, np.ndarray]:
    """Return (freq_hz, magnitude) single-sided spectrum."""
    y = np.asarray(y, dtype=np.float64)
    y = np.nan_to_num(y, nan=0.0, posinf=0.0, neginf=0.0)
    if y.size < 2:
        return np.array([0.0]), np.array([0.0])
    y = y - np.mean(y)
    n = y.size
    yf = np.fft.rfft(y * np.hanning(n))
    xf = np.fft.rfftfreq(n, d=1.0 / float(sample_rate))
    mag = 2.0 / n * np.abs(yf)
    return xf, mag


def process_waveform(y: np.ndarray, params: ProcessParams) -> Dict[str, Optional[np.ndarray]]:
    """Apply selected operations in a stable order. Returns original/processed/fft."""
    y0 = np.asarray(y, dtype=np.float64).copy()
    y_proc = y0.copy()

    if params.baseline:
        y_proc = apply_baseline(y_proc)
    if params.normalize:
        # normalize after baseline so filters see 0-1
        y_proc = apply_normalize(y_proc)
    if params.savgol:
        y_proc = apply_savgol(y_proc, params.savgol_window, params.savgol_poly)
    if params.moving_avg:
        y_proc = apply_moving_avg(y_proc, params.avg_window)
    if params.lowpass:
        y_proc = apply_lowpass(y_proc, params.cutoff_low, params.filter_order)
    if params.highpass:
        y_proc = apply_highpass(y_proc, params.cutoff_high, params.filter_order)

    out: Dict[str, Optional[np.ndarray]] = {
        "original": y0,
        "processed": y_proc,
        "fft_freq": None,
        "fft_mag": None,
    }
    if params.fft:
        freq, mag = apply_fft(y_proc, sample_rate=1.0)
        out["fft_freq"] = freq
        out["fft_mag"] = mag
    return out


def describe_process_stack(params: ProcessParams) -> str:
    steps = []
    if params.baseline:
        steps.append("基线去除")
    if params.normalize:
        steps.append("归一化")
    if params.savgol:
        steps.append(f"SG平滑(w={params.savgol_window})")
    if params.moving_avg:
        steps.append(f"滑动平均(w={params.avg_window})")
    if params.lowpass:
        steps.append(f"低通(fc={params.cutoff_low:.3f})")
    if params.highpass:
        steps.append(f"高通(fc={params.cutoff_high:.3f})")
    if params.fft:
        steps.append("FFT")
    return " → ".join(steps) if steps else "无处理"
