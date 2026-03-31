"""
doppler_specan.config — configuration loading.

Priority (highest to lowest):
  1. CLI flags
  2. doppler-specan.yml (if present)
  3. Defaults
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class DemoConfig:
    """Parameters for the built-in demo signal source."""

    tone_freq: float = 100e3  # Hz offset from center
    tone_power: float = -20.0  # dBm
    noise_floor: float = -90.0  # dBm


@dataclass
class SpEcanConfig:
    """
    Complete specan configuration.

    Attributes
    ----------
    source : str
        ``"demo"``, ``"file"``, or ``"socket"``.
    address : str
        File path (source="file") or ZMQ address (source="socket").
    fs : float
        Input sample rate in Hz.  Required for file sources;
        auto-discovered from packet headers for socket sources.
    center : float
        Center frequency in Hz.
    span : float
        Display bandwidth in Hz.  0 means auto (full input bandwidth).
    rbw : float
        Resolution bandwidth in Hz.  0 means auto (span / 401).
    level : float
        Reference level — top of display — in dBm.
    beta : float
        Kaiser window shape parameter.  Controls the RBW/sidelobe
        trade-off.  Default 6.0 (≈ −69 dB sidelobes, 1.47-bin ENBW).
    web : bool
        Launch the browser-based web UI instead of the terminal display.
    host : str
        Web server bind address (web mode only).
    port : int
        Web server port (web mode only).
    no_browser : bool
        Start web server without opening a browser window.
    demo : DemoConfig
        Parameters for the demo signal source.
    """

    source: str = "demo"
    address: str = ""
    fs: float = 0.0
    center: float = 0.0
    span: float = 0.0
    rbw: float = 0.0
    level: float = 0.0
    beta: float = 6.0
    web: bool = False
    host: str = "127.0.0.1"
    port: int = 8765
    no_browser: bool = False
    demo: DemoConfig = field(default_factory=DemoConfig)

    # ----------------------------------------------------------------
    # Derived properties
    # ----------------------------------------------------------------

    def effective_span(self, input_fs: float) -> float:
        """Return the display span in Hz, defaulting to full bandwidth."""
        if self.span > 0:
            return self.span
        # Default: full alias-free bandwidth of the input
        return input_fs * 0.8

    def effective_rbw(self, span: float) -> float:
        """Return the RBW in Hz, defaulting to span / 401."""
        if self.rbw > 0:
            return self.rbw
        return span / 401.0

    def fft_size(self, fs_out: float, rbw: float) -> int:
        """Return FFT size as the next power of two >= fs_out / rbw."""
        n = math.ceil(fs_out / rbw)
        # Round up to next power of two
        if n < 1:
            n = 1
        return 1 << (n - 1).bit_length()

    def fs_out(self, span: float) -> float:
        """Output sample rate from the resampler, given the span in Hz."""
        # alias-free BW ≈ 0.8 * fs_out  →  fs_out = span / 0.8
        return span / 0.8


# ------------------------------------------------------------------
# YAML loader
# ------------------------------------------------------------------

_YAML_PATH = Path("doppler-specan.yml")


def _load_yaml(path: Path) -> dict:
    """Load a YAML config file; return empty dict if not present."""
    if not path.exists():
        return {}
    try:
        import yaml  # type: ignore[import]
    except ImportError:
        return {}
    with path.open() as f:
        data = yaml.safe_load(f) or {}
    return data


def load_config(
    yml_path: Optional[Path] = None,
    **cli_overrides,
) -> SpEcanConfig:
    """
    Build a :class:`SpEcanConfig` from yml file + CLI overrides.

    Parameters
    ----------
    yml_path : Path, optional
        Path to a ``doppler-specan.yml`` file.  Defaults to
        ``./doppler-specan.yml``.
    **cli_overrides
        Keyword arguments that override yml values.  ``None`` values
        are ignored so that unset CLI flags don't clobber defaults.
    """
    data = _load_yaml(yml_path or _YAML_PATH)

    demo_data = data.pop("demo", {})
    demo = DemoConfig(
        tone_freq=demo_data.get("tone_freq", DemoConfig.tone_freq),
        tone_power=demo_data.get("tone_power", DemoConfig.tone_power),
        noise_floor=demo_data.get("noise_floor", DemoConfig.noise_floor),
    )

    cfg = SpEcanConfig(
        source=data.get("source", "demo"),
        address=data.get("address", data.get("path", "")),
        fs=data.get("fs", 0.0),
        center=data.get("center", 0.0),
        span=data.get("span", 0.0),
        rbw=data.get("rbw", 0.0),
        level=data.get("level", 0.0),
        beta=data.get("beta", 6.0),
        demo=demo,
    )

    # Apply CLI overrides (skip None values)
    for key, val in cli_overrides.items():
        if val is not None and hasattr(cfg, key):
            setattr(cfg, key, val)

    return cfg
