"""Register built-in metrics into the plugin registry (add new metrics here)."""

from __future__ import annotations

from typing import Any, Dict

from app.core.registry import register_metric
from app.processing.metrics import compute_waveform_metrics


@register_metric(
    id="basic_waveform",
    name="基础波形统计",
    category="波形",
    description="均值/标准差/能量/质心/FWHM/偏度/峰度/峰值数/地表冠层比",
    menu_order=10,
)
def metric_basic_waveform(result: Any) -> Dict[str, Any]:
    return compute_waveform_metrics(result, None)


@register_metric(
    id="rh_profile",
    name="RH 相对高度",
    category="L2A",
    description="RH25/50/75/98/100 与冠层高度代理",
    menu_order=20,
)
def metric_rh(result: Any) -> Dict[str, Any]:
    return compute_waveform_metrics(result, None)


@register_metric(
    id="geometry",
    name="光斑几何/定位",
    category="几何",
    description="经纬度、高程差、delta_time",
    menu_order=30,
)
def metric_geometry(result: Any) -> Dict[str, Any]:
    m = compute_waveform_metrics(result, None)
    keep = {
        k: m.get(k)
        for k in (
            "shot_number",
            "lon",
            "lat",
            "elev_top",
            "elev_bot",
            "canopy_height",
            "delta_time",
            "beam",
            "product",
        )
    }
    return keep
