"""Performance characterisation of both M-PSK receivers, as assertions.

The gallery demo (`src/doppler/examples/mpsk_receiver_performance_demo.py`)
*plots* this material over a random sweep; this module pins the parts that must
hold on every commit, on both the complex-baseband and real-IF paths:

* **false alarm** — noise only, no signal: neither detector may declare lock;
* **the lock statistic's H0 law** — its noise-only sd must match the analytic
  `sqrt(1/2 * alpha/(2-alpha))` = 0.1132 at *every* M, since that identity is
  what makes one `lock_thresh` mean one false-alarm probability;
* **level invariance** — EVM must not track absolute input level;
* **the coherent bound** — measured SER against the theoretical M-PSK curve at
  each order's own SER = 1e-3 operating point.

Every measurement obeys the project's loop-characterisation rules, which are
encoded in `_mpsk_rx_harness` rather than repeated here: a settling budget of
`2*(5/bn_t + 5/bn_c)` symbols, carrier offsets drawn *inside* the loop
bandwidth, and `bn <= 0.01` never widened to shorten a record. See
`docs/design/mpsk.md`.
"""

from __future__ import annotations

import math

import numpy as np
import pytest

from doppler.ber import ber_theory_ser

from ._mpsk_rx_harness import (
    IF_FS4,
    coherent_errors,
    demod,
    freq_offset_inside_bw,
    make_signal,
    ser_confidence,
    settle_floor,
    settle_from,
    symbol_metrics,
)

# The lock statistic is Re((z/|z|)^M) smoothed by an EMA. Limiting makes its H0
# variance 1/2 for EVERY M (the phase is uniform under H0), so after an EMA of
# coefficient alpha the noise-only sd is a single analytic number -- which is
# the whole reason a threshold maps to a Pfa. Mirrors
# CARRIER_NDA_LOCK_ALPHA / CARRIER_NDA_LOCK_NORM_SD in carrier_nda_core.h.
LOCK_ALPHA = 0.05
LOCK_H0_SD = math.sqrt(0.5 * LOCK_ALPHA / (2.0 - LOCK_ALPHA))  # 0.1132

ORDERS = (2, 4, 8)
PATHS = (False, True)  # complex baseband, real IF
PATH_ID = {False: "complex", True: "real"}


def _noise(n, *, real, rng):
    """Signal-free input at unit power, in the dtype the path expects."""
    if real:
        return rng.standard_normal(n).astype(np.float32)
    return (
        (rng.standard_normal(n) + 1j * rng.standard_normal(n)) / math.sqrt(2)
    ).astype(np.complex64)


def esn0_spec_db(m, target=1e-3):
    """The Es/N0 where coherent M-PSK reaches `target` SER, by bisection.

    Anchoring on a fixed SER rather than a fixed Es/N0 asks "does the receiver
    meet its bound" at the same place on the curve for every order -- 6.8 /
    10.3 / 15.7 dB for BPSK / QPSK / 8PSK.
    """
    lo, hi = 0.0, 30.0
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if ber_theory_ser(m, 10.0 ** (mid / 10.0)) > target:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


# ── false alarm ────────────────────────────────────────────────────────────


@pytest.mark.parametrize("real", PATHS, ids=lambda r: PATH_ID[r])
@pytest.mark.parametrize("m", ORDERS)
def test_no_false_declare_on_noise_only(m, real):
    """Signal-free input must not produce a lock declaration, at any order.

    This is the end-to-end form of the false-alarm budget: `lock_thresh = 0.5`
    is 4.42 sigma on a statistic whose H0 sd is 0.1132, i.e. a per-look Pfa of
    5e-6, so across a handful of runs the flag must never rise. Before the lock
    signal was limited this was not merely loose but meaningless at higher M --
    the statistic's noise-only mean at 8PSK sat an order of magnitude ABOVE its
    value at lock.
    """
    sps, m_out, nsym = 16, 4, 4000
    for seed in range(3):
        rng = np.random.default_rng(4000 + 97 * seed + m)
        x = _noise(nsym * sps, real=real, rng=rng)
        _y, pr = demod(x, real=real, sps=sps, m_out=m_out, m=m, fc=IF_FS4)
        assert not np.any(pr["car.locked"] > 0.5), (
            f"m={m} {PATH_ID[real]}: carrier lock declared on noise only "
            f"(peak lock statistic {pr['lock'].max():+.3f} against a 0.5 "
            f"threshold = 4.42 sigma)"
        )


