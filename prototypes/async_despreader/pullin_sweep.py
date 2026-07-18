"""Phase 1b of the coupled-tracker roadmap
(`~/.claude/plans/jiggly-munching-newell.md`): characterize the
already-validated `CoupledAsyncDespreader` (`despreader_coupled.py`,
unmodified) against a REALISTIC seed, not a cold start.

The earlier 100 Hz test that failed to pull in seeded the carrier LO
at `init_car_norm_freq=0.0` and asked bare Costas to close the entire
100 Hz gap from scratch -- an unrealistic scenario. A real receiver
always seeds the carrier loop from `Acquisition`'s own Doppler
estimate first (same as this repo's own `DsssReceiver` does today).
This story's own earlier stress work
(`project_dsss_acq_async_story.md`'s Acquisition-isolation section)
measured what that estimate actually looks like: mean |error| ~685 Hz,
median ~683 Hz, with `doppler_bins==1` (zero resolution) in ~91% of
on-cell trials at cn0>=45 dB-Hz.

Question this script answers: seeded with a residual GAP of a given
size (not the full physical Doppler, just what's left after a
plausible Acquisition estimate), does the unmodified coupled tracker's
Costas loop still pull in and hold lock? Sweeps the gap from 0 Hz up
past the ~685 Hz mean/1000+ Hz worst case found for Acquisition, so
the answer directly tells us whether Phase 1c's frequency-refinement
bridge is actually needed, or whether Acquisition's real seed quality
is already good enough for this loop's own pull-in range.

Run: `python pullin_sweep.py` (needs numpy + doppler; no signal-gen
dependency beyond `validate_stress.py`'s own `code`/`signal`).
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed
from signal_gen import code, signal, CHIP_RATE, SF, SPS, TE

SAMPLE_RATE_HZ = CHIP_RATE * SPS  # 4.092e6, matches the class default
CARRIER_FREQ_HZ = 2.2e9  # matches the class default (S-band)

N_SYM = 3000
EPOCHS_PER_SYMBOL = (1.0 / 1800.0) / (SF / CHIP_RATE)
TRUE_RESIDUAL_HZ = 300.0  # the "true" leftover Doppler after any
# open-loop pre-compensation -- a representative, non-extreme value;
# what varies below is how far off Acquisition's SEED of it is.

# Gaps to test: 0 (perfect seed) up through and past this story's own
# measured Acquisition error statistics (~263 Hz downstream pull-in
# boundary found earlier, ~685 Hz mean/1320+ Hz worst-case error).
SEED_GAPS_HZ = [0.0, 50.0, 100.0, 200.0, 263.0, 400.0, 685.0, 1000.0,
                1320.0]


def hz_to_norm_freq(hz):
    return hz / SAMPLE_RATE_HZ


def run_case(c, rx, seed_gap_hz, bn=0.002, bn_car=None, windows=6):
    true_norm_freq = hz_to_norm_freq(TRUE_RESIDUAL_HZ)
    seeded_norm_freq = hz_to_norm_freq(TRUE_RESIDUAL_HZ - seed_gap_hz)
    d = CoupledAsyncDespreader(
        c, SPS, bn=bn, zeta=0.707, windows=windows, bn_car=bn_car,
        init_car_norm_freq=seeded_norm_freq, aid_code=True,
        sample_rate_hz=SAMPLE_RATE_HZ, carrier_freq_hz=CARRIER_FREQ_HZ,
    )
    d.run(rx)
    final_car_hz = d.car_norm_freq * SAMPLE_RATE_HZ
    car_err_hz = abs(final_car_hz - TRUE_RESIDUAL_HZ)
    locked = car_err_hz < 5.0 and abs(d.code_rate - 1.0) < 1e-4
    return d, car_err_hz, locked


REFINE_PREFIX_EPOCHS = 300
REFINE_N_FFT = 64


def run_refined_case(c, rx, seed_gap_hz, bn=0.002, bn_car=None, windows=6):
    """Same scenario as `run_case`, but seeded through Phase 1c's
    frequency-refinement bridge first: a short frozen-carrier
    collection pass estimates the residual left after the (possibly
    bad) Acquisition-quality seed, then a fresh tracker is built at the
    REFINED seed and run normally."""
    true_norm_freq = hz_to_norm_freq(TRUE_RESIDUAL_HZ)
    seeded_norm_freq = hz_to_norm_freq(TRUE_RESIDUAL_HZ - seed_gap_hz)
    prefix_len = REFINE_PREFIX_EPOCHS * TE
    refined_norm_freq, residual_est_hz = refine_seed(
        CoupledAsyncDespreader, c, SPS, bn, seeded_norm_freq,
        rx[:prefix_len], SAMPLE_RATE_HZ, n_fft=REFINE_N_FFT,
        bn_car=bn_car, windows=windows,
    )
    d = CoupledAsyncDespreader(
        c, SPS, bn=bn, zeta=0.707, windows=windows, bn_car=bn_car,
        init_car_norm_freq=refined_norm_freq, aid_code=True,
        sample_rate_hz=SAMPLE_RATE_HZ, carrier_freq_hz=CARRIER_FREQ_HZ,
    )
    d.run(rx)
    final_car_hz = d.car_norm_freq * SAMPLE_RATE_HZ
    car_err_hz = abs(final_car_hz - TRUE_RESIDUAL_HZ)
    locked = car_err_hz < 5.0 and abs(d.code_rate - 1.0) < 1e-4
    return d, car_err_hz, locked, residual_est_hz


def main():
    c = code(11)
    rx, _, _ = signal(
        c, N_SYM, EPOCHS_PER_SYMBOL, 0.37 * TE,
        hz_to_norm_freq(TRUE_RESIDUAL_HZ), -8, 5,
    )

    print(
        f"--- pull-in sweep: true residual={TRUE_RESIDUAL_HZ:.0f} Hz, "
        f"{N_SYM} symbols, -8 dB SNR ---"
    )
    print("--- unrefined (Phase 1b, bare coupled tracker) ---")
    boundary = None
    for gap in SEED_GAPS_HZ:
        d, car_err_hz, locked = run_case(c, rx, gap)
        status = "LOCKED" if locked else "FAILED"
        print(
            f"  seed_gap={gap:7.1f} Hz  final_car_err={car_err_hz:8.2f} Hz  "
            f"code_rate={d.code_rate:.6f}  last_error={d.last_error:+.5f}  "
            f"[{status}]"
        )
        if not locked and boundary is None:
            boundary = gap

    print()
    if boundary is None:
        print(
            "UNREFINED RESULT: locked across the ENTIRE sweep, up to "
            "and past Acquisition's measured worst-case error -- "
            "Phase 1c's frequency-refinement bridge is NOT needed."
        )
    else:
        print(
            f"UNREFINED RESULT: pull-in boundary is between the last "
            f"LOCKED gap and {boundary:.0f} Hz."
        )

    print()
    print("--- refined (Phase 1c bridge: squaring+FFT, then track) ---")
    refined_boundary = None
    for gap in SEED_GAPS_HZ:
        d, car_err_hz, locked, residual_est_hz = run_refined_case(
            c, rx, gap
        )
        true_residual_after_seed = gap  # what the bridge had to find
        status = "LOCKED" if locked else "FAILED"
        print(
            f"  seed_gap={gap:7.1f} Hz  "
            f"refine_est={residual_est_hz:8.2f} Hz "
            f"(true={true_residual_after_seed:7.1f} Hz)  "
            f"final_car_err={car_err_hz:8.2f} Hz  "
            f"code_rate={d.code_rate:.6f}  [{status}]"
        )
        if not locked and refined_boundary is None:
            refined_boundary = gap

    print()
    if refined_boundary is None:
        print(
            "REFINED RESULT: locked across the ENTIRE sweep, including "
            "past Acquisition's measured worst-case error -- Phase 1c's "
            "bridge closes the gap Phase 1b found."
        )
    else:
        print(
            f"REFINED RESULT: still fails beyond {refined_boundary:.0f} "
            f"Hz seed gap -- bridge helps but doesn't fully close the "
            f"gap; needs a wider search span/more coherent blocks."
        )


if __name__ == "__main__":
    main()
