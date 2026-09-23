"""Application session state (visibility, selection, processing chain)."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set

from app.data.gedi_loader import ShotMeta, WaveformResult


@dataclass
class SessionState:
    visible_file_ids: Set[int] = field(default_factory=set)
    selected_file_id: Optional[int] = None
    selected_shot: Optional[ShotMeta] = None
    selected_result: Optional[WaveformResult] = None
    # ordered transform ids currently applied to displayed waveform
    transform_chain: List[str] = field(default_factory=list)
    transform_params: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    # map scalar field: None | "beam" | "sensitivity" | "quality" | ...
    map_scalar: str = "beam"
    show_footprint_circles: bool = True
    waveform_visible: bool = False
    last_metrics: Dict[str, Any] = field(default_factory=dict)
    log_lines: List[str] = field(default_factory=list)

    def log(self, msg: str) -> None:
        self.log_lines.append(msg)
        if len(self.log_lines) > 500:
            self.log_lines = self.log_lines[-400:]

    def is_file_visible(self, file_id: int) -> bool:
        return file_id in self.visible_file_ids

    def visible_ids(self) -> List[int]:
        return sorted(self.visible_file_ids)
