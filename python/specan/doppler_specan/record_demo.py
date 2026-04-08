"""
doppler_specan.record_demo — generate a pre-recorded demo trace.

Runs the real C DSP pipeline (DemoSource → SpecanEngine) for a fixed
number of frames and writes the result as a JSON array to stdout or a
file.  The output is consumed by the static docs/specan demo player.

Usage
-----
    python -m doppler_specan.record_demo                 # stdout
    python -m doppler_specan.record_demo -o frames.json  # file
    python -m doppler_specan.record_demo --frames 120 --fft-size 512
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _next_power_of_two(n: int) -> int:
    p = 1
    while p < n:
        p <<= 1
    return p


def record(
    n_frames: int = 120,
    fft_size: int = 512,
    sample_rate: float = 2.048e6,
    center_freq: float = 0.0,
    tone_freq: float = 100e3,
    tone_power: float = -20.0,
    noise_floor: float = -90.0,
) -> list[dict]:
    """
    Run the DSP pipeline and return *n_frames* spectrum frames.

    Parameters
    ----------
    n_frames : int
        Number of frames to capture.
    fft_size : int
        FFT size (forced via rbw calculation).
    sample_rate : float
        DemoSource sample rate in Hz.
    center_freq : float
        Center frequency in Hz (metadata).
    tone_freq : float
        Tone offset from DC in Hz.
    tone_power : float
        Tone power in dBm.
    noise_floor : float
        Noise floor in dBm.
    Returns
    -------
    list of dict
        Each dict has keys: db, fft_size, fs_out, center_freq, span, rbw.
    """
    from doppler_specan.config import SpecanConfig, DemoConfig
    from doppler_specan.engine import SpecanEngine
    from doppler_specan.source import DemoSource

    # Build config — force span so fft_size comes out exactly right.
    # With fs_out = span / 0.8 and fft_size = nextpow2(fs_out / rbw):
    # pick rbw = fs_out / fft_size → always lands on fft_size exactly.
    span = sample_rate * 0.8  # full-bandwidth span
    fs_out = span / 0.8  # = sample_rate
    rbw = fs_out / fft_size  # forces the desired FFT size

    cfg = SpecanConfig(
        source="demo",
        fs=sample_rate,
        center=center_freq,
        span=span,
        rbw=rbw,
        demo=DemoConfig(
            tone_freq=tone_freq,
            tone_power=tone_power,
            noise_floor=noise_floor,
        ),
    )

    source = DemoSource(
        sample_rate=sample_rate,
        center_freq=center_freq,
        tone_freq=tone_freq,
        tone_power=tone_power,
        noise_floor=noise_floor,
    )
    engine = SpecanEngine(cfg)

    source.set_fft_size(fft_size)

    frames: list[dict] = []
    block = max(fft_size * 4, 4096)

    while len(frames) < n_frames:
        iq, fs, cf = source.read(block)
        frame = engine.process(iq, fs, cf)
        if frame is None:
            continue
        # Notify source of actual FFT size on first frame
        if len(frames) == 0:
            source.set_fft_size(frame.fft_size)
        frames.append(
            {
                "db": [round(v, 1) for v in frame.db],
                "fft_size": frame.fft_size,
                "fs_out": frame.fs_out,
                "center_freq": frame.center_freq,
                "span": frame.span,
                "rbw": round(frame.rbw, 2),
            }
        )

    source.close()
    engine.close()
    return frames


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "-o", "--output", metavar="FILE", help="output file (default: stdout)"
    )
    ap.add_argument(
        "--frames", type=int, default=120, help="number of frames (default: 120)"
    )
    ap.add_argument("--fft-size", type=int, default=512, help="FFT size (default: 512)")
    ap.add_argument(
        "--fs", type=float, default=2.048e6, help="sample rate Hz (default: 2.048e6)"
    )
    ap.add_argument("--center", type=float, default=0.0)
    ap.add_argument("--tone-freq", type=float, default=100e3)
    ap.add_argument("--tone-power", type=float, default=-20.0)
    ap.add_argument("--noise-floor", type=float, default=-90.0)
    args = ap.parse_args()

    print("Recording demo frames...", file=sys.stderr)
    frames = record(
        n_frames=args.frames,
        fft_size=args.fft_size,
        sample_rate=args.fs,
        center_freq=args.center,
        tone_freq=args.tone_freq,
        tone_power=args.tone_power,
        noise_floor=args.noise_floor,
    )
    print(
        f"Captured {len(frames)} frames, "
        f"FFT={frames[0]['fft_size']}, "
        f"span={frames[0]['span'] / 1e3:.1f} kHz",
        file=sys.stderr,
    )

    blob = json.dumps(frames, separators=(",", ":"))

    if args.output:
        Path(args.output).write_text(blob)
        print(f"Written to {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(blob)


if __name__ == "__main__":
    main()
