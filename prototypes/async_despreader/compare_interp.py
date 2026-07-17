"""Compare `despreader_lf.py` (NCO + real LoopFilter already swapped)
against `despreader_interp.py` (same, plus replica generation swapped
to the real `doppler.interp.InterpolatedTable`), on identical input.

Unlike `compare_lf.py`, this IS expected to be a near-exact match:
`InterpolatedTable`'s periodic linear interpolation over a
2x-oversampled ±1-chip-sign table is mathematically identical to
`replica2x`'s own manual index/interpolation math (same double-
precision arithmetic, just computed in C instead of numpy) -- any
difference should be down to floating-point operation-order rounding,
not an algorithmic change.
"""
from __future__ import annotations

import numpy as np

import despreader_lf as prev
import despreader_interp as new

CHIP_RATE = 2.046e6
SF = 1023
SPS = 2
TE = SF * SPS
DATA_RATE = 1800.0
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)  # 1.11111
F0 = 1e-4


def code(seed=11):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def signal(c, nsym, epochs_per_symbol, phi, f0, snr_db, seed):
    rng = np.random.default_rng(seed)
    csign = np.where(c & 1, -1.0, 1.0)
    tsym = TE * epochs_per_symbol
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // SPS) % SF
    rx = (data[si] * csign[cph] * np.exp(2j * np.pi * f0 * idx)).astype(
        np.complex128
    )
    if snr_db is not None:
        p = np.sqrt(np.mean(np.abs(rx) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        rx = rx + (
            rng.normal(0, std / np.sqrt(2), n)
            + 1j * rng.normal(0, std / np.sqrt(2), n)
        )
    return rx, data, tsym


def run_one(mod, c, rx, windows, bn):
    n_epochs = len(rx) // TE
    rx = rx[: n_epochs * TE]
    d = mod.SimpleAsyncDespreader(c, SPS, bn=bn, zeta=0.707, windows=windows)
    out = d.run(rx)
    return d, out


MATCH_TOL = 1e-9  # floating-point-only difference expected


def main():
    c = code(11)
    scenarios = [
        ("noiseless, carrier", None),
        ("-8 dB noise, carrier", -8.0),
    ]
    for windows in (6, 11, 22):
        for bn in (0.002, 0.01, 0.02):
            for label, snr_db in scenarios:
                rx, _, _ = signal(
                    c, 3000, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, snr_db, 5
                )
                dp, out_p = run_one(prev, c, rx, windows, bn)
                dn, out_n = run_one(new, c, rx, windows, bn)
                out_diff = np.max(np.abs(out_p - out_n))
                rate_diff = abs(dp.code_rate - dn.code_rate)
                ok = out_diff < MATCH_TOL and rate_diff < MATCH_TOL
                status = "MATCH" if ok else "DIVERGE"
                print(
                    f"  windows={windows:2d} bn={bn:.4f} {label:22s}  "
                    f"max|out_diff|={out_diff:.3e}  "
                    f"code_rate_diff={rate_diff:.3e}  [{status}]"
                )


if __name__ == "__main__":
    main()
