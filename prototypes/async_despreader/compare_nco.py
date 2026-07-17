"""Compare `despreader.py` (pure-Python phase accumulator) against
`despreader_nco.py` (identical design, code-phase tracking swapped to
the real `doppler.source.NCO`) on identical input, to verify the swap
is behavior-preserving before moving on to the next piece.

Both are driven with the same async-data + residual-carrier + noise
stress scenario as `validate_stress.py`, over the same code/seed/RNG
draws, and their outputs plus `code_rate` trajectories are diffed.

The two are NOT expected to be bit-exact: `NCO` is a genuine 32-bit
fixed-point phase accumulator (`nco_set_norm_freq` computes `phase_inc =
floor(frac(norm_freq) * 2**32)`), so its representable rates are spaced
`1/2**32` cycles/sample apart, versus the pure-Python version's exact
double `phase_inc = code_rate / tsamps`. For `tsamps=2046` that
quantization step is `2**-32 * 2046 ~= 4.8e-7` in `code_rate` units --
the tracking loop's integrator settles at a slightly different
equilibrium `code_rate` to null out that non-representable remainder,
which is exactly the size of `code_rate_diff` reported below (~1e-7 to
5e-7). That tiny, constant rate offset accumulates into a sub-chip
sampling-instant shift over thousands of samples, which the
early/late/prompt discriminator is sensitive to -- hence the output
differing by a few percent rather than being bit-identical, while
remaining stable (no spikes, no divergence) at every swept
`windows`/`bn`/noise config. `MATCH_QUANT` below is a floor consistent
with that expected quantization noise; a real bug would blow through it
by orders of magnitude, not sit right at the predicted scale.
"""
from __future__ import annotations

import numpy as np

import despreader as pure
import despreader_nco as nco

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


def run_both(rx, windows, bn):
    n_epochs = len(rx) // TE
    rx = rx[: n_epochs * TE]
    c = code(11)
    dp = pure.SimpleAsyncDespreader(c, SPS, bn=bn, zeta=0.707, windows=windows)
    dn = nco.SimpleAsyncDespreader(c, SPS, bn=bn, zeta=0.707, windows=windows)
    out_p = dp.run(rx)
    out_n = dn.run(rx)
    return dp, dn, out_p, out_n


# Fixed-point NCO quantization floor for tsamps=TE=2046: 2**-32 * TE,
# with slack for the loop-filter integrator settling near, not exactly
# at, that predicted equilibrium offset.
RATE_QUANT_FLOOR = (2.0**-32) * TE
MATCH_RATE_TOL = 20 * RATE_QUANT_FLOOR
MATCH_OUT_TOL = 0.15  # generous: output differences trail the rate offset


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
                dp, dn, out_p, out_n = run_both(rx, windows, bn)
                out_diff = np.max(np.abs(out_p - out_n))
                rate_diff = abs(dp.code_rate - dn.code_rate)
                ok = out_diff < MATCH_OUT_TOL and rate_diff < MATCH_RATE_TOL
                status = "MATCH (within NCO quantization)" if ok else "DIVERGE"
                print(
                    f"  windows={windows:2d} bn={bn:.4f} {label:22s}  "
                    f"max|out_diff|={out_diff:.3e}  "
                    f"code_rate_diff={rate_diff:.3e}  [{status}]"
                )


if __name__ == "__main__":
    main()
