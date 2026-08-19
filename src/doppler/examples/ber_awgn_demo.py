"""Measuring an M-PSK error rate in AWGN, defensibly.

Puts `doppler.ber` on an ideal AWGN channel — no receiver in the loop — so
what is on trial is the MEASUREMENT, not a demodulator. Two questions:

1. Does the measured symbol error rate land on the coherent bound, at every
   constellation, with an interval that actually contains it?
2. Why stop on a fixed number of ERRORS rather than a fixed number of symbols?

The second is the whole reason `BerMeter` exists. Under inverse binomial
sampling — fix the errors `r`, let the symbol count `N` be what falls out —
the relative standard error is `1/sqrt(r)`, a function of the error count
ALONE. Stop on a fixed `N` instead and the precision depends on the very rate
you are trying to measure: it silently degrades as the rate falls, and the
widening scatter reads as real variation in the thing under test.

Run:
    uv run python src/doppler/examples/ber_awgn_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np

from doppler.ber import BerMeter, ber_esn0_db_for_ser, ber_theory_ser
from doppler.source import AWGN
from doppler.wfm import wfm_awgn_amplitude

#: Symbols per block handed to the meter at a time.
BLOCK = 50_000
#: Marker length used to DETECT the alignment (never to search for it).
MARKER = 256
#: Truth index the marker starts at, clear of the block edge.
MARKER_T0 = 300
#: Errors per point under inverse binomial sampling: ~7% relative.
TARGET_ERRORS = 200
#: Symbols per point under the fixed-N rule we compare against.
FIXED_SYMBOLS = 20_000


# --8<-- [start:measure]


def measure(m, esn0_db, target_errors=200, max_symbols=8_000_000, seed=1):
    """Symbol error rate at `esn0_db`, stopping on ERRORS not symbols.

    Es/N0 is set at the symbol: unit-energy M-PSK points plus complex AWGN of
    per-quadrature variance `sigma^2`, so `Es/N0 = 1/(2 sigma^2)`.

    The transmitted stream is rolled by an unknown lag and spun by an unknown
    phase, both of which `align()` must recover on its own. That is the point:
    it DETECTS the alignment by correlating a known marker under a false-alarm
    gate, rather than searching for the lag and rotation that minimise the
    error count. A search would be an optimisation over the answer -- it can
    find a lucky alignment on garbage, and it can miss the true one on a
    healthy stream and report chance.
    """
    rng = np.random.default_rng(seed)
    lag, phase = 9, 0.7  # unknown to the meter
    # The amplitude comes from the library's own law rather than a
    # transcription. At sps = 1 there is no pulse and no oversampling, so
    # Es/N0 and the per-sample SNR are the SAME number -- which is why one
    # call with unit signal power serves an Es/N0 axis here and would not in
    # an oversampled demo. `sqrt(0.5 / 10**(esn0/10))` said that implicitly,
    # in an expression where the 0.5 is the I/Q split and nothing names the
    # sps = 1 assumption at all.
    noise = AWGN(seed=seed, amplitude=wfm_awgn_amplitude(esn0_db, 1.0))
    meter = BerMeter(m=m, target_errors=target_errors, conf=0.99)

    while not meter.enough and meter.symbols < max_symbols:
        truth = rng.integers(0, m, 50_000).astype(np.uint8)
        phi0 = np.pi / 4 if m == 4 else 0.0
        clean = np.exp(1j * (2 * np.pi * truth / m + phi0 + phase))
        rx = (np.roll(clean, -lag) + noise.generate(truth.size)).astype(
            np.complex64
        )

        meter.set_truth(truth)
        if not meter.align(rx, t0=300, n_marker=256):
            continue  # no detection -> contribute nothing, never a guess
        # Score only AFTER the marker: the symbols that fixed the alignment
        # must not also be scored, or they flatter the rate.
        meter.score(rx, lo=300 + 256 - meter.lag, hi=truth.size)

    return meter.ser()


# --8<-- [end:measure]


def measure_fixed_n(m, esn0_db, n_symbols=FIXED_SYMBOLS, seed=1):
    """The same measurement stopped on a fixed SYMBOL count, for contrast."""
    rng = np.random.default_rng(seed)
    lag, phase = 9, 0.7
    noise = AWGN(seed=seed, amplitude=wfm_awgn_amplitude(esn0_db, 1.0))
    # target_errors is unreachable, so `enough` never trips and the loop is
    # bounded by the symbol budget instead -- exactly the fixed-N habit.
    meter = BerMeter(m=m, target_errors=10**9, conf=0.99)

    while meter.symbols < n_symbols:
        truth = rng.integers(0, m, BLOCK).astype(np.uint8)
        phi0 = np.pi / 4 if m == 4 else 0.0
        clean = np.exp(1j * (2 * np.pi * truth / m + phi0 + phase))
        rx = (np.roll(clean, -lag) + noise.generate(truth.size)).astype(
            np.complex64
        )
        meter.set_truth(truth)
        if not meter.align(rx, t0=MARKER_T0, n_marker=MARKER):
            continue
        lo = MARKER_T0 + MARKER - meter.lag
        hi = min(truth.size, lo + (n_symbols - meter.symbols))
        meter.score(rx, lo=lo, hi=hi)
    return meter.ser()


def main(out_path="ber_awgn_demo.png"):
    orders = (2, 4, 8)
    colors = {2: "#2e6f9e", 4: "#c1666b", 8: "#4f9d69"}

    fig, (ax_curve, ax_prec) = plt.subplots(1, 2, figsize=(11.5, 4.6))

    for m in orders:
        anchor = ber_esn0_db_for_ser(m, 1e-3)
        # Bounded above so every point can actually REACH its error target
        # inside the symbol budget -- a truncated point would be a fixed-N
        # measurement wearing an inverse-binomial label.
        sweep = np.round(anchor + np.array([-4.0, -2.0, 0.0, 1.0]), 2)

        meas, lo, hi, rel_ib, rel_fx = [], [], [], [], []
        for db in sweep:
            r = measure(m, float(db), TARGET_ERRORS, seed=int(db * 7) + m)
            meas.append(r.p_hat)
            lo.append(r.p_hat - r.lo)
            hi.append(r.hi - r.p_hat)
            rel_ib.append((r.hi - r.lo) / (2.0 * r.p_hat))

            f = measure_fixed_n(m, float(db), seed=int(db * 7) + m)
            # A fixed-N run that catches no errors has no point estimate; its
            # interval is one-sided and effectively unbounded above.
            rel_fx.append(
                (f.hi - f.lo) / (2.0 * f.p_hat) if f.errors >= 2 else np.nan
            )

            # PHYSICAL CHECK, not decoration: an ideal channel has no
            # implementation loss, so the exact 99% interval must contain the
            # coherent bound. If it does not, the measurement is wrong.
            th = ber_theory_ser(m, 10 ** (float(db) / 10.0))
            assert r.lo <= th <= r.hi, (
                f"M={m} {db} dB: theory {th:.3e} outside "
                f"[{r.lo:.3e}, {r.hi:.3e}] (r={r.errors} N={r.symbols})"
            )
            assert r.errors >= TARGET_ERRORS, (
                f"M={m} {db} dB: stopped at {r.errors} errors"
            )

        fine = np.linspace(sweep[0] - 0.5, sweep[-1] + 0.5, 200)
        ax_curve.semilogy(
            fine,
            [ber_theory_ser(m, 10 ** (d / 10.0)) for d in fine],
            color=colors[m],
            lw=1.2,
            alpha=0.55,
        )
        ax_curve.errorbar(
            sweep,
            meas,
            yerr=[lo, hi],
            fmt="o",
            ms=5,
            capsize=3,
            color=colors[m],
            label=f"{m}-PSK",
        )

        ax_prec.plot(
            sweep, np.array(rel_ib) * 100, "o-", color=colors[m], lw=1.6
        )
        ax_prec.plot(
            sweep,
            np.array(rel_fx) * 100,
            "s--",
            color=colors[m],
            lw=1.2,
            alpha=0.6,
        )

    ax_curve.set_xlabel("$E_s/N_0$ (dB)")
    ax_curve.set_ylabel("symbol error rate")
    ax_curve.set_title(
        "Measured SER sits on the coherent bound\n"
        "(points: 200 errors each, bars: exact 99% interval)",
        fontsize=10,
    )
    ax_curve.grid(alpha=0.3, which="both")
    ax_curve.legend(fontsize=9)

    # The nominal 200-error half-width. Inverse-binomial points sit AT or
    # BELOW it at every rate (below only because a 50k block overshoots the
    # target at high SER); fixed-N points blow straight through it.
    ax_prec.axhline(18.3, color="0.35", lw=1.0, ls=":")
    ax_prec.text(
        2.6, 19.6, "nominal half-width at 200 errors", fontsize=8, color="0.35"
    )
    ax_prec.set_xlabel("$E_s/N_0$ (dB)")
    ax_prec.set_ylabel("half-width of the 99% interval (% of the rate)")
    ax_prec.set_title(
        "Stopping on ERRORS holds precision; stopping on\n"
        "symbols (dashed) loses it as the rate falls",
        fontsize=10,
    )
    ax_prec.set_yscale("log")
    ax_prec.grid(alpha=0.3, which="both")

    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
