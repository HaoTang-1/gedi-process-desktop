"""Matplotlib Chinese font bootstrap for Windows scientific UI."""

from __future__ import annotations

from pathlib import Path

import matplotlib

_CANDIDATE_FONT_FILES = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\msyhbd.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\Deng.ttf",
    r"C:\Windows\Fonts\simsun.ttc",
]
_CANDIDATE_NAMES = [
    "Microsoft YaHei",
    "SimHei",
    "DengXian",
    "Microsoft JhengHei",
    "Noto Sans CJK SC",
    "Source Han Sans SC",
]


def setup_matplotlib_chinese() -> str:
    """Configure matplotlib for Chinese labels; return active font family."""
    from matplotlib import font_manager

    # Register Windows CJK fonts explicitly (fontconfig on some setups misses TTC)
    for fp in _CANDIDATE_FONT_FILES:
        p = Path(fp)
        if p.exists():
            try:
                font_manager.fontManager.addfont(str(p))
            except Exception:
                pass

    available = {f.name for f in font_manager.fontManager.ttflist}
    chosen = None
    for name in _CANDIDATE_NAMES:
        if name in available:
            chosen = name
            break
    if chosen is None:
        # fuzzy match
        lower_map = {n.lower(): n for n in available}
        for name in _CANDIDATE_NAMES:
            hit = lower_map.get(name.lower())
            if hit:
                chosen = hit
                break

    if chosen:
        matplotlib.rcParams["font.sans-serif"] = [chosen, "DejaVu Sans"]
    else:
        matplotlib.rcParams["font.sans-serif"] = ["DejaVu Sans"]
    matplotlib.rcParams["axes.unicode_minus"] = False
    matplotlib.rcParams["font.family"] = "sans-serif"
    return chosen or "DejaVu Sans"


# Apply on import so every canvas sees the same fonts
ACTIVE_FONT = setup_matplotlib_chinese()
