"""
python -m doppler.polyphase  — polyphase bank generator CLI.

Examples
--------
# Default: 4096 phases, 19 taps, Kaiser 60 dB → stdout C header
python -m doppler.polyphase

# Save as C header
python -m doppler.polyphase --out bank.h

# Save as NumPy array
python -m doppler.polyphase --out bank.npy

# Least-squares design
python -m doppler.polyphase --method firls --out bank.h

# Custom parameters
python -m doppler.polyphase --phases 1024 --taps 31 --attenuation 80
"""

from __future__ import annotations

import argparse
import sys

from ._polyphase import design_bank, to_c_header, to_npy


def main() -> None:
    p = argparse.ArgumentParser(
        prog="python -m doppler.polyphase",
        description="Design a polyphase FIR filter bank.",
    )
    p.add_argument(
        "--phases", type=int, default=4096, metavar="N",
        help="Number of polyphase branches (default: 4096)",
    )
    p.add_argument(
        "--taps", type=int, default=19, metavar="N",
        help="Taps per branch (default: 19)",
    )
    p.add_argument(
        "--bands", type=float, nargs="+",
        default=[0.0, 0.4, 0.6, 1.0], metavar="F",
        help="Band edges normalised to [0,1] (default: 0.0 0.4 0.6 1.0)",
    )
    p.add_argument(
        "--amps", type=float, nargs="+",
        default=[1.0, 1.0, 0.0, 0.0], metavar="A",
        help="Amplitude at each band edge (default: 1 1 0 0)",
    )
    p.add_argument(
        "--attenuation", type=float, default=60.0, metavar="dB",
        help="Stopband attenuation in dB (default: 60.0)",
    )
    p.add_argument(
        "--method", choices=["kaiser", "firls"], default="kaiser",
        help="Design method: kaiser (default) or firls (needs SciPy)",
    )
    p.add_argument(
        "--out", metavar="FILE",
        help=(
            "Output file.  Extension determines format: "
            ".h → C header, .npy → NumPy array.  "
            "Omit to print the C header to stdout."
        ),
    )
    args = p.parse_args()

    if len(args.bands) != len(args.amps):
        p.error("--bands and --amps must have the same number of values")
    if len(args.bands) % 2 != 0:
        p.error("--bands must have an even number of values")

    bank = design_bank(
        num_phases=args.phases,
        num_taps=args.taps,
        bands=args.bands,
        amps=args.amps,
        attenuation_db=args.attenuation,
        method=args.method,
    )

    if args.out is None:
        text = to_c_header(
            bank, method=args.method,
            attenuation_db=args.attenuation,
        )
        sys.stdout.write(text)
        return

    out = args.out
    if out.endswith(".npy"):
        to_npy(bank, out)
        print(f"Saved NumPy bank ({args.phases}×{args.taps}) → {out}")
    else:
        to_c_header(
            bank, path=out,
            method=args.method,
            attenuation_db=args.attenuation,
        )
        print(f"Saved C header ({args.phases}×{args.taps}) → {out}")


if __name__ == "__main__":
    main()