@pytest.mark.parametrize("m", ORDERS)
def test_lock_statistic_h0_sd_matches_the_analytic_value(m):
    """The noise-only sd of the lock EMA must be 0.1132 at EVERY order.

    This identity is the load-bearing one: it is what makes a single
    `lock_thresh` mean a single false-alarm probability regardless of M, and it
    holds only because the lock signal limits `|z|` before taking the M-th
    power. Without the limiter the same 0.5 threshold was 4.4 sigma at BPSK,
    0.9 sigma at QPSK and 0.02 sigma at 8PSK.

    Tolerance is deliberately generous (25%): the point is that the sd does not
    SCALE with M -- a factor of 3 between orders is the failure this catches,
    not a few percent of estimator error.
    """
    # The EMA's memory is N_eff = (2-alpha)/alpha = 39 symbols, so consecutive
    # outputs are strongly correlated -- sample one per 2/alpha symbols to get
    # quasi-independent looks, and pool across runs. Estimating from one final
    # value per run instead gives a ~15% standard error on the sd, which is the
    # same size as the effect being tested.
    sps, m_out, nsym, trials = 16, 4, 3000, 6
    stride = round(2.0 / LOCK_ALPHA)
    looks = []
    for seed in range(trials):
        rng = np.random.default_rng(9000 + 131 * seed + m)
        x = _noise(nsym * sps, real=False, rng=rng)
        _y, pr = demod(x, real=False, sps=sps, m_out=m_out, m=m, fc=IF_FS4)
        lk = pr["lock"]
        looks.extend(float(v) for v in lk[200::stride])
    sd = float(np.std(looks))
    mean = float(np.mean(looks))
    assert sd == pytest.approx(LOCK_H0_SD, rel=0.25), (
        f"m={m}: lock EMA H0 sd {sd:.4f}, analytic {LOCK_H0_SD:.4f}. An sd "
        f"that scales with M means the statistic is not limited, and then no "
        f"single lock_thresh maps to a false-alarm probability."
    )
    # Zero-mean too: a bias is what a missing normalisation looks like, and it
    # is not something averaging can remove.
    assert abs(mean) < 4.0 * sd, f"m={m}: H0 mean {mean:+.4f} is not ~0"


# ── level invariance ───────────────────────────────────────────────────────


@pytest.mark.parametrize("real", PATHS, ids=lambda r: PATH_ID[r])
def test_evm_does_not_track_absolute_level(real):
    """Both receivers AGC-normalise, so EVM must be flat across decades.

    A slope here is a gain-staging bug. The range stops below unity on purpose:
    a cascade that plans a CIC bounds its input to +-1.0 and clips silently
    past it, which costs ~25 dB of EVM that no lock metric reveals -- so the
    top of the range is checked for clipping instead of being pushed into it.
    """
    sps, m_out, nsym, esn0 = 16, 4, 4000, 15.0
    settle = settle_floor()
    evms = {}
    for scale in (0.01, 0.1, 1.0):
        x, idx = make_signal(sps, nsym, real=real, esn0_db=esn0, seed=17)
        x = (x * scale).astype(x.dtype)
        y, _pr = demod(
            x,
            real=real,
            sps=sps,
            m_out=m_out,
            freq_offset=freq_offset_inside_bw(0.01, sps),
        )
        evm, ser, _ = symbol_metrics(y, idx, settle=settle)
        evms[scale] = evm
        # A residual SER is not what this test is about (that is the coherent-
        # bound test); it is here only to catch a scale that stops decoding
        # altogether.
        assert ser < 5e-3, f"scale {scale}: SER {ser} at Es/N0 {esn0} dB"
    spread = max(evms.values()) - min(evms.values())
    assert spread < 2.0, (
        f"{PATH_ID[real]}: EVM varies by {spread:.1f} dB across two decades "
        f"of input level ({evms}); it must not track level at all"
    )


# ── the coherent bound ─────────────────────────────────────────────────────


#: Errors to accumulate before stopping — inverse binomial sampling. The
#: relative standard error of a rate measured this way is 1/sqrt(errors) and
#: depends ONLY on the error count, so this number IS the precision: 200
#: errors is ~7% relative, ~±14% at 95%. Stopping on a fixed SYMBOL count
#: instead makes precision depend on the very rate being measured -- a
#: 20 000-symbol burst at SER 1e-3 yields ~20 errors and ~22% relative
#: error, which reads as real seed-to-seed variation in the receiver, and
#: is exactly how it was misread here.
TARGET_ERRORS = 200

