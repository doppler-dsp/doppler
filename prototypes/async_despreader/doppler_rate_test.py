"""Does an FLL-style CONTINUOUS re-estimation loop track a Doppler
RAMP where the current one-shot batch bridge (`freq_refine.py`) can't?

Per the user's own diagnosis: the ~2.7 s the batch bridge needs isn't a
pull-in-RANGE problem, it's a noise-averaging-TIME problem -- an FLL
doesn't remove that requirement (same SNR x time trade), so it's not
expected to speed up the STATIC case this story already characterized.
Where a continuous loop's architecture SHOULD matter is Doppler RATE
(untested until now): the batch bridge collects one long window and
assumes the residual is ~static across it -- at the LEO rate this
story cares about (+-5 kHz/s, `docs/design/dsss-acquisition.md` Sec8),
the true frequency can drift thousands of Hz across a single
~2.7 s/2700-epoch collection window, smearing the non-coherent
accumulation (each block's true peak lands in a DIFFERENT bin as the
collection progresses) instead of reinforcing it.

Two strategies compared, BOTH built from the exact same existing
primitive (`freq_refine.estimate_residual_freq` via
`despreader_coupled.CoupledAsyncDespreader`'s `freeze_carrier`
collection pass -- no new estimator, per "never reimplement"):

1. **Static batch** (what Phase 1c already validated): ONE long
   (2700-epoch) frozen-carrier collection at the start, ONE estimate,
   done. Matches exactly what `pullin_sweep.py`/`improve_low_snr.py`
   already tested -- just now under a ramp instead of a static
   residual.
2. **FLL-assist** (new, the thing being tested): the SAME estimator,
   but run REPEATEDLY over SHORT (300-epoch) blocks, each time
   re-seeding the derotation from the latest estimate before the next
   block -- a simple type-1 recursive loop using the batch estimator as
   its discriminator. This is the natural, reuse-everything way to
   build "an FLL" here: the discriminator IS the already-validated
   near-ML block estimator, just driven continuously instead of once.

Run: `python doppler_rate_test.py` (needs numpy + doppler).
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import CoupledAsyncDespreader
from freq_refine import estimate_residual_freq
from characterize_snr import (
    SAMPLE_RATE_HZ, N_FFT, ZERO_PAD, WINDOWS, BN, es_n0_to_chip_snr_db,
)
from signal_gen import TE, SF, SPS, DATA_RATE, code

ES_N0_DB = 10.0  # moderate, already-clean-at-300-epochs operating
# point (per characterize_snr.py) -- isolates the RATE effect from the
# low-SNR gross-error effect already characterized separately.
RATE_LIST_HZ_PER_S = [0.0, 1000.0, 5000.0]
STATIC_COLLECT_EPOCHS = 2700  # the Phase-1c-validated static window
FLL_BLOCK_EPOCHS = 300  # the shortest block characterize_snr.py
# already showed converges cleanly at this Es/N0
N_FLL_BLOCKS = 9  # 9*300 = 2700 epochs total -- same total duration
# as the static batch, for a fair comparison


def make_ramp_signal(
    c, chip_snr_db, f0_hz, rate_hz_per_s, seed, n_epochs,
):
    """Same async-BPSK-over-Gold-ish-code construction this whole
    folder already uses (`validate_stress.signal`'s own formula),
    generalized from a fixed residual to a LINEAR RAMP: instantaneous
    frequency `f(t) = f0_hz + rate_hz_per_s * t`, phase = the integral
    of that (a chirp), everything else unchanged."""
    rng = np.random.default_rng(seed)
    sf = len(c)
    csign = np.where(c & 1, -1.0, 1.0)
    te = sf * SPS
    tsym = SAMPLE_RATE_HZ / DATA_RATE
    n = n_epochs * te + 2 * te
    idx = np.arange(n)
    t = idx / SAMPLE_RATE_HZ
    phase = 2.0 * np.pi * (f0_hz * t + 0.5 * rate_hz_per_s * t * t)
    n_sym_needed = int(n / tsym) + 4
    data = (rng.integers(0, 2, n_sym_needed) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // SPS) % sf
    sig = data[si] * csign[cph] * np.exp(1j * phase)
    p = np.sqrt(np.mean(np.abs(sig) ** 2))
    std = np.sqrt(10.0 ** (-chip_snr_db / 10.0)) * p
    noise = (std / np.sqrt(2.0)) * (
        rng.standard_normal(n) + 1j * rng.standard_normal(n)
    )
    return (sig + noise).astype(np.complex128)


def one_estimate(c, x_block, seeded_norm_freq):
    """One frozen-carrier collection + squaring/FFT estimate over
    `x_block`, using this story's already-validated parameters
    (n_fft=64, zero_pad=4, interp=True -- Phase 1c's settled choice)."""
    d0 = CoupledAsyncDespreader(
        c, SPS, bn=BN, zeta=0.707, windows=WINDOWS,
        init_car_norm_freq=seeded_norm_freq, aid_code=False,
        sample_rate_hz=SAMPLE_RATE_HZ, freeze_carrier=True,
    )
    out0 = d0.run(x_block)
    window_rate_hz = SAMPLE_RATE_HZ / d0.step_size
    residual_hz = estimate_residual_freq(
        out0, window_rate_hz, n_fft=N_FFT, zero_pad=ZERO_PAD,
        interp=True,
    )
    return residual_hz


def run_static_batch(c, chip_snr_db, rate_hz_per_s, seed):
    """Strategy 1: one long collection, seeded at the TRUE initial
    residual (isolating the ramp-during-collection effect from a bad
    static seed, which Phase 1b/1c already characterized separately)."""
    te = SF * SPS
    x = make_ramp_signal(
        c, chip_snr_db, 0.0, rate_hz_per_s, seed, STATIC_COLLECT_EPOCHS,
    )
    x_block = x[:STATIC_COLLECT_EPOCHS * te]
    est_hz = one_estimate(c, x_block, 0.0)
    duration_s = STATIC_COLLECT_EPOCHS * te / SAMPLE_RATE_HZ
    true_at_end = rate_hz_per_s * duration_s
    true_mean = 0.5 * rate_hz_per_s * duration_s  # energy-weighted-ish
    return {
        "est_hz": est_hz,
        "true_at_end_hz": true_at_end,
        "true_mean_hz": true_mean,
        "err_vs_end": abs(est_hz - true_at_end),
        "err_vs_mean": abs(est_hz - true_mean),
    }


def run_fll_assist(c, chip_snr_db, rate_hz_per_s, seed):
    """Strategy 2: N_FLL_BLOCKS short collections, each re-seeded from
    the running tracked estimate before the next block -- a simple
    type-1 recursive loop using the existing batch estimator as its
    discriminator."""
    te = SF * SPS
    total_epochs = FLL_BLOCK_EPOCHS * N_FLL_BLOCKS
    x = make_ramp_signal(
        c, chip_snr_db, 0.0, rate_hz_per_s, seed, total_epochs,
    )
    tracked_hz = 0.0
    errs = []
    for block in range(N_FLL_BLOCKS):
        pos = block * FLL_BLOCK_EPOCHS * te
        x_block = x[pos:pos + FLL_BLOCK_EPOCHS * te]
        seeded_norm_freq = tracked_hz / SAMPLE_RATE_HZ
        residual_hz = one_estimate(c, x_block, seeded_norm_freq)
        tracked_hz += residual_hz
        block_end_s = (block + 1) * FLL_BLOCK_EPOCHS * te / SAMPLE_RATE_HZ
        true_now = rate_hz_per_s * block_end_s
        errs.append(abs(tracked_hz - true_now))
    return {
        "final_tracked_hz": tracked_hz,
        "true_at_end_hz": rate_hz_per_s * total_epochs * te / SAMPLE_RATE_HZ,
        "err_vs_end": errs[-1],
        "err_history": errs,
    }


def run_integrated(c, chip_snr_db, rate_hz_per_s, seed, fll_block_epochs):
    """The REAL end-to-end check: one single `CoupledAsyncDespreader`
    instance, Costas + code loop + (optionally) FLL-assist all running
    together continuously via ONE `run()` call -- not the isolated
    per-block harness above. `fll_block_epochs=None` reproduces the
    OLD (pre-FLL-assist) behavior for direct comparison."""
    te = SF * SPS
    total_epochs = FLL_BLOCK_EPOCHS * N_FLL_BLOCKS
    x = make_ramp_signal(c, chip_snr_db, 0.0, rate_hz_per_s, seed, total_epochs)
    x = x[:total_epochs * te]
    d = CoupledAsyncDespreader(
        c, SPS, bn=BN, zeta=0.707, windows=WINDOWS,
        init_car_norm_freq=0.0, aid_code=True,
        sample_rate_hz=SAMPLE_RATE_HZ, fll_block_epochs=fll_block_epochs,
    )
    out = d.run(x)
    duration_s = total_epochs * te / SAMPLE_RATE_HZ
    true_at_end = rate_hz_per_s * duration_s
    tracked_hz = d.car_norm_freq * SAMPLE_RATE_HZ
    return {
        "tracked_hz": tracked_hz,
        "true_at_end_hz": true_at_end,
        "err": abs(tracked_hz - true_at_end),
        "code_rate": d.code_rate,
        "fll_corrections": d.fll_corrections,
        "out_max_mag": float(np.max(np.abs(out))),
    }


def main():
    c = code(11)
    chip_snr_db = es_n0_to_chip_snr_db(ES_N0_DB)
    print(
        f"=== Doppler-RATE test: static batch vs. FLL-assist "
        f"(Es/N0={ES_N0_DB:.0f} dB) ==="
    )
    print(
        f"Static: {STATIC_COLLECT_EPOCHS} epochs, one shot. "
        f"FLL: {N_FLL_BLOCKS} x {FLL_BLOCK_EPOCHS}-epoch blocks, "
        f"re-seeded each time. Same total duration either way."
    )
    for rate in RATE_LIST_HZ_PER_S:
        print(f"\n--- rate={rate:.0f} Hz/s ---")
        s = run_static_batch(c, chip_snr_db, rate, seed=42)
        print(
            f"  static batch:  est={s['est_hz']:8.1f} Hz  "
            f"true@end={s['true_at_end_hz']:8.1f} Hz  "
            f"err_vs_end={s['err_vs_end']:7.1f} Hz  "
            f"err_vs_mean={s['err_vs_mean']:7.1f} Hz"
        )
        f = run_fll_assist(c, chip_snr_db, rate, seed=42)
        hist = ", ".join(f"{e:.0f}" for e in f["err_history"])
        print(
            f"  FLL-assist:    final_tracked={f['final_tracked_hz']:8.1f} Hz  "
            f"true@end={f['true_at_end_hz']:8.1f} Hz  "
            f"err_vs_end={f['err_vs_end']:7.1f} Hz"
        )
        print(f"    per-block err history (Hz): {hist}")

        print("  --- integrated CoupledAsyncDespreader (Costas+code+FLL "
              "together) ---")
        i_off = run_integrated(c, chip_snr_db, rate, 42, fll_block_epochs=None)
        print(
            f"  fll disabled:  tracked={i_off['tracked_hz']:8.1f} Hz  "
            f"true@end={i_off['true_at_end_hz']:8.1f} Hz  "
            f"err={i_off['err']:8.1f} Hz  code_rate={i_off['code_rate']:.6f}  "
            f"out_max={i_off['out_max_mag']:.3f}"
        )
        i_on = run_integrated(
            c, chip_snr_db, rate, 42, fll_block_epochs=FLL_BLOCK_EPOCHS,
        )
        print(
            f"  fll enabled:   tracked={i_on['tracked_hz']:8.1f} Hz  "
            f"true@end={i_on['true_at_end_hz']:8.1f} Hz  "
            f"err={i_on['err']:8.1f} Hz  code_rate={i_on['code_rate']:.6f}  "
            f"out_max={i_on['out_max_mag']:.3f}  "
            f"fll_corrections={i_on['fll_corrections']}"
        )


if __name__ == "__main__":
    main()
