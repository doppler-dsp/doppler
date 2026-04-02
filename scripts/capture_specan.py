#!/usr/bin/env python3
"""
Capture doppler-specan WebSocket frames to a JSON file.

Requires a running specan instance:

    doppler-specan --web --source demo --no-browser

Then run:

    uv run --with websockets python scripts/capture_specan.py \\
        --frames 150 --chirp --out docs/specan/chirp_frames.json

Options
-------
--url URL       WebSocket endpoint (default: ws://127.0.0.1:8765/ws)
--frames N      Number of frames to capture (default: 150)
--chirp         Enable chirp sweep before capturing
--warmup N      Frames to discard before recording (default: 15)
--out PATH      Output JSON file (default: docs/specan/chirp_frames.json)
--tune JSON     Send a tune command before capturing, e.g.
                '{"ref_level": -10}' (optional)
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path


async def _capture(
    url: str,
    n_frames: int,
    enable_chirp: bool,
    warmup: int,
    out: Path,
    tune_cmd: dict | None,
) -> None:
    try:
        import websockets  # type: ignore
    except ImportError:
        print(
            "websockets not found — run with:\n"
            "  uv run --with websockets python scripts/capture_specan.py",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"Connecting to {url} …")

    async with websockets.connect(url) as ws:
        if enable_chirp:
            await ws.send(json.dumps({"chirp": True}))
            print("Chirp enabled.")

        if tune_cmd:
            await ws.send(json.dumps(tune_cmd))

        if warmup > 0:
            print(f"Warming up ({warmup} frames) …")
            for _ in range(warmup):
                await ws.recv()

        print(f"Capturing {n_frames} frames …")
        frames: list[dict] = []
        for i in range(n_frames):
            raw = await ws.recv()
            msg = json.loads(raw)
            frames.append(
                {
                    "fft_size": msg["fft_size"],
                    "fs_out": msg["fs_out"],
                    "center_freq": msg["center_freq"],
                    "span": msg["span"],
                    "rbw": msg["rbw"],
                    # Round to 1 dp to keep file size manageable
                    "db": [round(v, 1) for v in msg["db"]],
                }
            )
            if (i + 1) % 30 == 0 or i + 1 == n_frames:
                print(f"  {i + 1} / {n_frames}")

        if enable_chirp:
            await ws.send(json.dumps({"chirp": False}))

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(frames, separators=(",", ":")))

    kb = out.stat().st_size / 1024
    print(f"\nSaved {len(frames)} frames → {out}  ({kb:.0f} KB)")


def main() -> None:
    p = argparse.ArgumentParser(
        description="Capture doppler-specan WebSocket frames to JSON.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--url", default="ws://127.0.0.1:8765/ws")
    p.add_argument("--frames", type=int, default=150, help="Frames to record")
    p.add_argument(
        "--chirp",
        action="store_true",
        default=False,
        help="Enable chirp sweep before capturing",
    )
    p.add_argument(
        "--warmup", type=int, default=15, help="Frames to discard before recording"
    )
    p.add_argument(
        "--out", default="docs/specan/chirp_frames.json", help="Output JSON path"
    )
    p.add_argument(
        "--tune", default=None, help="JSON tune command to send before capture"
    )
    args = p.parse_args()

    tune_cmd = json.loads(args.tune) if args.tune else None

    asyncio.run(
        _capture(
            url=args.url,
            n_frames=args.frames,
            enable_chirp=args.chirp,
            warmup=args.warmup,
            out=Path(args.out),
            tune_cmd=tune_cmd,
        )
    )


if __name__ == "__main__":
    main()
