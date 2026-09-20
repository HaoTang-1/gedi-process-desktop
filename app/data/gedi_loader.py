"""GEDI L1B / L2A HDF5 loaders.

Large waveform arrays stay on disk; only compact per-shot metadata is cached.
Waveforms / RH profiles are read on demand for the selected shot.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import h5py
import numpy as np

from app.config import BEAM_IDS


@dataclass
class ShotMeta:
    file_id: int
    beam: str
    index: int
    shot_number: int
    lon: float
    lat: float
    elev_top: Optional[float] = None
    elev_bot: Optional[float] = None
    sensitivity: Optional[float] = None
    quality: Optional[int] = None
    degrade: Optional[int] = None
    delta_time: Optional[float] = None
    n_samples: Optional[int] = None


@dataclass
class WaveformResult:
    product: str
    beam: str
    shot_number: int
    lon: float
    lat: float
    elev_top: Optional[float]
    elev_bot: Optional[float]
    sensitivity: Optional[float]
    quality: Optional[int]
    degrade: Optional[int]
    delta_time: Optional[float]
    # L1B
    waveform: Optional[np.ndarray] = None
    # shared vertical axis for L1B (height above ellipsoid-ish) or L2A RH
    heights: Optional[np.ndarray] = None
    rh: Optional[np.ndarray] = None
    energy_total: Optional[float] = None
    selected_algorithm: Optional[int] = None

    def has_waveform(self) -> bool:
        return self.waveform is not None and self.waveform.size > 0

    def has_rh(self) -> bool:
        return self.rh is not None and self.rh.size > 0


@dataclass
class GediFile:
    path: Path
    product: str  # "L1B" | "L2A"
    short_name: str
    file_id: int
    beams: Dict[str, dict] = field(default_factory=dict)
    n_shots: int = 0

    @property
    def display_name(self) -> str:
        return f"[{self.product}] {self.path.name}"


def detect_product(path: Path) -> str:
    name = path.name.upper()
    if "GEDI01_B" in name or "_L1B" in name or "GEDI_L1B" in name:
        return "L1B"
    if "GEDI02_A" in name or "_L2A" in name or "GEDI_L2A" in name:
        return "L2A"
    # fallback via HDF5 attribute
    try:
        with h5py.File(path, "r") as f:
            sn = str(f.attrs.get("short_name", ""))
            if "L1B" in sn:
                return "L1B"
            if "L2A" in sn:
                return "L2A"
    except Exception:
        pass
    return "UNKNOWN"


def _safe_read(ds: h5py.Dataset) -> np.ndarray:
    return np.asarray(ds[()])


def _first(h5: h5py.File, *candidates: str) -> Optional[np.ndarray]:
    for c in candidates:
        if c in h5:
            obj = h5[c]
            if isinstance(obj, h5py.Dataset):
                return _safe_read(obj)
    return None


class GediLoader:
    """Load compact metadata for one or more GEDI L1B/L2A granules."""

    def __init__(self) -> None:
        self.files: List[GediFile] = []
        self._next_id = 0
        self._handles: Dict[int, h5py.File] = {}

    def open_files(self, paths: List[Path]) -> List[GediFile]:
        opened = []
        for p in paths:
            gf = self.open_file(p)
            if gf is not None:
                opened.append(gf)
        return opened

    def open_file(self, path: Path | str) -> Optional[GediFile]:
        path = Path(path)
        if not path.exists():
            return None
        product = detect_product(path)
        if product == "UNKNOWN":
            raise ValueError(f"无法识别产品类型（需 L1B/L2A）: {path.name}")

        file_id = self._next_id
        self._next_id += 1
        gf = GediFile(
            path=path,
            product=product,
            short_name=path.name,
            file_id=file_id,
        )

        try:
            f = h5py.File(path, "r")
        except Exception as e:
            raise RuntimeError(f"无法打开 HDF5: {path.name}\n{e}") from e

        self._handles[file_id] = f
        short = str(f.attrs.get("short_name", product))
        gf.short_name = short

        for beam in BEAM_IDS:
            if beam not in f:
                continue
            g = f[beam]
            if not isinstance(g, h5py.Group):
                continue
            meta = self._read_beam_meta(g, product)
            if meta is None:
                continue
            gf.beams[beam] = meta
            gf.n_shots += int(meta["shot_number"].shape[0])

        self.files.append(gf)
        return gf

    def _read_beam_meta(self, g: h5py.Group, product: str) -> Optional[dict]:
        shot = _first(g, "shot_number")
        if shot is None:
            return None
        n = shot.shape[0]

        delta = _first(g, "delta_time", "geolocation/delta_time")
        if delta is None:
            delta = np.full(n, np.nan)

        if product == "L1B":
            lon = _first(g, "geolocation/longitude_bin0", "lon_lowestmode", "longitude_bin0")
            lat = _first(g, "geolocation/latitude_bin0", "lat_lowestmode", "latitude_bin0")
            elev_top = _first(g, "geolocation/elevation_bin0", "elev_highestreturn", "elevation_bin0")
            elev_bot = _first(g, "geolocation/elevation_lastbin", "elev_lowestmode", "elevation_lastbin")
            degrade = _first(g, "geolocation/degrade", "degrade_flag")
            sens = _first(g, "sensitivity", "geolocation/sensitivity")
            quality = _first(g, "rx_clipflag")  # 1 = clipped, treat as bad
            if quality is not None:
                quality = 1 - quality  # invert so 1=good-ish for L1B display
            start = _first(g, "rx_sample_start_index")
            count = _first(g, "rx_sample_count")
            if lon is None or lat is None or start is None or count is None:
                return None
            # L1B start index is 1-based in this product version
            start0 = start.astype(np.int64)
            # normalize to 0-based offsets into rxwaveform
            if start0.size and int(np.nanmin(start0)) >= 1:
                start0 = start0 - 1
            meta = {
                "shot_number": shot.astype(np.int64),
                "lon": lon.astype(np.float64),
                "lat": lat.astype(np.float64),
                "elev_top": None if elev_top is None else elev_top.astype(np.float64),
                "elev_bot": None if elev_bot is None else elev_bot.astype(np.float64),
                "sensitivity": None if sens is None else sens.astype(np.float32),
                "quality": None if quality is None else quality.astype(np.int16),
                "degrade": None if degrade is None else degrade.astype(np.int16),
                "delta_time": delta.astype(np.float64),
                "wf_start": start0,
                "wf_count": count.astype(np.int32),
            }
        else:
            lon = _first(g, "lon_lowestmode", "geolocation/longitude_bin0", "lon_highestreturn")
            lat = _first(g, "lat_lowestmode", "geolocation/latitude_bin0", "lat_highestreturn")
            elev_top = _first(g, "elev_highestreturn", "geolocation/elev_highestreturn_a10")
            elev_bot = _first(g, "elev_lowestmode", "geolocation/elev_lowestmode_a10")
            sens = _first(g, "sensitivity")
            quality = _first(g, "l2a_quality_flag_rel3", "l2a_quality_flag_rel2")
            degrade = _first(g, "degrade_flag", "geolocation/degrade_include_flag")
            # RH (N x 101) stays on disk — loaded per-shot on demand
            has_rh = "rh" in g and isinstance(g["rh"], h5py.Dataset)
            energy = _first(g, "energy_total")
            sel_alg = _first(g, "selected_algorithm")
            if lon is None or lat is None:
                return None
            meta = {
                "shot_number": shot.astype(np.int64),
                "lon": lon.astype(np.float64),
                "lat": lat.astype(np.float64),
                "elev_top": None if elev_top is None else elev_top.astype(np.float64),
                "elev_bot": None if elev_bot is None else elev_bot.astype(np.float64),
                "sensitivity": None if sens is None else sens.astype(np.float32),
                "quality": None if quality is None else quality.astype(np.int16),
                "degrade": None if degrade is None else degrade.astype(np.int16),
                "delta_time": delta.astype(np.float64),
                "has_rh": has_rh,
                "energy_total": None if energy is None else energy.astype(np.float32),
                "selected_algorithm": None if sel_alg is None else sel_alg.astype(np.int16),
            }
        return meta

    def iter_shot_meta(
        self,
        file_id: int,
        beam: Optional[str] = None,
        max_points: Optional[int] = None,
        quality_only: bool = False,
        max_points_per_beam: Optional[int] = None,
    ) -> List[ShotMeta]:
        gf = self.get_file(file_id)
        if gf is None:
            return []
        beams = [beam] if beam and beam in gf.beams else list(gf.beams.keys())
        out: List[ShotMeta] = []
        for b in beams:
            meta = gf.beams[b]
            n = meta["shot_number"].shape[0]
            idx = np.arange(n)
            lon = meta["lon"]
            lat = meta["lat"]
            valid = np.isfinite(lon) & np.isfinite(lat)
            if quality_only and meta.get("quality") is not None:
                valid = valid & (meta["quality"] == 1)
            idx = idx[valid]
            if max_points_per_beam is not None and idx.size > max_points_per_beam:
                stride = max(1, idx.size // max_points_per_beam)
                idx = idx[::stride]
            for i in idx:
                i = int(i)
                n_samples = None
                if "wf_count" in meta:
                    n_samples = int(meta["wf_count"][i])
                out.append(
                    ShotMeta(
                        file_id=file_id,
                        beam=b,
                        index=i,
                        shot_number=int(meta["shot_number"][i]),
                        lon=float(meta["lon"][i]),
                        lat=float(meta["lat"][i]),
                        elev_top=None if meta["elev_top"] is None else float(meta["elev_top"][i]),
                        elev_bot=None if meta["elev_bot"] is None else float(meta["elev_bot"][i]),
                        sensitivity=None if meta["sensitivity"] is None else float(meta["sensitivity"][i]),
                        quality=None if meta["quality"] is None else int(meta["quality"][i]),
                        degrade=None if meta["degrade"] is None else int(meta["degrade"][i]),
                        delta_time=None if meta["delta_time"] is None else float(meta["delta_time"][i]),
                        n_samples=n_samples,
                    )
                )
            if max_points is not None and len(out) >= max_points:
                out = out[:max_points]
                break
        return out

    def get_file(self, file_id: int) -> Optional[GediFile]:
        for f in self.files:
            if f.file_id == file_id:
                return f
        return None

    def get_handle(self, file_id: int) -> Optional[h5py.File]:
        return self._handles.get(file_id)

    def load_waveform_result(self, file_id: int, beam: str, index: int) -> Optional[WaveformResult]:
        gf = self.get_file(file_id)
        if gf is None or beam not in gf.beams:
            return None
        meta = gf.beams[beam]
        h5 = self._handles.get(file_id)
        if h5 is None:
            return None
        g = h5[beam]

        def s(arr, i):
            return None if arr is None else arr[i]

        res = WaveformResult(
            product=gf.product,
            beam=beam,
            shot_number=int(meta["shot_number"][index]),
            lon=float(meta["lon"][index]),
            lat=float(meta["lat"][index]),
            elev_top=s(meta["elev_top"], index),
            elev_bot=s(meta["elev_bot"], index),
            sensitivity=s(meta["sensitivity"], index),
            quality=s(meta["quality"], index),
            degrade=s(meta["degrade"], index),
            delta_time=s(meta["delta_time"], index),
        )

        if gf.product == "L1B":
            start = int(meta["wf_start"][index])
            count = int(meta["wf_count"][index])
            if count > 0 and "rxwaveform" in g:
                wf = g["rxwaveform"][start : start + count]
                res.waveform = np.asarray(wf, dtype=np.float64)
                elev_top = res.elev_top
                elev_bot = res.elev_bot
                if elev_top is not None and elev_bot is not None:
                    # linear height axis from first bin to last bin
                    res.heights = np.linspace(float(elev_top), float(elev_bot), count)
                else:
                    res.heights = np.arange(count, dtype=np.float64)
        else:
            if meta.get("has_rh") and "rh" in g:
                res.rh = np.asarray(g["rh"][index], dtype=np.float64)
                res.heights = np.arange(res.rh.size, dtype=np.float64)
            if meta.get("energy_total") is not None:
                res.energy_total = float(meta["energy_total"][index])
            if meta.get("selected_algorithm") is not None:
                res.selected_algorithm = int(meta["selected_algorithm"][index])
        return res

    def close_file(self, file_id: int) -> None:
        h5 = self._handles.pop(file_id, None)
        if h5 is not None:
            try:
                h5.close()
            except Exception:
                pass
        self.files = [f for f in self.files if f.file_id != file_id]

    def close_all(self) -> None:
        for fid in list(self._handles.keys()):
            self.close_file(fid)

    def collect_map_points(
        self,
        file_ids: Optional[List[int]] = None,
        beam: Optional[str] = None,
        max_points: int = 4000,
        quality_only: bool = False,
    ) -> List[ShotMeta]:
        ids = file_ids if file_ids is not None else [f.file_id for f in self.files]
        per_beam = max(50, max_points // max(1, len(ids) * 4))
        pts: List[ShotMeta] = []
        for fid in ids:
            pts.extend(
                self.iter_shot_meta(
                    fid,
                    beam=beam,
                    quality_only=quality_only,
                    max_points_per_beam=per_beam,
                )
            )
        if len(pts) > max_points:
            stride = max(1, len(pts) // max_points)
            pts = pts[::stride][:max_points]
        return pts


def load_waveforms_batch(
    loader: GediLoader,
    file_id: int,
    beam: str,
    indices: List[int],
) -> List[WaveformResult]:
    """Load a list of shots from one beam (used by batch algorithms)."""
    out = []
    for i in indices:
        r = loader.load_waveform_result(file_id, beam, i)
        if r is not None:
            out.append(r)
    return out
