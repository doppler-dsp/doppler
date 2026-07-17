"""Long-run divergence stress test for `SimpleAsyncDespreader`.

Reproduces the scenario that broke both `native/src/dll/dll_core.c`'s
`segments > 1` path and an earlier from-scratch Python reimplementation
of the design doc's pseudocode: SF=1023, sps=2 (2046 samples/epoch),
`EPOCHS_PER_SYMBOL = 10/9` (non-integer -- the data-symbol clock
continuously slides through the code-epoch grid), asynchronous BPSK
data, several thousand epochs, with and without a residual carrier and
AWGN. Both prior implementations showed `last_error` creep steadily in
magnitude over ~1000-2000 epochs, then suddenly saturate the
discriminator clamp and the despread output amplitude blow up by an
order of magnitude -- and never recover, even under ZERO noise.

Run: `python validate_stress.py` (needs numpy only; no doppler import).
Prints one summary line per configuration; a run is "clean" when
`code_rate` stays at 1.0 and no output sample exceeds ~2x the nominal
amplitude.
"""
from __future__ import annotations

import numpy as np

from despreader import SimpleAsyncDespreader

CHIP_RATE = 2.046e6
SF = 1023
SPS = 2
TE = SF * SPS
DATA_RATE = 1800.0
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)  # 1.11111
F0 = 1e-4  # residual carrier, cycles/sample


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


def run_one(c, rx, windows, bn):
    n_epochs = len(rx) // TE
    rx = rx[: n_epochs * TE]
    d = SimpleAsyncDespreader(c, SPS, bn=bn, zeta=0.707, windows=windows)
    out = d.run(rx)
    mag = np.abs(out)
    return d, mag


def main():
    c = code(11)

    print("--- async data + residual carrier, noiseless, 12000 symbols ---")
    rx, _, _ = signal(c, 12000, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, None, 5)
    for windows in (6, 11, 22):
        for bn in (0.002, 0.01, 0.02):
            d, mag = run_one(c, rx, windows, bn)
            n_spike = int((mag > 5).sum())
            status = "CLEAN" if n_spike == 0 else "DIVERGED"
            print(
                f"  windows={windows:2d} bn={bn:.4f}  "
                f"code_rate={d.code_rate:.6f} last_error={d.last_error:+.4f}  "
                f"mean={mag.mean():.3f} max={mag.max():.3f}  "
                f"n_spike={n_spike}/{len(mag)}  [{status}]"
            )

    print("--- same, with -8 dB additive noise ---")
    rx2, _, _ = signal(c, 12000, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, -8, 5)
    for windows in (6, 11):
        for bn in (0.002, 0.02):
            d, mag = run_one(c, rx2, windows, bn)
            n_spike = int((mag > 5).sum())
            status = "CLEAN" if n_spike == 0 else "DIVERGED"
            print(
                f"  windows={windows:2d} bn={bn:.4f}  "
                f"code_rate={d.code_rate:.6f} last_error={d.last_error:+.4f}  "
                f"mean={mag.mean():.3f} max={mag.max():.3f}  "
                f"n_spike={n_spike}/{len(mag)}  [{status}]"
            )


if __name__ == "__main__":
    main()
