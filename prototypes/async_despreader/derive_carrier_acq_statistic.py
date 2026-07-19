"""derive_carrier_acq_statistic.py -- derives, and validates against
Monte Carlo, the ACTUAL null/signal distribution of CarrierAcquisition's
test statistic (a power-spectrum-vs-known-template correlation), to
replace the borrowed `det_pd_noncoherent`/`det_threshold_noncoherent`
model that `characterize_carrier_acq_detection.py` confirmed does NOT
apply here (see FINISHING_PLAN.md's CarrierAcquisition section).

THE STATISTIC, exactly as `carrier_acq_core.c` computes it
--------------------------------------------------------------------
1. Each n_fft-sample block is windowed, FFT'd, magnitude-squared, and
   folded into a running arithmetic mean (`psd_core.c`'s `mode=0`/mean
   -- a genuine unbounded Welford mean, NOT an EMA; `alpha` is a no-op
   in this mode, confirmed by reading `acc_trace_core.c` directly
   before trusting anything built on top of it).
2. Read back CG^2-normalised (`psd_power_twosided`): `Pavg[m] =
   pwr_avg_raw[m] / cg^2`, where `pwr_avg_raw[m]` is the raw averaged
   `|FFT|^2`.
3. `Pavg` (real, non-negative, length nfft) is circularly
   cross-correlated against the known template `T` (also real,
   non-negative, DC-centred, peak-normalised to 1) via `corr_core`'s
   FFT-based, 1/n-normalised correlation: `C[k] = sum_m Pavg[m] *
   T[(m-k) mod nfft]` -- exactly `doppler.spectral.Corr`, reused
   directly below rather than re-derived, so this validation exercises
   the REAL primitive, not a hand-rolled stand-in.
4. `test_stat = C[k_hat] / median_k(C[k])`, `k_hat = argmax_k C[k]`.

THE DERIVATION (H0, exact, no free parameters)
--------------------------------------------------------------------
Under H0 (i.i.d. complex Gaussian noise, `E[|w|^2] = sigma^2`), a
SINGLE windowed periodogram sample at bin m is `|FFT(w*noise)[m]|^2 ~
Exponential(mean = s2*sigma^2)` (`s2 = sum(window^2)`, the window's own
incoherent power gain -- standard periodogram theory for i.i.d. input;
approximate for bins closer together than the window's own spectral
leakage width, exact in the rectangular-window limit). Averaging
`n_blocks` i.i.d. such samples (independent noise realisations) gives
EXACTLY `pwr_avg_raw[m] ~ Gamma(shape=n_blocks, scale=(s2*sigma^2)
/n_blocks)` -- valid at ANY n_blocks >= 1, no CLT needed for this step.
CG^2-normalising just rescales: `Pavg[m] ~ Gamma(n_blocks,
mu/n_blocks)`, `mu := s2*sigma^2/cg^2`.

`C[k] = sum_m Pavg[m]*T[(m-k) mod nfft]` is then a POSITIVELY-WEIGHTED
SUM of independent Gamma(n_blocks, mu/n_blocks) variables -- exact
mean/variance regardless of n_blocks:

    E[C[k]]   = mu * S_T,     S_T  = sum_m T[m]      (shift-invariant)
    Var[C[k]] = (mu^2/n_blocks) * S_T2,  S_T2 = sum_m T[m]^2

The KEY move vs. the borrowed model: the Gaussian approximation for
`C[k]` comes from the CLT over the ~nfft SPATIAL terms being summed
(many bins), NOT over n_blocks (many looks) -- so it should hold up
even at small n_blocks (1-48), unlike a temporal-CLT argument that
would need many looks to kick in. `noise_est = median_k(C[k])`
concentrates near `mu*S_T` (every lag shares the same H0 mean).

H1 (signal present): a BPSK-data-modulated tone's AVERAGE periodogram
shape (over random data realisations) is BY CONSTRUCTION the same
sinc^2/RRC template `T` used for detection (that's the whole premise
of the known-PSD-template approach) -- so the signal's mean
contribution to `Pavg[m]` is `G * T[(m-k0) mod nfft]` for some gain
`G` (fit empirically below, NOT re-derived from first principles: a
per-block realisation's own noncentral-chi-square parameter varies
with that block's own random bit pattern, a "self-noise" contribution
this derivation deliberately keeps separate from the H0 fluctuation
term rather than pretend to solve in closed form). This predicts the
mean shift at the true peak lag k0 is proportional to the template's
OWN AUTOCORRELATION at zero lag, `R_T[0] = S_T2` -- confirmed below.

Run: `python derive_carrier_acq_statistic.py` (needs numpy + doppler).
Prints validation tables; no assertions -- this is a derivation
exercise, not a gate.
"""
from __future__ import annotations

