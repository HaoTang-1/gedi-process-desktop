"""Application-wide constants and visual tokens."""

from pathlib import Path

APP_NAME = "GEDI Process Desktop"
APP_VERSION = "1.0.0"
ORG_NAME = "GEDI-Process"

# GEDI footprint nominal diameter (meters)
FOOTPRINT_DIAMETER_M = 25.0

# Map downsampling
MAP_MAX_POINTS = 6000

# Default waveform processing sampling assumption (GEDI digitizer ~1 ns / sample ≈ 0.15 m)
DEFAULT_BIN_SIZE_M = 0.15
DEFAULT_SAMPLE_RATE_HZ = 1.0e9  # 1 GHz digitizer, relative frequency axis

# Beams
BEAM_IDS = [
    "BEAM0000",
    "BEAM0001",
    "BEAM0010",
    "BEAM0011",
    "BEAM0101",
    "BEAM0110",
    "BEAM1000",
    "BEAM1011",
]

BEAM_COLORS = {
    "BEAM0000": "#0B3D91",
    "BEAM0001": "#1AA3C8",
    "BEAM0010": "#2E7D32",
    "BEAM0011": "#C45C26",
    "BEAM0101": "#6A1B9A",
    "BEAM0110": "#AD1457",
    "BEAM1000": "#00838F",
    "BEAM1011": "#5D4037",
}

# Design tokens (scientific workstation, light instrument panel)
INK = "#1C2733"
PAPER = "#EEF2F6"
PANEL = "#FFFFFF"
NASA = "#0B3D91"
CYAN = "#1AA3C8"
WARN = "#C45C26"
GRID = "#D5DEE7"
MUTED = "#5B6B7C"
OK = "#2E7D32"

FOOTPRINT_RADIUS_M = FOOTPRINT_DIAMETER_M / 2.0

# meters per degree (approx, for footprint circle on map)
M_PER_DEG_LAT = 111_320.0


def m_per_deg_lon(lat_deg: float) -> float:
    import math

    return 111_320.0 * max(0.05, math.cos(math.radians(lat_deg)))


def footprint_deg_radius(lat_deg: float) -> float:
    """Approximate footprint radius in degrees for drawing at a given latitude."""
    rx = FOOTPRINT_RADIUS_M / m_per_deg_lon(lat_deg)
    ry = FOOTPRINT_RADIUS_M / M_PER_DEG_LAT
    return (rx + ry) / 2.0


def sample_data_dir() -> Path:
    return Path(r"F:\GEDI_process\data_sample")
