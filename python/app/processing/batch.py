"""Batch processing over multiple GEDI files."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional

import pandas as pd

from app.core.registry import METRICS
from app.data.gedi_loader import GediLoader
from app.processing.metrics import METRIC_COLUMNS, compute_waveform_metrics, metrics_to_row
from app.processing.waveform_ops import ProcessParams


ProgressFn = Callable[[int, int, str], None]


@dataclass
class BatchConfig:
    file_ids: List[int]
    beams: Optional[List[str]] = None  # None = all beams
    max_shots_per_beam: Optional[int] = None  # None = all
    quality_only: bool = False
    stride: int = 1
    process_params: Optional[ProcessParams] = None
    metric_ids: Optional[List[str]] = None  # registry metric ids; None = comprehensive
    output_csv: Optional[Path] = None


@dataclass
class BatchResult:
    records: List[Dict] = field(default_factory=list)
    n_ok: int = 0
    n_skip: int = 0
    output_path: Optional[Path] = None

    def to_dataframe(self) -> pd.DataFrame:
        if not self.records:
            return pd.DataFrame(columns=METRIC_COLUMNS)
        return pd.DataFrame(self.records)


def run_batch(
    loader: GediLoader,
    config: BatchConfig,
    progress: Optional[ProgressFn] = None,
) -> BatchResult:
    """Compute metrics for selected files/beams/shots and optionally export CSV."""
    result = BatchResult()

    # Build work list first for progress totals
    jobs = []
    for fid in config.file_ids:
        gf = loader.get_file(fid)
        if gf is None:
            continue
        beams = config.beams if config.beams else list(gf.beams.keys())
        for beam in beams:
            if beam not in gf.beams:
                continue
            meta = gf.beams[beam]
            n = meta["shot_number"].shape[0]
            indices = list(range(n))
            if config.quality_only and meta.get("quality") is not None:
                indices = [i for i in indices if int(meta["quality"][i]) == 1]
            if config.stride > 1:
                indices = indices[:: config.stride]
            if config.max_shots_per_beam is not None:
                indices = indices[: config.max_shots_per_beam]
            for i in indices:
                jobs.append((fid, gf.path.name, beam, i))

    total = len(jobs)
    if progress is not None:
        progress(0, total, "准备批处理…")

    for k, (fid, fname, beam, idx) in enumerate(jobs):
        try:
            wr = loader.load_waveform_result(fid, beam, idx)
            if wr is None:
                result.n_skip += 1
            else:
                if config.metric_ids:
                    metrics = {}
                    for mid in config.metric_ids:
                        spec = METRICS.get(mid)
                        if spec is None:
                            continue
                        try:
                            part = spec.func(wr)
                            metrics.update(part)
                        except Exception:
                            continue
                    if not metrics:
                        metrics = compute_waveform_metrics(wr, config.process_params)
                else:
                    metrics = compute_waveform_metrics(wr, config.process_params)
                result.records.append(metrics_to_row(fname, metrics))
                result.n_ok += 1
        except Exception:
            result.n_skip += 1
        if progress is not None and (k % 20 == 0 or k == total - 1):
            progress(k + 1, total, f"处理 {fname} / {beam} #{idx}")

    if config.output_csv is not None:
        df = result.to_dataframe()
        out = Path(config.output_csv)
        out.parent.mkdir(parents=True, exist_ok=True)
        df.to_csv(out, index=False, encoding="utf-8-sig")
        result.output_path = out
    return result
