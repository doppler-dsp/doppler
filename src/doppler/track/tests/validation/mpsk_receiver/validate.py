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

Two surfaces deliberately do NOT appear as Python measurements:
``mpsk_rx_derive_m_out`` and ``mpsk_rx_updates_per_symbol`` are
``JM_FORCEINLINE`` with no binding, so claims about them are reported
**C-ONLY** with the C section that covers them. Measuring the receiver and
calling it the rule is exactly the substitution ``docs/dev/validation.md``
warns about.

Run:  make validate
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from doppler.ber import ber_evm_db, ber_settle_syms, ber_theory_ser
from doppler.mpsk import mpsk_demap, mpsk_map
from doppler.snr import snr_m2m4_db
from doppler.tests._validation_common import Report, clamp_evm_db, cli
from doppler.track import ContinuousMpskReceiver, MpskReceiver

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"

#: Amplitude is 0.5, not 1.0, and the reason is the same one
#: ``test_mpsk_receiver_core.c``'s header gives: a cascade that plans a CIC
#: bounds its input to +-1.0 and clips silently past it, costing ~25 dB of
#: EVM that no lock metric reveals. A unit-amplitude constellation plus
#: noise sits right on that edge.
TX_AMP = 0.5

#: Symbols per Es/N0 cell. 20 000 puts a 1e-3 SER's own standard error near
#: 7%, so the tolerances below are about the receiver rather than about how
#: long the sweep ran.
NSYM = 20000

SPS = 8.0
PHI0 = {2: 0.0, 4: np.pi / 4, 8: 0.0}


# ── stimulus ─────────────────────────────────────────────────────────────


def _signal(m, esn0_db, sps=SPS, nsym=NSYM, foff=0.0, seed=0, amp=TX_AMP):
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
    if foff:
        x = x * np.exp(2j * np.pi * foff * np.arange(n))

    es = amp**2 * sps
    var = es / 10 ** (esn0_db / 10.0)
    x = x + (rng.standard_normal(n) + 1j * rng.standard_normal(n)) * np.sqrt(
        var / 2
    )
    return np.ascontiguousarray(x.astype(np.complex64)), lab


