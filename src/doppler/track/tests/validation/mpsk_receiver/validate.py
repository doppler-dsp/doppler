#!/usr/bin/env python3
"""MpskReceiver validation — produces this folder's certification evidence.

Writes ``results.md`` (the authoritative report), the plots it embeds, and
the raw sweeps under ``data/`` so any number in the report can be re-derived
without re-running the measurement.

Three phases, in order:

1. **Characterise** — measure complete behaviour across the input range and
   over time. Tables and plots, no verdicts.
2. **Review** — judge the characterisation: correct-by-design, or a gap.
3. **Limits** — the envelope a caller may rely on, asserted.

Every number is measured from the C through its own binding, and nothing
here models what the C ought to do. The stimulus is the library's own
(``doppler.mpsk.mpsk_map`` for the constellation, so the labelling is the
one ``dp_ber_score`` inverts); the measurement is the library's own
(``ber_theory_ser`` for the bound, ``ber_evm_db`` self-referenced,
``snr_m2m4_db`` blind). The harness owns no estimator and no level
convention of its own — ``docs/design/rx-test.md`` goals 5 and 10, which
``check_stimulus_sources.py`` already enforces.

**The header is the SSOT and it was read first.** ``mpsk_receiver_core.h``
states the construction contract, what the cascade buys, and where the NDA
discriminator reads; ``test_mpsk_receiver_core.c`` pins what a unit test can
pin. This report measures the LAWS those two state in prose — the ones a
point assertion cannot express — and section 1's claim table says, for every
prose claim, which of the three carries it.

Claims that do NOT appear as Python measurements are reported rather than
skipped, and in two different kinds. **C-ONLY** names what carries a claim
no binding reaches: ``mpsk_rx_derive_m_out`` and
``mpsk_rx_updates_per_symbol`` are ``JM_FORCEINLINE``, so claims about them
are reported with the C section that covers them (F1). Measuring the
receiver and calling it the rule is exactly the substitution
``docs/dev/contributing/validation.md`` warns about.

**F7** is the other kind: claims a binding DOES reach and nothing measures,
filed as gh-814 so they are visible outside this file.

The ``nda_tap`` trade is no longer a trade: the discriminator reads the
on-time strobe and there is no other node to choose. What ``mf_in`` cost, and
how a ranking read off the wrong waveform came to pin it in the first place,
is recorded in ``docs/design/mpsk.md`` §3.3 rather than measured here — the
measurement ranked three taps, and two of them no longer exist.

**Two things about the sweep design are load-bearing, and both were wrong
once.** The Es/N0 grid is DERIVED per M from the bound and the record
length (:func:`_esn0_grid`), because one grid across all three orders puts
BPSK where it makes no errors and 8PSK where it makes thousands — and a
limit over a cell with no errors is satisfied by any receiver at all (F6).
And every bound goes through :func:`_bound`, because ``ber_theory_ser``
takes LINEAR Es/N0 and this file passed dB, which made every theory figure
the bound at the wrong operating point (F8).

Run:  make validate
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from doppler.ber import (
    BerMeter,
    ber_evm_db,
    ber_settle_syms,
    ber_theory_ser,
)
from doppler.mpsk import mpsk_map
from doppler.snr import snr_m2m4_db
from doppler.tests._validation_common import Report, clamp_evm_db, cli
from doppler.track import MpskReceiver
from doppler.track.tests._mpsk_rx_harness import freq_offset_inside_bw

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"

#: Amplitude is 0.5, not 1.0, and the reason is the same one
#: ``test_mpsk_receiver_core.c``'s header gives: a cascade that plans a CIC
#: bounds its input to +-1.0 and clips silently past it, costing ~25 dB of
#: EVM that no lock metric reveals. A unit-amplitude constellation plus
#: noise sits right on that edge.
TX_AMP = 0.5

#: The blind alignment's marker length, lag span and false-alarm probability.
#: These are the meter's own gate, not a tolerance chosen here: 256 symbols of
#: truth fix the lag and the M-fold phase, +-64 covers the cascade's group
#: delay, and 1e-6 is the per-detection Pfa the C harnesses use.
ALIGN_SYMS = 256
ALIGN_LAG_SPAN = 64
ALIGN_PFA = 1e-6

#: The meter's stopping rule, shared with every other harness so an interval
#: quoted here means what it means everywhere else.
TARGET_ERRORS = 100
CONF = 0.95

#: Symbols per Es/N0 cell. 20 000 puts a 1e-3 SER's own standard error near
#: 7%, so the tolerances below are about the receiver rather than about how
#: long the sweep ran.
NSYM = 20000

SPS = 8.0
PHI0 = {2: 0.0, 4: np.pi / 4, 8: 0.0}


# ── stimulus ─────────────────────────────────────────────────────────────


def _signal(
    m, esn0_db, sps=SPS, nsym=NSYM, freq_offset=0.0, seed=0, amp=TX_AMP
):
    """One M-PSK record at a stated matched-filter Es/N0.

    The constellation comes from ``mpsk_map`` rather than from an exp()
    written here, so the labelling is the library's canonical one — the
    same map ``dp_ber_score`` inverts. `sps` may be any double: the symbol
    index is ``floor(n / sps)``, so a non-integer rate puts the symbol
    boundary between samples rather than resampling the stimulus.

    Noise is per-sample with variance set so the MATCHED-FILTER Es/N0 is
    the number asked for: Es = amp^2 * sps, so N0 = Es / (Es/N0).
    """
    rng = np.random.default_rng(seed)
    lab = rng.integers(0, m, nsym).astype(np.uint8)
    syms = (mpsk_map(lab, m) * amp).astype(np.complex64)

    n = int(nsym * sps)
    idx = np.minimum((np.arange(n) / sps).astype(int), nsym - 1)
    x = syms[idx].astype(np.complex64)
    if freq_offset:
        # Stated in cycles per SYMBOL, like both loop bandwidths; the
        # stimulus advances per SAMPLE, so it is converted here and
        # nowhere else.
        x = x * np.exp(2j * np.pi * (freq_offset / sps) * np.arange(n))

    es = amp**2 * sps
    var = es / 10 ** (esn0_db / 10.0)
    x = x + (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * np.sqrt(
        var / 2
    )
    return np.ascontiguousarray(x.astype(np.complex64)), lab


def _gray_to_index(lab, m):
    """Gray label -> constellation index, which is what `BerMeter` scores on.

    `mpsk_map` takes a **Gray label**; `BerMeter.set_truth` documents its
    argument as "the transmitted symbol INDICES (0..m-1, not Gray labels)".
    They agree only at M = 2, and feeding labels straight in does not fail
    loudly -- it reads SER 0.504 on QPSK and 0.753 on 8PSK at 20 dB Es/N0,
    which looks like a receiver that cannot demodulate rather than a harness
    comparing two different alphabets. It also costs the ALIGNMENT 6 dB of
    margin (+5.2 dB against +11.5), so the detector degrades quietly first.
    """
    b = lab.astype(np.int32).copy()
    sh = 1
    while sh < 8:
        b ^= b >> sh
        sh <<= 1
    return np.ascontiguousarray((b % m).astype(np.uint8))


def _score(out, lab, m, lo):
    """SER over a settled window, through the SHIPPED meter.

    This replaces a genie estimator that searched `m` rotations x 81 lags for
    the MINIMUM error rate. That helper could not refuse: handed a record with
    no alignment at all it still returned the best of 324 tries, so a receiver
    that never locked scored a plausible number instead of declining to be
    measured, and findings were written against numbers produced that way.

    `BerMeter` is the library's own alignment and scorer, and the difference
    that matters is `align_ok`: its detection carries a false-alarm gate, so
    "not aligned" is an OUTCOME rather than the largest of a pile of guesses.

    **The marker goes past the settling point, not at index 0.** A blind
    marker borrows a stretch of truth to fix the lag and the M-fold phase, and
    at `t0 = 0` that stretch lands in the cold-start transient where there is
    no constellation to correlate against -- measured, every cell then refuses
    at about -5 dB margin while `lock` reads 0.82 to 0.97, which is the
    harness failing and looking like the receiver failing. Placed at `lo` it
    detects at +10.9 dB with `lag = 1`, the cascade's group delay.

    Returns
    -------
    (ser, meter)
        `ser` is None when the meter refused to align, or when the record is
        too short to hold a marker and a scored window. The meter comes back
        either way so the caller can report `align_margin_db` rather than just
        the verdict.
    """
    hi = out.size - out.size // 8
    t0 = int(lo)
    if t0 + ALIGN_SYMS + 400 >= hi:
        return None, None
    seg = np.ascontiguousarray(out.astype(np.complex64))
    mtr = BerMeter(m=m, target_errors=TARGET_ERRORS, conf=CONF)
    mtr.set_truth(_gray_to_index(lab, m))
    mtr.align(seg, t0, ALIGN_SYMS, 0, ALIGN_LAG_SPAN, ALIGN_PFA)
    if not mtr.align_ok:
        return None, mtr
    # Past the marker's END: the symbols that fixed the alignment must not
    # also be scored.
    mtr.score(seg, t0 + ALIGN_SYMS, hi)
    return (mtr.ser().p_hat if mtr.symbols else None), mtr


def _limit_ser(R, ser, thresh, subject, claim_fn):
    """Record one SER limit, where a REFUSAL fails instead of vanishing.

    Every SER in this report can come back `None` — `_score` returns it
    when `BerMeter` will not defend an alignment — and a limit written as
    a bare `ser < thresh` does one of two wrong things with that: raises
    `TypeError`, or, if the caller guards with `if ser is not None`,
    silently drops the row so the tally still reports every limit holding.
    The second is worse, because it reads as coverage.

    A refusal is neither a pass nor a regression. It is a claim the meter
    declined to establish, so it is recorded as a FAILING limit whose text
    says exactly that — the certified envelope is not established until
    something measures the cell.

    This is the only place that decides it, so the three call sites cannot
    drift into three different answers.

    Parameters
    ----------
    R : Report
        The report accumulating limits.
    ser : float or None
        The measured symbol error rate, or None if the meter refused.
    thresh : float
        The upper bound the SER must sit under.
    subject : str
        Names the cell, e.g. ``"sps=31.7"``. Used to word the refusal.
    claim_fn : callable
        ``claim_fn(ser) -> str``, the claim text when there IS a number.
        A callable rather than a string because the text embeds the value,
        which does not exist on the refusal path.

    Returns
    -------
    bool
        Whether the limit holds.
    """
    if ser is None:
        return R.limit(
            False,
            f"{subject}: the meter REFUSED this cell, so the claim is "
            f"unestablished rather than met",
        )
    return R.limit(ser < thresh, claim_fn(ser))


#: Errors a cell needs before its rate is a measurement of the receiver.
#:
#: `BerMeter` is built `target_errors = 100`, which is the library's own
#: statement of how many it wants, and at `NSYM` per cell only the highest
#: rates get there -- so a floor has to be chosen rather than inherited.
#:
#: 10 is where the point estimate's own standard error, `1/sqrt(k)`, falls
#: to ~32% of the rate. The bound is steep in Es/N0, so 32% of a RATE is a
#: few tenths of a dB of LOSS, which is well inside the `LOSS_MAX_DB` the
#: limits assert; at 3 errors it is 58% and the estimate moves by a factor
#: of two on the next seed.
#:
#: The interval-width test this replaced was `hi <= 10 * theory`, and it
#: was too weak in the direction that matters. It admitted M = 4 at 12 dB,
#: which had **3 errors** and a point estimate of 1.97e-4 against a bound
#: of 5.32e-4 -- a loss of **-1.86 dB**, i.e. the cell reported the
#: receiver beating the matched-filter bound. Nothing can beat that bound,
#: so a negative loss is the estimator talking, and a limit written over
#: it certifies noise. An error COUNT catches that directly where an
#: interval ratio did not, because the ratio was being compared against a
#: bound that is itself tiny.
MIN_ERRORS = 10

#: The envelope §4 asserts, in the unit §2.1 already computes. A RATE ratio
#: has no fixed dB meaning -- the same 10x is +1.34 dB where the curve is
#: shallow and a fraction of a dB where it is steep -- so the count above
#: decides measurability and the dB is what a caller designs to.
LOSS_MAX_DB = 3.0


#: Expected errors a cell is PLANNED for, against `MIN_ERRORS` measured.
#:
#: 4x the floor, so a cell chosen by @ref _esn0_grid still resolves when the
#: receiver lands a little better than the bound (fewer errors than planned)
#: or the seed is unlucky. Planning exactly at the floor puts half the cells
#: under it.
PLAN_ERRORS = MIN_ERRORS * 4

#: Symbols actually SCORED per cell, which is what sets the error count --
#: not `NSYM`. `_score` measures over `[lo + ALIGN_SYMS, hi)` where `hi`
#: drops the last eighth, so the usable count is smaller than the record
#: and the grid must be planned on the smaller number.
NSCORED = int(NSYM - NSYM // 8) - ALIGN_SYMS - int(ber_settle_syms(0.01, 0.01))


#: The top of the operating window this report certifies over.
#:
#: A measurability ceiling alone is not enough, because the two ends of the
#: order axis run away in opposite directions. BPSK's bound falls off a
#: cliff -- 4.1e-3 at 3.5 dB and 3.2e-5 by 8 -- so its window is narrow and
#: LOW. 8PSK's barely falls at all: `ber_theory_ser(8, e)` still predicts 43
#: errors in `NSCORED` symbols at **30.5 dB**, so an uncapped rule would
#: certify 8PSK at an Es/N0 no link runs at and no other section measures.
#: 20 dB is where the rest of this report already stops (§2.2, §2.3), so the
#: cap keeps the sections comparable.
ESN0_MAX_DB = 20.0


def _bound(m, esn0_db):
    """The coherent bound at an Es/N0 in **dB**.

    One conversion site, because there was none and the report was wrong
    for it. `ber_theory_ser`'s second argument is LINEAR Es/N0 — the
    header says so in capitals ("at matched-filter Es/N0 (LINEAR)") and
    every other caller in the tree writes `10 ** (db / 10)`, from
    `ber_esn0_db_for_ser.c` to `ber_awgn_demo.py`. This validator passed
    dB, so every "SER theory" it printed was the bound at
    `10*log10(esn0_db)` dB: the 8 dB row was scored against the 9.03 dB
    bound and the 16 dB row against the 12.04 dB one.

    The consequence was not a uniform offset but a FOLD — the map
    `db -> 10*log10(db)` is compressive, so the whole sweep collapsed into
    9-13 dB and the error grew in opposite directions at the two ends. It
    read as a receiver that fell behind the bound at low Es/N0 and caught
    up at high, which is a plausible-looking shape and is why it survived:
    at M = 8 it made the object look 20 dB BETTER than a bound that was
    really the 12 dB one (F8).
    """
    return float(ber_theory_ser(m, 10.0 ** (esn0_db / 10.0)))


def _esn0_grid(m, n=3, step_db=1.0):
    """The Es/N0 points where THIS M's error rate is measurable.

    A fixed grid across every M measures the wrong thing. The coherent
    bound at 12 dB is 4.8e-7 for BPSK and 6.1e-2 for 8PSK -- five orders
    apart -- so one grid puts BPSK where it makes no errors at all and
    8PSK where it makes thousands. The first three cells then certify
    nothing (see F6) while looking like coverage.

    So the grid is DERIVED per M: find the highest Es/N0 whose bound still
    predicts `PLAN_ERRORS` in `NSCORED` symbols, and step down from there.
    That puts every cell inside the measurable window by construction, and
    it moves with `NSYM` -- a longer record automatically reaches further
    up each curve instead of needing the numbers retyped.

    Returns
    -------
    list of float
        `n` points, ascending, `step_db` apart, the highest being the
        measurability ceiling.
    """
    # M = 8 is the case where the rule cannot be followed, and that is a
    # RESULT rather than an exception to paper over. Its measurability
    # ceiling is 14.5 dB, and the object does not work there: measured
    # across 12-22 dB the rate is non-monotone and seed-dependent below
    # ~17 dB (refused at 12 and 14, 9.7e-1 at 13, 3.8e-2 at 15) and clean
    # from 17 up. So the window where the bound is measurable and the
    # window where the receiver works do not overlap, and a grid inside
    # either one alone would report half the story. This grid spans the
    # threshold instead, and F5 carries the consequence.
    if m == 8:
        return [14.0, 17.0, 20.0]
    hi = 0.0
    e = 0.0
    while e <= 40.0:
        if _bound(m, e) * NSCORED >= PLAN_ERRORS:
            hi = e
        e += 0.5
    hi = min(hi, ESN0_MAX_DB)
    return [hi - step_db * k for k in range(n - 1, -1, -1)]


def _resolves(theory):
    """Can this cell's DESIGN resolve the loss? Asked of the bound, not
    of the outcome.

    With zero errors `BerMeter`'s 95% upper bound is the rule-of-three
    floor, ~3/N, which at 20 000 symbols is 1.5e-4 -- above the M = 2,
    12 dB bound of 4.8e-7 by 300x. A limit reading "within 10x the bound"
    there is satisfied by any receiver at all, which is coverage the cell
    does not have. `theory > 0` guards the same emptiness from the other
    end: there is no loss to measure against a bound of exactly zero.

    **Keyed on the expected count, `theory * NSCORED`, not the measured
    one.** Two reasons, and the second is what forced the change:

    1. It is the correct statistical statement. Whether an experiment can
       resolve an effect is a property of its DESIGN -- the bound and the
       record length -- fixed before any data arrives. Deciding it from
       the observed count conditions the criterion on the outcome, so a
       receiver that happened to make a few extra errors would be judged
       measurable *because* it did worse.
    2. The measured count is machine-dependent and this verdict was
       therefore machine-dependent too. Measured across two toolchains
       (gcc 15.2/glibc 2.43 against 13.3/2.39, one CPU): 8PSK at 17 dB
       gave 20 errors on one and 6 on the other, straddling `MIN_ERRORS`,
       so `resolves` read `yes` on one machine and `no` on the other --
       and with it, the SET OF ASSERTED LIMITS changed. A cell dropped out
       of the certified envelope depending on which compiler built it.
       `ber_theory_ser` is a closed form over `erfc`, which is bit-
       identical across those toolchains, so the expected count is not.

    The measured `errors` column stays in the table: it is what tells a
    reader whether the design's expectation was borne out.
    """
    return theory > 0.0 and theory * NSCORED >= MIN_ERRORS


def _loss_db(m, esn0_db, ser):
    """dB the measured rate sits behind the bound, by inverting the bound.

    The same closed form §2.1 quotes, solved for the Es/N0 that would
    PRODUCE `ser`; the loss is how far that sits below the Es/N0 actually
    applied. Monotone in Es/N0, so a bisection is exact enough and needs no
    inverse-Q of its own.
    """
    if ser is None or ser <= 0.0:
        return None
    lo, hi = -5.0, esn0_db + 20.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if _bound(m, mid) > ser:
            lo = mid
        else:
            hi = mid
    return esn0_db - 0.5 * (lo + hi)


def _csv(path, header, rows):
    if not path.parent.exists():
        path.parent.mkdir(parents=True)
    with path.open("w") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v}" for v in r) + "\n")


# ── phase 1: characterise ────────────────────────────────────────────────


def characterise(R, write):
    """Measured behaviour. Tables and plots, no verdicts."""
    d = {}

    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured from the shipped C through its own binding. No verdicts "
        "here — section 3 judges these."
    )
    R.md()

    # 2.1 ---------------------------------------------------------------
    R.md("### 2.1 Symbol error rate against the coherent bound (C §2)")
    R.md()
    R.md(
        "The anchor, and it is anchored to THEORY rather than to another "
        "configuration: `ber_theory_ser` is the closed form, so "
        '"every M agrees" can never be mistaken for "every M is '
        'correct" (`docs/design/rx-test.md` goal 3). Implementation loss '
        "is the dB the measured rate sits behind the bound, read by "
        "inverting the same closed form."
    )
    R.md()
    R.md(
        "**The grid is DERIVED per M, not shared across them.** The bound "
        "at 12 dB is 4.8e-7 for BPSK and 3.1e-2 for 8PSK — five orders "
        "apart — so one grid across all three orders puts BPSK where it "
        "makes no errors at all and 8PSK where it makes thousands. Each M "
        "is therefore measured at the highest Es/N0 whose bound still "
        f"predicts {PLAN_ERRORS} errors in the {NSCORED} symbols actually "
        "scored, and two steps below it. That is a function of the record "
        "length, so a longer sweep reaches further up each curve without "
        "any number here being retyped (F6 is what this replaced)."
    )
    R.md()
    R.md(
        "**`errors` is the column that decides what any other column is "
        "worth.** `BerMeter` is built `target_errors=100`, which is its "
        "own statement of how many it needs before a rate means "
        f"something. `resolves` is the floor this report uses — "
        f"{MIN_ERRORS} errors — and §4 asserts only over cells whose "
        "**design** clears it (`theory * scored symbols`), never the "
        "measured count. Two reasons: a criterion read off the outcome "
        "would call a receiver measurable *because* it did worse, and the "
        "measured count is machine-dependent, so keying on it made this "
        "verdict — and with it the set of asserted limits — differ between "
        "toolchains (F8).\n\n"
        "A cell with three errors has a point estimate that moves by a "
        "factor of two on the next seed, and one such cell previously "
        "reported the receiver **beating** the matched-filter bound, "
        "which nothing can do."
    )
    R.md()

    rows, sweep = [], []
    for m in (2, 4, 8):
        for esn0 in _esn0_grid(m):
            x, lab = _signal(
                m,
                esn0,
                freq_offset=freq_offset_inside_bw(0.01, m, 1.0),
                seed=100 + m,
            )
            rx = MpskReceiver(m=m, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
            out = rx.steps(x)
            lo = ber_settle_syms(0.01, 0.01)
            ser, mtr = _score(out, lab, m, lo)
            theory = _bound(m, esn0)
            iv = mtr.ser() if (mtr is not None and mtr.symbols) else None
            nerr = int(iv.errors) if iv is not None else 0
            hi = float(iv.hi) if iv is not None else None
            resolves = _resolves(theory)
            loss = _loss_db(m, esn0, ser)
            # A refusal is a RESULT and is printed as one. The estimator this
            # replaced could not produce this row: it returned the best of 324
            # rotation/lag tries whether or not the record had an alignment.
            rows.append(
                [
                    f"{m}",
                    f"{esn0:.1f}",
                    f"{ser:.3e}" if ser is not None else "refused",
                    f"{theory:.3e}",
                    f"{loss:+.2f}" if loss is not None else "—",
                    f"{nerr}" if ser is not None else "—",
                    f"{hi:.2e}" if hi is not None else "—",
                    "yes" if resolves else "**no**",
                    f"{rx.lock:.3f}",
                ]
            )
            sweep.append(
                [m, esn0, ser, theory, loss, nerr, hi, resolves, rx.lock]
            )
    d["ser"] = sweep
    R.table(
        [
            "M",
            "Es/N0 dB",
            "SER measured",
            "SER theory",
            "loss dB",
            "errors",
            "95% hi",
            "resolves",
            "lock",
        ],
        rows,
    )
    R.md()
    los = {
        m: [r[4] for r in sweep if r[0] == m and r[4] is not None]
        for m in (2, 4, 8)
    }
    R.md(
        "**`loss dB` is the headline number, and it is small and stable.** "
        f"BPSK sits {min(los[2]):+.2f} to {max(los[2]):+.2f} dB behind the "
        f"coherent bound across its window and QPSK "
        f"{min(los[4]):+.2f} to {max(los[4]):+.2f} dB — a receiver within "
        "about half a dB of theory, which is what a fused matched filter "
        "in a self-planning cascade ought to deliver and had not "
        "previously been measured. The loss drifting by a fifth of a dB "
        "across each 2 dB window rather than jumping is the other half of "
        "the result: an implementation loss should be roughly constant in "
        "Es/N0, because it is the receiver's own contribution and does not "
        "scale with N0.\n\n"
        "**8PSK used to be a different object at these rates, and it was "
        "the stimulus that made it one.** This report previously recorded "
        "it as below its own acquisition threshold at 14 dB — the meter "
        "refusing, or the rate coming back near 1.0 — and excluded it "
        "from the loss envelope on the grounds that certifying a figure "
        "there would certify a number the object does not produce. The "
        "number it does not produce was ours: the offset was held in "
        "cycles per SAMPLE while this sweep's axis is `M`, and the "
        "carrier bound is `bn_carrier / m`, so BPSK was asked for 0.8x "
        "its bound and 8PSK for 3.2x (doppler#843). Seeded at the bound, "
        "8PSK resolves at 14 dB with the tightest loss in this grid, and "
        "§4 asserts the envelope at every order. Its measurability "
        "ceiling of 14.5 dB is real and unchanged — that half of F5 "
        "stands — but the working window now reaches under it."
    )
    R.md()
    if write:
        _csv(
            DATA / "ser_vs_esn0.csv",
            "m,esn0_db,ser_measured,ser_theory,loss_db,errors,ser_hi,"
            "resolves,lock",
            sweep,
        )

    # 2.2 ---------------------------------------------------------------
    R.md("### 2.2 EVM against the matched-filter bound (C §2)")
    R.md()
    R.md(
        "`EVM_dB = -(Es/N0)_dB` is the bound a matched filter reaches, so "
        "an EVM *below* it is a broken measurement rather than a good "
        "receiver — which is why it is reported beside the SER rather "
        "than alone. `ber_evm_db` is self-referenced: it needs no truth, "
        "so it is the metric that still works on a field capture."
    )
    R.md()

    rows, evm_sweep = [], []
    for esn0 in (8.0, 12.0, 16.0, 20.0):
        x, _lab = _signal(
            4,
            esn0,
            freq_offset=freq_offset_inside_bw(0.01, 4, 1.0),
            seed=7,
        )
        rx = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
        out = rx.steps(x)
        lo = ber_settle_syms(0.01, 0.01)
        evm = clamp_evm_db(
            float(
                ber_evm_db(np.ascontiguousarray(out), lo=lo, hi=out.size, m=4)
            )
        )
        rows.append(
            [f"{esn0:.0f}", f"{evm:.2f}", f"{-esn0:.2f}", f"{evm + esn0:.2f}"]
        )
        evm_sweep.append([esn0, evm, -esn0, evm + esn0])
    d["evm"] = evm_sweep
    R.table(["Es/N0 dB", "EVM dB", "bound dB", "excess dB"], rows)
    R.md()
    R.md(
        "**The excess is under half a dB everywhere and it grows with "
        "Es/N0** — -0.34 dB at 8 dB, +0.49 at 20. The sign flip is the "
        "informative part rather than an anomaly: at 8 dB the measured EVM "
        "is BELOW the bound, which cannot be a receiver beating a matched "
        "filter and is instead the error vector still carrying a little "
        "residual carrier and timing jitter that the self-referenced "
        "metric folds into its own reference. `ber_evm_db` normalises by "
        "the constellation it decides on, so at low Es/N0 it is measuring "
        "against a slightly rotated reference and flatters itself. That is "
        "why §4 asserts the bound in BOTH directions with half a dB of "
        "slack rather than only from above: a large negative excess is a "
        "broken measurement, and a bound with no floor under it would "
        "certify one.\n\n"
        "The growth with Es/N0 is the implementation loss becoming "
        "visible. Once the noise stops dominating, what is left in the "
        "error vector is the receiver's own contribution — ISI from the "
        "m_out-tap matched filter and the two loops' jitter — which does "
        "not shrink with N0. §2.8 is where that floor is isolated by "
        "moving the filter rather than the noise."
    )
    R.md()
    if write:
        _csv(
            DATA / "evm_vs_esn0.csv",
            "esn0_db,evm_db,bound_db,excess_db",
            evm_sweep,
        )

    # 2.3 ---------------------------------------------------------------
    R.md("### 2.3 The truth-free twin: blind M2M4 beside the SER")
    R.md()
    R.md(
        "`snr_m2m4_db` estimates Es/N0 from moments alone — no truth, no "
        "decisions. Reported beside the data-aided rate because **the two "
        "fail differently and the disagreement is the diagnostic** "
        "(`rx-test.md` goal 4): a healthy M2M4 next to a collapsed SER "
        "says the amplitudes are fine and the phase is not, which no "
        "error rate alone could say."
    )
    R.md()

    rows, m2m4 = [], []
    for esn0 in (8.0, 12.0, 16.0, 20.0):
        x, lab = _signal(
            4,
            esn0,
            freq_offset=freq_offset_inside_bw(0.01, 4, 1.0),
            seed=11,
        )
        rx = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
        out = rx.steps(x)
        lo = ber_settle_syms(0.01, 0.01)
        tail = np.ascontiguousarray(out[lo:])
        blind = float(snr_m2m4_db(tail))
        ser, mtr = _score(out, lab, 4, lo)
        rows.append(
            [
                f"{esn0:.0f}",
                f"{blind:.2f}",
                f"{blind - esn0:+.2f}",
                f"{ser:.3e}" if ser is not None else "refused",
            ]
        )
        m2m4.append([esn0, blind, blind - esn0, ser])
    d["m2m4"] = m2m4
    R.table(["Es/N0 dB asked", "M2M4 dB", "error dB", "SER"], rows)
    R.md()
    R.md(
        "**The blind estimate is biased LOW by 0.2 to 0.4 dB and the bias "
        "grows with Es/N0**, which is the expected direction and is what "
        "makes it usable as a cross-check. M2M4 attributes everything "
        "non-constant-modulus to noise, so the receiver's own residual — "
        "the same ISI and jitter §2.2 sees — is counted as noise and the "
        "reported SNR comes out under the truth. It therefore reads the "
        "SUM of channel noise and implementation loss, which is exactly "
        "what a field capture needs and exactly why it must not be used to "
        "measure implementation loss — that is §2.1's job, against a "
        "bound.\n\n"
        "The value is in the DISAGREEMENT, not the agreement. Both columns "
        "are healthy here, so this table is the control: it establishes "
        "what the pair looks like when nothing is wrong, which is what "
        "makes the pattern F2 describes legible. At the false lock of §2.7 "
        "the constellation is stationary and M2M4 reads clean while the "
        "frequency is wrong by `F/M` — a healthy blind SNR beside a "
        "collapsed error rate says the amplitudes are fine and the phase "
        "is not, and no error rate alone could say that."
    )
    R.md()
    # The reference lives in the section that owns the measurement, never
    # in `plots()` -- that runs only when `write=True`, so markdown emitted
    # there is absent from the `--check` render and the staleness gate is
    # then permanently red with a diff that looks like drift.
    R.md("![SER against the coherent bound; EVM and blind M2M4](quality.png)")
    R.md()
    if write:
        _csv(
            DATA / "m2m4_vs_esn0.csv",
            "esn0_db,m2m4_db,error_db,ser",
            m2m4,
        )

    # 2.4 ---------------------------------------------------------------
    R.md("### 2.4 The derived construction parameters, read back (C §1b)")
    R.md()
    R.md(
        "gh-644: five parameters that are not design axes derive when "
        "passed `0`, and **every one is reported back** — without the "
        "readback, `0` is an instruction whose result nobody can see. "
        "The rule itself (`mpsk_rx_derive_m_out`) is `JM_FORCEINLINE` "
        "with no binding, so it is **C-ONLY** (F1); what is measurable "
        "from Python is the answer it produced."
    )
    R.md()

    rx = MpskReceiver(m=2, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
    derived = [
        ["m_out", f"{rx.m_out}", "8"],
        ["zeta", f"{rx.zeta:.6f}", "0.707107"],
        ["num_phases", f"{rx.num_phases}", "64"],
        ["lock_thresh", f"{rx.lock_thresh:.4f}", "0.4999"],
        ["bn_agc_ratio", f"{rx.bn_agc_ratio:.4f}", "0.0500"],
    ]
    d["derived"] = {
        "m_out": rx.m_out,
        "zeta": rx.zeta,
        "num_phases": rx.num_phases,
        "lock_thresh": rx.lock_thresh,
        "bn_agc_ratio": rx.bn_agc_ratio,
    }
    R.table(["parameter", "derived at sps=8", "expected"], derived)
    R.md()
    R.md(
        "**The readback is the whole mechanism, and it is what makes `0` "
        "auditable rather than magic.** Each expected value is the "
        "header's own stated derivation, so this table fails if a "
        "derivation changes without the header changing with it — which is "
        "the drift a construct-time default cannot have and a derived one "
        "can. `lock_thresh` is the row that carries the most: 0.4999 is "
        "`sigma_H0 * eta(Pfa)` = `0.1132 * 4.4159`, and neither factor is "
        "this object's — the 0.1132 is the M-th-power statistic's "
        "noise-only sd, derived and measured in `carrier_nda`'s own "
        "certification (its §2.7), which is why this report reads the "
        "number back rather than re-deriving it.\n\n"
        "What is NOT established here is that the derivation is right at "
        "any other rate. Every row is measured at `sps = 8`, where the "
        "header states the answer, so the table pins the answer and not "
        "the rule: `m_out` derives to the largest even count in 2..8 the "
        "RATE allows, and a sweep over `sps` is the only thing that could "
        "show it doing so. That is F1 — the rule is `JM_FORCEINLINE` with "
        "no binding, and `test_mpsk_receiver_core.c` §1b is what covers it."
    )
    R.md()

    # 2.5 ---------------------------------------------------------------
    R.md("### 2.5 An irrational `sps` (C §1e)")
    R.md()
    R.md(
        "The header's headline claim — a modem at **any** input rate, "
        'where "17.33389 is equally valid", because the terminal '
        "accumulator is a double and the loop only has to steer the "
        "strobe. Measured at that exact rate, with the symbol boundary "
        "falling between samples rather than on them."
    )
    R.md()
    R.md(
        "Two independent readings of the same claim, which is why "
        "`rate recovered` is here beside the count. The output COUNT is a "
        "bulk property — it would survive a loop that steered to the "
        "wrong rate and dropped or duplicated symbols in compensating "
        "amounts — while `timing_rate` is the terminal accumulator's own "
        "converged value, so it says the loop found the rate rather than "
        "merely emitting the right number of things."
    )
    R.md()

    rows, irr = [], []
    for sps in (8.0, 17.33389, 24.0, 31.7):
        x, lab = _signal(
            4,
            16.0,
            sps=sps,
            nsym=6000,
            freq_offset=freq_offset_inside_bw(0.01, 4, 1.0),
            seed=5,
        )
        rx = MpskReceiver(m=4, sps=sps, bn_carrier=0.01, bn_timing=0.01)
        out = rx.steps(x)
        lo = ber_settle_syms(0.01, 0.01)
        ser, mtr = _score(out, lab, 4, lo)
        expect = x.size / sps
        rate = rx.timing_rate
        rows.append(
            [
                f"{sps:g}",
                f"{out.size}",
                f"{expect:.0f}",
                f"{100 * abs(out.size - expect) / expect:.2f}",
                f"{rate:.5f}",
                f"{1e6 * (rate - sps) / sps:+.0f}",
                f"{ser:.3e}" if ser is not None else "refused",
            ]
        )
        irr.append([sps, out.size, expect, rate, ser])
    d["irrational"] = irr
    R.table(
        [
            "sps",
            "symbols out",
            "expected",
            "error %",
            "rate recovered",
            "rate err ppm",
            "SER",
        ],
        rows,
    )
    R.md()
    worst_ppm = max(1e6 * abs(r[3] - r[0]) / r[0] for r in irr)
    irr_row = next(r for r in irr if r[0] == 17.33389)
    bad = next(r for r in irr if r[0] == 31.7)
    R.md(
        f"**The rate is recovered to within {worst_ppm:.0f} ppm at every "
        f"rate measured, including the irrational one** — "
        f"{irr_row[3]:.5f} against a true 17.33389, "
        f"{1e6 * abs(irr_row[3] - irr_row[0]) / irr_row[0]:.0f} ppm — so "
        "the header's claim is carried by the loop's own accumulator and "
        "not only by a count that could be right for the wrong reason. An "
        "irrational rate is not the hard case: 17.33389 and the integer 8 "
        "are recovered to the same precision, and both emit the same "
        "0.03-0.07% output-count error, which is the settling transient at "
        "the head of the record rather than a rate error.\n\n"
        f"**`sps = {bad[0]:g}` used to be the row that separated the two "
        "readings, and it is why F4 was CONFIRMED against the receiver.** "
        "Its count was inside 0.1% and its recovered rate within a few ppm "
        "— tighter than the integer rate's — while the meter refused to "
        "align the record at all. A rate that close cannot explain a "
        "failure to demodulate, so the failure was localised to "
        "acquisition, and it was: **this sweep's acquisition, not the "
        "receiver's.** The offset was held in cycles per SAMPLE while the "
        "axis here is `sps`, so the loop was handed a question that got "
        "4x harder across the sweep and this row sat at 5.07x its "
        "acquisition bound, past the 4-5x where acquisition stops being "
        "repeatable (doppler#843).\n\n"
        f"Held in cycles per SYMBOL, where the bound lives and where it "
        "does not move with `sps`, **every row demodulates**: SER "
        + ", ".join(
            f"{r[4]:.1e} at sps {r[0]:g}" for r in irr if r[4] is not None
        )
        + ". The irrational rate is not the hard case and neither is the "
        "high one; there is no `sps` ceiling in the certified envelope."
    )
    R.md()
    if write:
        _csv(
            DATA / "irrational_sps.csv",
            "sps,symbols_out,symbols_expected,rate_recovered,ser",
            irr,
        )

    # 2.6 ---------------------------------------------------------------
    R.md("### 2.6 One discriminator, and nothing gates it (C §1c)")
    R.md()
    R.md(
        "The header claims the M-th-power NDA error steers the LO from the "
        "first strobe to the last, and that the carrier lock indicator "
        "steers no loop and gates no output. Half of that is now structural "
        "and half is still falsifiable, and they are reported separately "
        "because only one of them can be measured.\n\n"
        "**Structural half.** There is no second discriminator to hand over "
        "to. `acq_to_track`, `tracking`, `configure_lock` and "
        "`ContinuousMpskReceiver` — the view whose entire purpose was to pin "
        "the handover off — are gone (doppler#877), so no construction "
        "reachable from this API can enable one. This section used to build "
        "its control row with `acq_to_track = 1` precisely so that "
        "`tracking == 0` read as a pinned property rather than as a failure "
        "to reach 1; that falsifier cannot be built any more, and a claim "
        "whose falsifier is unbuildable is a claim about the TYPE, so it is "
        "stated rather than measured. The row below replaces it with one "
        "that can still fail.\n\n"
        "**Falsifiable half.** An ungated steer is not a type fact: an "
        "`if (locked)` in front of it would compile, would still reach lock, "
        "and would pass every end-of-run number in this report. It has one "
        "signature — the tracked frequency sits at exactly 0.0 until the "
        "indicator declares — so the estimate is read at the LAST symbol "
        "before the declaration rather than at the end of the record."
    )
    R.md()

    ungated = []
    for m, esn0, seed in ((2, 20.0, 21), (4, 20.0, 3), (8, 24.0, 5)):
        foff = freq_offset_inside_bw(0.02, m, 1.0)
        x, _lab = _signal(m, esn0, freq_offset=foff, seed=seed)
        rxu = MpskReceiver(
            m=m, sps=SPS, m_out=8, bn_carrier=0.02, bn_timing=0.01
        )
        blk = int(SPS)  # one symbol
        f_und, at, declared = 0.0, 0, False
        for i in range(0, x.size - blk, blk):
            rxu.steps(x[i : i + blk])
            if rxu.locked:
                declared, at = True, i // blk
                break
            f_und = rxu.norm_freq
        # `freq_offset` is cycles per SYMBOL; `norm_freq` is per SAMPLE.
        truth = foff / SPS
        ungated.append(
            {
                "m": m,
                "declared_at": at if declared else None,
                "f_undeclared": f_und,
                "truth": truth,
                "frac": f_und / truth if truth else 0.0,
            }
        )
    d["ungated"] = ungated

    R.table(
        ["M", "declared at (sym)", "f before declare", "true offset", "frac"],
        [
            [
                f"{u['m']}",
                "never" if u["declared_at"] is None else f"{u['declared_at']}",
                f"{u['f_undeclared']:.3e}",
                f"{u['truth']:.3e}",
                f"{u['frac']:.3f}",
            ]
            for u in ungated
        ],
    )
    R.md()
    R.md(
        "**Every row is a non-zero fraction of the true offset, acquired "
        "before anything declared.** That is the whole measurement: a steer "
        "gated on the indicator reads 0.000 in that column at every order.\n\n"
        "The fraction itself is reported and NOT interpreted. `locked` is a "
        "threshold test with hysteresis on the M-th-power lock statistic, "
        "whose H1 mean is a function of Es/N0 alone — so the detector never "
        "sees frequency error and its declaration instant is not evidence "
        "about convergence in either direction "
        "([lock detection §1](../../../../../../docs/design/lock-detect.md)). "
        "The numbers above are two quantities sampled at a common instant, "
        "not a relationship; a reader wanting convergence asks for it "
        "directly, with `lock_time` plus a settling budget (§2.4)."
    )
    R.md()
    R.md(
        "**What the deleted handover was worth**, since this section "
        "previously certified that a flag existed rather than that it "
        "helped. Measured on the shipped receiver immediately before the "
        "deletion, same record through `MpskReceiver` twice at operating "
        "points where the meter is not saturated (SER 1.5e-2 to 7.1e-2):"
    )
    R.md()
    R.table(
        ["axis", "result"],
        [
            ["recovered symbols that differ", "19764–19928 of 19998 (~99%)"],
            ["largest difference", "0.30–0.48 (unit-radius constellation)"],
            ["SER over 10 engaged cells", "mean ratio 0.9999, t = 0.28"],
            ["cells where the handover won", "6 of 10"],
            [
                "8PSK anchor, from `mpsk_receiver_ber.c`",
                "0.09 dB (0.44 → 0.53)",
            ],
        ],
    )
    R.md()
    R.md(
        "So it perturbed essentially the whole sample path and moved the "
        "decisions by scatter with no sign. Those numbers are a RECORD, not "
        "a re-measurement — the construction that produced them no longer "
        "exists, which is the point of deleting it. They are here because a "
        "reader of this report is entitled to know that the branch this "
        "section used to certify was removed on evidence rather than on "
        "taste; the issue carries the full table."
    )
    R.md()

    # 2.7 ---------------------------------------------------------------
    R.md("### 2.7 The stable false lock at `Δf = k·F/M` (C §1f)")
    R.md()
    R.md(
        "`F/M` is exactly where an M-th power at update rate `F` aliases "
        "onto zero, so the M-fold ambiguity is a FREQUENCY ambiguity as "
        "well as a phase one. Seeded there, the loop does not move — and "
        "reports a lock statistic a caller would trust.\n\n"
        "**Every metric the receiver can compute is reported beside it, "
        "and a true lock on the same geometry is the control.** The claim "
        "under test is not that the false lock exists — one row shows that "
        "— but that it is INVISIBLE, and invisibility is a statement about "
        "the metrics, so each one has to be measured at the false lock and "
        "compared against its own healthy value. `F` here is the strobe "
        f"tap's update rate, `Rs` = 1/{SPS:g} = {1.0 / SPS:.3f} "
        "cycles/sample."
    )
    R.md()

    rows, fl_sweep = [], []
    lo = ber_settle_syms(0.01, 0.01)
    for k in (0, 1, 2):
        # The M-th-power alias grid, `k*Rs/M`, stated where it is simplest:
        # in cycles per SYMBOL it is just `k/M`, with no `sps` in it. The
        # cycles-per-sample value below is for the report and for comparing
        # against `norm_freq`, which is the one quantity here still in
        # sample-rate units.
        foff_sym = k / 4.0
        foff = foff_sym / SPS
        x, lab = _signal(4, 20.0, freq_offset=foff_sym, seed=5)
        rxf = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
        out = rxf.steps(x)
        ser, _mtr = _score(out, lab, 4, lo)
        evm = clamp_evm_db(
            float(
                ber_evm_db(np.ascontiguousarray(out), lo=lo, hi=out.size, m=4)
            )
        )
        blind = float(snr_m2m4_db(np.ascontiguousarray(out[lo:])))
        err = rxf.norm_freq - foff
        rows.append(
            [
                "TRUE (control)" if k == 0 else f"**false, k = {k}**",
                f"{foff:.5f}",
                f"{rxf.norm_freq:+.5f}",
                f"{err:+.5f}",
                f"{rxf.lock:.3f}",
                f"{evm:.2f}",
                f"{blind:.2f}",
                f"{ser:.3e}" if ser is not None else "**refused**",
            ]
        )
        fl_sweep.append([k, foff, rxf.norm_freq, err, rxf.lock, evm, blind])
    d["false_lock"] = {
        "k1": fl_sweep[1],
        "control": fl_sweep[0],
        "sweep": fl_sweep,
    }
    R.table(
        [
            "which lock",
            "true Δf",
            "tracked",
            "error",
            "lock",
            "EVM dB",
            "M2M4 dB",
            "SER",
        ],
        rows,
    )
    R.md()
    R.md(
        "**The lattice is exact and the loop sits at zero, not near it.** "
        "At `k = 1` the tracked frequency is -0.00000 against a true "
        "0.03125, so the error is `F/M` to five decimals, and at `k = 2` it "
        "is `2F/M` to the same precision. This is not a loop that drifted; "
        "it is a loop at an equilibrium, which is why no amount of record "
        "length recovers from it.\n\n"
        "**And every self-referenced metric stays healthy.** Against the "
        "true lock's +0.977 / -19.60 dB / 19.62 dB, the `k = 1` false lock "
        "reads +0.963 / -18.60 / 18.69 — a lock statistic 1.4% lower, an "
        "EVM 1.0 dB worse and a blind SNR 0.9 dB lower, none of which is "
        "distinguishable from a slightly noisier channel. `k = 2` degrades "
        "further but still reads +0.885 and -15.07 dB, which any caller "
        "would accept. **The only column that detects it is the SER, and "
        "it detects it by REFUSING**: `BerMeter` cannot align a record "
        "whose symbols are consistently wrong, and its false-alarm gate "
        "turns that into a declined measurement rather than a plausible "
        "number. That is the finding — the one metric that catches this "
        "needs truth, so nothing a deployed receiver computes about itself "
        "can (F2).\n\n"
        "**Independently corroborated across the order axis, which this "
        "section does not sweep.** `docs/design/rx-test.md` (its section "
        "8.6) measures the same false lock at every M through a different "
        "harness and "
        "finds the truth-free penalty *shrinking* as M rises — 3.56 dB of "
        "EVM at BPSK, 1.05 at QPSK, 0.21 at 8PSK — while the receiver "
        "declares lock and the alignment refuses at all three. The QPSK "
        "figure there is this section's 1.0 dB, reached from a different "
        "direction, and the trend is the part that matters: the metric "
        "that can half-see this at BPSK goes blind exactly where the "
        "decision margin is already smallest. So the defence cannot be a "
        "threshold on any of these three columns at any M."
    )
    R.md()

    if write:
        _csv(
            DATA / "false_lock.csv",
            "k,true_foff,tracked,error,lock,evm_db,m2m4_db",
            fl_sweep,
        )

    # 2.8 ---------------------------------------------------------------
    R.md("### 2.8 What `m_out` costs — the matched filter's resolution")
    R.md()
    R.md(
        "The header's longest single claim, and the one it argues hardest: "
        "`m_out` is not a performance knob but a correctness condition, "
        "**"
        "not optional at M = 8**. Two mechanisms are offered — the I&D "
        "filter is an `m_out`-tap sum spanning one symbol, so a smaller "
        "`m_out` samples the same integral more coarsely (M-independent), "
        "and `z^M` spreads energy over ~`M*Rs` so whatever exceeds the "
        "update rate folds back (M-DEPENDENT, worsening with M).\n\n"
        "Isolated by halving `m_out` at a FIXED Es/N0 and reading the EVM "
        "excess over the matched-filter bound, so the noise is held still "
        "and the only thing that moves is the filter. Measured at 18 dB, "
        "which is the Es/N0 the header quotes its own QPSK figures at."
    )
    R.md()

    rows, mo_sweep = [], []
    lo = ber_settle_syms(0.01, 0.01)
    for m in (2, 4, 8):
        cells = {}
        for mo in (8, 4):
            x, lab = _signal(m, 18.0, seed=31 + m, nsym=8000)
            rx = MpskReceiver(
                m=m, sps=SPS, m_out=mo, bn_carrier=0.01, bn_timing=0.01
            )
            out = rx.steps(x)
            evm = clamp_evm_db(
                float(
                    ber_evm_db(
                        np.ascontiguousarray(out), lo=lo, hi=out.size, m=m
                    )
                )
            )
            cells[mo] = (evm + 18.0, rx.lock)
        cost = cells[4][0] - cells[8][0]
        rows.append(
            [
                f"{m}",
                f"{cells[8][0]:+.2f}",
                f"{cells[4][0]:+.2f}",
                f"{cost:+.2f}",
                f"{cells[8][1]:.3f}",
                f"{cells[4][1]:.3f}",
            ]
        )
        mo_sweep.append([m, cells[8][0], cells[4][0], cost])
    d["m_out"] = mo_sweep
    R.table(
        [
            "M",
            "excess dB, m_out = 8",
            "excess dB, m_out = 4",
            "cost dB",
            "lock, m_out = 8",
            "lock, m_out = 4",
        ],
        rows,
    )
    R.md()
    q = next(r for r in mo_sweep if r[0] == 4)
    costs = [r[3] for r in mo_sweep]
    R.md(
        "**The direction is the header's and the magnitude reproduces its "
        f"QPSK figure**: 8 taps sit {q[1]:+.2f} dB off the bound and 4 "
        f"taps {q[2]:+.2f}, against the header's 0.41 and 3.11 at the same "
        "Es/N0. So the first mechanism — a coarser sampling of the same "
        "integral — is confirmed, and it is expensive: half the taps costs "
        f"about {sum(costs) / len(costs):.1f} dB of an EVM budget that is "
        "otherwise a quarter of a dB.\n\n"
        "**The M-DEPENDENCE is not confirmed, and this is the measurement "
        f"that says so.** The cost is {costs[0]:+.2f}, {costs[1]:+.2f} and "
        f"{costs[2]:+.2f} dB at M = 2, 4 "
        f"and 8 — flat to within {max(costs) - min(costs):.2f} dB across "
        "the whole order "
        "axis, where the header's own figures (1.7, 1.6, **3.0**) put 8PSK "
        "at nearly twice BPSK's cost. The two are not in contradiction, "
        "because they are not the same measurement: the header anchors "
        "each M at its own `SER = 1e-3` operating point, which converts a "
        "rate penalty into dB, while this table holds Es/N0 fixed and reads "
        "the error vector. An EVM excess is blind to where the decision "
        "boundary is, and the folding mechanism acts precisely on the "
        "decision margin — +-pi/8 at 8PSK against +-pi/2 at BPSK. So this "
        "sweep measures the M-independent half cleanly and cannot see the "
        "M-dependent half at all.\n\n"
        "What that costs the report: the header's headline conclusion is "
        "carried by the SER-anchored figures, and reaching a `1e-3` anchor "
        "per M is the same record-length problem §2.1 runs into from the "
        "other side (F6, [#781](https://github.com/doppler-dsp/doppler/"
        "issues/781)). §4 therefore asserts what this geometry establishes "
        "— that halving `m_out` costs at least 2 dB at every M, so it is "
        "never free — and not the ordering across M. The LOCK column is "
        "the corroborating hint: it falls with `m_out` and it falls "
        "hardest at M = 8 (0.802 to 0.687, against BPSK's 0.989 to 0.986), "
        "which is the folding mechanism showing up in the one statistic "
        "built on `z^M` even where the EVM cannot see it — the lock "
        "statistic moving before the error metric, the same way it does "
        "for a level error (§2.9)."
    )
    R.md()
    if write:
        _csv(
            DATA / "m_out_cost.csv",
            "m,excess_db_m_out_8,excess_db_m_out_4,cost_db",
            mo_sweep,
        )

    # 2.9 ---------------------------------------------------------------
    R.md("### 2.9 The AGC's gain readback is the level diagnostic (C §9, §11)")
    R.md()
    R.md(
        "The receiver has exactly ONE AGC, in the cascade immediately "
        "before the terminal matched stage, and it serves BOTH loops. The "
        "header calls `get_agc_gain_db` *the* diagnostic for a level "
        "problem and states what it settles to — `-10*log10(P_in/P_ref)`, "
        "where `P_ref` is the power a unit-amplitude symbol stream has "
        "where the AGC sits. That is a law with a slope and an offset, and "
        "both are measurable: sweeping the input amplitude at a fixed "
        "Es/N0 must move the readback by exactly -1 dB per dB, and the "
        "offset at unit amplitude says whether `P_ref` really is the "
        "unit-amplitude reference the header claims.\n\n"
        "The `agc = 0` column is the control, and it is what the header's "
        "other claim needs: with the AGC off the receiver is un-levelled, "
        "so the timing detector — which normalises by a slope computed at "
        "construction for a unit-amplitude stream — is under-driven by "
        "`A^2`. **Which is why the metric reported for it is the recovered "
        "RATE and not the lock statistic.** Es/N0 is held at 20 dB while "
        "the amplitude moves, so the only thing changing is the level the "
        "loops see."
    )
    R.md()

    rows, agc_sweep = [], []
    for amp in (1.0, 0.5, 0.125, 0.03125):
        cells = {}
        for agc in (1, 0):
            x, lab = _signal(4, 20.0, seed=3, amp=amp, nsym=4000)
            rx = MpskReceiver(
                m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01, agc=agc
            )
            out = rx.steps(x)
            ser, _mtr = _score(out, lab, 4, ber_settle_syms(0.01, 0.01))
            cells[agc] = (
                rx.agc_gain_db,
                1e6 * abs(rx.timing_rate - SPS) / SPS,
                rx.lock,
                ser,
                rx.clipped,
            )
        g = cells[1][0]
        rows.append(
            [
                f"{amp:g}",
                f"{g:+.3f}",
                f"{g + 20 * np.log10(amp):+.3f}",
                f"{cells[1][1]:.0f}",
                f"{cells[0][1]:.0f}",
                f"{cells[1][2]:.3f}",
                f"{cells[0][2]:.3f}",
                f"{cells[1][4]}",
            ]
        )
        agc_sweep.append(
            [
                amp,
                g,
                g + 20 * np.log10(amp),
                cells[1][1],
                cells[0][1],
                cells[1][2],
                cells[0][2],
            ]
        )
    d["agc"] = agc_sweep
    R.table(
        [
            "input amplitude",
            "agc_gain_db",
            "+ 20log10(amp)",
            "rate err ppm, agc = 1",
            "rate err ppm, agc = 0",
            "lock, agc = 1",
            "lock, agc = 0",
            "clipped",
        ],
        rows,
    )
    R.md()
    offs = [r[2] for r in agc_sweep]
    on_ppm = [r[3] for r in agc_sweep]
    off_ppm = [r[4] for r in agc_sweep]
    R.md(
        "**The level law is exact.** The fourth column is the readback plus "
        "`20*log10(amp)`, which the header's law says must be a constant, "
        f"and it is: {min(offs):+.3f} to {max(offs):+.3f} dB across a 32x "
        "span, moving by under a hundredth of a dB. So the AGC applies "
        "precisely the reciprocal of the input level, and `P_ref` IS the "
        f"unit-amplitude reference to within {abs(offs[0]):.2f} dB — the "
        "residual being the bank's own pulse energy, which the header says "
        "the reference is derived from rather than chosen. A reading far "
        "from 0 dB therefore means what the header says it means, and the "
        "number is usable as an absolute level estimate rather than only "
        "as a trend.\n\n"
        "**The `A^2` under-drive lands on the recovered rate, and the "
        "carrier lock statistic cannot see it at all.** With the AGC on, "
        f"the rate error is {min(on_ppm):.0f}-{max(on_ppm):.0f} ppm with no "
        f"trend in level; with it off, it grows from {off_ppm[0]:.0f} ppm at "
        f"unit amplitude to {max(off_ppm):.0f} ppm as the level falls — "
        "about a factor of ten, in the direction and on the axis the "
        "header names. Meanwhile `lock` reads 0.96-0.97 in **both** "
        "columns and is not even monotone in level (0.892 at amplitude "
        "0.125, 0.967 at 0.03125), which is not a defect but a "
        "consequence: `lock` is the CARRIER statistic and "
        "`carrier_nda_disc` divides out its own `|z|^M`, so it is immune "
        "to the level by construction — the property `carrier_nda`'s own "
        "certification measures over eight decades. **The receiver "
        "therefore publishes two health readouts with disjoint blind "
        "spots, and the one a caller reaches for first is blind to this "
        "one.** A level problem is visible in `agc_gain_db` and in "
        "`timing_rate`, and in neither `lock` nor, at 20 dB, the error "
        "rate — every cell here recovers every symbol. The `clipped` "
        "column stays 0 throughout, which attributes the effect to loop "
        "gain rather than to the +-1.0 ceiling the cascade clips at."
    )
    R.md()
    if write:
        _csv(
            DATA / "agc_level.csv",
            "amplitude,agc_gain_db,offset_db,ppm_agc_on,ppm_agc_off,"
            "lock_agc_on,lock_agc_off",
            agc_sweep,
        )
    R.md("![what m_out costs, and the AGC's level law](cascade.png)")
    R.md()

    # 2.10 --------------------------------------------------------------
    R.md("### 2.10 Lifecycle, reset, telemetry and state (C §1, §10)")
    R.md()
    R.md(
        "The surfaces a composing caller needs and no earlier section "
        "reaches. A receiver this deep in a cascade is checkpointed and "
        "resumed rather than restarted, so the state triplet is not a "
        "convenience — and its telemetry is the only way to see inside a "
        "composition that publishes one `lock` number."
    )
    R.md()

    x, lab = _signal(
        4,
        20.0,
        nsym=4000,
        freq_offset=freq_offset_inside_bw(0.01, 4, 1.0),
        seed=6,
    )
    half = (x.size // 2) & ~7
    rx = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
    rx.steps(x[:half])
    blob = rx.get_state()
    tail = rx.steps(x[half:])
    warm = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
    warm.steps(x[:half])
    warm.set_state(blob)
    resumed = np.array_equal(tail, warm.steps(x[half:]))

    rr = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
    first = rr.steps(x)
    rr.reset()
    reproduces = np.array_equal(first, rr.steps(x))

    try:
        warm.set_state(b"\x00" * len(blob))
        rejects = False
    except ValueError:
        rejects = True

    # The trajectories behind every number above, FILED as evidence rather
    # than measured and discarded (doppler#846). `Report.capture` owns the
    # order — attach, then arm, then run — and refuses to file a capture the
    # ring lost a record from, so what lands in `data/` is whole.
    with R.capture(DATA, "mpsk_receiver", ring_records=1 << 20) as cap:
        rt = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
        rt.set_telemetry(cap.telemetry, "rx", 8)
        cap.arm(8192)
        rt.steps(x[:8192])
    counts = {k: len(v) for k, v in cap.probes.items()}
    nprobe = len(counts)
    nrec = sorted(set(counts.values()))
    d["life"] = {
        "state_bytes": len(blob),
        "resumed": resumed,
        "reproduces": reproduces,
        "rejects": rejects,
        "nprobe": nprobe,
        "nrec": nrec[0] if len(nrec) == 1 else None,
        "probes": sorted(counts),
    }
    R.table(
        ["property", "measured"],
        [
            ["`reset()` reproduces the first run", f"{reproduces}"],
            ["serialized size", f"{len(blob)} bytes"],
            ["resume from a blob, mid-stream", f"bit-exact: {resumed}"],
            ["a clobbered blob is rejected", f"ValueError: {rejects}"],
            ["telemetry probes published", f"{nprobe}"],
            [
                "trajectories filed",
                f"`data/mpsk_receiver.tlm`, {nprobe} probes",
            ],
            [
                "records per probe over 8192 samples at decim 8",
                f"{nrec[0] if len(nrec) == 1 else nrec}",
            ],
        ],
    )
    R.md()
    R.md(
        "**The resume is checked against a WARM instance, not a fresh "
        "one.** A `set_state` into a newly constructed receiver would pass "
        "on an object that restores nothing at all, because the fresh "
        "state and the restored state agree wherever the blob is silent. "
        "The instance here is driven through the same first half, then "
        "overwritten from the blob, so the assertion is that the blob "
        "DETERMINES the continuation rather than merely not contradicting "
        "it — the vacuity discipline the campaign applies to reject tests, "
        "applied to a round-trip.\n\n"
        f"**{nprobe} probes, and they emit once per SYMBOL** — "
        f"{nrec[0] if len(nrec) == 1 else nrec} records each over 8192 "
        "input samples at `decim = 8`, which is 8192/`sps`/8 and not "
        "8192/8. Worth knowing, because `carrier_nda`'s probes emit per "
        "input SAMPLE (`carrier_nda/results.md`, its lifecycle section) "
        "and a caller reading both at one `decim` gets two different time "
        "bases. Every probe carries the same "
        "count, which is the attach-fails-whole property: a partially "
        "attached composition would show a short probe rather than an "
        "error. The set spans all three subsystems — `rx.car.*` for the "
        "carrier loop, `rx.sync.*` for timing, `rx.agc.*` for the level — "
        "so the composition is observable per constituent and not only at "
        "its output."
    )
    R.md()

    return d


# ── phase 2: review ──────────────────────────────────────────────────────


def review(R, d):
    """Judgements about problems. No verdict means 'this works'."""
    R.md("## 3. Review")
    R.md()
    R.md(
        "`BY DESIGN` — correct and intended. `C-ONLY` — a claim no "
        "binding reaches, carried in C. A verdict is a judgement about a "
        "PROBLEM; a result that holds is a limit, not a finding."
    )
    R.md()

    R.find(
        "F1",
        "C-ONLY",
        "`mpsk_rx_derive_m_out()` and `mpsk_rx_updates_per_symbol()` are "
        "`JM_FORCEINLINE` with no binding, so the derivation RULE and the "
        "`mf_in` tap's actual update rate are unreachable from Python by "
        "construction. §2.4 measures the answer the rule produced, which "
        "is not the same claim. They are carried by "
        "`test_mpsk_receiver_core.c` §1b/§1d and by "
        "`native/validation/rx_battery.c`.",
    )

    R.find(
        "F2",
        "BY DESIGN",
        "The stable false lock at `Δf = k·F/M` (§2.7) reads as a defect "
        "and is not one: an M-th-power detector updating at `F` aliases "
        "onto zero there, so the loop is at a genuine equilibrium. It is "
        "recorded because the failure is QUIET — the constellation is "
        "stationary, so self-referenced EVM and blind M2M4 both look "
        "clean and the lock statistic reads healthy. Detecting it needs "
        "an external frequency reference or a sync word; no metric this "
        "receiver computes can.",
    )

    R.find(
        "F4",
        "FIXED",
        "**A receiver that recovered the symbol rate to 2 ppm and could "
        "not be demodulated — and the defect was in this report's own "
        "stimulus, not in the receiver.** As written, F4 read: at "
        "`sps = 31.7` (§2.5) the output count is the integral of the rate "
        "to 0.08% and `timing_rate` converges tighter than at the integer "
        "rate, yet `BerMeter` refuses to align the record; `sps = 24` on "
        "the same sweep is clean; therefore the failure is in acquisition "
        "and the certified envelope stops at `sps <= 24`.\n\n"
        "The acquisition half was right and the attribution was wrong. "
        "`_signal` held the carrier offset in cycles per **sample** while "
        "this sweep's axis is `sps`, so the loop was handed `foff * sps` "
        "— a different question at every row, varying 4x across the "
        "sweep. Against a carrier acquisition bound of `bn_carrier / m` "
        "cycles per symbol, the four rows sat at 1.28x, 2.77x, 3.84x and "
        "**5.07x** the bound. The pull-in envelope, swept independently "
        "by `doppler.track.tests.characterization.pull_in`, puts QPSK "
        "reliable out to 4x and dead by 5x — which accounts for this "
        "sweep row by row rather than merely in direction: 24 sits at "
        "3.84x, inside, and demodulated; 31.7 sits at 5.07x, past the "
        "shoulder, and refused. The `sps = 31.7` row was simply the one "
        "seeded past the cliff, and `sps = 24` the last one inside it — "
        "which is why the two differed while nothing about the receiver "
        "did.\n\n"
        "Both converses were measured before this was believed: 31.7 at "
        "24's offset scores 0.0 SER, and 24 at 31.7's offset refuses. "
        "With the offset now held in cycles per SYMBOL, where the bound "
        "is stated and where it does not move with `sps`, **all four "
        "rows demodulate at SER 0.0** — 8, 17.33389, 24 and 31.7 alike, "
        "with the recovered rate inside 60 ppm at every one. There is no "
        "`sps <= 24` envelope; §4 asserts the error rate at every rate "
        "measured. The corroboration this finding claimed with "
        "[#781](https://github.com/doppler-dsp/doppler/issues/781) does "
        "not survive either: that report is about `m_out = 4`, reached "
        "through a randomised geometry, and shares nothing with this but "
        "the symptom.",
    )

    R.find(
        "F5",
        "FIXED",
        "**8PSK's implementation loss was reported unmeasurable, and the "
        "reason was that 8PSK alone was being seeded past its acquisition "
        "bound.** As written, F5 read: the bound at M = 8 falls slowly — "
        "1.7e-5 at 18 dB — so reaching "
        f"{MIN_ERRORS} errors needs ~2.3M scored symbols against the "
        f"{NSCORED} this sweep scores, putting the highest usable Es/N0 "
        "at **14.5 dB**; while the receiver was non-monotone and "
        "seed-dependent below ~17 dB, so the window where the bound is "
        "measurable and the window where the receiver works did not "
        "overlap.\n\n"
        "The first half stands: the bound really does fall too slowly to "
        "resolve above ~14.5 dB with this record length. The second half "
        "was the stimulus. `_signal` held the offset in cycles per "
        "**sample** while this sweep's axis is `M`, and the carrier "
        "acquisition bound is `bn_carrier / m` — so the same literal put "
        "BPSK at 0.8x its bound and **8PSK at 3.2x**, asking one order a "
        "four times harder question than another and calling the "
        "difference a property of 8PSK.\n\n"
        "F5's own diagnosis named the mechanism and missed the cause: *a "
        "non-monotone rate across a monotone axis is the signature of an "
        "acquisition that succeeds or fails per record rather than a "
        "steady-state loss*. Exactly so — the marginal acquisition was "
        "this report's, not the receiver's. Controlled comparison, same "
        "code and same seeds, only the offset differing (three seeds per "
        "cell):\n\n"
        "| Es/N0 | at 3.2x the bound | at the bound |\n"
        "|---|---|---|\n"
        "| 12 dB | refused / 9.75e-1 / refused "
        "| 3.94e-2 / 4.22e-2 / 3.73e-2 |\n"
        "| 14 dB | refused / 9.92e-1 / refused "
        "| 7.87e-3 / 1.02e-2 / 9.25e-3 |\n"
        "| 16 dB | refused / 9.91e-1 / 9.94e-1 "
        "| 1.31e-3 / 1.51e-3 / 1.44e-3 |\n"
        "| 17 dB | 1.00 / 1.00 / 1.00 | 6.56e-5 / 7.22e-4 / 5.90e-4 |\n\n"
        "Seeded at the bound, 8PSK is strictly monotone from 12 to 20 dB "
        "with a seed spread that is counting noise. The two windows "
        "overlap comfortably: **14.0 dB resolves at +0.41 dB of "
        "implementation loss**, the tightest cell in §2.1's grid, against "
        "+0.54 to +0.58 for BPSK and +0.43 to +0.52 for QPSK. §4 asserts "
        "the loss envelope at every order, 8PSK included.\n\n"
        "F4 was the same defect on the `sps` axis. One stimulus error "
        "produced two findings, each blamed on the receiver, and neither "
        "survived being asked in the loop's own units (doppler#843).",
    )

    R.find(
        "F6",
        "GAP",
        "**Some Es/N0 cells cannot bound the implementation loss at all, "
        "and the sweep did not say so until it was asked.** "
        "At "
        f"{NSYM} symbols a cell measuring zero errors has a 95% upper "
        "bound of the rule-of-three floor, ~3/N = 1.5e-4 — which at M = 2, "
        "12 dB sits 300x ABOVE the coherent bound of 4.8e-7. A limit "
        'reading "within 10x the bound" there is satisfied by every '
        "receiver that exists, so it reported coverage the measurement did "
        "not have. §2.1 now carries the meter's own `errors` and `95% hi` "
        "columns and a `resolves` verdict per cell, and §4 asserts only "
        "over the cells that resolve — 7 of the 9 as this runs, spanning "
        "all three orders, the two unresolved ones being the 8PSK cells "
        "above the measurability ceiling. It was 3 of 9 while F5's "
        "seeding defect kept 8PSK out of the envelope entirely, which is "
        "worth noting here: this finding's mechanism was real, and its "
        "SIZE was inflated by a defect in another part of the same "
        "report. The guard it replaced keyed on the "
        "BOUND being small (`theory < 1e-2`), which is a different "
        "question from whether the RECORD can resolve it, and admitted "
        "exactly the vacuous cells. This is a property of the record "
        "length rather than of the receiver, and the fix is symbols: "
        "reaching `target_errors = 100` at a 1e-6 bound needs ~1e8 of "
        "them, which is a `make characterize` sweep and not a per-push "
        "validator (`docs/dev/contributing/validation.md`, and the "
        "category exists for this). It is the same wall §2.8 hits from "
        "the other side — the "
        "header's `m_out` figures are anchored at `SER = 1e-3` per M, "
        "which this geometry cannot reach either — and it is what "
        "[#781](https://github.com/doppler-dsp/doppler/issues/781) is "
        "asking for.",
    )

    R.find(
        "F7",
        "GAP",
        f"**{d['claims']['absent']} of the header's "
        f"{d['claims']['total']} claims carry something covered by "
        "nothing, in either language** — and the inventory in §1.1 is "
        "what made that visible, which is the argument for the inventory "
        "being IN the report. They split three ways, and the split is "
        "what says who should act. **Six are actionable and tracked by "
        "[gh-814](https://github.com/doppler-dsp/doppler/issues/814)**: a "
        "binding reaches every one, so a measurement is possible and "
        "simply does not exist. **Two have no binding at all** — C7's "
        "per-arm tap count and C22's refusal of `bn_agc_ratio >= 1` — so "
        "they belong in `native/validation/` if anywhere, and calling "
        "them a Python gap would misfile them. **One is the differential "
        "demap** (C24), already named in §5 and older than this "
        "certification.\n\n"
        "**Two of the six have since been closed in C, and one of the six "
        "was this table being wrong.** C15's verify counts are now pinned "
        "by `test_mpsk_receiver_core.c` §12 as TIME hysteresis — on one "
        "record a shorter `n_up` must declare strictly earlier, and by at "
        "least the extra symbols asked for, which a count wired to "
        "nothing fails. And **C6 was never as absent as this row "
        "claimed**: §4 pins the flip, the drop-back AND the re-declare, "
        "so the inventory understated it. What §4 genuinely cannot test "
        "is the part the header actually argues — that the estimate is "
        "carried across rather than re-acquired — because §4 re-seeds the "
        "carrier by hand across the outage, and would therefore pass "
        "against a receiver that cleared it. §4b tests BOTH "
        "transitions directly, in one-symbol steps so the measurement "
        "straddles each flip. The lesson is the inventory's "
        "own: a row read from a test's HEADLINE rather than its "
        "assertions is the same error the campaign calls "
        "pinned-only-at-literals, committed by the auditor instead of "
        "the author.\n\n"
        "**C9 and C13 are closed too, and C9 is the one that mattered "
        "most.** `bn_carrier`'s symbol-rate normalisation is the "
        "`@warning`'s own headline — *\"at the old default `sps = 8` the "
        'same number is now an 8x wider loop"* — and a regression to '
        "input-rate normalisation would have looked correct at `sps = 8`, "
        "the rate every other test in the C file uses, and been wrong "
        "everywhere else. It is now C §13: settling time in SYMBOLS is "
        "invariant across a 4x span of `sps` at one `bn` (measured 320 "
        "symbols at `sps` 8, 16 and 32), where an input-rate `bn` would "
        "scale it by 4. The earlier attempt through `lock_time` was "
        "abandoned rather than faked — 955 symbols at `sps = 4` against 51 "
        "at 16 — and the fix was to read `get_norm_freq` directly instead "
        "of a statistic with its own EMA and verify counts. C13's sharp "
        "edge is C §14, pinned as the degeneracy it is (+11 dB of EVM "
        'against `m_out = 8`) rather than as *"fails about half the '
        'time"*, which is a distribution over seeds and not an '
        "assertion.\n\n"
        "**What is left is left because it was measured and did not "
        "hold**, which is a different statement from unmeasured. C21's "
        "`A^2` under-drive: §2.9 shows a level error reaching "
        "`timing_rate`, but the proxy is not monotone — at 25 dB and "
        "amplitude 0.25 the un-levelled receiver reads BETTER (4 ppm "
        "against 5), so an assertion on it would have been true at one "
        "operating point and false at another. C16's saturation claim is "
        "worse than unmeasured: swept 4 to 1024 arms at an off-grid rate "
        "the EVM is flat to 0.08 dB, so this geometry saturates below 4 "
        "and does not locate 64 as the knee at all. Both need a harsher "
        "stimulus than a validator builds — a larger clock offset, a lower "
        "Es/N0, trials — so the home is `make characterize` or "
        "`native/validation/` "
        "(`docs/dev/contributing/adding-algorithms.md` phase 7). With "
        "C15's 0.8x level ratio, that is what "
        "[gh-814](https://github.com/doppler-dsp/doppler/issues/814) still "
        "tracks; none is a regression, and the object is certified on what "
        "IS measured.",
    )

    R.find(
        "F8",
        "FIXED",
        "**Every `SER theory` figure in this report was the bound at the "
        "wrong Es/N0, and the report's own headline numbers moved when it "
        "was corrected.** `ber_theory_ser`'s second argument is LINEAR "
        'Es/N0 — its header says so in capitals, *"at matched-filter '
        'Es/N0 (LINEAR)"* — and this validator passed dB. Every other '
        "caller in the tree writes the conversion, from "
        "`ber_esn0_db_for_ser.c` to `ber_awgn_demo.py` to "
        "`test_mpsk_receiver_performance.py`, so the defect was this "
        "file's alone. The consequence was not an offset but a FOLD: "
        "`db -> 10*log10(db)` is compressive, so a sweep of 8/12/16 dB "
        "was scored against the bound at 9.0/10.8/12.0 dB and the sign of "
        "the error reversed across the sweep — the object read as falling "
        "behind at low Es/N0 and catching up at high, which is a "
        "plausible-looking shape and is why it survived review. It also "
        "made 8PSK look **20 dB better than the bound** at 18 dB, which "
        "is the impossible reading that finally exposed it (nothing beats "
        "a matched filter). Corrected numbers: BPSK's loss is +0.47 to "
        "+0.67 dB and QPSK's +0.33 to +0.39, where the old table implied "
        "+1.34 dB at BPSK and a 10x rate deficit at QPSK. Fixed by "
        "routing every bound through one `_bound(m, esn0_db)` helper, so "
        "there is a single conversion site rather than three call sites "
        "that agreed by luck. Not open: the defect was in the evidence, "
        "not the object, and the object is better than the old report "
        "said.",
    )

    R.find(
        "F3",
        "BY DESIGN",
        "The M-fold phase ambiguity is **permanent** (§2.6): there is "
        "no decision-directed stage anywhere in this receiver, so nothing "
        "ever pins absolute phase. That is why `_ser()` here searches the "
        "rotation, and why a caller wanting bits rather than symbols needs "
        "either `differential=1` or a downstream sync word. Coherent "
        "demapping with neither is a misconfiguration, not a choice.",
    )


# ── phase 3: limits ──────────────────────────────────────────────────────


def limits(R, d):
    """The envelope a caller may rely on. A failure here is a regression."""
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding — each is asserted by "
        "`src/doppler/track/tests/test_validation_limits.py`."
    )
    R.md()

    # Asserted only where the MEASUREMENT can carry the claim, which is
    # `resolves` and not the size of the bound. The guard this replaced was
    # `theory < 1e-2` -- a statement about the bound being tight, which is
    # a different question from whether 20 000 symbols can tell a receiver
    # ON that bound from one 10x behind it. It admitted the two BPSK cells
    # that measure zero errors, where the meter's own 95% floor is 300x
    # above the bound and the assertion is satisfied by any receiver at
    # all. Those cells stay in §2.1 as characterisation; F6 judges them.
    n_res = 0
    for m, esn0, ser, theory, loss, _nerr, _hi, resolves, _lock in d["ser"]:
        # M = 8 used to be EXCLUDED here, on F5's reading that the object
        # did not deliver a measurable loss at that order. It does: F5's
        # collapse was this report seeding 8PSK at 3.2x its acquisition
        # bound while BPSK sat at 0.8x, and at the bound the 14 dB cell
        # resolves at +0.41 dB -- the tightest in this grid. Every order
        # that resolves is now certified on the same terms, which is what
        # the exclusion was costing.
        if not resolves:
            continue
        n_res += 1
        if ser is None:
            R.limit(
                False,
                f"M={int(m)} at Es/N0 {esn0:.1f} dB: the meter REFUSED "
                f"this cell, so the claim is unestablished rather than met",
            )
            continue
        # The dB, not the rate ratio: a ratio has no fixed dB meaning
        # across the curve, and dB is the unit a link budget is written in.
        # `loss is None` only where `ser == 0.0`, which cannot reach here
        # -- a cell with no errors cannot have MIN_ERRORS of them -- so it
        # is a genuine impossibility rather than a case being swallowed.
        R.limit(
            loss is not None and loss <= LOSS_MAX_DB,
            f"M={int(m)} at Es/N0 {esn0:.1f} dB: implementation loss "
            f"{loss:+.2f} dB against the coherent bound, inside "
            f"{LOSS_MAX_DB:.0f} dB (SER {ser:.2e} vs {theory:.2e}, "
            f"{_nerr} errors)",
        )
    # Without this the loop above is vacuous if `resolves` ever goes all
    # False -- zero limits recorded, and a tally that still reads 100%.
    # The same shape as a reject test that passes because its precondition
    # stopped holding.
    R.limit(
        n_res >= 6,
        f"the sweep resolves {n_res} cells with enough errors to bound the "
        f"implementation loss at all — the assertions above are not "
        f"vacuous, and the grid is derived to keep them that way",
    )
    # 8PSK's two limits used to PIN a collapse -- one asserting the low
    # cell was broken, one that the high cell was clean, so that a fix
    # would show up as a failing limit. The fix arrived (F5) and they did
    # exactly that. What replaces them is the claim the pinning stood in
    # for: 8PSK is MONOTONE across its axis, which is the property whose
    # absence F5 read as a receiver defect and which a per-cell loss bound
    # does not capture. A rate that rises with Es/N0 anywhere is the
    # signature of an acquisition failing per record, and that is the
    # thing worth a gate rather than any single cell's value.
    eight = [r for r in d["ser"] if r[0] == 8]
    rates = [(r[1], r[2]) for r in eight if r[2] is not None]
    mono = len(rates) == len(eight) and all(
        b[1] <= a[1] for a, b in zip(rates, rates[1:])
    )
    R.limit(
        mono,
        "8PSK's error rate falls monotonically across its Es/N0 sweep "
        "with no refusals ("
        + ", ".join(f"{e:.0f} dB {v:.2e}" for e, v in rates)
        + ") — a non-monotone rate on a monotone axis is how an "
        "acquisition that fails per record shows itself (F5)",
    )

    for esn0, evm, bound, excess in d["evm"]:
        R.limit(
            evm > bound - 0.5,
            f"EVM at Es/N0 {esn0:.0f} dB does not beat the matched-filter "
            f"bound ({evm:.2f} dB vs {bound:.2f} dB) — a lower EVM would "
            f"be a broken measurement",
        )
        R.limit(
            excess < 6.0,
            f"EVM at Es/N0 {esn0:.0f} dB is within 6 dB of the bound "
            f"({excess:.2f} dB)",
        )

    for esn0, _blind, err, _ser_v in d["m2m4"]:
        R.limit(
            abs(err) < 3.0,
            f"blind M2M4 at Es/N0 {esn0:.0f} dB agrees with the "
            f"data-aided figure to {err:+.2f} dB",
        )

    dv = d["derived"]
    R.limit(dv["m_out"] == 8, "m_out derives to 8 at sps=8")
    R.limit(dv["num_phases"] == 64, "num_phases derives to 64")
    R.limit(
        abs(dv["zeta"] - 0.70710678118654752) < 1e-12,
        "zeta derives to 1/sqrt(2)",
    )
    R.limit(
        abs(dv["lock_thresh"] - 0.4999) < 1e-12,
        "lock_thresh derives to 0.4999 (Pfa 5e-6)",
    )
    R.limit(
        abs(dv["bn_agc_ratio"] - 0.05) < 1e-12,
        "bn_agc_ratio derives to 0.05",
    )

    # The output count is a rate claim and holds at EVERY rate measured.
    # The SER claim is narrower on purpose: sps = 31.7 locks and still
    # misses the bound (F4), so certifying it would be certifying a
    # result the object does not deliver.
    for sps, got, expect, rate, ser in d["irrational"]:
        R.limit(
            abs(got - expect) / expect < 0.02,
            f"sps={sps:g}: output count is the integral of the rate to 2% "
            f"({int(got)} vs {expect:.0f})",
        )
        # The loop's own accumulator, which the count cannot substitute
        # for: a rate steered wrong and compensated by dropped symbols
        # gives the right count. Asserted at EVERY rate, unlike the SER.
        R.limit(
            abs(rate - sps) / sps < 1e-3,
            f"sps={sps:g}: and the timing loop RECOVERS that rate to "
            f"{1e6 * abs(rate - sps) / sps:.0f} ppm ({rate:.5f}) — so the "
            f"count is right for the right reason",
        )
        # No `sps <= 24` guard any more. It stood because F4 read the
        # 31.7 row as a receiver limit; that row was seeded past its
        # acquisition bound, and at the bound every rate here demodulates.
        _limit_ser(
            R,
            ser,
            3e-3,
            f"sps={sps:g}",
            lambda s, sp=sps: (
                f"sps={sp:g}: recovers symbols (SER {s:.2e}) — an "
                f"irrational rate is no harder than an integer one, and "
                f"neither is a high one"
            ),
        )

    # The steer is UNGATED, at every order. Stated per row rather than as
    # an aggregate: a single M passing would be equally satisfied by a
    # receiver that gated the steer everywhere except there.
    for u in d["ungated"]:
        R.limit(
            u["declared_at"] is not None,
            f"M={u['m']}: the lock indicator declares at all (symbol "
            f"{u['declared_at']}) — without a declaration the row below "
            f"would be vacuous",
        )
        R.limit(
            abs(u["frac"]) > 0.25 and u["frac"] > 0.0,
            f"M={u['m']}: the NDA steer is UNGATED — it had already "
            f"acquired {u['frac']:.3f} of the true offset, toward it, at "
            f"the last symbol before the indicator declared. A steer gated "
            f"on the indicator reads exactly 0.000 here",
        )
    # NOTHING is asserted about the SIZE of that fraction. The detector is
    # a threshold test on the M-th-power lock statistic, which measures
    # phase coherence and not frequency error, so the declaration instant
    # carries no convergence information and a bound in either direction
    # would gate on an incidental correlation.

    # The false lock is asserted as a LATTICE and as an invisibility, and
    # both halves have to hold for the finding to be the finding: a lock
    # that drifted would not be an equilibrium, and one that showed up in
    # the metrics would not be silent.
    _k, ftrue, tracked, ferr, flock, fevm, fblind = d["false_lock"]["k1"]
    _ck, _cf, _ct, _ce, clock, cevm, cblind = d["false_lock"]["control"]
    R.limit(
        abs(ferr + ftrue) < 1e-4,
        f"the false lock at k·F/M is an EQUILIBRIUM, not a drift: the loop "
        f"sits at {tracked:+.5f} against a true {ftrue:.5f}, so the error "
        f"is F/M to within 1e-4 cycles/sample",
    )
    R.limit(
        flock > 0.5,
        f"and reports a lock statistic a caller would trust ({flock:.3f}, "
        f"above the derived declare threshold of 0.4999)",
    )
    R.limit(
        clock - flock < 0.1 and cevm - fevm < 2.0 and cblind - fblind < 2.0,
        f"while NO self-referenced metric separates it from the true lock: "
        f"lock {flock:.3f} vs {clock:.3f}, EVM {fevm:.2f} vs {cevm:.2f} dB, "
        f"blind M2M4 {fblind:.2f} vs {cblind:.2f} dB — every one inside "
        f"what a slightly noisier channel would give",
    )
    R.limit(
        d["false_lock"]["sweep"][1][6] > 10.0,
        f"and the blind estimator is positively healthy there "
        f"({fblind:.2f} dB), so it cannot be used as a false-lock alarm "
        f"either — defend with an external reference or a sync word (F2)",
    )

    # m_out: what this geometry establishes is the DIRECTION and a floor on
    # the magnitude, at every M. Not the ordering across M -- see §2.8.
    for m, ex8, ex4, cost in d["m_out"]:
        R.limit(
            cost > 2.0,
            f"M={int(m)}: halving m_out from 8 to 4 costs {cost:+.2f} dB of "
            f"EVM ({ex8:+.2f} -> {ex4:+.2f} dB over the bound) — the "
            f"matched filter's resolution is never free",
        )
    R.limit(
        max(r[1] for r in d["m_out"]) < 1.0,
        f"and the DERIVED m_out = 8 is within 1 dB of the bound at every M "
        f"(worst {max(r[1] for r in d['m_out']):+.2f} dB), so the "
        f"derivation lands on a configuration that costs nothing",
    )

    # The AGC's law is a slope and an offset, and the slope is the claim.
    offs = [r[2] for r in d["agc"]]
    R.limit(
        max(offs) - min(offs) < 0.05,
        f"the AGC applies exactly the reciprocal of the input level: "
        f"`agc_gain_db + 20log10(amp)` is constant to "
        f"{max(offs) - min(offs):.4f} dB across a 32x amplitude span, so "
        f"the readback is an absolute level estimate and not just a trend",
    )
    R.limit(
        abs(offs[0]) < 1.0,
        f"and that constant is {offs[0]:+.3f} dB, so `P_ref` IS the "
        f"unit-amplitude reference the header names, to within a dB",
    )
    on_ppm = [r[3] for r in d["agc"]]
    off_ppm = [r[4] for r in d["agc"]]
    R.limit(
        max(on_ppm) < 100.0,
        f"the LEVELLED receiver recovers the symbol rate to within "
        f"{max(on_ppm):.0f} ppm at every amplitude across a 32x span — the "
        f"AGC removes the level from the timing loop's gain entirely",
    )
    R.limit(
        max(off_ppm) > 2.0 * max(on_ppm),
        f"and the un-levelled control does NOT ({max(off_ppm):.0f} ppm "
        f"worst against {max(on_ppm):.0f}) — so the AGC is load-bearing "
        f"and this pair is not satisfied by an AGC that does nothing",
    )
    R.limit(
        off_ppm[0] < max(off_ppm),
        f"with the un-levelled error growing as the level falls "
        f"({off_ppm[0]:.0f} ppm at unit amplitude to {max(off_ppm):.0f}), "
        f"which is the `A^2` under-drive on the axis the header names",
    )
    # The blind spot, asserted. A future change that made `lock` sensitive
    # to level would take this red -- which is the point: it says WHERE
    # this diagnostic works, and a caller relies on that.
    lk_on = [r[5] for r in d["agc"]]
    lk_off = [r[6] for r in d["agc"]]
    R.limit(
        max(lk_on) - min(lk_on) < 0.05 and max(lk_off) - min(lk_off) < 0.1,
        f"while the CARRIER lock statistic is blind to all of it — "
        f"{min(lk_on):.3f}-{max(lk_on):.3f} levelled and "
        f"{min(lk_off):.3f}-{max(lk_off):.3f} un-levelled — because the "
        f"discriminator divides out its own `|z|^M`. A level problem is "
        f"visible in `agc_gain_db` and `timing_rate`, and in neither "
        f"`lock` nor the error rate",
    )

    li = d["life"]
    R.limit(li["reproduces"], "`reset()` reproduces the first run bit for bit")
    R.limit(
        li["resumed"],
        f"a {li['state_bytes']}-byte blob resumes the receiver bit-exactly "
        f"mid-stream, checked by overwriting a WARM instance so the blob "
        f"has to DETERMINE the continuation rather than not contradict it",
    )
    R.limit(
        li["rejects"],
        "and a clobbered blob is rejected rather than reinterpreted",
    )
    R.limit(
        li["nprobe"] == 15,
        f"the receiver publishes {li['nprobe']} telemetry probes spanning "
        f"all three subsystems (`rx.car.*`, `rx.sync.*`, `rx.agc.*`)",
    )
    R.limit(
        li["nrec"] is not None,
        f"and the attach succeeds WHOLE: every probe carries the same "
        f"record count ({li['nrec']}), emitted once per symbol and thinned "
        f"by `decim` alone",
    )


# ── plots ────────────────────────────────────────────────────────────────


def plots(d):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))

    for m, mark in ((2, "o"), (4, "s"), (8, "^")):
        pts = [(r[1], r[2], r[3]) for r in d["ser"] if r[0] == m]
        # A refused cell has no SER, and it must not be drawn as one. The
        # floor this used to apply would have put `None` -> 1e-6 at the
        # BOTTOM of a log axis, rendering "the meter would not defend a
        # number here" as the best result on the plot. Dropping the point
        # leaves a visible gap in the line, which is the honest picture,
        # and the theory curve stays whole so the gap is legible against
        # it. The tick marks are per-point, so a gap does not shift any
        # remaining marker.
        meas = [(p[0], p[1]) for p in pts if p[1] is not None]
        ax[0].semilogy(
            [e for e, _ in meas],
            [max(s, 1e-6) for _, s in meas],
            mark + "-",
            label=f"M={m} measured",
        )
        ax[0].semilogy(
            [p[0] for p in pts],
            [max(p[2], 1e-6) for p in pts],
            "k--",
            alpha=0.4,
        )
    ax[0].set_xlabel("Es/N0 (dB)")
    ax[0].set_ylabel("SER")
    ax[0].set_title("SER vs the coherent bound (dashed)")
    ax[0].grid(True, alpha=0.3)
    ax[0].legend(fontsize=8)

    e = d["evm"]
    ax[1].plot([r[0] for r in e], [r[1] for r in e], "o-", label="EVM")
    ax[1].plot(
        [r[0] for r in e], [r[2] for r in e], "k--", label="bound −(Es/N0)"
    )
    mm = d["m2m4"]
    ax[1].plot(
        [r[0] for r in mm], [-r[1] for r in mm], "s:", label="−M2M4 (blind)"
    )
    ax[1].set_xlabel("Es/N0 (dB)")
    ax[1].set_ylabel("dB")
    ax[1].set_title("EVM and blind M2M4 against the bound")
    ax[1].grid(True, alpha=0.3)
    ax[1].legend(fontsize=8)

    fig.tight_layout()
    fig.savefig(HERE / "quality.png", dpi=110)
    plt.close(fig)

    # The cascade's two configuration axes, side by side: what the matched
    # filter's resolution costs, and the AGC's level law. Both are floors
    # rather than curves against Es/N0, which is why they get their own
    # figure instead of another trace on the one above.
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))

    ms = [r[0] for r in d["m_out"]]
    xpos = np.arange(len(ms))
    ax[0].bar(
        xpos - 0.18,
        [r[1] for r in d["m_out"]],
        0.36,
        label="m_out = 8 (derived)",
    )
    ax[0].bar(xpos + 0.18, [r[2] for r in d["m_out"]], 0.36, label="m_out = 4")
    for i, r in enumerate(d["m_out"]):
        ax[0].annotate(
            f"+{r[3]:.2f} dB",
            (i, max(r[1], r[2]) + 0.12),
            ha="center",
            fontsize=8,
        )
    ax[0].set_xticks(xpos)
    ax[0].set_xticklabels([f"M = {m}" for m in ms])
    ax[0].set_ylabel("EVM excess over the bound (dB)")
    ax[0].set_title("What halving m_out costs, at 18 dB Es/N0")
    ax[0].grid(True, alpha=0.3, axis="y")
    # Headroom for the annotations, which otherwise clip against the frame.
    ax[0].set_ylim(0.0, max(r[2] for r in d["m_out"]) * 1.22)
    ax[0].legend(fontsize=8, loc="center right")

    amps = [r[0] for r in d["agc"]]
    ax[1].semilogx(amps, [r[1] for r in d["agc"]], "o-", label="agc_gain_db")
    ax[1].semilogx(
        amps,
        [-20 * np.log10(a) for a in amps],
        "k--",
        alpha=0.6,
        label="the law: −20log10(amp)",
    )
    ax[1].set_xlabel("input amplitude")
    ax[1].set_ylabel("dB")
    ax[1].set_title("The AGC applies the reciprocal of the level")
    ax[1].grid(True, alpha=0.3)
    ax[1].legend(fontsize=8)

    # The rate error on its own axis: it is what the A^2 under-drive moves,
    # and plotting it against the gain law puts the diagnostic that WORKS
    # beside the one that is exact.
    axr = ax[1].twinx()
    axr.semilogx(
        amps,
        [r[3] for r in d["agc"]],
        "s:",
        color="tab:green",
        label="agc=1",
    )
    axr.semilogx(
        amps, [r[4] for r in d["agc"]], "^:", color="tab:red", label="agc=0"
    )
    axr.set_ylabel("|rate error| (ppm)")
    axr.legend(
        fontsize=8,
        loc="center right",
        title="timing rate",
        title_fontsize=8,
    )

    fig.tight_layout()
    fig.savefig(HERE / "cascade.png", dpi=110)
    plt.close(fig)


# ── the runner ───────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    R = Report(write=write)

    R.md("## 1. The object")
    R.md()
    R.md(
        "`track.MpskReceiver` — a streaming M-PSK receiver that owns no "
        "filter, no NCO and no interpolator of its own: a matched DDC "
        "with two loops closed around its two control ports."
    )
    R.md()
    R.md(
        "**One object, three faces.** `track.MpskReceiverR` is the same "
        "core reached through `mpsk_receiver_create_real()` — a matched "
        "DDCR instead of a matched DDC — and is a view rather than a "
        "second type as of the collapse "
        "([`docs/design/mpsk.md`, the collapse]"
        "(../../../../../../docs/design/mpsk.md"
        "#12-the-collapse-one-object-two-faces)). Every loop, "
        "discriminator and demapper decision below the "
        "front end is one implementation, which is what makes a claim "
        "about the loops a claim about all three faces."
    )
    R.md()
    R.md(
        "**This report measures the COMPLEX face.** The three rate "
        "conventions the real face changes are pinned in C and named as "
        "C29-C33 below, each with the section that carries it; the "
        "characterisation in section 2 has NOT been re-run through "
        "`MpskReceiverR`, and saying so is the point of this paragraph "
        "rather than an apology for it. What that leaves unmeasured from "
        "Python — the SER-against-bound curve, the false-lock geometry "
        "and the level diagnostics, all of which could differ behind an "
        "R2C halfband — is gh-830."
    )
    R.md()
    R.md(
        "- Design: [`docs/design/mpsk.md`]"
        "(../../../../../../docs/design/mpsk.md)"
    )
    R.md(
        "- Header (the SSOT): "
        "[`native/inc/mpsk_receiver/mpsk_receiver_core.h`]"
        "(../../../../../../native/inc/mpsk_receiver/mpsk_receiver_core.h)"
    )
    R.md(
        "- C pins: "
        "[`native/tests/test_mpsk_receiver_core.c`]"
        "(../../../../../../native/tests/test_mpsk_receiver_core.c)"
    )
    R.md(
        "- The standard battery: "
        "[`native/validation/rx_battery.c`]"
        "(../../../../../../native/validation/rx_battery.c)"
    )
    R.md(
        "- Naming and layer: "
        "[`docs/design/api-taxonomy.md`]"
        "(../../../../../../docs/design/api-taxonomy.md) — composite "
        "receivers, layer 5"
    )
    R.md()
    R.md(
        "This section links rather than restates. Where the design "
        "explains *why*, it is the design's job; this report measures "
        "*whether*."
    )
    R.md()

    R.md("### 1.1 Claim coverage — every prose claim in the header")
    R.md()
    R.md(
        "The campaign's order is header first: enumerate what "
        "`mpsk_receiver_core.h` asserts, then ask of each whether it is "
        "pinned, pinned only at literals, or absent. This table is that "
        "inventory, and it is in the report rather than in a notebook "
        "because it is the only place a reader can see what is NOT "
        "covered. **C-ONLY** marks a claim no binding reaches; a row with "
        "no report section is carried by C alone, and a row with neither "
        "is named as absent rather than omitted."
    )
    R.md()

    # The inventory is a literal because it is a JUDGEMENT about the
    # header's prose, not a measurement -- deriving it would mean parsing
    # Doxygen and guessing which sentences are claims. It is checked the
    # only way it can be: every §ref below is validated by
    # `Report._self_check`, so a section that is renamed or removed takes
    # the render down rather than leaving a dangling promise here.
    claims = [
        [
            "C1",
            "a complete inline modem at **any** input rate; owns no "
            "filter, NCO or interpolator of its own",
            "C §1 + §2.5",
        ],
        [
            "C2",
            "the matched DDC's terminal polyphase stage IS the matched "
            "filter, and the arm it selects IS the fractional timing delay",
            "**C-ONLY** — structural; C §2, and `ddc`'s own certification",
        ],
        [
            "C3",
            "the timing half is literally RateSync's loop, not a copy",
            "**C-ONLY** — structural; RateSync's own report certifies it",
        ],
        [
            "C4",
            "predetection de-rotation in the LO at the front, "
            "postdetection discrimination on the matched-filtered symbols",
            "**C-ONLY** — structural; C §2",
        ],
        [
            "C5",
            "ONE discriminator steers the LO: the NDA M-th-power error, "
            "needing no data and no timing, on every strobe",
            "C §1c + §4 + §2.6",
        ],
        [
            "C6",
            "nothing gates it — the steer runs whether or not the lock "
            "indicator has declared, and the estimate it builds survives "
            "a declaration and a withdrawal",
            "C §1c (0.376 of the offset acquired before the declaration; "
            "a gated steer reads 0.0) + C §4 (the estimate is CONTINUOUS "
            "across both lock edges)",
        ],
        [
            "C7",
            "the cascade plans itself: ~34 taps/arm across a 64x span "
            "of input rates, against 4225 for a single-stage design",
            "**absent** — no binding reaches the tap count (F7)",
        ],
        [
            "C8",
            "an irrational `sps` is no harder than an integer one",
            "§2.5 — count AND the loop's own recovered rate",
        ],
        [
            "C9",
            "`bn_carrier` is now normalised to the SYMBOL rate, not the "
            "input rate (the @warning's behaviour change)",
            "C §13 NEW — settling in SYMBOLS is invariant across a 4x "
            "span of `sps` at one `bn`; an input-rate `bn` would scale "
            "it 4x, which is what makes the invariance the test",
        ],
        [
            "C10",
            "zero means derive for five parameters, each read back, "
            "and the derivation runs BEFORE validation",
            "C §1b + §2.4",
        ],
        [
            "C11",
            "the derivation RULE: the largest even `m_out` in 2..8 the "
            "rate allows",
            "**C-ONLY** — `JM_FORCEINLINE`, no binding (F1)",
        ],
        [
            "C12",
            "`m_out = 8` is not optional at M = 8; halving it costs "
            "1.7 / 1.6 / **3.0** dB at M = 2 / 4 / 8",
            "§2.8 — direction and magnitude confirmed, the M-ORDERING is "
            "not (F6)",
        ],
        [
            "C13",
            "never pair `m_out = 2` with I&D — acquisition fails about "
            "half the time",
            "C §14 NEW — pinned as the DEGENERACY (+11 dB of EVM against "
            "`m_out = 8`) rather than as the failure RATE, which is a "
            "distribution over seeds and not a unit test",
        ],
        [
            "C14",
            "`lock_thresh` derives as `sigma_H0 * eta(Pfa)` = "
            "`0.1132 * 4.4159`; the sd is 0.1132 for EVERY M",
            "§2.4 reads the value back; the sd is `carrier_nda` §2.7",
        ],
        [
            "C15",
            "the drop threshold sits at 0.8x with 8-up / 32-down "
            "verify counts",
            "C §12 NEW — the counts are pinned as TIME hysteresis "
            "(a shorter `n_up` declares strictly earlier); the 0.8x "
            "level ratio itself is **absent** (F7)",
        ],
        [
            "C16",
            "`num_phases` derives to 64, the measured saturation point "
            "against the old 1024",
            "§2.4 pins that it derives to 64. SATURATION is **absent** "
            "and not merely unmeasured: swept 4 to 1024 at an off-grid "
            "rate the EVM is flat to 0.08 dB, so this geometry saturates "
            "BELOW 4 and does not locate 64 as the knee at all (F7)",
        ],
        [
            "C17",
            "an M-th-power detector at update rate `F` observes only "
            "`|df| < F/(2M)`; the discriminator updates once per symbol, "
            "so the range is `Rs/(2M)`",
            "**C-ONLY** — the relation governs the derived pull-in; the "
            "three-tap ranking that measured it is retired with the taps",
        ],
        [
            "C18",
            "`df = k*F/M` is a stable FALSE lock at every tap, "
            "reporting a healthy statistic no self-referenced metric flags",
            "§2.7 — measured at k = 1 and 2 against a true-lock control; "
            "*at every tap* is **C-ONLY**",
        ],
        [
            "C19",
            "one AGC, before the terminal stage, serving BOTH loops, "
            "sized against the slower",
            "C §9 + §2.9",
        ],
        [
            "C20",
            "`get_agc_gain_db` settles at `-10*log10(P_in/P_ref)` and "
            "is THE diagnostic for a level problem",
            "§2.9 — slope exact",
        ],
        [
            "C21",
            "with `agc = 0` the timing loop is under-driven by `A^2`",
            "§2.9 shows a level error reaching `timing_rate`; the `A^2` "
            "LAW is **absent**, and the rate-error proxy does not carry "
            "it — at 25 dB and amplitude 0.25 the un-levelled receiver "
            "reads BETTER (4 ppm against 5), so the effect is not "
            "monotone in level (F7)",
        ],
        [
            "C22",
            "`bn_agc_ratio` must be in (0, 1); construction REFUSES 1 "
            "or above rather than warning",
            "C §1 — **absent** from Python",
        ],
        [
            "C23",
            "there is no handover, no warmup, no lock gate and no timing "
            "gate anywhere in the receiver — structurally, not by default",
            "C §1c + §2.6 (the knobs are REFUSED, not defaulted)",
        ],
        [
            "C24",
            "and its M-fold ambiguity is PERMANENT, so `differential` "
            "defaults to 1",
            "§2.6 + F3 — the DEMAP itself is **absent**",
        ],
        [
            "C26",
            "the cascade rate is `m_out/sps <= 1`, so one input "
            "completes at most one on-time strobe",
            "**C-ONLY** — the inline step function",
        ],
        [
            "C27",
            "reset re-seeds everything, and the same input twice "
            "around a reset reproduces bit-for-bit",
            "C §1 + §2.10",
        ],
        [
            "C28",
            "pointer-bearing state resumes exactly from a blob, and a "
            "clobbered envelope is rejected",
            "C §10 + §2.10",
        ],
        [
            "C29",
            "the real face is the SAME object behind an R2C halfband — "
            "every loop, discriminator and demapper is "
            "one implementation over one `mpsk_rx_loops_t`",
            "C §15-21 (the real face through every lifecycle, SER, "
            "lock and state claim) + C §22 (telemetry reaches the "
            "OTHER front end's AGC) + C §20 (a blob from one face is "
            "refused by the other, by envelope magic)",
        ],
        [
            "C30",
            "the LO runs at HALF the input rate, and every caller-facing "
            "frequency is converted back to the input rate",
            "C §23 NEW — both halves, each proven by sabotage: the loop "
            "GAIN against `theta_ss = 2*pi*r/wn^2` on a RAMP (a step "
            "cannot see it — a type-2 loop nulls a step regardless of "
            "gain, which is how gh-765 survived every test in the tree), "
            "and the READBACK against a known offset. Asserted by "
            "NEITHER receiver's test before the collapse",
        ],
        [
            "C31",
            "`sps` must exceed `2 * m_out` STRICTLY on the real face, "
            "against `sps >= m_out` on the complex one, and a derived "
            "`m_out` honours the same bound",
            "C §17 — both directions on one geometry (`sps == m_out` "
            "accepted by the complex face, refused by the real), plus "
            "C §21's derived `m_out = 6` at `sps = 16` and the REFUSAL "
            "at `sps = 4`",
        ],
        [
            "C32",
            "`init_norm_freq` is the real IF CENTRE, and the usable band "
            "constrains the OCCUPIED band rather than that centre",
            "C §19 — the centre is clean and an IF whose skirt reaches "
            "the halfband's edge is visibly worse. *This face does not "
            "acquire from a cold zero* is **absent**",
        ],
        [
            "C33",
            "the hot path is not tagged: two `step` entry points, so the "
            "front end is a compile-time fact and `real` is read only on "
            "cold paths",
            "**C-ONLY** — structural, and visible in the header rather "
            "than measurable from either language",
        ],
    ]
    R.table(["#", "claim in `mpsk_receiver_core.h`", "covered by"], claims)
    R.md()
    # Counted from the table rather than written beside it. A hand-typed
    # tally is the first thing to go stale, and the numbers here are the
    # ones the executive summary and F7 both quote.
    n_here = sum(1 for c in claims if "§2." in c[2])
    n_conly = sum(1 for c in claims if c[2].startswith("**C-ONLY**"))
    n_absent = sum(1 for c in claims if "**absent**" in c[2])
    d_claims = {
        "total": len(claims),
        "here": n_here,
        "c_only": n_conly,
        "absent": n_absent,
    }
    R.md(
        f"**{len(claims)} claims: {n_here} reach a measurement in this "
        f"report, {n_conly} are C-ONLY by construction, and {n_absent} "
        f"carry something ABSENT in both languages.** The last group is "
        "the report's own statement of what it does not establish, "
        "collected as F7 rather than left to be noticed. The distinction "
        "between it and the C-ONLY group is the one that matters for what "
        "to do next: a C-ONLY claim is covered, just not from here, while "
        "an absent one is covered by nothing at all. Several rows are "
        "PARTIAL — C16 pins that `num_phases` derives to 64 without "
        "establishing that 64 is the saturation point, C21 shows a level "
        "error reaching the lock statistic without measuring the `A^2` "
        "law — and each is named at the row rather than rounded up to "
        "covered."
    )
    R.md()

    d = characterise(R, write)
    d["claims"] = d_claims
    review(R, d)
    limits(R, d)

    lo2 = [r[4] for r in d["ser"] if r[0] == 2 and r[4] is not None]
    lo4 = [r[4] for r in d["ser"] if r[0] == 4 and r[4] is not None]
    R.executive(
        "MpskReceiver",
        [
            "**BPSK and QPSK sit about half a dB behind theory: "
            f"{min(lo2):+.2f} to {max(lo2):+.2f} dB at M = 2 and "
            f"{min(lo4):+.2f} to {max(lo4):+.2f} dB at M = 4** (§2.1), "
            "roughly constant across each window, which is what an "
            "implementation loss should be. That is the number to put in "
            "a link budget, and it is the number the previous revision of "
            "this report got wrong — it compared against the bound at the "
            "wrong Es/N0 and read ~1.3 dB at BPSK and a 10x rate deficit "
            "at QPSK (F8).",
            "**8PSK is certified, at the one Es/N0 where its bound is "
            "resolvable at all — and the window is narrow for a reason "
            "that is about the BOUND, not the receiver.** The M = 8 curve "
            "falls so slowly that resolving it needs ~2.3M scored symbols "
            "at 18 dB, so the highest Es/N0 this record length can resolve "
            "anything at is 14.5 dB. The receiver reaches under that "
            "comfortably: 14 dB resolves at **+0.41 dB** of loss, the "
            "tightest cell in the grid, against +0.54 to +0.58 at BPSK and "
            "+0.43 to +0.52 at QPSK (§2.1). Anything above 14.5 dB needs "
            "trials per point, which is `make characterize`'s job.",
            "**This report's two unexplained failures were one defect, "
            "and it was in the stimulus.** F4 (`sps = 31.7` recovers the "
            "rate to 2 ppm and cannot be aligned) and F5 (8PSK "
            "non-monotone and seed-dependent) were both the carrier "
            "offset held in cycles per SAMPLE while the sweep's axis "
            "moved — `sps` in one case, `M` in the other — so the loop "
            "was asked a different question at every point and the "
            "hardest ones sat past its acquisition bound. Stated in "
            "cycles per SYMBOL, all four `sps` rows demodulate at SER 0 "
            "and 8PSK is monotone from 12 to 20 dB. Both are now FIXED, "
            "there is no `sps <= 24` ceiling, and the diagnosis both "
            "findings reached — that a converged rate beside an "
            "unalignable record means acquisition, not tracking — was "
            "correct about the mechanism and wrong about whose it was "
            "(§2.5, doppler#843).",
            "**The NDA steer is UNGATED, measured where it could still "
            "fail.** At every order the loop has already acquired a "
            "non-zero fraction of the true offset — toward it — at the "
            "last symbol before the lock indicator declares, where a "
            "steer gated on that indicator would read exactly 0.0. The "
            "handover this section used to certify is gone, on the "
            "measurement recorded in §2.6 (doppler#877).",
            "**Halving `m_out` costs about 2.7 dB of EVM at every M** "
            "(§2.8) — the derivation to 8 lands within a quarter of a dB "
            "of the bound, and 4 costs nearly 3 dB. The header's "
            "M-DEPENDENT figures (1.7 / 1.6 / 3.0 dB) are anchored at "
            "each M's own SER, which this record length cannot reach, so "
            "the ordering across M is unverified here (F6).",
            "**`agc_gain_db` is an absolute level estimate, not just a "
            "trend**: `agc_gain_db + 20log10(amp)` is constant to under "
            "0.01 dB across a 32x amplitude span (§2.9), so a reading far "
            "from 0 dB means what the header says it means.",
            "**The two published health readouts have disjoint blind "
            "spots, and the one a caller reaches for first is the blinder "
            "of the two.** With the AGC off, the recovered symbol rate "
            "degrades from 17 to 172 ppm as the level falls — the `A^2` "
            "under-drive, on the timing loop — while `lock` reads "
            "0.96-0.97 throughout and is not even monotone in level, "
            "because the carrier discriminator divides out its own "
            "`|z|^M` (§2.9). So diagnose a level problem with "
            "`agc_gain_db` and `timing_rate`; `lock` cannot see one, and "
            "at 20 dB neither can the error rate.",
            "**A false lock at `Δf = k·F/M` is invisible to every metric "
            "the receiver computes** (§2.7): the loop parks at an "
            "equilibrium with the frequency wrong by exactly `F/M`, and "
            "against a true-lock control the statistic is 1.4% lower, the "
            "EVM 1.0 dB worse and the blind M2M4 0.9 dB lower. Only the "
            "truth-requiring alignment catches it, by refusing. Defend "
            "with an external frequency reference or a sync word (F2).",
        ],
    )

    if write:
        plots(d)

    R.summary(
        "\n- Raw sweeps: `data/ser_vs_esn0.csv`, `data/evm_vs_esn0.csv`, "
        "`data/m2m4_vs_esn0.csv`, `data/irrational_sps.csv`, "
        "`data/false_lock.csv`, `data/m_out_cost.csv`, "
        "`data/agc_level.csv` — so any number above can be re-derived "
        "rather than taken on the report's word"
        "\n- **Not covered**, and §1.1 is the full accounting rather than "
        "this line: `bits(differential)` resolving the M-fold ambiguity "
        "has no test in either language, the claims F7 collects "
        "(gh-814) have none either, and 8PSK's loss is established at one "
        "Es/N0 only — 14 dB, the highest this record length can resolve "
        "the M = 8 bound at, so the shape of its curve is not covered "
        "here (F6). FER is absent because THIS "
        "OBJECT carries no framing — the tree does: `wfm.Frame`, "
        "`ccsds_tm_frame.h` and the CCSDS chain sit a layer up, and "
        "`native/validation/rx_frame_fer.c` already measures FER on a "
        "receiver through them. So `rx-test.md` goal 4's fourth metric is "
        "reachable, just not from a report scoped to one object. Soft "
        "decisions are the same shape and worth knowing about: "
        "`mpsk_soft_demap` produces per-bit LLRs and `MpskReceiver` "
        "exposes none, so a caller feeding a soft-decision decoder demaps "
        "from `steps()` output themselves rather than asking the receiver "
        "for it. "
        "The burst flavor (`BurstMpskReceiver`, api-taxonomy) does not "
        "exist yet."
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
