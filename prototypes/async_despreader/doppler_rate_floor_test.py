"""Extends `doppler_rate_test.py`'s static-batch vs. FLL-assist
Doppler-RATE comparison down to the REAL SPEC worst-case floor
(Es/N0 = 3 dB, `SPEC.md` line 12), which that script never tests (it
only ever runs at a comfortable ES_N0_DB = 10.0). Two things change
relative to it, on purpose:

1. **Loop bandwidths pinned at the SPEC-derived bound.** `SPEC.md`
   derives "`bn <= 0.01` for every tracking loop, sized against the
   worst-case Es/N0 floor" (not a comfortable operating point). This
   script passes `bn=0.01` AND an explicit `bn_car=0.01` to every
   `CoupledAsyncDespreader` construction -- NOT
   `doppler_rate_test.py`'s own `BN=0.002`/its 10x-default `bn_car`
   (`0.02`), which were calibrated for the easier 10 dB point this
   script does not touch or modify.
2. **Multiple seeds per point.** `characterize_snr.py` already found
   the underlying squaring+FFT estimator (`freq_refine.
   estimate_residual_freq`) gets UNRELIABLE -- gross wrong-peak
   errors, not just added jitter -- as Es/N0 drops toward this floor.
   FLL-assist's whole mechanism is running that SAME estimator
   repeatedly over SHORT (300-epoch) blocks; if it is unreliable at
   short-block sizes near 3 dB, FLL-assist could inject WRONG
   corrections rather than helping. `doppler_rate_test.py` runs a
   single seed=42 per point, which cannot distinguish "reliably good"
   from "got lucky once" -- this script runs N_SEEDS independent
   noise realizations per (Es/N0, rate) point and reports a
   success/gross-error FRACTION.

Reuses `doppler_rate_test.py`'s `make_ramp_signal` (bn-independent,
imported unchanged) and mirrors its `one_estimate`/`run_static_batch`/
`run_fll_assist`/`run_integrated` -- same primitives
(`CoupledAsyncDespreader`, `freq_refine.estimate_residual_freq`), no
new estimator, only the bn/bn_car/seed-count changes above. Does NOT
modify `doppler_rate_test.py` itself -- its own `ES_N0_DB = 10.0`
default and existing functions are untouched; `characterize_snr.py`'s
calibrated numbers are likewise untouched.

Run: `python doppler_rate_floor_test.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import CoupledAsyncDespreader
from freq_refine import estimate_residual_freq
from characterize_snr import (
    SAMPLE_RATE_HZ, N_FFT, ZERO_PAD, WINDOWS, es_n0_to_chip_snr_db,
)
from signal_gen import SF, SPS, code
from doppler_rate_test import (
    make_ramp_signal, FLL_BLOCK_EPOCHS, N_FLL_BLOCKS, STATIC_COLLECT_EPOCHS,
)

ES_N0_DB_LIST = [3.0, 5.0, 10.0]  # the real SPEC floor, one context
# point above it, and doppler_rate_test.py's own 10 dB point for a
# direct before/after trend comparison at the identical rates.
RATE_LIST_HZ_PER_S = [0.0, 1000.0, 5000.0]  # SPEC's own +/-5 kHz/s
N_SEEDS = 8  # independent noise realizations per (Es/N0, rate) point

BN = 0.01      # SPEC-derived bn<=0.01 floor bound -- code loop
BN_CAR = 0.01  # ditto, carrier loop -- explicit, NOT the 10x default

TE = SF * SPS
# The FFT peak search's own coarse (un-padded, pre-interpolation) bin
# width at the FLL-assist block geometry actually used here (Phase
# 1c's n_fft=64 over a `windows`-chunk-rate signal), same formula
# `characterize_snr.characterize_estimator` uses for its "found the
# right peak" gate. A wrong-peak error lands many bins away, not a
# fraction of one -- so a per-block discriminator error beyond a
# handful of bins is a GROSS error, not ordinary jitter.
_STEP_SIZE = TE // WINDOWS
_WINDOW_RATE_HZ = SAMPLE_RATE_HZ / _STEP_SIZE
COARSE_BIN_HZ = _WINDOW_RATE_HZ / N_FFT / 2.0
GROSS_ERROR_HZ = 5.0 * COARSE_BIN_HZ  # a few coarse bins away = gross


def one_estimate_floor(c, x_block, seeded_norm_freq):
    """`doppler_rate_test.one_estimate`, with bn/bn_car pinned to the
    SPEC floor bound instead of characterize_snr's 10-dB-calibrated
    BN."""
    d0 = CoupledAsyncDespreader(
        c, SPS, bn=BN, bn_car=BN_CAR, zeta=0.707, windows=WINDOWS,
        init_car_norm_freq=seeded_norm_freq, aid_code=False,
        sample_rate_hz=SAMPLE_RATE_HZ, freeze_carrier=True,
    )
    out0 = d0.run(x_block)
    window_rate_hz = SAMPLE_RATE_HZ / d0.step_size
    return estimate_residual_freq(
        out0, window_rate_hz, n_fft=N_FFT, zero_pad=ZERO_PAD, interp=True,
    )


def run_static_batch_floor(c, chip_snr_db, rate_hz_per_s, seed):
    """`doppler_rate_test.run_static_batch`, bn/bn_car pinned to the
    floor bound."""
    x = make_ramp_signal(
        c, chip_snr_db, 0.0, rate_hz_per_s, seed, STATIC_COLLECT_EPOCHS,
    )
    x_block = x[:STATIC_COLLECT_EPOCHS * TE]
    est_hz = one_estimate_floor(c, x_block, 0.0)
    duration_s = STATIC_COLLECT_EPOCHS * TE / SAMPLE_RATE_HZ
    true_at_end = rate_hz_per_s * duration_s
    return {
        "est_hz": est_hz,
        "true_at_end_hz": true_at_end,
        "err_vs_end": abs(est_hz - true_at_end),
    }


def run_fll_assist_floor(c, chip_snr_db, rate_hz_per_s, seed):
    """`doppler_rate_test.run_fll_assist`, bn/bn_car pinned to the
    floor bound, PLUS a per-block discriminator-error trace: how far
    off was THIS block's estimate from the true mean frequency change
    over just that block (isolated from any accumulated history), vs.
    the running tracked error (which compounds prior blocks' errors).
    Isolating per-block error is what lets us tell "one bad correction
    knocked us off track" apart from "we're structurally lagging the
    ramp"."""
    total_epochs = FLL_BLOCK_EPOCHS * N_FLL_BLOCKS
    x = make_ramp_signal(
        c, chip_snr_db, 0.0, rate_hz_per_s, seed, total_epochs,
    )
    t_block = FLL_BLOCK_EPOCHS * TE / SAMPLE_RATE_HZ
    tracked_hz = 0.0
    errs = []
    disc_errs = []
    for block in range(N_FLL_BLOCKS):
        pos = block * FLL_BLOCK_EPOCHS * TE
        x_block = x[pos:pos + FLL_BLOCK_EPOCHS * TE]
        seeded_norm_freq = tracked_hz / SAMPLE_RATE_HZ
        residual_hz = one_estimate_floor(c, x_block, seeded_norm_freq)

        t0, t1 = block * t_block, (block + 1) * t_block
        true_mean_over_block = rate_hz_per_s * 0.5 * (t0 + t1)
        expected_residual = true_mean_over_block - tracked_hz
        disc_errs.append(abs(residual_hz - expected_residual))

        tracked_hz += residual_hz
        true_now = rate_hz_per_s * t1
        errs.append(abs(tracked_hz - true_now))
    return {
        "final_tracked_hz": tracked_hz,
        "true_at_end_hz": rate_hz_per_s * total_epochs * TE / SAMPLE_RATE_HZ,
        "err_vs_end": errs[-1],
        "err_history": errs,
        "disc_err_history": disc_errs,
        "max_disc_err": max(disc_errs),
        "gross_error": max(disc_errs) > GROSS_ERROR_HZ,
    }


def run_integrated_floor(c, chip_snr_db, rate_hz_per_s, seed, fll_block_epochs):
    """`doppler_rate_test.run_integrated`, bn/bn_car pinned to the
    floor bound."""
    total_epochs = FLL_BLOCK_EPOCHS * N_FLL_BLOCKS
    x = make_ramp_signal(c, chip_snr_db, 0.0, rate_hz_per_s, seed, total_epochs)
    x = x[:total_epochs * TE]
    d = CoupledAsyncDespreader(
        c, SPS, bn=BN, bn_car=BN_CAR, zeta=0.707, windows=WINDOWS,
        init_car_norm_freq=0.0, aid_code=True,
        sample_rate_hz=SAMPLE_RATE_HZ, fll_block_epochs=fll_block_epochs,
    )
    d.run(x)
    duration_s = total_epochs * TE / SAMPLE_RATE_HZ
    true_at_end = rate_hz_per_s * duration_s
    tracked_hz = d.car_norm_freq * SAMPLE_RATE_HZ
    return {
        "tracked_hz": tracked_hz,
        "true_at_end_hz": true_at_end,
        "err": abs(tracked_hz - true_at_end),
        "code_rate": d.code_rate,
        "fll_corrections": d.fll_corrections,
    }


def _stats(vals):
    a = np.asarray(vals, dtype=float)
    return float(np.mean(a)), float(np.median(a)), float(np.max(a))


def main():
    c = code(11)
    print(
        f"=== Doppler-RATE FLOOR test: bn={BN}, bn_car={BN_CAR} "
        f"(SPEC bn<=0.01 rule), {N_SEEDS} seeds/point ==="
    )
    print(
        f"coarse FFT bin width ~{COARSE_BIN_HZ:.1f} Hz; per-block "
        f"discriminator error beyond {GROSS_ERROR_HZ:.1f} Hz "
        f"(~5 bins) flagged as a GROSS wrong-peak error, not jitter."
    )

    for es_n0_db in ES_N0_DB_LIST:
        chip_snr_db = es_n0_to_chip_snr_db(es_n0_db)
        print(f"\n########## Es/N0 = {es_n0_db:.1f} dB ##########")
        for rate in RATE_LIST_HZ_PER_S:
            static_errs, fll_errs, fll_max_disc = [], [], []
            n_gross = 0
            off_errs, on_errs = [], []
            for i in range(N_SEEDS):
                seed = 2000 + i
                stat = run_static_batch_floor(c, chip_snr_db, rate, seed)
                static_errs.append(stat["err_vs_end"])

                fll = run_fll_assist_floor(c, chip_snr_db, rate, seed)
                fll_errs.append(fll["err_vs_end"])
                fll_max_disc.append(fll["max_disc_err"])
                n_gross += int(fll["gross_error"])

                off = run_integrated_floor(
                    c, chip_snr_db, rate, seed, fll_block_epochs=None,
                )
                off_errs.append(off["err"])
                on = run_integrated_floor(
                    c, chip_snr_db, rate, seed,
                    fll_block_epochs=FLL_BLOCK_EPOCHS,
                )
                on_errs.append(on["err"])

            n_worse = sum(
                1 for j in range(N_SEEDS) if on_errs[j] > off_errs[j]
            )
            s_mean, s_med, s_max = _stats(static_errs)
            f_mean, f_med, f_max = _stats(fll_errs)
            off_mean, off_med, off_max = _stats(off_errs)
            on_mean, on_med, on_max = _stats(on_errs)

            print(f"\n--- rate={rate:.0f} Hz/s ---")
            print(
                f"  static batch      err_vs_end (Hz): "
                f"mean={s_mean:8.1f}  median={s_med:8.1f}  max={s_max:8.1f}"
            )
            print(
                f"  FLL-assist (iso)  err_vs_end (Hz): "
                f"mean={f_mean:8.1f}  median={f_med:8.1f}  max={f_max:8.1f}  "
                f"|  max per-block discriminator err across seeds: "
                f"{max(fll_max_disc):8.1f} Hz  |  gross-error seeds: "
                f"{n_gross}/{N_SEEDS}"
            )
            print(
                f"  integrated FLL=off  err (Hz): "
                f"mean={off_mean:8.1f}  median={off_med:8.1f}  "
                f"max={off_max:8.1f}"
            )
            print(
                f"  integrated FLL=on   err (Hz): "
                f"mean={on_mean:8.1f}  median={on_med:8.1f}  "
                f"max={on_max:8.1f}  |  seeds where FLL=on is WORSE "
                f"than FLL=off: {n_worse}/{N_SEEDS}"
            )


if __name__ == "__main__":
    main()