#: Bursts to draw before giving up on TARGET_ERRORS. Hitting this cap means
#: the SER is LOWER than budgeted for -- a pass with a wider interval, not a
#: failure -- so the test reports the achieved count instead of asserting it.
MAX_BURSTS = 40


@pytest.mark.parametrize("real", PATHS, ids=lambda r: PATH_ID[r])
@pytest.mark.parametrize("m", ORDERS)
def test_ser_lands_on_the_coherent_bound(m, real):
    """Measured SER must sit on the theoretical curve at each order's own
    SER = 1e-3 point, within the implementation loss.

    **Stops on ERRORS, not symbols, and compares COHERENT to coherent.** Both
    halves were wrong before, and both inflated the answer:

    * A fixed 20 000-symbol burst yields ~20 errors at this operating point, so
      each measurement carried ~22% relative error. That spread looked like
      sequence dependence in the receiver and was answered with a median over
      seeds, which is a way of averaging out counting noise without admitting
      that is what it is. Fixing the error count to 200 makes the precision ~7%
      by construction (`1/sqrt(r)`), independent of the rate.
    * `symbol_metrics` returns a DIFFERENTIAL SER, and it was being compared
      against a coherent bound. A differential decision fails if either of its
      two symbols is wrong, so it reads ~2x coherent -- measured 1.88 to 2.11
      here. Half of the apparent "implementation loss" was that factor.

    With both fixed, the receivers are 1.2-2.4x the bound, i.e. **0.3-1.0 dB**,
    where the old form reported 2-4.75x and was given a tolerance of 10:

        m=2  complex 1.56 [1.36, 1.78]   real 1.76 [1.54, 2.01]
        m=4  complex 1.55 [1.35, 1.77]   real 1.56 [1.37, 1.78]
        m=8  complex 1.23 [1.08, 1.41]   real 2.35 [2.07, 2.66]

    8PSK on the real path is the only cell above 2x, and it is the one genuine
    steady-state penalty in the matrix.

    The assertion is one-sided on the interval's LOWER limit: it fails only
    when even the most favourable reading of the data exceeds the bound, so it
    cannot flake on counting noise. `m_out = 8` is explicit because the claim
    is about that value, and `sps = 24` follows from `sps > 2*m_out` on the
    real path.
    """
    sps, m_out, nsym = 24, 8, 20000
    esn0 = esn0_spec_db(m)
    expect = ber_theory_ser(m, 10.0 ** (esn0 / 10.0))
    errors = symbols = bursts = 0
    for seed in range(20, 20 + MAX_BURSTS):
        if errors >= TARGET_ERRORS:
            break
        x, idx = make_signal(
            sps, nsym, real=real, m=m, esn0_db=esn0, seed=seed
        )
        y, pr = demod(
            x,
            real=real,
            sps=sps,
            m_out=m_out,
            m=m,
            freq_offset=freq_offset_inside_bw(0.01, sps),
            acq_to_track=1,
            lock_thresh=0.3,
            warmup_syms=300,
        )
        settle = settle_from(pr)
        assert settle is not None, (
            f"m={m} {PATH_ID[real]} seed {seed}: both loops must lock at the "
            f"SER=1e-3 operating point"
        )
        got = coherent_errors(y, idx, m, settle)
        assert got is not None, (
            f"m={m} {PATH_ID[real]} seed {seed}: only {len(y) - settle} "
            f"symbols after settling ({settle}); lengthen the burst"
        )
        errors += got[0]
        symbols += got[1]
        bursts += 1

    assert errors >= 20, (
        f"m={m} {PATH_ID[real]}: only {errors} errors in {symbols} symbols "
        f"over {bursts} bursts -- too few for any interval at all"
    )
    p_hat, lo, _hi = ser_confidence(errors, symbols)
    assert lo < 4.0 * expect, (
        f"m={m} {PATH_ID[real]}: SER {p_hat:.3e} = {p_hat / expect:.2f}x the "
        f"coherent bound at Es/N0 {esn0:.1f} dB, 95% lower limit "
        f"{lo / expect:.2f}x, from {errors} errors in {symbols} symbols "
        f"({bursts} bursts). More than ~1.5 dB of implementation loss."
    )
