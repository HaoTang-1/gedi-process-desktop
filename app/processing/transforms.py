"""Waveform transforms registered as menu-driven modules (FFT / wavelet / filters).

Each transform returns a dict:
  {"y": processed series, "x": optional x axis, "label": str,
   "aux_x": optional, "aux_y": optional, "aux_label": str, "kind": "signal"|"spectrum"}
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

import numpy as np
from scipy import signal

from app.core.registry import ParamSpec, register_transform
from app.processing import waveform_ops as ops

# Ensure package import side effects
import pywt


def _base(y: np.ndarray) -> np.ndarray:
    return np.asarray(y, dtype=np.float64).copy()


@register_transform(
    id="normalize",
    name="归一化 (0–1)",
    category="归一化",
    result_kind="signal",
    menu_order=10,
    description="将波形线性拉伸到 [0, 1]",
)
def tr_normalize(y: np.ndarray, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_normalize(_base(y))
    return {"y": yn, "label": "归一化", "kind": "signal"}


@register_transform(
    id="baseline",
    name="基线去除",
    category="归一化",
    result_kind="signal",
    menu_order=20,
    params=[ParamSpec("quantile", "噪声地板分位数 (%)", "float", 5.0, 0.0, 50.0, 1.0)],
    description="减去低分位噪声地板",
)
def tr_baseline(y: np.ndarray, quantile: float = 5.0, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_baseline(_base(y), quantile=float(quantile))
    return {"y": yn, "label": f"基线去除 q={quantile}", "kind": "signal"}


@register_transform(
    id="savgol",
    name="Savitzky-Golay 平滑",
    category="滤波",
    result_kind="signal",
    menu_order=30,
    params=[
        ParamSpec("window", "窗口长度", "int", 11, 5, 101, 2),
        ParamSpec("poly", "多项式阶数", "int", 3, 1, 5, 1),
    ],
)
def tr_savgol(y: np.ndarray, window: int = 11, poly: int = 3, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_savgol(_base(y), int(window), int(poly))
    return {"y": yn, "label": f"SG(w={int(window)},p={int(poly)})", "kind": "signal"}


@register_transform(
    id="moving_avg",
    name="滑动平均",
    category="滤波",
    result_kind="signal",
    menu_order=40,
    params=[ParamSpec("window", "窗口长度", "int", 5, 1, 51, 1)],
)
def tr_moving_avg(y: np.ndarray, window: int = 5, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_moving_avg(_base(y), int(window))
    return {"y": yn, "label": f"滑动平均 w={int(window)}", "kind": "signal"}


@register_transform(
    id="lowpass",
    name="Butterworth 低通",
    category="滤波",
    result_kind="signal",
    menu_order=50,
    params=[
        ParamSpec("cutoff", "截止频率 (×Nyquist)", "float", 0.15, 0.001, 0.49, 0.01),
        ParamSpec("order", "滤波阶数", "int", 4, 1, 10, 1),
    ],
)
def tr_lowpass(y: np.ndarray, cutoff: float = 0.15, order: int = 4, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_lowpass(_base(y), float(cutoff), int(order))
    return {"y": yn, "label": f"低通 fc={float(cutoff):.3f}", "kind": "signal"}


@register_transform(
    id="highpass",
    name="Butterworth 高通",
    category="滤波",
    result_kind="signal",
    menu_order=60,
    params=[
        ParamSpec("cutoff", "截止频率 (×Nyquist)", "float", 0.05, 0.001, 0.49, 0.01),
        ParamSpec("order", "滤波阶数", "int", 4, 1, 10, 1),
    ],
)
def tr_highpass(y: np.ndarray, cutoff: float = 0.05, order: int = 4, **_: Any) -> Dict[str, Any]:
    yn = ops.apply_highpass(_base(y), float(cutoff), int(order))
    return {"y": yn, "label": f"高通 fc={float(cutoff):.3f}", "kind": "signal"}


@register_transform(
    id="fft",
    name="傅里叶变换 (FFT)",
    category="变换",
    result_kind="spectrum",
    menu_order=100,
    params=[ParamSpec("window", "窗函数", "choice", 0, choices=["Hanning", "Hamming", "矩形"])],
    description="计算单边幅度谱（菜单调用，不常驻显示）",
)
def tr_fft(y: np.ndarray, window: int = 0, **_: Any) -> Dict[str, Any]:
    y0 = _base(y)
    y0 = np.nan_to_num(y0, nan=0.0, posinf=0.0, neginf=0.0)
    if y0.size < 2:
        return {"y": y0, "label": "FFT", "kind": "spectrum", "aux_x": np.array([0.0]), "aux_y": np.array([0.0]), "aux_label": "FFT"}
    y0 = y0 - np.mean(y0)
    n = y0.size
    if int(window) == 0:
        w = np.hanning(n)
        wname = "Hanning"
    elif int(window) == 1:
        w = np.hamming(n)
        wname = "Hamming"
    else:
        w = np.ones(n)
        wname = "矩形"
    yf = np.fft.rfft(y0 * w)
    xf = np.fft.rfftfreq(n, d=1.0)
    mag = 2.0 / n * np.abs(yf)
    return {
        "y": y0,
        "label": f"FFT({wname})",
        "kind": "spectrum",
        "aux_x": xf,
        "aux_y": mag,
        "aux_label": f"FFT 幅度谱 · {wname}",
        "aux_xlabel": "归一化频率 (cycles/sample)",
        "aux_ylabel": "|FFT|",
    }


@register_transform(
    id="dwt_denoise",
    name="小波去噪 (DWT)",
    category="变换",
    result_kind="signal",
    menu_order=110,
    params=[
        ParamSpec("wavelet", "小波基", "choice", 0, choices=["sym4", "db4", "db8", "coif3", "haar"]),
        ParamSpec("level", "分解层数", "int", 3, 1, 6, 1),
        ParamSpec("mode", "阈值模式", "choice", 0, choices=["soft", "hard"]),
    ],
    description="离散小波阈值去噪",
)
def tr_dwt_denoise(
    y: np.ndarray,
    wavelet: int = 0,
    level: int = 3,
    mode: int = 0,
    **_: Any,
) -> Dict[str, Any]:
    names = ["sym4", "db4", "db8", "coif3", "haar"]
    wname = names[int(wavelet)] if isinstance(wavelet, (int, np.integer)) else str(wavelet)
    thresh_mode = "soft" if int(mode) == 0 else "hard"
    y0 = _base(y)
    y0 = np.nan_to_num(y0, nan=0.0)
    max_level = pywt.dwt_max_level(y0.size, pywt.Wavelet(wname).dec_len)
    lv = max(1, min(int(level), max_level))
    coeffs = pywt.wavedec(y0, wname, level=lv)
    # universal threshold on detail coeffs
    sigma = np.median(np.abs(coeffs[-1])) / 0.6745 if coeffs[-1].size else 0.0
    uthresh = float(sigma * np.sqrt(2.0 * np.log(max(y0.size, 2)))) if sigma > 0 else 0.0
    new_coeffs = [coeffs[0]]
    for c in coeffs[1:]:
        new_coeffs.append(pywt.threshold(c, uthresh, mode=thresh_mode))
    yn = pywt.waverec(new_coeffs, wname)
    if yn.size > y0.size:
        yn = yn[: y0.size]
    elif yn.size < y0.size:
        yn = np.pad(yn, (0, y0.size - yn.size), mode="edge")
    return {
        "y": yn,
        "label": f"小波去噪 {wname} L={lv} {thresh_mode}",
        "kind": "signal",
    }


@register_transform(
    id="cwt_scalogram",
    name="小波时频图 (CWT)",
    category="变换",
    result_kind="spectrum",
    menu_order=120,
    params=[
        ParamSpec("wavelet", "小波基", "choice", 0, choices=["morl", "mexh", "gaus4"]),
        ParamSpec("num_scales", "尺度数", "int", 32, 8, 64, 1),
    ],
    description="连续小波变换 scalogram（在演示区显示）",
)
def tr_cwt(y: np.ndarray, wavelet: int = 0, num_scales: int = 32, **_: Any) -> Dict[str, Any]:
    names = ["morl", "mexh", "gaus4"]
    wname = names[int(wavelet)] if isinstance(wavelet, (int, np.integer)) else str(wavelet)
    y0 = _base(y)
    y0 = np.nan_to_num(y0, nan=0.0)
    if y0.size < 4:
        return {"y": y0, "label": "CWT", "kind": "spectrum", "aux_x": None, "aux_y": None, "aux_label": "CWT", "image": None}
    scales = np.arange(1, int(num_scales) + 1)
    coef, freqs = pywt.cwt(y0, scales, wname)
    return {
        "y": y0,
        "label": f"CWT {wname}",
        "kind": "spectrum",
        "aux_x": np.arange(y0.size),
        "aux_y": coef,
        "aux_label": f"CWT Scalogram · {wname}",
        "image": np.abs(coef),
        "image_xlabel": "采样点",
        "image_ylabel": "尺度",
    }


@register_transform(
    id="stft",
    name="短时傅里叶 (STFT)",
    category="变换",
    result_kind="spectrum",
    menu_order=130,
    params=[
        ParamSpec("nperseg", "每段长度", "int", 64, 16, 256, 8),
        ParamSpec("noverlap", "重叠点数", "int", 32, 0, 255, 8),
    ],
)
def tr_stft(y: np.ndarray, nperseg: int = 64, noverlap: int = 32, **_: Any) -> Dict[str, Any]:
    y0 = _base(y)
    y0 = np.nan_to_num(y0, nan=0.0)
    nper = int(min(max(8, nperseg), max(8, y0.size)))
    nover = int(min(max(0, noverlap), nper - 1))
    f, t, Z = signal.stft(y0, nperseg=nper, noverlap=nover)
    return {
        "y": y0,
        "label": f"STFT nperseg={nper}",
        "kind": "spectrum",
        "aux_x": t,
        "aux_y": np.abs(Z),
        "aux_label": f"STFT 时频图 nperseg={nper}",
        "image": np.abs(Z),
        "image_xlabel": "时间 (段)",
        "image_ylabel": "频率 bin",
    }


def apply_transform_by_id(y: np.ndarray, spec_id: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    from app.core.registry import TRANSFORMS

    spec = TRANSFORMS.get(spec_id)
    if spec is None:
        raise KeyError(f"未知变换: {spec_id}")
    kwargs = dict(params or {})
    return spec.func(y, **kwargs)


def apply_chain(y: np.ndarray, spec_ids: List[str], params_by_id: Optional[Dict[str, Dict[str, Any]]] = None) -> Dict[str, Any]:
    """Apply signal-domain transforms in sequence; last spectrum view kept if any."""
    y_cur = _base(y)
    labels: List[str] = []
    aux = None
    aux_label = ""
    kind = "signal"
    image = None
    for sid in spec_ids:
        p = (params_by_id or {}).get(sid, {})
        out = apply_transform_by_id(y_cur, sid, p)
        y_cur = out.get("y", y_cur)
        labels.append(out.get("label", sid))
        if out.get("kind") == "spectrum":
            kind = "spectrum"
            aux = out.get("aux_y")
            aux_label = out.get("aux_label", sid)
            image = out.get("image")
    return {
        "y": y_cur,
        "label": " → ".join(labels) if labels else "无处理",
        "kind": kind,
        "aux_y": aux,
        "aux_label": aux_label,
        "image": image,
        "chain": labels,
    }