import numpy as np

from doppler.spectral import PSD, Corr, hann_window
from doppler.detection import det_threshold_noncoherent
from spec_full_characterization import es_n0_to_cn0_dbhz

SAMPLE_RATE_HZ = 8000.0
SYMBOL_RATE_HZ = 1000.0
SPS = 8
N_FFT = 64  # matches characterize_carrier_acq_detection.py's own choice
ZERO_PAD = 1
NFFT = N_FFT  # next_pow2(64*1) == 64
TONE_HZ = 137.0
N_TRIALS = 500


def amp_snr_from_esn0(esn0_db):
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db, sym_rate=SYMBOL_RATE_HZ)
    return float(np.sqrt(10.0 ** (cn0_dbhz / 10.0) / SAMPLE_RATE_HZ))


def default_template(nfft=NFFT, fs=SAMPLE_RATE_HZ, sym_rate=SYMBOL_RATE_HZ):
    """Ported (for independent validation only -- NOT reused inside
    CarrierAcquisition itself, which builds this internally in C and
    never surfaces it) from `carrier_acq_core.c`'s own
    `_default_template` / `freq_refine.py`'s `_known_symbol_psd_template`:
    the DC-centred sinc^2 average-PSD shape of a random NRZ BPSK
    stream, peak-normalised to 1 at zero offset."""
    freqs = (np.arange(nfft) - nfft // 2) * fs / nfft
    return (np.sinc(freqs / sym_rate) ** 2).astype(np.float64)


def make_noise(n_samples, esn0_db, seed):
    rng = np.random.default_rng(seed)
    sigma = 1.0 / amp_snr_from_esn0(esn0_db)
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n_samples) + 1j * rng.standard_normal(n_samples)
    )
    return noise.astype(np.complex64)


def make_signal(n_samples, esn0_db, seed, tone_hz=TONE_HZ):
    noise = make_noise(n_samples, esn0_db, seed)
    rng = np.random.default_rng(seed + 500_000)  # independent data stream
    n_sym = int(np.ceil(n_samples / SPS)) + 1
    bits = np.where(rng.integers(0, 2, n_sym), 1.0, -1.0)
    data = np.repeat(bits, SPS)[:n_samples]
    t = np.arange(n_samples)
    tone = np.exp(2j * np.pi * tone_hz * t / SAMPLE_RATE_HZ)
    return (data * tone + noise).astype(np.complex64)


def measure_pavg(x, n_blocks):
    """The EXACT quantity carrier_acq_core.c calls `s->pwr_buf`: a
    running mean of the windowed, FFT'd, magnitude-squared, CG^2-
    normalised periodogram -- read back via doppler.spectral.PSD, the
    real shared primitive (not reimplemented)."""
    psd = PSD(n=N_FFT, fs=SAMPLE_RATE_HZ, window="hann", pad=ZERO_PAD,
              mode="mean")
    psd.accumulate(x[: n_blocks * N_FFT])
    return psd.power_twosided().astype(np.float64)


def correlate(pavg, template):
    """The EXACT primitive detector_core.c calls internally: an FFT-
    based, 1/n-normalised circular cross-correlation -- reused directly
    via doppler.spectral.Corr rather than hand-rolled, so this
    validates against the REAL detector math, not a stand-in."""
    ref = template.astype(np.complex64)
    corr = Corr(ref=ref, dwell=1)
    out = corr.execute(pavg.astype(np.complex64))
    return np.real(out)  # real by construction (both inputs real)


