"""Follow-up to `characterize_snr.py`: try to improve Phase 1c's
frequency-refinement bridge in the low-Es/N0 region it found unreliable
(gross wrong-FFT-bin errors below ~5-10 dB, from squaring loss), per
direct user request -- two levers, tried both separately and combined:

1. **Multi-look non-coherent averaging.** `estimate_residual_freq`
   already non-coherently sums `|FFT|^2` over every `n_fft`-sample
   block in its input; a longer collection window is just MORE blocks.
   No new code, just a `REFINE_PREFIX_EPOCHS` sweep (300 -> 900 -> 2700
   epochs, 3x and 9x the original).
2. **Matched-filter the known Dirichlet mainlobe shape** (`use_mf=True`
   in `freq_refine.py`) -- cross-correlates the power spectrum against
   the known rectangular-window kernel shape before picking the peak,
   suppressing single-bin noise spikes relative to a genuine
   `~zero_pad`-bin-wide peak.

Run: `python improve_low_snr.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import CoupledAsyncDespreader
from freq_refine import estimate_residual_freq
from characterize_snr import (
    SAMPLE_RATE_HZ, EPOCHS_PER_SYMBOL, N_FFT, ZERO_PAD, WINDOWS, BN,
    hz_to_norm_freq, es_n0_to_chip_snr_db,
)
from signal_gen import code, signal, TE, SPS

N_TRIALS = 12
ES_N0_DB_LIST = [2.0, 5.0]
RESIDUAL_HZ_CASES = [685.0, 1320.0]
PREFIX_EPOCHS_LIST = [300, 900, 2700]

# The un-padded coarse-bin width, used as the "found the right peak"
# tolerance -- same definition characterize_snr.py uses.
COARSE_BIN_HZ = (SAMPLE_RATE_HZ / (TE // WINDOWS)) / N_FFT / 2.0


def one_trial(c, residual_hz, es_n0_db, trial_seed, prefix_epochs, use_mf):
    chip_snr_db = es_n0_to_chip_snr_db(es_n0_db)
    rx, _, _ = signal(
        c, prefix_epochs, EPOCHS_PER_SYMBOL, 0.37 * TE,
        hz_to_norm_freq(residual_hz), chip_snr_db, trial_seed,
    )
    d0 = CoupledAsyncDespreader(
        c, SPS, bn=BN, zeta=0.707, windows=WINDOWS,
        init_car_norm_freq=0.0, aid_code=False,
        sample_rate_hz=SAMPLE_RATE_HZ, freeze_carrier=True,
    )
    out0 = d0.run(rx[:prefix_epochs * TE])
    window_rate_hz = SAMPLE_RATE_HZ / d0.step_size
    est = estimate_residual_freq(
        out0, window_rate_hz, n_fft=N_FFT, zero_pad=ZERO_PAD,
        interp=True, use_mf=use_mf,
    )
    return abs(est - residual_hz)


def main():
    c = code(11)
    print(
        f"=== Low-Es/N0 improvement sweep (coarse bin "
        f"~{COARSE_BIN_HZ:.1f} Hz) ==="
    )
    for residual_hz in RESIDUAL_HZ_CASES:
        print(f"\n--- true residual = {residual_hz:.0f} Hz ---")
        for es_n0_db in ES_N0_DB_LIST:
            for prefix_epochs in PREFIX_EPOCHS_LIST:
                for use_mf, label in ((False, "no MF"), (True, "+MF")):
                    n_found = 0
                    errs = []
                    for trial in range(N_TRIALS):
                        err = one_trial(
                            c, residual_hz, es_n0_db, 3000 + trial,
                            prefix_epochs, use_mf,
                        )
                        if err < COARSE_BIN_HZ:
                            n_found += 1
                            errs.append(err)
                    found_frac = n_found / N_TRIALS
                    rms = (
                        f"{np.sqrt(np.mean(np.square(errs))):6.2f} Hz"
                        if errs else "   n/a"
                    )
                    print(
                        f"  Es/N0={es_n0_db:4.1f} dB  "
                        f"prefix={prefix_epochs:5d} epochs  {label:6s}  "
                        f"found_right_peak={found_frac:5.2f}  "
                        f"RMS_err|found={rms}"
                    )


if __name__ == "__main__":
    main()
