"""
doppler.specan.config — configuration loading.

Priority (highest to lowest):
  1. CLI flags
  2. doppler-specan.yml (if present)
  3. Defaults
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class DemoConfig:
    """Parameters for the built-in demo signal source."""

    tone_freq: float = 100e3  # Hz offset from center
    tone_power: float = -20.0  # dBm
    noise_floor: float = -90.0  # dBm


@dataclass
class SpecanConfig:
    """
    Complete specan configuration.

    Attributes
    ----------
    source : str
        ``"demo"``, ``"file"``, or ``"socket"``.
    address : str
        File path (source="file") or NATS address (source="socket").
    fs : float
        Input sample rate in Hz.  Required for file sources;
        auto-discovered from packet headers for socket sources.
    center : float
        Display center frequency in Hz. Until it is set -- by `center` in
        the yml, ``--center``, or a retune in either UI -- the display
        follows the source's own center (a stream header's
        ``center_freq``), so a live RF capture is on screen at its real
        frequency without being told what it already says.
    center_pinned : bool
        Whether `center` was set rather than adopted from the source.
    span : float
        Display bandwidth in Hz.  0 means auto (full input bandwidth).
    rbw : float
        Resolution bandwidth in Hz.  0 means auto (span / 100); the
        analyzer resolves it.
    level : float
        Reference level — top of display — in dBm.
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
    center_pinned: bool = False
    span: float = 0.0
    rbw: float = 0.0
    level: float = 0.0
    timeout: int = 2000  # socket receive timeout in milliseconds
    web: bool = False
    host: str = "127.0.0.1"
    port: int = 8765
    no_browser: bool = False
    demo: DemoConfig = field(default_factory=DemoConfig)

    # ----------------------------------------------------------------
    # Derived properties
    # ----------------------------------------------------------------


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
    yml_path: Path | None = None,
    **cli_overrides: object,
) -> SpecanConfig:
    """
    Build a :class:`SpecanConfig` from yml file + CLI overrides.

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

    cfg = SpecanConfig(
        source=data.get("source", "demo"),
        address=data.get("address", data.get("path", "")),
        fs=data.get("fs", 0.0),
        center=data.get("center", 0.0),
        span=data.get("span", 0.0),
        rbw=data.get("rbw", 0.0),
        level=data.get("level", 0.0),
        timeout=data.get("timeout", 2000),
        demo=demo,
    )

    # Apply CLI overrides (skip None values)
    for key, val in cli_overrides.items():
        if val is not None and hasattr(cfg, key):
            setattr(cfg, key, val)

    # A center someone gave, in the yml or on the command line, is theirs;
    # one nobody gave follows the source (see SpecanConfig.center).
    cfg.center_pinned = (
        "center" in data or cli_overrides.get("center") is not None
    )

    return cfg
