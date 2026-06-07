"""doppler — wavegen: Synth command-line interface.

Scaffolded by just-makeit.  Re-running `just-makeit app` overwrites this file;
edit for custom logic.

Install:  pip install -e .
Run:      wavegen --help
"""

import argparse
import sys

import numpy as np

from . import Synth


def _make_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="wavegen",
        description="doppler: Synth-powered stream tool.",
    )
    p.add_argument(
        "--input", "-i", default=None,
        help="input file (default: stdin)",
    )
    p.add_argument(
        "--output", "-o", default=None,
        help="output file (default: stdout)",
    )
    p.add_argument(
        "--type", choices=['tone', 'noise', 'pn', 'bpsk', 'qpsk'], default='tone',
        help="type (default: tone)",
    )
    p.add_argument(
        "--fs", type=float, default=1000000.0,
        help="fs (default: 1000000.0)",
    )
    p.add_argument(
        "--freq", type=float, default=0.0,
        help="freq (default: 0.0)",
    )
    p.add_argument(
        "--snr", type=float, default=100.0,
        help="snr (default: 100.0)",
    )
    p.add_argument(
        "--snr_mode", choices=['auto', 'fs', 'ebno', 'esno'], default='auto',
        help="snr_mode (default: auto)",
    )
    p.add_argument(
        "--seed", type=int, default=1,
        help="seed (default: 1)",
    )
    p.add_argument(
        "--sps", type=int, default=8,
        help="sps (default: 8)",
    )
    p.add_argument(
        "--pn_length", type=int, default=7,
        help="pn_length (default: 7)",
    )
    p.add_argument(
        "--pn_poly", type=int, default=0,
        help="pn_poly (default: 0)",
    )
    p.add_argument(
        "--count", type=int, default=1024,
        help="number of samples to generate",
    )
    p.add_argument(
        "--sample_type", choices=['cf32', 'cf64', 'ci32', 'ci16', 'ci8'], default='cf32',
        help="output wire sample type",
    )
    return p


def main() -> None:
    args = _make_parser().parse_args()
    obj = Synth(type=args.type, fs=args.fs, freq=args.freq, snr=args.snr, snr_mode=args.snr_mode, seed=args.seed, sps=args.sps, pn_length=args.pn_length, pn_poly=args.pn_poly)
    out = np.asarray(
        obj.steps(args.count), dtype=np.complex64
    )
    _st = args.sample_type
    if _st == "cf32":
        _buf = out.astype(np.complex64).tobytes()
    elif _st == "cf64":
        _buf = out.astype(np.complex128).tobytes()
    else:
        _iq = np.empty(out.size * 2, dtype=np.float64)
        _iq[0::2] = out.real
        _iq[1::2] = out.imag
        _iq = np.clip(_iq, -1.0, 1.0)
        _scale = {"ci32": 2147483647.0, "ci16": 32767.0,
                  "ci8": 127.0}[_st]
        _dt = {"ci32": np.int32, "ci16": np.int16,
               "ci8": np.int8}[_st]
        _buf = (_iq * _scale).astype(_dt).tobytes()
    if args.output:
        with open(args.output, "wb") as _f:
            _f.write(_buf)
    else:
        sys.stdout.buffer.write(_buf)


if __name__ == "__main__":
    main()
