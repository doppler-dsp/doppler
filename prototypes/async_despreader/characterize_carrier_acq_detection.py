"""characterize_carrier_acq_detection.py -- Monte Carlo characterization
of `doppler.acquire.CarrierAcquisition`'s own detection statistic
against the `det_pd_noncoherent`/`det_threshold_noncoherent` theory it
is built on (the same Pfa/Pd framework `Acquisition` itself uses). This
is the direct follow-up to `FINISHING_PLAN.md`'s parked
CarrierAcquisition open item: the real-capture A/B
(`carrier_acq_c_vs_python.py`) found the C object's CFAR gate did not
converge the way the theory implied it should on ONE real capture --
that was an observation, never an actual characterization. This script
is that characterization.

Two questions, kept SEPARATE (per direct user hypothesis mid-session:
is this a distributional-model problem, or a repeated-testing/
optional-stopping problem from sequential mode re-testing the SAME
running average at every block?):

1. **Single-shot calibration** (Studies 1/2) -- THE DOMINANT USE CASE
   (per direct user steer): non-sequential mode waits a FIXED dwell
   then tests EXACTLY ONCE, the literal scenario
   `det_pd_noncoherent`/`det_threshold_noncoherent` were derived for
   (one test, no peeking). If empirical Pd/Pfa disagree with theory
   even here, the problem is the DISTRIBUTION itself -- this test
   statistic (a power-spectrum-vs-template correlation) may not
   actually follow the Rice/Rayleigh model the classic complex-
   correlator peak/noise-ratio statistic does.
2. **Sequential repeated-testing inflation** (Study 3, secondary):
   sequential mode re-tests the SAME running PSD average at every
   block against that block's own threshold. Even if Studies 1/2 show
   the per-look distribution itself is fine, testing it repeatedly
   along a correlated sequence is a classic multiple-comparisons/
   optional-stopping problem -- the cumulative chance ANY look fires
   can sit far above the nominal per-look Pfa. Study 3 isolates this
   by comparing sequential mode's own empirical "fired by block n"
   curve against the single-shot theory at matching n.

Signal model matches `src/doppler/acquire/tests/test_carrier_acq.py`'s
own `_make_signal` (rectangular-NRZ BPSK x residual tone, the exact
shape `CarrierAcquisition`'s default `psd_template` assumes), just
parameterized by exact sample count and by real Es/N0 via this
project's own established C/N0 -> per-sample-amplitude-SNR convention
(`amp_snr = sqrt(10**(cn0_dbhz/10)/fs)`, the same one `Acquisition`
itself and every sibling demo in `src/doppler/examples/` already use).

`design_snr`'s effect on `dwell_target` is highly nonlinear near
`pd_min` (see the probe sweep this script's own constants were picked
from) -- design_snr < ~0.1 at pfa=1e-2/pd=0.9/n_fft=64 pushes
`dwell_target` into the tens of thousands, which would blow up both
the signal arrays and the trial count needed for this script's own
Monte Carlo (see `feedback_wsl_memory_guard_large_arrays` -- WSL has
already OOM-crashed twice this session from exactly this class of
underestimate). DESIGN_SNR_SWEEP is deliberately bounded to the region
[0.1, 0.42] where dwell_target stays in [1, 48] -- plenty of dynamic
range for Study 1 without the blowup.

Run: `python characterize_carrier_acq_detection.py` (needs numpy +
doppler). Prints tables; no assertions -- exploratory, per the user's
own "park it, document honestly" decision on the underlying question.
"""
from __future__ import annotations

import numpy as np

from doppler.acquire import CarrierAcquisition
from doppler.detection import det_pd_noncoherent, det_threshold_noncoherent
from spec_full_characterization import es_n0_to_cn0_dbhz

SAMPLE_RATE_HZ = 8000.0
SYMBOL_RATE_HZ = 1000.0
SPS = 8  # SAMPLE_RATE_HZ / SYMBOL_RATE_HZ, exact
RESOLUTION_HZ = 125.0  # n_fft = 8000/125 = 64 (8 symbols/block, exact pow2)
N_FFT = int(round(SAMPLE_RATE_HZ / RESOLUTION_HZ))
ZERO_PAD = 1  # nfft(padded) = next_pow2(64*1) = 64 -- keeps MC fast
PFA = 1e-2  # moderate, per detection_sim.py's own MC-statistics precedent
TONE_HZ = 137.0
NO_TEMPLATE = np.array([], dtype=np.float32)
N_TRIALS = 400

CA_KWARGS = dict(resolution_hz=RESOLUTION_HZ, zero_pad=ZERO_PAD, pfa=PFA)

# See module docstring: bounded so dwell_target stays in [1, 48], not
# the tens-of-thousands a naive wider sweep hits.
DESIGN_SNR_SWEEP = np.geomspace(0.10, 0.42, 12)


