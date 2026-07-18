"""Characterize Phase 1c's frequency-refinement bridge
(`freq_refine.py`) vs. Es/N0, per direct user request: "first
characterize vs Es/N0 2, 5, 10, 20, 50 dB ... you can also zero pad
and peak interpolate if needed."

Why this matters and isn't a formality: the bridge's first step is
SQUARING the despread output to remove BPSK data modulation before the
FFT peak search (see `freq_refine.py`'s own docstring). Squaring loss
is a well-known NONLINEAR effect
(`[[reference_squaring_loss_ebno_not_cno]]`) -- unlike a linear
discriminator, a squaring-based estimator's accuracy does not degrade
gracefully as Es/N0 drops; at some point the noise floor in the
squared spectrum starts winning the peak search outright (a GROSS
error -- wrong bin entirely), not just adding a little jitter around
the right one. `pullin_sweep.py`'s validation ran at a single fixed
-8 dB operating point; this script sweeps the actual axis that
controls whether squaring loss bites.

Two things are measured at each Es/N0, kept SEPARATE on purpose (do
not average across a mode transition and report one misleading
number):

1. **Raw estimator accuracy** (`characterize_estimator`): does the FFT
   peak search find the RIGHT bin? Reports, over N_TRIALS independent
   noise realizations, the FRACTION of trials that landed within one
   coarse (un-padded) FFT bin of truth (a "found the right peak" flag)
   separately from the RMS error AMONG those found-right trials (do
   zero-pad + parabolic interpolation actually sharpen it once the
   peak is right?). Run with and without the zero-pad/interpolation
   enhancement to show what it does and does not fix.
2. **End-to-end lock rate** (`characterize_lock`): does the full
   refine -> re-seed -> track chain (`pullin_sweep.py`'s own
   `run_refined_case` logic, reused here) still lock, at a couple of
   representative seed gaps (685 Hz = Acquisition's measured mean
   error, 1320 Hz = its measured worst case)?

Run: `python characterize_snr.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import CoupledAsyncDespreader
from freq_refine import estimate_residual_freq, refine_seed
from signal_gen import code, signal, CHIP_RATE, SF, SPS, TE, DATA_RATE

SAMPLE_RATE_HZ = CHIP_RATE * SPS
CARRIER_FREQ_HZ = 2.2e9
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)

# `validate_stress.signal()`'s own `snr_db` is a plain per-RAW-CHIP-SAMPLE
# power ratio (p^2/noise_power at the FULL SAMPLE_RATE_HZ bandwidth) --
# NOT Es/N0. The project's own established relationship
# (`src/doppler/examples/dsss_acq_async_data_demo.py`: "Es/N0 = C/N0 -
# 10*log10(Rs)", plus `Acquisition`'s own `amp_snr = sqrt(10**(cn0_dbhz/10)
# / FS)` per-sample convention, which is algebraically the SAME quantity
# as `signal()`'s `snr_db` when the noiseless signal has unit power) gives
# the conversion: `snr_db = Es/N0 - 10*log10(sample_rate / symbol_rate)`.
# Getting this wrong (i.e. treating Es/N0 as if it were `snr_db` directly)
# silently tests an enormously higher effective SNR than intended --
# caught in an early draft of this script, where every Es/N0 from 2-50 dB
# showed 100% success because "2 dB" was landing ~30+ dB too high once fed
# straight into `signal()`. Convert BEFORE generating, every time.
_SAMPLE_TO_SYMBOL_RATE_DB = 10.0 * np.log10(SAMPLE_RATE_HZ / DATA_RATE)


def es_n0_to_chip_snr_db(es_n0_db):
    return es_n0_db - _SAMPLE_TO_SYMBOL_RATE_DB


ES_N0_DB_LIST = [2.0, 5.0, 10.0, 20.0, 50.0]
N_TRIALS = 8
N_FFT = 64
ZERO_PAD = 4
REFINE_PREFIX_EPOCHS = 300
WINDOWS = 6
BN = 0.002

# The realistic residual an Acquisition-quality (near-worthless) seed
# leaves behind -- this story's own Acquisition-isolation stress work
# measured mean |error| ~685 Hz, worst case ~1320 Hz.
RESIDUAL_HZ_CASES = [685.0, 1320.0]


def hz_to_norm_freq(hz):
    return hz / SAMPLE_RATE_HZ


def make_prefix(c, residual_hz, es_n0_db, trial_seed, n_epochs):
    """A short segment at the given residual/Es-N0, long enough for
    one frozen-carrier collection pass."""
    chip_snr_db = es_n0_to_chip_snr_db(es_n0_db)
    rx, _, _ = signal(
        c, n_epochs, EPOCHS_PER_SYMBOL, 0.37 * TE,
        hz_to_norm_freq(residual_hz), chip_snr_db, trial_seed,
    )
    return rx


def characterize_estimator():
    """Raw bridge accuracy vs. Es/N0: does it find the right FFT bin,
    and how precise is it once it does?"""
    c = code(11)
    coarse_bin_hz = (SAMPLE_RATE_HZ / (TE // WINDOWS)) / N_FFT / 2.0

    print("=== Raw estimator accuracy vs Es/N0 ===")
    print(
        f"(coarse bin width ~{coarse_bin_hz:.1f} Hz pre-enhancement; "
        f"'found right peak' = within one coarse bin of truth)"
    )
    for residual_hz in RESIDUAL_HZ_CASES:
        print(f"\n--- true residual = {residual_hz:.0f} Hz ---")
        for es_n0_db in ES_N0_DB_LIST:
            for zero_pad, interp, label in (
                (1, False, "baseline (n_fft=64, no pad/interp)"),
                (ZERO_PAD, True, f"enhanced (zero_pad={ZERO_PAD}, interp)"),
            ):
                errs = []
                n_found = 0
                for trial in range(N_TRIALS):
                    rx = make_prefix(
                        c, residual_hz, es_n0_db,
                        1000 + trial, REFINE_PREFIX_EPOCHS,
                    )
                    d0 = CoupledAsyncDespreader(
                        c, SPS, bn=BN, zeta=0.707, windows=WINDOWS,
                        init_car_norm_freq=0.0, aid_code=False,
                        sample_rate_hz=SAMPLE_RATE_HZ,
                        freeze_carrier=True,
                    )
                    out0 = d0.run(rx[:REFINE_PREFIX_EPOCHS * TE])
                    window_rate_hz = SAMPLE_RATE_HZ / d0.step_size
                    est = estimate_residual_freq(
                        out0, window_rate_hz, n_fft=N_FFT,
                        zero_pad=zero_pad, interp=interp,
                    )
                    err = abs(est - residual_hz)
                    found = err < coarse_bin_hz
                    if found:
                        n_found += 1
                        errs.append(err)
                found_frac = n_found / N_TRIALS
                if errs:
                    rms = float(np.sqrt(np.mean(np.square(errs))))
                    rms_str = f"{rms:7.2f} Hz"
                else:
                    rms_str = "    n/a"
                print(
                    f"  Es/N0={es_n0_db:5.1f} dB  {label:38s}  "
                    f"found_right_peak={found_frac:5.2f}  "
                    f"RMS_err|found={rms_str}"
                )


def characterize_lock():
    """End-to-end refine -> re-seed -> track lock rate vs Es/N0, at
    the two representative seed-gap cases."""
    c = code(11)
    n_sym = 3000

    print("\n=== End-to-end refine+track lock rate vs Es/N0 ===")
    for residual_hz in RESIDUAL_HZ_CASES:
        print(f"\n--- seed gap (= true residual, seeded at 0) "
              f"= {residual_hz:.0f} Hz ---")
        for es_n0_db in ES_N0_DB_LIST:
            chip_snr_db = es_n0_to_chip_snr_db(es_n0_db)
            n_locked = 0
            for trial in range(N_TRIALS):
                rx, _, _ = signal(
                    c, n_sym, EPOCHS_PER_SYMBOL, 0.37 * TE,
                    hz_to_norm_freq(residual_hz), chip_snr_db, 2000 + trial,
                )
                refined_norm_freq, _ = refine_seed(
                    CoupledAsyncDespreader, c, SPS, BN, 0.0,
                    rx[:REFINE_PREFIX_EPOCHS * TE], SAMPLE_RATE_HZ,
                    n_fft=N_FFT, zero_pad=ZERO_PAD, interp=True,
                    windows=WINDOWS,
                )
                d = CoupledAsyncDespreader(
                    c, SPS, bn=BN, zeta=0.707, windows=WINDOWS,
                    init_car_norm_freq=refined_norm_freq, aid_code=True,
                    sample_rate_hz=SAMPLE_RATE_HZ,
                    carrier_freq_hz=CARRIER_FREQ_HZ,
                )
                d.run(rx)
                final_car_hz = d.car_norm_freq * SAMPLE_RATE_HZ
                car_err_hz = abs(final_car_hz - residual_hz)
                locked = car_err_hz < 5.0 and abs(d.code_rate - 1.0) < 1e-4
                n_locked += int(locked)
            print(
                f"  Es/N0={es_n0_db:5.1f} dB  "
                f"lock_rate={n_locked}/{N_TRIALS}"
            )


if __name__ == "__main__":
    characterize_estimator()
    characterize_lock()