def _ser(out, lab, m, lo):
    """Genie SER over a settled window, tolerant of the M-fold rotation.

    The decision is `mpsk_demap` — the library's own hard-decider, and the
    exact inverse of the `mpsk_map` the stimulus used. It is NOT a
    `round(angle)` written here, and that distinction is the whole reason
    this helper is worth a docstring: the byte `mpsk_map` takes is a
    **Gray label**, while `round(angle · m / 2π)` recovers the
    **constellation index**. Those agree only at M = 2, so a hand-rolled
    demapper reads a clean BPSK rate and ~50% on QPSK — which is exactly
    what it did here before this was fixed, and exactly the defect
    `wfm_synth`'s four-copy bits→symbol map produced.

    The rotation search is not a convenience: the NDA loop locks to one of
    `m` phases by construction (F3), so an absolute-phase comparison would
    measure the ambiguity rather than the receiver. It is applied to the
    SYMBOLS before demapping, because a rotation in the constellation is
    not an offset in Gray-label space. The lag search covers the cascade's
    group delay, which is not a round number of symbols.
    """
    hi = out.size - out.size // 8
    if lo + 400 >= hi:
        return 1.0
    seg = np.ascontiguousarray(out[lo:hi].astype(np.complex64))
    best = 1.0
    for rot in range(m):
        turn = np.exp(-2j * np.pi * rot / m)
        got = mpsk_demap(
            np.ascontiguousarray((seg * turn).astype(np.complex64)), m
        )
        for lag in range(-40, 41):
            base = np.arange(lo, hi) + lag
            if base.min() < 0 or base.max() >= lab.size:
                continue
            best = min(best, float(np.mean(got != lab[base])))
    return best


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

    rows, sweep = [], []
    for m in (2, 4, 8):
        for esn0 in (8.0, 12.0, 16.0):
            x, lab = _signal(m, esn0, foff=0.0005, seed=100 + m)
            rx = MpskReceiver(m=m, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
            out = rx.steps(x)
            lo = ber_settle_syms(0.01, 0.01)
            ser = _ser(out, lab, m, lo)
            theory = float(ber_theory_ser(m, esn0))
            rows.append(
                [
                    f"{m}",
                    f"{esn0:.0f}",
                    f"{ser:.3e}",
                    f"{theory:.3e}",
                    f"{rx.lock:.3f}",
                ]
            )
            sweep.append([m, esn0, ser, theory, rx.lock])
    d["ser"] = sweep
    R.table(["M", "Es/N0 dB", "SER measured", "SER theory", "lock"], rows)
    R.md()
    if write:
        _csv(
            DATA / "ser_vs_esn0.csv",
            "m,esn0_db,ser_measured,ser_theory,lock",
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
        x, _lab = _signal(4, esn0, foff=0.0005, seed=7)
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
        x, lab = _signal(4, esn0, foff=0.0005, seed=11)
        rx = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
        out = rx.steps(x)
        lo = ber_settle_syms(0.01, 0.01)
        tail = np.ascontiguousarray(out[lo:])
        blind = float(snr_m2m4_db(tail))
        ser = _ser(out, lab, 4, lo)
        rows.append(
            [
                f"{esn0:.0f}",
                f"{blind:.2f}",
                f"{blind - esn0:+.2f}",
                f"{ser:.3e}",
            ]
        )
        m2m4.append([esn0, blind, blind - esn0, ser])
    d["m2m4"] = m2m4
    R.table(["Es/N0 dB asked", "M2M4 dB", "error dB", "SER"], rows)
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

    rows, irr = [], []
    for sps in (8.0, 17.33389, 31.7):
        x, lab = _signal(4, 16.0, sps=sps, nsym=6000, foff=0.0004, seed=5)
        rx = MpskReceiver(m=4, sps=sps, bn_carrier=0.01, bn_timing=0.01)
        out = rx.steps(x)
        lo = ber_settle_syms(0.01, 0.01)
        ser = _ser(out, lab, 4, lo)
        expect = x.size / sps
        rows.append(
            [
                f"{sps:g}",
                f"{out.size}",
                f"{expect:.0f}",
                f"{100 * abs(out.size - expect) / expect:.2f}",
                f"{ser:.3e}",
            ]
        )
        irr.append([sps, out.size, expect, ser])
    d["irrational"] = irr
    R.table(["sps", "symbols out", "expected", "error %", "SER"], rows)
    R.md()
    if write:
        _csv(
            DATA / "irrational_sps.csv",
            "sps,symbols_out,symbols_expected,ser",
            irr,
        )

    # 2.6 ---------------------------------------------------------------
    R.md("### 2.6 The continuous flavor never hands over (C §1c)")
    R.md()
    R.md(
        "`ContinuousMpskReceiver` pins the gating: no handover, no "
        "warmup, no lock gate, no timing gate. Measured against the "
        "handover-enabled receiver on the SAME record, because "
        "`tracking == 0` alone is equally satisfied by a signal that "
        "never locked."
    )
    R.md()

    x, lab = _signal(2, 16.0, foff=0.0008, seed=21)
    cont = ContinuousMpskReceiver(
        m=2, sps=SPS, bn_carrier=0.02, bn_timing=0.01
    )
    out_c = cont.steps(x)
    hand = MpskReceiver(
        m=2,
        sps=SPS,
        m_out=8,
        bn_carrier=0.02,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.65,
    )
    hand.steps(x)
    lo = ber_settle_syms(0.01, 0.02)
    d["cont"] = {
        "tracking": cont.tracking,
        "lock": cont.lock,
        "ser": _ser(out_c, lab, 2, lo),
        "control_tracking": hand.tracking,
    }
    R.table(
        ["receiver", "tracking", "lock", "SER"],
        [
            [
                "ContinuousMpskReceiver",
                f"{cont.tracking}",
                f"{cont.lock:.3f}",
                f"{d['cont']['ser']:.3e}",
            ],
            ["MpskReceiver (control)", f"{hand.tracking}", "—", "—"],
        ],
    )
    R.md()

    # 2.7 ---------------------------------------------------------------
    R.md("### 2.7 The stable false lock at `Δf = k·F/M` (C §1f)")
    R.md()
    R.md(
        "`F/M` is exactly where an M-th power at update rate `F` aliases "
        "onto zero, so the M-fold ambiguity is a FREQUENCY ambiguity as "
        "well as a phase one. Seeded there, the loop does not move — and "
        "reports a lock statistic a caller would trust."
    )
    R.md()

    alias = 1.0 / (4.0 * SPS)
    x, _ = _signal(4, 20.0, foff=alias, seed=5)
    rxf = MpskReceiver(m=4, sps=SPS, bn_carrier=0.01, bn_timing=0.01)
    rxf.steps(x)
    d["false_lock"] = {
        "true": alias,
        "tracked": rxf.norm_freq,
        "lock": rxf.lock,
    }
    R.table(
        ["true Δf", "tracked", "lock reported"],
        [[f"{alias:.5f}", f"{rxf.norm_freq:.5f}", f"{rxf.lock:.3f}"]],
    )
    R.md()

    R.md("![SER against the coherent bound; EVM and blind M2M4](quality.png)")
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
        "CONFIRMED",
        "**A healthy lock statistic beside a rate that misses the bound**, "
        "in two independent places. At `sps = 31.7` (§2.5) the receiver "
        "locks — 0.85 at 6 000 symbols, 0.90 at 20 000 — and still reads "
        "SER 3.1e-1 and 4.6e-2 against a bound of 6e-5; `sps = 24` on the "
        "same sweep is clean, so it is not the irrational rate itself. "
        "The rate falling with record length is the signature of a slip "
        "during acquisition that a longer settled tail dilutes, rather "
        "than a steady-state loss. That is the same shape as "
        "[#781](https://github.com/doppler-dsp/doppler/issues/781), "
        "reached through a completely different path — a randomised "
        "Monte-Carlo geometry there, a fixed high oversampling here — "
        "which is corroboration rather than a second defect. The certified "
        "envelope therefore stops at `sps <= 24`; §4 asserts the output "
        "count at every rate measured and the error rate only inside it.",
    )

    R.find(
        "F5",
        "CONFIRMED",
        "**8PSK does not reach the bound at low Es/N0** (§2.1): 8.7e-1 "
        "measured against 1.3e-1 at 8 dB, and 5.9e-1 against 6.1e-2 at "
        "12 dB, with `m_out` derived to 8 — the value the header says is "
        "not optional at M = 8. It recovers by 16 dB (7.0e-3 against "
        "3.0e-2, inside the bound). The M-th-power discriminator's "
        "squaring loss grows with M and the decision margin shrinks to "
        "+-pi/8, so a floor at low Es/N0 is expected; what is not "
        "established is where it should sit, and no measurement here "
        "pins that. Tracked by "
        "[#781](https://github.com/doppler-dsp/doppler/issues/781), which "
        "asks the same question about implementation loss the sweep "
        "cannot currently answer. §4 asserts only the cells where a 10x "
        "bound constrains anything.",
    )

    R.find(
        "F3",
        "BY DESIGN",
        "The M-fold phase ambiguity is **permanent** in the continuous "
        "flavor (§2.6): with no decision-directed stage, nothing ever "
        "pins absolute phase. That is why `_ser()` here searches the "
        "rotation, and why `bits()` defaults to the differential demap "
        "on `ContinuousMpskReceiver`. Coherent demapping without a "
        "downstream sync word is a misconfiguration, not a choice.",
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

    # Asserted only where a 10x bound is a real constraint. At M=8 and
    # 8 dB the closed form is already 1.26e-1, so "within 10x" is
    # satisfied by a receiver that is not working at all -- a vacuous
    # limit reads as coverage it does not have. Those cells stay in
    # §2.1's table as characterisation, and F4 judges them.
    for m, esn0, ser, theory, _lock in d["ser"]:
        if theory < 1e-2:
            R.limit(
                ser < max(10 * theory, 3e-3),
                f"M={int(m)} at Es/N0 {esn0:.0f} dB: SER {ser:.2e} is "
                f"within 10x the coherent bound {theory:.2e}",
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
    for sps, got, expect, ser in d["irrational"]:
        R.limit(
            abs(got - expect) / expect < 0.02,
            f"sps={sps:g}: output count is the integral of the rate to 2% "
            f"({int(got)} vs {expect:.0f})",
        )
        if sps <= 24.0:
            R.limit(
                ser < 3e-3,
                f"sps={sps:g}: recovers symbols (SER {ser:.2e}) — an "
                f"irrational rate is no harder than an integer one",
            )

    c = d["cont"]
    R.limit(
        c["tracking"] == 0,
        "ContinuousMpskReceiver never hands over (tracking stays 0)",
    )
    R.limit(
        c["control_tracking"] == 1,
        "the control DOES hand over on the same record — so the line "
        "above is not satisfied by a signal that never locked",
    )
    R.limit(c["lock"] > 0.5, "ContinuousMpskReceiver locks")
    R.limit(
        c["ser"] < 3e-3,
        f"ContinuousMpskReceiver recovers BPSK (SER {c['ser']:.2e})",
    )

    fl = d["false_lock"]
    R.limit(
        fl["lock"] > 0.5
        and abs(fl["tracked"] - fl["true"]) > 0.5 * fl["true"],
        "the false lock at k·F/M reports a HEALTHY statistic with a wrong "
        "frequency — pinned so a change in either half is visible",
    )


# ── plots ────────────────────────────────────────────────────────────────


def plots(d):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))

    for m, mark in ((2, "o"), (4, "s"), (8, "^")):
        pts = [(e, s, t) for mm, e, s, t, _ in d["ser"] if mm == m]
        ax[0].semilogy(
            [p[0] for p in pts],
            [max(p[1], 1e-6) for p in pts],
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


# ── the runner ───────────────────────────────────────────────────────────


def build(write: bool = True) -> Report:
    R = Report(write=write)

    R.md("## 1. The object")
    R.md()
    R.md(
        "`track.MpskReceiver` — a streaming M-PSK receiver that owns no "
        "filter, no NCO and no interpolator of its own: a matched DDC "
        "with two loops closed around its two control ports. "
        "`track.ContinuousMpskReceiver` is its continuous flavor, a view "
        "over the same core that pins the gating."
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

    d = characterise(R, write)
    review(R, d)
    limits(R, d)

    R.executive(
        "MpskReceiver",
        [
            "**Design to the bound, and know where it stops.** BPSK and "
            "QPSK track `ber_theory_ser` within 10x from 8 dB Es/N0, and "
            "EVM sits within 6 dB of `-(Es/N0)` without ever beating it "
            "(§2.1, §2.2) — an EVM below the bound would be a broken "
            "measurement, so that direction is asserted too. **8PSK does "
            "not reach the bound below ~16 dB** (F5), and the limits say "
            "so by asserting only the cells where a 10x bound constrains "
            "anything.",
            "**The certified rate envelope is `sps <= 24`.** The output "
            "count is the integral of the rate at every rate measured, "
            "but at `sps = 31.7` the receiver locks and still misses the "
            "bound (F4) — the same clean-lock/bad-rate shape as gh-781, "
            "reached from a different direction.",
            "**An irrational `sps` costs nothing inside that envelope.** "
            "At 17.33389 the output count is the integral of the rate to "
            "2% and the SER is unchanged from the integer case — the "
            "header's headline claim, measured rather than asserted "
            "(§2.5).",
            "**The continuous flavor's `tracking == 0` is checked against "
            "a control.** The handover-enabled receiver on the same "
            "record DOES flip, so the claim is not satisfied by a signal "
            "that never locked (§2.6).",
            "**A false lock at `Δf = k·F/M` is invisible to every metric "
            "this receiver computes** — stationary constellation, clean "
            "EVM, clean blind M2M4, healthy lock statistic. Defend "
            "against it with an external frequency reference or a sync "
            "word, not with `lock` (§2.7, F2).",
            "**The derivation rule is not measurable from Python.** "
            "`mpsk_rx_derive_m_out` is `JM_FORCEINLINE` with no binding; "
            "§2.4 measures the answer, and the rule is C-ONLY (F1).",
        ],
    )

    if write:
        plots(d)

    R.summary(
        "\n- Raw sweeps: `data/ser_vs_esn0.csv`, `data/evm_vs_esn0.csv`, "
        "`data/m2m4_vs_esn0.csv`, `data/irrational_sps.csv`"
        "\n- **Not covered:** `bits(differential)` resolving the M-fold "
        "ambiguity has no test in either language; the burst framing "
        "(`BurstMpskReceiver`, api-taxonomy) does not exist yet."
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