def amp_snr_from_esn0(esn0_db):
    """Per-sample amplitude SNR at CarrierAcquisition's own input rate
    (SAMPLE_RATE_HZ), via this project's own established C/N0 -> per-
    sample-amplitude-SNR convention (`Acquisition`'s own `amp_snr`,
    reused verbatim -- see e.g. `dsss_acq_async_data_demo.py`):
    `amp_snr = sqrt(10**(cn0_dbhz/10) / fs)`,
    `cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db)`.

    This is the value fed to `det_pd_noncoherent`'s own `snr` parameter
    for every theory curve below -- an ASSUMPTION UNDER TEST here, not
    a proven identity: CarrierAcquisition's statistic is a power-
    spectrum-vs-template correlation, not the plain complex-correlator
    peak/noise ratio this per-sample convention was originally defined
    for. Whether it's even the right stand-in is exactly what Studies
    1/2 below check.
    """
    cn0_dbhz = es_n0_to_cn0_dbhz(esn0_db, sym_rate=SYMBOL_RATE_HZ)
    return float(np.sqrt(10.0 ** (cn0_dbhz / 10.0) / SAMPLE_RATE_HZ))


def make_signal(n_samples, esn0_db, seed, tone_hz=TONE_HZ, signal_present=True):
    """`n_samples` of NRZ BPSK (SPS samples/symbol) at SYMBOL_RATE_HZ,
    optionally times a residual tone (`signal_present=False` gives pure
    AWGN -- H0), plus AWGN at the given real Es/N0. Same rectangular-
    NRZ model CarrierAcquisition's own default `psd_template` assumes
    (`test_carrier_acq.py`'s own `_make_signal`), just parameterized by
    exact sample count instead of symbol count."""
    rng = np.random.default_rng(seed)
    amp_snr = amp_snr_from_esn0(esn0_db)
    sigma = 1.0 / amp_snr
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(n_samples) + 1j * rng.standard_normal(n_samples)
    )
    if not signal_present:
        return noise.astype(np.complex64)
    n_sym = int(np.ceil(n_samples / SPS)) + 1
    bits = np.where(rng.integers(0, 2, n_sym), 1.0, -1.0)
    data = np.repeat(bits, SPS)[:n_samples]
    t = np.arange(n_samples)
    tone = np.exp(2j * np.pi * tone_hz * t / SAMPLE_RATE_HZ)
    return (data * tone + noise).astype(np.complex64)


def _dwell_for_design_snr(design_snr, pd=0.9):
    probe = CarrierAcquisition(
        NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, design_snr=design_snr,
        pd=pd, sequential=False, **CA_KWARGS,
    )
    return probe.dwell_target


def single_shot_trial(dwell, esn0_db, seed, design_snr, pd=0.9,
                       signal_present=True):
    """One non-sequential CarrierAcquisition instance, ONE test at
    exactly `dwell` blocks (no repeated testing) -- the literal
    single-shot scenario `det_pd_noncoherent` models. Returns whether
    it fired."""
    n = dwell * N_FFT
    x = make_signal(n, esn0_db, seed, signal_present=signal_present)
    ca = CarrierAcquisition(
        NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, design_snr=design_snr,
        pd=pd, sequential=False, **CA_KWARGS,
    )
    ca.steps(x)
    return ca.ready


def study_1_pd_vs_dwell(esn0_db=5.0, pd_target=0.9):
    """DOMINANT-USE-CASE study: fixed real Es/N0, sweep design_snr
    (which drives dwell_target via det_n_noncoh), read back the ACTUAL
    dwell_target achieved at each design_snr, and compare empirical
    single-shot Pd (non-sequential, N_TRIALS fresh instances per point)
    against det_pd_noncoherent(amp_snr_from_esn0(esn0_db), N_FFT, dwell,
    det_threshold_noncoherent(PFA, dwell))."""
    print(f"\n=== Study 1: single-shot Pd vs dwell (Es/N0={esn0_db} dB) ===")
    true_snr = amp_snr_from_esn0(esn0_db)
    print(f"amp_snr (per-sample, from Es/N0) = {true_snr:.4f}")
    seen_dwells = set()
    print(
        f"{'design_snr':>10}  {'dwell':>6}  {'Pd (MC)':>9}  "
        f"{'Pd (theory)':>11}  {'diff':>7}"
    )
    for ds in DESIGN_SNR_SWEEP:
        dwell = _dwell_for_design_snr(float(ds), pd=pd_target)
        if dwell in seen_dwells:
            continue
        seen_dwells.add(dwell)
        fired = sum(
            single_shot_trial(
                dwell, esn0_db, 10_000 + t, float(ds), pd=pd_target
            )
            for t in range(N_TRIALS)
        )
        pd_mc = fired / N_TRIALS
        eta = det_threshold_noncoherent(PFA, dwell)
        pd_th = det_pd_noncoherent(true_snr, N_FFT, dwell, eta)
        print(
            f"{ds:10.4f}  {dwell:6d}  {pd_mc:9.3f}  {pd_th:11.3f}  "
            f"{pd_mc - pd_th:+7.3f}"
        )


