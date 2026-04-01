"""
doppler-specan entry point.

    uvx --from doppler-specan doppler-specan            # terminal display
    uvx --from doppler-specan doppler-specan --web      # browser UI
    doppler-specan --source demo --span 200e3 --rbw 500
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime, timezone


def _log(msg: str) -> None:
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print(f"[{ts}] {msg}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="doppler-specan",
        description="Live spectrum analyzer — doppler NCO + FFT",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    # Source
    parser.add_argument(
        "--source",
        default=None,
        choices=["demo", "file", "socket", "pull"],
        help="Signal source",
    )
    parser.add_argument(
        "--address",
        default=None,
        metavar="PATH_OR_ADDR",
        help=(
            "File path (source=file) or ZMQ endpoint (source=socket). "
            "ZMQ endpoints are auto-detected: tcp://, ipc://, inproc://"
        ),
    )
    parser.add_argument(
        "--fs",
        type=float,
        default=None,
        metavar="HZ",
        help="Input sample rate (required for source=file)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=None,
        metavar="MS",
        help="Socket receive timeout in milliseconds (source=socket, default: 2000)",
    )

    # Display
    parser.add_argument(
        "--center",
        type=float,
        default=None,
        metavar="HZ",
        help="Center frequency in Hz",
    )
    parser.add_argument(
        "--span",
        type=float,
        default=None,
        metavar="HZ",
        help="Display span in Hz (default: full input bandwidth)",
    )
    parser.add_argument(
        "--rbw",
        type=float,
        default=None,
        metavar="HZ",
        help="Resolution bandwidth in Hz (default: span/401)",
    )
    parser.add_argument(
        "--level",
        type=float,
        default=None,
        metavar="DBM",
        help="Reference level — top of display — in dBm",
    )
    parser.add_argument(
        "--beta",
        type=float,
        default=None,
        metavar="BETA",
        help="Kaiser window β (0=rectangular, 6=spectrum analyser)",
    )

    # Demo source
    parser.add_argument(
        "--tone-freq",
        type=float,
        default=None,
        metavar="HZ",
        help="Demo tone frequency offset from center in Hz",
    )
    parser.add_argument(
        "--tone-power",
        type=float,
        default=None,
        metavar="DBM",
        help="Demo tone power in dBm",
    )
    parser.add_argument(
        "--noise-floor",
        type=float,
        default=None,
        metavar="DBM",
        help="Demo noise floor in dBm",
    )

    # Config file
    parser.add_argument(
        "--config",
        default=None,
        metavar="PATH",
        help="Path to doppler-specan.yml (default: ./doppler-specan.yml)",
    )

    # Mode
    parser.add_argument(
        "--web",
        action="store_true",
        default=False,
        help="Launch browser UI instead of terminal display",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        metavar="PORT",
        help="Web server port (web mode only)",
    )
    parser.add_argument(
        "--host",
        default=None,
        metavar="ADDR",
        help="Web server bind address (web mode only)",
    )
    parser.add_argument(
        "--no-browser",
        action="store_true",
        default=False,
        help="Start web server without opening a browser window",
    )

    args = parser.parse_args()

    # ----------------------------------------------------------------
    # Build config from yml + CLI overrides
    # ----------------------------------------------------------------
    from pathlib import Path  # noqa: PLC0415

    from doppler_specan.config import load_config  # noqa: PLC0415

    yml_path = Path(args.config) if args.config else None

    # Collect demo overrides into sub-namespace
    demo_overrides: dict = {}
    if args.tone_freq is not None:
        demo_overrides["tone_freq"] = args.tone_freq
    if args.tone_power is not None:
        demo_overrides["tone_power"] = args.tone_power
    if args.noise_floor is not None:
        demo_overrides["noise_floor"] = args.noise_floor

    # Auto-infer source=socket from ZMQ address prefix
    source = args.source
    if source is None and args.address:
        _ZMQ_PREFIXES = ("tcp://", "ipc://", "inproc://", "pgm://", "epgm://")
        if any(args.address.startswith(p) for p in _ZMQ_PREFIXES):
            source = "socket"

    cfg = load_config(
        yml_path=yml_path,
        source=source,
        address=args.address,
        fs=args.fs,
        center=args.center,
        span=args.span,
        rbw=args.rbw,
        level=args.level,
        beta=args.beta,
        web=args.web,
        host=args.host,
        port=args.port,
        no_browser=args.no_browser,
        timeout=args.timeout,
    )

    # Apply demo sub-overrides
    for k, v in demo_overrides.items():
        setattr(cfg.demo, k, v)

    # ----------------------------------------------------------------
    # Dispatch to terminal or web mode
    # ----------------------------------------------------------------
    mode = "web" if cfg.web else "terminal"
    source_info = f"source={cfg.source}"
    if cfg.address:
        source_info += f" address={cfg.address}"
    _log(f"doppler-specan started — mode={mode} {source_info}")

    if cfg.web:
        _run_web(cfg)
    else:
        _run_terminal(cfg)


def _run_terminal(cfg) -> None:
    """Launch the terminal spectrum display."""
    from doppler_specan.engine import SpecanEngine  # noqa: PLC0415
    from doppler_specan.source import make_source  # noqa: PLC0415
    from doppler_specan.terminal import TerminalDisplay  # noqa: PLC0415

    source = make_source(cfg)
    engine = SpecanEngine(cfg)
    display = TerminalDisplay(engine, cfg, source)
    try:
        display.run()
    finally:
        source.close()
        engine.close()


def _run_web(cfg) -> None:
    """Launch the FastAPI / WebSocket browser UI."""
    try:
        import fastapi  # noqa: F401
        import uvicorn  # noqa: F401
    except ImportError:
        print(
            "doppler-specan web mode requires the 'web' extra:\n\n"
            "    pip install 'doppler-specan[web]'\n",
            file=sys.stderr,
        )
        sys.exit(1)

    from doppler_specan.server import main as serve  # noqa: PLC0415

    serve(
        host=cfg.host,
        port=cfg.port,
        open_browser=not cfg.no_browser,
        cfg=cfg,
    )


if __name__ == "__main__":
    main()
