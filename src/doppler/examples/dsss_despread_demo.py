"""dsss_despread_demo.py — DSSS BPSK acquisition + despreading, end to end.

Builds a realistic burst — a preamble of ``REPS`` periods of a long
acq code, then a DSSS-BPSK payload spread by a *distinct*, shorter data code —
adds AWGN at Es/N0 = 10 dB, and runs the full receiver:

  1. **Acquisition** — a 2-D (Doppler x code-phase) matched-filter search
     preamble against the acq code (coherently summing the REPS periods), using
     ``doppler.spectral.Corr``. The peak test statistic (peak / noise estimate)
     is gated against a threshold from ``doppler.detection.det_threshold``.
  2. **Despreading** — the peak (Doppler bin, code phase) seeds
     ``doppler.dsss.Despreader``; ``set_acq`` refines loops on the preamble,
     then the DLL + Costas track the payload and emit symbols.

Three views (saved to a PNG):
  * **Acquisition** — test statistic across code phase at the winning Doppler
    bin, with the detector threshold; the peak clears it (signal present).
  * **Loop stress vs time** — carrier-frequency estimate converging to truth,
    lock metric ramping (preamble shaded).
  * **Soft decisions vs time** — prompt symbol real part as dots.

Run:  python -m doppler.examples.dsss_despread_demo  [out.png]
"""

from __future__ import annotations

import math
import sys

import numpy as np

from doppler.detection import det_threshold
from doppler.dsss import Despreader
from doppler.spectral import Corr

ACQ_SF, DATA_SF, REPS, SPS = 512, 32, 5, 2
N_PAYLOAD = 220
F0 = 8e-5  # true residual carrier offset, cycles/sample
ESN0_DB = 10
PFA = 1e-4  # system false-alarm probability for the CFAR gate


def add_noise(rng, x, esn0_db):
    """AWGN at the given per-symbol Es/N0 (Es = DATA_SF*SPS for unit chips)."""
    sigma2 = (DATA_SF * SPS) / 10.0 ** (esn0_db / 10.0)
    noise = np.sqrt(sigma2 / 2.0) * (
        rng.standard_normal(len(x)) + 1j * rng.standard_normal(len(x))
    )
    return (x + noise).astype(np.complex64)


def acquire(rx, pre_len, asig):
    """2-D Doppler x code-phase search of the preamble against the acq code.

    Returns (surface, doppler_grid, peak_dop_idx, peak_lag, threshold, sigma).
    """
    period = ACQ_SF * SPS
    pre = rx[: REPS * period]
    n = np.arange(len(pre))
    # Doppler grid resolves to ~1/(REPS*period) so the REPS periods sum
    # coherently; span a few bins either side of zero.
    df = 1.0 / (2.0 * REPS * period)
    grid = np.arange(-8, 9) * df
    ref = np.repeat(asig, SPS).astype(np.complex64)  # one acq period
    surf = np.empty((len(grid), period), np.float32)
    with Corr(ref, dwell=1) as c:
        for i, f in enumerate(grid):
            drot = pre * np.exp(-2j * np.pi * f * n)
            coh = drot.reshape(REPS, period).sum(0).astype(np.complex64)
            surf[i] = np.abs(c.execute(coh))
    di, lag = np.unravel_index(surf.argmax(), surf.shape)
    # CFAR: noise from cells away from the peak; test stat = peak/noise.
    mask = np.ones_like(surf, bool)
    mask[max(0, di - 1) : di + 2, max(0, lag - SPS) : lag + SPS + 1] = False
    noise = float(np.median(surf[mask]))
    surf_ts = surf / noise
    npix = surf.size
    pfa_cell = 1.0 - (1.0 - PFA) ** (1.0 / npix)
    theta = det_threshold(pfa_cell) * math.sqrt(2.0 / math.pi)
    return surf_ts, grid, di, lag, theta


def run_despreader(rx, pre_len, acq_code, data_code, init_freq, init_chip):
    d = Despreader(
        data_code,
        sf=DATA_SF,
        sps=SPS,
        init_norm_freq=init_freq,
        init_chip_phase=init_chip,
    )
    d.set_acq(acq_code, REPS)
    freq, lock = [], []
    pre = rx[:pre_len]
    for i in range(0, len(pre), ACQ_SF * SPS):
        d.steps(pre[i : i + ACQ_SF * SPS])
        freq.append(d.norm_freq)
        lock.append(d.lock_metric)
    soft = []
    pay = rx[pre_len:]
    step = DATA_SF * SPS
    for i in range(0, len(pay) - step + 1, step):
        s = d.steps(pay[i : i + step])
        if len(s):
            soft.append(s[0])
        freq.append(d.norm_freq)
        lock.append(d.lock_metric)
    return np.array(freq), np.array(lock), np.array(soft)