def study_2_pd_vs_esn0(
    design_snr=0.15,
    pd_target=0.9,
    esn0_list=(-2.0, 0.0, 2.0, 4.0, 5.0, 6.0, 8.0, 12.0),
):
    """DOMINANT-USE-CASE study: fixed dwell (via one FIXED design_snr,
    read back once -- the planning-time choice a real caller would
    actually make), sweep real Es/N0, compare empirical single-shot Pd
    against det_pd_noncoherent(amp_snr_from_esn0(esn0_db), N_FFT,
    dwell, det_threshold_noncoherent(PFA, dwell))."""
    dwell = _dwell_for_design_snr(design_snr, pd=pd_target)
    eta = det_threshold_noncoherent(PFA, dwell)
    print(
        f"\n=== Study 2: single-shot Pd vs Es/N0 (fixed dwell={dwell}, "
        f"design_snr={design_snr}) ==="
    )
    print(
        f"{'Es/N0 (dB)':>10}  {'Pd (MC)':>9}  {'Pd (theory)':>11}  "
        f"{'diff':>7}"
    )
    for esn0_db in esn0_list:
        true_snr = amp_snr_from_esn0(esn0_db)
        fired = sum(
            single_shot_trial(
                dwell, esn0_db, 20_000 + t, design_snr, pd=pd_target
            )
            for t in range(N_TRIALS)
        )
        pd_mc = fired / N_TRIALS
        pd_th = det_pd_noncoherent(true_snr, N_FFT, dwell, eta)
        print(
            f"{esn0_db:10.1f}  {pd_mc:9.3f}  {pd_th:11.3f}  "
            f"{pd_mc - pd_th:+7.3f}"
        )


def sequential_first_fire_block(esn0_db, seed, max_n_blocks,
                                 signal_present=True):
    """One sequential-mode CarrierAcquisition instance, fed one block
    of `max_n_blocks * N_FFT` samples so it runs to completion (fires
    or gives up at max_n_blocks). Returns the block index it fired at
    (1-based), or None if it never fired."""
    n = max_n_blocks * N_FFT
    x = make_signal(n, esn0_db, seed, signal_present=signal_present)
    ca = CarrierAcquisition(
        NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, pfa=PFA,
        sequential=True, max_n_blocks=max_n_blocks, zero_pad=ZERO_PAD,
        resolution_hz=RESOLUTION_HZ,
    )
    ca.steps(x)
    return ca.n_blocks if ca.ready else None


def study_3_sequential_inflation(
    esn0_db=5.0, max_n_blocks=64, checkpoints=(4, 8, 16, 32, 64)
):
    """SECONDARY study (sequential mode is not the dominant use case,
    per direct user steer -- kept lighter-weight): sequential mode's
    own empirical "fired by block n" curve, both H0 (pure noise --
    answers directly whether repeated testing inflates Pfa above the
    nominal PFA) and H1 (real Es/N0), vs. the single-shot theory AT
    EACH n (det_pd_noncoherent/det_threshold_noncoherent evaluated
    fresh at n_noncoh=n, as if n were the only test ever run) --
    isolating whether RE-TESTING the same running average at every
    block, rather than the underlying per-look distribution, is the
    inflation source."""
    print(
        f"\n=== Study 3 (secondary): sequential repeated-testing "
        f"inflation (Es/N0={esn0_db} dB) ==="
    )
    true_snr = amp_snr_from_esn0(esn0_db)

    fire_blocks_h0 = [
        sequential_first_fire_block(
            esn0_db, 30_000 + t, max_n_blocks, signal_present=False
        )
        for t in range(N_TRIALS)
    ]
    fire_blocks_h1 = [
        sequential_first_fire_block(
            esn0_db, 40_000 + t, max_n_blocks, signal_present=True
        )
        for t in range(N_TRIALS)
    ]

    print(
        f"{'n':>4}  {'Pfa: fired-by-n (MC)':>21}  {'PFA (design)':>13}  "
        f"{'Pd: fired-by-n (MC)':>20}  {'Pd: single-shot theory':>23}"
    )
    for n in checkpoints:
        pfa_cum_mc = float(
            np.mean([fb is not None and fb <= n for fb in fire_blocks_h0])
        )
        pd_cum_mc = float(
            np.mean([fb is not None and fb <= n for fb in fire_blocks_h1])
        )
        eta = det_threshold_noncoherent(PFA, n)
        pd_single = det_pd_noncoherent(true_snr, N_FFT, n, eta)
        print(
            f"{n:4d}  {pfa_cum_mc:21.4f}  {PFA:13.4g}  "
            f"{pd_cum_mc:20.4f}  {pd_single:23.4f}"
        )


if __name__ == "__main__":
    study_1_pd_vs_dwell()
    study_2_pd_vs_esn0()
    study_3_sequential_inflation()
