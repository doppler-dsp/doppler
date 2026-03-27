"""
doppler-specan entry point.

    uvx --from doppler-dsp[specan] doppler-specan
    doppler-specan --port 8765 --fft-size 512 --no-browser
"""

from __future__ import annotations

import argparse
import sys


def _check_deps() -> None:
    missing = []
    for pkg in ("fastapi", "uvicorn", "websockets"):
        try:
            __import__(pkg)
        except ImportError:
            missing.append(pkg)
    if missing:
        print(
            "doppler-specan requires the 'specan' extra:\n\n"
            "    pip install 'doppler-dsp[specan]'\n\n"
            f"Missing: {', '.join(missing)}",
            file=sys.stderr,
        )
        sys.exit(1)


def main() -> None:
    _check_deps()
    parser = argparse.ArgumentParser(
        prog="doppler-specan",
        description="Live spectrum analyzer — doppler NCO + FFT",
    )
    parser.add_argument(
        "--port", type=int, default=8765,
        help="HTTP/WebSocket port (default: 8765)",
    )
    parser.add_argument(
        "--host", default="127.0.0.1",
        help="Bind address (default: 127.0.0.1)",
    )
    parser.add_argument(
        "--fft-size", type=int, default=512,
        help="FFT size, must be power of two (default: 512)",
    )
    parser.add_argument(
        "--no-browser", action="store_true",
        help="Do not open a browser window automatically",
    )
    args = parser.parse_args()

    if args.fft_size & (args.fft_size - 1) != 0:
        print("error: --fft-size must be a power of two", file=sys.stderr)
        sys.exit(1)

    # Apply initial FFT size to shared state before starting server
    from doppler.specan.server import _state  # noqa: PLC0415
    _state.fft_size = args.fft_size

    from doppler.specan.server import main as serve  # noqa: PLC0415
    serve(host=args.host, port=args.port, open_browser=not args.no_browser)


if __name__ == "__main__":
    main()
