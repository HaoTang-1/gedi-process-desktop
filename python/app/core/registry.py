"""Plugin-style registries for transforms and metrics (CloudCompare-like extensibility).

New algorithms register themselves at import time; menus are rebuilt from the registry.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional

import numpy as np


@dataclass
class ParamSpec:
    name: str
    label: str
    kind: str = "float"  # float | int | choice
    default: Any = 0.0
    minimum: Optional[float] = None
    maximum: Optional[float] = None
    step: Optional[float] = None
    choices: List[str] = field(default_factory=list)


@dataclass
class TransformSpec:
    id: str
    name: str
    category: str  # 变换 / 滤波 / 归一化 ...
    func: Callable[..., Dict[str, Any]]
    params: List[ParamSpec] = field(default_factory=list)
    description: str = ""
    # "signal" => result is a waveform-like series
    # "spectrum" => result is freq/coeff domain, shown as secondary view
    result_kind: str = "signal"
    menu_order: int = 100


@dataclass
class MetricSpec:
    id: str
    name: str
    category: str
    func: Callable[[Any], Dict[str, Any]]
    description: str = ""
    menu_order: int = 100


class TransformRegistry:
    def __init__(self) -> None:
        self._items: Dict[str, TransformSpec] = {}

    def register(self, spec: TransformSpec) -> TransformSpec:
        self._items[spec.id] = spec
        return spec

    def get(self, spec_id: str) -> Optional[TransformSpec]:
        return self._items.get(spec_id)

    def all(self) -> List[TransformSpec]:
        return sorted(self._items.values(), key=lambda s: (s.menu_order, s.category, s.name))

    def by_category(self) -> Dict[str, List[TransformSpec]]:
        out: Dict[str, List[TransformSpec]] = {}
        for s in self.all():
            out.setdefault(s.category, []).append(s)
        return out

    def ids(self) -> List[str]:
        return [s.id for s in self.all()]


class MetricRegistry:
    def __init__(self) -> None:
        self._items: Dict[str, MetricSpec] = {}

    def register(self, spec: MetricSpec) -> MetricSpec:
        self._items[spec.id] = spec
        return spec

    def get(self, spec_id: str) -> Optional[MetricSpec]:
        return self._items.get(spec_id)

    def all(self) -> List[MetricSpec]:
        return sorted(self._items.values(), key=lambda s: (s.menu_order, s.category, s.name))

    def by_category(self) -> Dict[str, List[MetricSpec]]:
        out: Dict[str, List[MetricSpec]] = {}
        for s in self.all():
            out.setdefault(s.category, []).append(s)
        return out


TRANSFORMS = TransformRegistry()
METRICS = MetricRegistry()


def register_transform(
    id: str,
    name: str,
    category: str,
    result_kind: str = "signal",
    params: Optional[List[ParamSpec]] = None,
    description: str = "",
    menu_order: int = 100,
):
    def decorator(fn: Callable[..., Dict[str, Any]]):
        TRANSFORMS.register(
            TransformSpec(
                id=id,
                name=name,
                category=category,
                func=fn,
                params=params or [],
                description=description,
                result_kind=result_kind,
                menu_order=menu_order,
            )
        )
        return fn

    return decorator


def register_metric(
    id: str,
    name: str,
    category: str,
    description: str = "",
    menu_order: int = 100,
):
    def decorator(fn: Callable[[Any], Dict[str, Any]]):
        METRICS.register(
            MetricSpec(
                id=id,
                name=name,
                category=category,
                func=fn,
                description=description,
                menu_order=menu_order,
            )
        )
        return fn

    return decorator