def study_h0_exact():
    """H0 (noise-only): compare the CLOSED-FORM, free-parameter-free
    mean/variance of C[k] (at one fixed, arbitrary lag k=0 -- any lag
    has the same H0 marginal) against Monte Carlo, across n_blocks."""
    w = np.zeros(N_FFT, dtype=np.float32)
    hann_window(w)
    cg, s2 = float(np.sum(w)), float(np.sum(w**2))
    template = default_template()
    s_t, s_t2 = float(np.sum(template)), float(np.sum(template**2))

    print("=== Study H0: exact Gamma-sum model vs Monte Carlo ===")
    print(f"cg={cg:.4f}  s2={s2:.4f}  S_T={s_t:.4f}  S_T2={s_t2:.4f}")
    for esn0_db in (0.0, 5.0, 10.0):
        sigma = 1.0 / amp_snr_from_esn0(esn0_db)
        mu = s2 * sigma**2 / cg**2
        for n_blocks in (1, 4, 16, 48):
            c0 = np.empty(N_TRIALS)
            for t in range(N_TRIALS):
                x = make_noise(n_blocks * N_FFT, esn0_db, 1_000_000 + t)
                pavg = measure_pavg(x, n_blocks)
                c0[t] = correlate(pavg, template)[0]
            mean_pred, std_pred = mu * s_t, np.sqrt(mu**2 / n_blocks * s_t2)
            mean_mc, std_mc = float(c0.mean()), float(c0.std(ddof=1))
            print(
                f"  Es/N0={esn0_db:4.1f}dB n_blocks={n_blocks:3d}  "
                f"mean: pred={mean_pred:10.4g} mc={mean_mc:10.4g} "
                f"({100*(mean_mc/mean_pred-1):+5.1f}%)  "
                f"std: pred={std_pred:10.4g} mc={std_mc:10.4g} "
                f"({100*(std_mc/std_pred-1):+5.1f}%)"
            )


def study_h1_gain():
    """H1 (signal present): confirm the mean-shift at the TRUE peak lag
    scales with S_T2 (the template's own zero-lag autocorrelation, per
    the derivation), and fit the one free gain constant G this
    derivation deliberately leaves empirical (per-block self-noise from
    the random data pattern makes G analytically intractable in closed
    form -- see module docstring)."""
    w = np.zeros(N_FFT, dtype=np.float32)
    hann_window(w)
    cg, s2 = float(np.sum(w)), float(np.sum(w**2))
    template = default_template()
    s_t, s_t2 = float(np.sum(template)), float(np.sum(template**2))

    print("\n=== Study H1: mean-shift gain G at the true peak lag ===")
    # NOTE: the correlation LAG index maps DIRECTLY to bin_pos (matching
    # carrier_acq_core.c's own _finish()'s `bin_pos = lag + frac`) --
    # it is NOT additionally offset by nfft/2 the way Pavg's own
    # DC-centred bin index is. Caught by checking np.argmax(c) directly
    # against this formula before trusting a near-zero "shift" that
    # turned out to just be sampling the wrong (off-peak) lag.
    k0 = int(round(TONE_HZ / (SAMPLE_RATE_HZ / NFFT)))
    for esn0_db in (0.0, 5.0, 10.0):
        sigma = 1.0 / amp_snr_from_esn0(esn0_db)
        mu = s2 * sigma**2 / cg**2
        for n_blocks in (4, 16, 48):
            ck0 = np.empty(N_TRIALS)
            for t in range(N_TRIALS):
                x = make_signal(n_blocks * N_FFT, esn0_db, 2_000_000 + t)
                pavg = measure_pavg(x, n_blocks)
                ck0[t] = correlate(pavg, template)[k0 % NFFT]
            mean_h1 = float(ck0.mean())
            shift = mean_h1 - mu * s_t
            g_fit = shift / s_t2
            print(
                f"  Es/N0={esn0_db:4.1f}dB n_blocks={n_blocks:3d}  "
                f"E[C(k0)]={mean_h1:10.4g}  shift={shift:10.4g}  "
                f"G=shift/S_T2={g_fit:10.4g}  G/mu={g_fit/mu:8.3f}"
            )