def main(out_path="dsss_despread_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rng = np.random.default_rng(0)
    acq_code = rng.integers(0, 2, ACQ_SF).astype(np.uint8)
    data_code = rng.integers(0, 2, DATA_SF).astype(np.uint8)
    asig = np.where(acq_code & 1, -1.0, 1.0).astype(np.float32)
    dsig = np.where(data_code & 1, -1.0, 1.0).astype(np.float32)
    bits = rng.integers(0, 2, N_PAYLOAD).astype(np.uint8)
    syms = np.where(bits == 1, -1.0, 1.0).astype(np.float32)
    pre = np.concatenate([np.repeat(asig, SPS) for _ in range(REPS)])
    pay = np.concatenate([np.repeat(s * dsig, SPS) for s in syms])
    burst = np.concatenate([pre, pay]).astype(np.complex64)
    nn = np.arange(len(burst))
    burst = (burst * np.exp(2j * np.pi * F0 * nn)).astype(np.complex64)
    pre_len = len(pre)

    rx = add_noise(np.random.default_rng(7), burst, ESN0_DB)

    surf_ts, grid, di, lag, theta = acquire(rx, pre_len, asig)
    init_freq = float(grid[di])
    init_chip = (lag / SPS) % DATA_SF
    peak_ts = float(surf_ts[di, lag])

    freq, lock, soft = run_despreader(
        rx, pre_len, acq_code, data_code, init_freq, init_chip
    )

    fig, ax = plt.subplots(3, 1, figsize=(9, 8))
    fig.suptitle(
        f"DSSS acquisition + despreading — acq {ACQ_SF}x{REPS}, "
        f"data {DATA_SF} chips/sym, sps={SPS}, Es/N0={ESN0_DB} dB",
        fontsize=12,
    )

    # Acquisition: test statistic vs code phase at the winning Doppler bin
    a = ax[0]
    chips = np.arange(surf_ts.shape[1]) / SPS
    a.plot(chips, surf_ts[di], color="tab:blue", lw=0.9)
    a.axhline(
        theta,
        color="tab:red",
        ls="--",
        lw=1.2,
        label=f"detector threshold (Pfa={PFA:g})",
    )
    a.plot(
        lag / SPS,
        peak_ts,
        "o",
        color="tab:green",
        ms=8,
        label=f"peak test stat = {peak_ts:.1f}",
    )
    a.set_xlabel("code phase (chips)")
    a.set_ylabel("test statistic\n(peak / noise)")
    a.set_title(
        f"Acquisition — Doppler bin {di - len(grid) // 2:+d} "
        f"({init_freq:+.5f} cyc/sample)",
        fontsize=10,
    )
    a.legend(fontsize=8, loc="upper right")

    # ── Loop stress vs time ──
    b = ax[1]
    per = np.arange(len(freq)) - REPS
    b.axvspan(per[0], 0, color="0.9", label="preamble")
    b.axhline(F0, color="tab:green", ls="--", lw=1, label="true f0")
    b.plot(per, freq, color="tab:blue", lw=1.3, label="freq est")
    b.set_ylabel("freq (cyc/sample)")
    b.set_xlabel("code period (0 = payload start)")
    b.set_title("Loop stress vs time", fontsize=10)
    b2 = b.twinx()
    b2.plot(per, lock, color="tab:red", lw=1, alpha=0.7)
    b2.set_ylim(0, 1.05)
    b2.set_ylabel("lock", color="tab:red")
    b.legend(fontsize=8, loc="center right")

    # ── Soft decisions vs time (dots) ──
    c = ax[2]
    c.axhline(1, color="0.7", lw=0.8)
    c.axhline(-1, color="0.7", lw=0.8)
    c.scatter(
        np.arange(len(soft)),
        soft.real,
        s=10,
        c="tab:purple",
        alpha=0.6,
        edgecolors="none",
    )
    c.set_ylim(-2, 2)
    c.set_xlabel("payload symbol")
    c.set_ylabel("Re(symbol)")
    c.set_title("Soft decisions vs time", fontsize=10)

    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=120)
    print(
        f"wrote {out_path}  (peak test stat {peak_ts:.1f} vs threshold "
        f"{theta:.1f}; acq freq {init_freq:+.5f}, chip {init_chip:.2f})"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dsss_despread_demo.png")
