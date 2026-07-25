"""ratesync_demo.py — arbitrary-rate reception on a matched-filter cascade.

Builds RRC-shaped BPSK at a deliberately awkward **non-integer** samples per
symbol (17.33389 — an ADC clock with no rational relationship to the symbol
clock), offsets the clock by 200 ppm on top of that, adds noise, and hands the
stream to :class:`doppler.track.RateSync`.

`RateSync` owns a :class:`doppler.resample.MatchedRateConverter` whose
**terminal stage carries the pulse**, so the cascade's last dot product *is*
the matched filter and the polyphase arm it selects *is* the fractional
timing delay. One filter, no Farrow. Two things follow, and they are what
this demo shows:

  * `sps` is a `double`, because the terminal stage's accumulator is one. No
    integer relationship between the two clocks is required anywhere.
  * The bank is sized by the **post-decimation** rate, because the cascade's
    HB/CIC stages do the bulk decimation at no multiplies. A matched filter at
    256 input samples per symbol costs the same as one at 4.

Three views (saved to a PNG):
  * **Recovered constellation** — matched-filtered symbols at a fractional
    sps, tight on +/-1 once the loop locks.
  * **Tracked clock** — recovered samples/symbol converging onto the true
    (non-integer, offset) rate.
  * **Matched-filter cost** — taps per arm against input samples per symbol,
    against what the same filter would cost applied at the input rate.

Run:  python -m doppler.examples.ratesync_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.track import RateSync

SPS = 17.33389  # non-integer samples per symbol -- the whole point
BETA = 0.35
SPAN = 8
NSYM = 1500
CLOCK_PPM = 200.0  # the true rate is this far off the nominal
ES_N0_DB = 15.0  # symbol energy to noise density -- NOT a per-sample SNR


def rrc_h(t, beta=BETA):
    """Analytic RRC pulse at arbitrary (non-grid) times, in symbol periods."""
    t = np.asarray(t, float)
    out = np.empty_like(t)
    pt = np.pi * t
    at_zero = np.abs(t) < 1e-9
    at_sing = (
        np.abs(np.abs(t) - 1.0 / (4 * beta)) < 1e-9
        if beta > 0
        else np.zeros(t.shape, bool)
    )
    gen = ~(at_zero | at_sing)
    out[at_zero] = 1.0 - beta + 4.0 * beta / np.pi
    if at_sing.any():
        a = np.pi / (4 * beta)
        out[at_sing] = (beta / np.sqrt(2)) * (
            (1 + 2 / np.pi) * np.sin(a) + (1 - 2 / np.pi) * np.cos(a)
        )
    tg, ptg = t[gen], pt[gen]
    num = np.sin(ptg * (1 - beta)) + 4 * beta * tg * np.cos(ptg * (1 + beta))
    out[gen] = num / (ptg * (1 - (4 * beta * tg) ** 2))
    return out


def make_signal(sps, tau=0.37, seed=7, es_n0_db=ES_N0_DB):
    """RRC-shaped BPSK at a fractional sps, offset by tau, in AWGN.

    Noise is set from **Es/N0**, the symbol energy over the noise density,
    because "SNR" alone is ambiguous here: at 17.33 samples per symbol a
    per-sample SNR is ~12 dB below the Es/N0 the receiver actually sees, so
    the same number means two very different links.

    The noise is **complex**, N0 total (N0/2 per dimension), because a real
    receiver's baseband is complex and its Q channel carries noise even when
    the modulation is real. Injecting real-only noise instead is the same
    thing as discarding Q, and it flatters the reported EVM by 3 dB against
    the convention everyone else quotes -- EVM is measured on the I/Q plane
    unless it says otherwise.

    Amplitude is kept well inside +-1.0: the planned cascade contains a CIC,
    which bounds its input and clips silently past that (RateSync.clipped is
    the only signal, and this demo asserts it stays clear).
    """
    rng = np.random.default_rng(seed)
    syms = np.where(rng.integers(0, 2, NSYM) > 0, 1.0, -1.0)
    n = int(NSYM * sps) + 64
    idx = np.arange(n, dtype=float)
    x = np.zeros(n)
    for k, a in enumerate(syms):
        t = (idx - (k + SPAN) * sps) / sps - tau
        near = np.abs(t) <= SPAN
        x[near] += a * rrc_h(t[near])
    x *= 0.25 / np.max(np.abs(x))  # headroom for the CIC's +-1.0 bound
    # Average symbol energy from the STEADY-STATE span only. Including the
    # ramp-up and the tail padding underestimates the power, which would
    # quietly set a higher Es/N0 than advertised -- and the giveaway is an
    # EVM that beats the matched-filter bound, which nothing can do.
    core = x[int(SPAN * sps) : -int(SPAN * sps)]
    es = np.mean(core**2) * sps
    n0 = es / 10 ** (es_n0_db / 10)
    noise = (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * np.sqrt(
        n0 / 2
    )
    return (x + noise).astype(np.complex64), syms


def evm_floor_db(es_n0_db=ES_N0_DB):
    """Matched-filter EVM bound on the I/Q plane.

    At the matched-filter output the error vector is the complex noise, of
    total variance N0, against a reference of energy Es -- so
    EVM^2 = N0/Es and the bound in dB is simply -(Es/N0). (The familiar
    factor of two belongs to an I-only measurement, which discards the Q
    channel; EVM is a plane quantity unless stated otherwise.)
    """
    return -es_n0_db


# The receiver: one object, one call. `sps` is the NOMINAL rate -- the loop
# finds the true one, which is 200 ppm away.
true_sps = SPS * (1.0 + CLOCK_PPM * 1e-6)
rx, tx_syms = make_signal(true_sps)

sync = RateSync(sps=SPS, pulse="rrc", beta=BETA, span=SPAN, m=2, bn=0.005)
symbols = np.asarray(sync.steps(rx))
# --8<-- [end:signal]


def _evm_db(y):
    """EVM against the LS-scaled hard decision, over the settled tail.

    Judge LOCK by ``lock_stat``, not by this: a window containing a single
    acquisition cycle slip reads ~20 dB worse with the eye wide open.
    """
    y = np.asarray(y)[3 * len(y) // 4 :]
    d = np.where(y.real >= 0, 1.0, -1.0)
    g = float(np.dot(d, y.real) / len(d))
    return 20 * np.log10(
        float(np.linalg.norm(y - g * d) / (abs(g) * np.sqrt(len(d))))
    )


def track_rate(sps, chunk=4096):
    """Re-run in chunks, sampling the tracked clock as the loop converges."""
    sig, _ = make_signal(sps * (1.0 + CLOCK_PPM * 1e-6))
    obj = RateSync(sps=sps, pulse="rrc", beta=BETA, span=SPAN, m=2, bn=0.005)
    rates = []
    for i in range(0, len(sig), chunk):
        obj.steps(sig[i : i + chunk])
        rates.append(obj.rate)
    return np.array(rates)


# --8<-- [start:cost]
from doppler.resample import MatchedRateConverter  # noqa: E402


# Why a high input rate is nearly free: the cascade's HB/CIC stages do the
# bulk decimation at no multiplies, so the matched filter is sized by the
# POST-decimation rate. Applied at the input rate it would grow with sps.
def bank_cost(sps_values, span=SPAN, m=2):
    """(taps/arm on the cascade, taps/arm at the input rate) per sps."""
    cascade, at_input = [], []
    for s in sps_values:
        rc = MatchedRateConverter(
            rate=m / s,
            compensate=1,
            pulse="rrc",
            span=span,
            pulse_sps=float(m),
        )
        cascade.append(rc.bank_shape[1])
        at_input.append(int(np.ceil((2 * span + 1.0 / m) * s)) + 1)
    return np.array(cascade), np.array(at_input)


# --8<-- [end:cost]


def main(out_path="ratesync_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    evm = _evm_db(symbols)
    rates = track_rate(SPS)
    sps_grid = np.array([4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0])
    cascade_taps, input_taps = bank_cost(sps_grid)

    fig, axes = plt.subplots(3, 1, figsize=(9, 11))

    axes[0].plot(symbols.real, ".", ms=2, alpha=0.6)
    axes[0].axhline(0, lw=0.5, color="k", alpha=0.3)
    axes[0].set_title(
        f"Recovered symbols — sps = {SPS} (+{CLOCK_PPM:g} ppm), "
        f"Es/N0 {ES_N0_DB:g} dB\n"
        f"settled EVM {evm:.1f} dB   (matched-filter bound "
        f"{evm_floor_db():.1f} dB)"
    )
    axes[0].set_xlabel("symbol index")
    axes[0].set_ylabel("Re")
    axes[0].grid(alpha=0.3)

    axes[1].plot(rates, lw=1.2, label="RateSync.rate")
    axes[1].axhline(
        SPS * (1 + CLOCK_PPM * 1e-6), ls="--", color="C3", label="true rate"
    )
    axes[1].axhline(SPS, ls=":", color="grey", label="nominal sps")
    axes[1].set_title("Tracked clock — the loop finds a rate it was not given")
    axes[1].set_xlabel("chunk")
    axes[1].set_ylabel("samples / symbol")
    axes[1].legend(loc="best", fontsize=8)
    axes[1].grid(alpha=0.3)

    axes[2].semilogy(
        sps_grid,
        input_taps,
        "o--",
        color="C3",
        label="matched filter at the input rate",
    )
    axes[2].semilogy(
        sps_grid,
        cascade_taps,
        "o-",
        color="C0",
        label="on the cascade's terminal stage",
    )
    axes[2].set_title(
        "Matched-filter cost\nsized by the post-decimation rate, "
        "not the input rate"
    )
    axes[2].set_xlabel("input samples per symbol")
    axes[2].set_ylabel("taps per arm")
    axes[2].set_xscale("log", base=2)
    axes[2].legend(loc="best", fontsize=8)
    axes[2].grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path}")

    # ---- self-validation: exit 0 must mean demonstrated AND checked ----
    # Locked, judged by the eye statistic rather than an EVM window.
    assert sync.locked, f"timing never locked (lock_stat {sync.lock_stat:.3f})"
    assert sync.lock_stat > 0.55, f"lock_stat {sync.lock_stat:.3f}"
    # The front end stayed inside the CIC's +-1.0 input bound.
    assert not sync.clipped, "front end overdriven -- the CIC clipped"
    # The loop found the true rate, which is 200 ppm off the nominal it was
    # built with, at a non-integer sps.
    target = SPS * (1 + CLOCK_PPM * 1e-6)
    assert abs(sync.rate - target) < 0.01, (
        f"tracked rate {sync.rate:.5f} != true {target:.5f}"
    )
    assert abs(sync.rate - SPS) > 1e-4, "rate never moved off the nominal"
    # Data recovery on the settled tail, checked against THEORY rather than an
    # arbitrary threshold: a matched filter cannot beat EVM^2 = 1/(2*Es/N0),
    # and a correct one gets within a decibel or two of it. The lower bound
    # matters as much as the upper -- beating the bound would mean the
    # measurement is wrong, not that the receiver is brilliant.
    # A single 375-symbol window carries ~0.4 dB of estimator noise (the
    # relative std of a variance estimate is sqrt(2/N)), so one seed can land
    # either side of the bound; over 12 seeds the mean sits 0.03 dB from it.
    # +-1.5 dB is three sigma on one seed, not a fudge factor.
    # ...and the RELATION, not just one point: EVM should track -(Es/N0)
    # across operating points. Two extra points cost ~1 s and turn "it hit
    # the bound once" into "it is on the bound".
    for esn0 in (10.0, 20.0):
        sig, _ = make_signal(SPS * (1 + CLOCK_PPM * 1e-6), es_n0_db=esn0)
        obj = RateSync(
            sps=SPS, pulse="rrc", beta=BETA, span=SPAN, m=2, bn=0.005
        )
        got = _evm_db(np.asarray(obj.steps(sig)))
        assert abs(got - evm_floor_db(esn0)) < 1.5, (
            f"Es/N0 {esn0:g} dB: EVM {got:.1f} dB, bound {-esn0:.1f} dB"
        )

    floor = evm_floor_db()
    assert abs(evm - floor) < 1.5, (
        f"EVM {evm:.1f} dB is {evm - floor:+.1f} dB from the matched-filter "
        f"bound {floor:.1f} dB"
    )
    assert (
        len(symbols) == int(NSYM * (1 + CLOCK_PPM * 1e-6))
        or abs(len(symbols) - NSYM) < 0.03 * NSYM
    ), f"{len(symbols)} symbols recovered, expected ~{NSYM}"
    # The cost claim the third panel makes: bounded on the cascade, growing
    # at the input rate. The only step is +6 taps, which is the CIC droop
    # compensator folding into the bank once the planner picks a CIC (a
    # halfband cascade has no droop to correct) -- not growth with sps.
    assert cascade_taps.max() - cascade_taps.min() <= 6, (
        f"bank grew with sps: {cascade_taps}"
    )
    assert cascade_taps.max() < 64, f"bank unexpectedly large: {cascade_taps}"
    assert input_taps[-1] > 50 * cascade_taps[-1], (
        f"input-rate filter only {input_taps[-1]} taps vs {cascade_taps[-1]}"
    )
    print(
        f"validated: locked (lock_stat {sync.lock_stat:.3f}), rate "
        f"{sync.rate:.5f} vs true {target:.5f}, EVM {evm:.1f} dB "
        f"({evm - floor:+.1f} dB from the {floor:.1f} dB bound), "
        f"bank {cascade_taps[0]} taps/arm at every sps "
        f"(vs {input_taps[-1]} at the input rate, sps=256)"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ratesync_demo.png")