def study_ratio_threshold_calibration(dwell=13, pfa=1e-2, n_calib=2000):
    """The decisive study: what test_stat threshold (peak/median RATIO,
    i.e. exactly what carrier_acq_core.c gates on) does the REAL H0
    null distribution of the ARGMAX-over-nfft-correlated-lags statistic
    actually require to hit the target Pfa -- vs. what the borrowed
    det_threshold_noncoherent(pfa, dwell) demands. This isolates the
    THIRD effect (beyond wrong-distribution-family and data-modulation
    self-noise): searching the argmax over ~nfft correlated lags needs
    its own (Bonferroni/Sidak-style, per `acq_core.c`'s own
    `pfa_cell` -- the SAME idea, not re-derived from scratch) threshold
    correction, and det_threshold_noncoherent's Rice-model correction
    is not it."""
    template = default_template()
    ts = np.empty(n_calib)
    for t in range(n_calib):
        x = make_noise(dwell * N_FFT, 5.0, 8_000_000 + t)
        c = correlate(measure_pavg(x, dwell), template)
        k_hat = int(np.argmax(c))
        noise_est = float(np.median(c))
        ts[t] = c[k_hat] / noise_est
    ts_sorted = np.sort(ts)
    idx = min(int(round((1.0 - pfa) * n_calib)), n_calib - 1)
    eta_calibrated = float(ts_sorted[idx])
    eta_borrowed = det_threshold_noncoherent(pfa, dwell)
    print(
        f"\n=== Study: ratio-threshold calibration (dwell={dwell}, "
        f"pfa={pfa}) ==="
    )
    print(f"borrowed  det_threshold_noncoherent = {eta_borrowed:.4f}")
    print(f"empirical (argmax null, {n_calib} trials) = {eta_calibrated:.4f}")
    return eta_calibrated, eta_borrowed


def study_pd_with_calibrated_threshold(
    dwell=13, eta_calibrated=1.43, eta_borrowed=None,
    esn0_list=(-2.0, 0.0, 2.0, 4.0, 5.0, 6.0, 8.0, 12.0),
):
    """The payoff: Pd across the SAME Es/N0 grid
    `characterize_carrier_acq_detection.py`'s Study 2 used, gating the
    REAL argmax test_stat against the calibrated threshold vs. the
    borrowed one -- confirms whether fixing JUST the threshold (not a
    full from-scratch statistic replacement) recovers the Pd the
    borrowed model always claimed was there."""
    template = default_template()
    print(
        f"\n=== Study: Pd with calibrated vs. borrowed threshold "
        f"(dwell={dwell}) ==="
    )
    print(
        f"{'Es/N0':>7} {'Pd(calibrated)':>15} "
        + (f"{'Pd(borrowed)':>13}" if eta_borrowed else "")
    )
    for esn0_db in esn0_list:
        fc = fb = 0
        for t in range(N_TRIALS):
            x = make_signal(dwell * N_FFT, esn0_db, 7_700_000 + t)
            c = correlate(measure_pavg(x, dwell), template)
            k_hat = int(np.argmax(c))
            noise_est = float(np.median(c))
            test_stat = c[k_hat] / noise_est
            fc += int(test_stat > eta_calibrated)
            if eta_borrowed:
                fb += int(test_stat > eta_borrowed)
        line = f"{esn0_db:7.1f} {fc/N_TRIALS:15.3f}"
        if eta_borrowed:
            line += f" {fb/N_TRIALS:13.3f}"
        print(line)


if __name__ == "__main__":
    study_h0_exact()
    study_h1_gain()
    eta_cal, eta_borrowed = study_ratio_threshold_calibration()
    study_pd_with_calibrated_threshold(
        eta_calibrated=eta_cal, eta_borrowed=eta_borrowed
    )
