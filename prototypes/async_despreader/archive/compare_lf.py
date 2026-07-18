"""Compare `despreader_nco.py` (NCO already swapped, pure-Python
`LoopFilter`) against `despreader_lf.py` (same, plus the loop filter
swapped to the real `doppler.track.LoopFilter`), on identical input.

Unlike `compare_nco.py`, this is NOT expected to be a near-match: the
real `LoopFilter` is a structurally different filter (see
`despreader_lf.py`'s module docstring) with different gains for the
same `(bn, zeta)`, so the two will settle at different `code_rate`
trajectories and their outputs will differ by more than a quantization
floor. The actual question here is STABILITY, not numeric agreement:
does the real `LoopFilter`, driven with the validated scaling (full
`step()` output / `tsamps`), stay stable across the same long-run async
stress sweep that `validate_stress.py` already proved the pure-Python
version survives? If so, the eventual C fix is just correcting
`dll_update()`'s scaling around the EXISTING `loop_filter_core.h`
engine -- no new filter design needed in C.
"""
from __future__ import annotations

import numpy as np

import despreader_nco as prev
import despreader_lf as new

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


def gains(mod, bn, zeta=0.707):
    """kp/ki each filter algorithm derives for the same (bn, zeta), to
    report just how close (or not) the two are -- constructed the same
    way `SimpleAsyncDespreader.__init__` does (t=1.0 for the real one)."""
    if hasattr(mod, "LoopFilter"):
        lf = mod.LoopFilter(bn, zeta) if mod is prev else mod.LoopFilter(
            bn=bn, zeta=zeta, t=1.0
        )
        return lf.kp, lf.ki
    return None, None


def main():
    c = code(11)
    scenarios = [
        ("noiseless, carrier", None),
        ("-8 dB noise, carrier", -8.0),
    ]
    print("--- gains: pure-Python bilinear filter vs real Stephens & Thomas form ---")
    for bn in (0.002, 0.01, 0.02):
        kp_p, ki_p = gains(prev, bn)
        kp_n, ki_n = gains(new, bn)
        print(
            f"  bn={bn:.4f}  prev kp={kp_p:.6f} ki={ki_p:.8f}   "
            f"new kp={kp_n:.6f} ki={ki_n:.8f}   "
            f"(kp diff {abs(kp_p - kp_n):.2e}, ki diff {abs(ki_p - ki_n):.2e})"
        )

    print("--- stability sweep: 12000 symbols, same as validate_stress.py ---")
    for windows in (6, 11, 22):
        for bn in (0.002, 0.01, 0.02):
            for label, snr_db in scenarios:
                rx, _, _ = signal(
                    c, 12000, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, snr_db, 5
                )
                dp, out_p = run_one(prev, c, rx, windows, bn)
                dn, out_n = run_one(new, c, rx, windows, bn)
                mag_p, mag_n = np.abs(out_p), np.abs(out_n)
                spike_p = int((mag_p > 5).sum())
                spike_n = int((mag_n > 5).sum())
                status_p = "CLEAN" if spike_p == 0 else "DIVERGED"
                status_n = "CLEAN" if spike_n == 0 else "DIVERGED"
                print(
                    f"  windows={windows:2d} bn={bn:.4f} {label:22s}  "
                    f"prev(nco-only): code_rate={dp.code_rate:.6f} "
                    f"max={mag_p.max():.3f} [{status_p}]   "
                    f"new(+real LoopFilter): code_rate={dn.code_rate:.6f} "
                    f"max={mag_n.max():.3f} [{status_n}]"
                )


if __name__ == "__main__":
    main()
