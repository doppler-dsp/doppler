# RateSync Timing Recovery

**Status:** characterised. One decision taken and **not yet implemented** —
DTTL becomes the default TED for BPSK/QPSK (§4).
**Scope:** what actually sets the quality of the symbols coming out of the
matched-filter-and-timing-recovery pair — `RateConverter`'s terminal polyphase
stage steered by `ratesync_loop_t` — measured on its own, with no carrier loop
anywhere in the path. This is the pair `track.RateSync` exposes directly and
that `track.MpskReceiver` embeds.

Related: [MPSK Receiver](mpsk.md) (the object that embeds this pair; §1
measures how much its carrier loop costs, which is ~0.1 dB), [Timing Lock
Detector](timing_lock_detector.md) (the lock statistic, a separate concern from
the recovery quality measured here), [Continuously Variable
Resampler](RESAMPLER.md) (the bank and accumulator underneath).

______________________________________________________________________

## 1. Why this is measured standalone

A receiver's EVM is the sum of three mechanisms — matched-filter mismatch,
timing jitter, and carrier-loop jitter — and no amount of care with one number
separates them. Gardner's TED is carrier-blind (`|·|²`), so the timing pair
runs perfectly well with no carrier recovery at all, which makes the
separation trivial to arrange and worth arranging first:

Measured as a **median over 500-symbol blocks** (§7 explains why the block
size is load-bearing), on a clean rectangular stream at Es/N0 = 60 dB:

| path                                                           | median block EVM |
| -------------------------------------------------------------- | ---------------- |
| coherent bound                                                 | −60.00 dB        |
| matched filter + timing, standalone                            | −60.03 dB        |
| the same input through `MpskReceiver`                          | −59.95 dB        |
| `MpskReceiver` with the carrier loop frozen (`bn_carrier = 0`) | −60.03 dB        |

**Every path is at the bound.** On a clean rectangular stream the matched
filter, the timing loop and the carrier loop together cost about 0.1 dB, and
there is nothing to attribute to any of them.

The symbol path explains the frozen-loop row exactly. `mpsk_rx_take_output`
emits the strobe straight out of `ratesync_loop_take_output` — the arm-filtered
sample, not anything taken from the de-rotator — times a fixed `sym_rot`. With
`bn_carrier = 0` and a zero seed the LO is a unit multiply, so the receiver
reduces to precisely the standalone pair; the two symbol streams agree to
2.5e−07, which is float32 rounding.

(The floor is real, not a clamp: `ber_evm_db` returns −280 dB on an
algebraically perfect stream and tracks injected noise to within 0.1 dB.)

!!! danger "This section previously reported 12.2 dB, and a §1.1 reported a 14 dB drift. Both were measurement artifacts."

    `ber_evm_db` fits **one** constellation rotation across whatever window it
    is given. The carrier loop has a slow residual phase walk — roughly 35–45°
    over 30 000 symbols, a residual of ~4e−6 cycles/symbol — which is invisible
    within a block and inflates any long-window aggregate. An 8000-symbol
    window read −47.80 dB on a stream whose per-block median is −59.95.

    The retracted §1.1 was the same mechanism seen from the other side:
    standalone windows late in a rate-error record read −45.96 dB against a
    per-block median of −60.03, which looked like a timing loop degrading as a
    constant offset accumulated. Nothing was degrading. The rotation fit was.

    Both numbers were quoted here as headline results, and both were wrong for
    the same reason: a long EVM window cannot separate a slowly rotating
    constellation from a noisy one. Everything in §§2–6 was re-measured
    per-block afterwards; what survived is marked as such.

Unless stated otherwise every number below is QPSK, `sps = 8`, `m = 8`,
`num_phases = 32`, `pulse = "rrc"` with `beta = 0.35, span = 8` on both sides,
Es/N0 = 60 dB so that quantisation and ISI rather than noise set the result,
one seed per cell, and a measurement window computed from the loop bandwidth
under test (§7).

______________________________________________________________________

## 2. What sets the RRC floor: TED gain scales with excess bandwidth

Where the I&D path reaches the bound, the RRC path floors around −43.5 dB at
the default settings. That floor is **timing jitter**, and its magnitude is
set by the transmit pulse's excess bandwidth:

| `beta` | EVM       | timing-error std |
| ------ | --------- | ---------------- |
| 0.15   | −36.09 dB | 0.185            |
| 0.35   | −47.18 dB | 0.138            |
| 1.00   | −58.13 dB | **0.0027**       |

At `beta = 1` the TED's output noise collapses by ~50× and the recovered
symbols reach the bound. This is the textbook property rather than a defect:
a Gardner detector extracts timing from the transition sample, and the
information carried there vanishes with the excess bandwidth.

**Three explanations eliminated, each by its own control:**

- **Not truncation.** Sweeping the RRC span saturates by 6 and does not
    improve through 24 (−43.22 dB at span 6, −43.90 at 24), whether the span is
    swept on the receive side, the transmit side, or both together.
- **Not the sample-clock offset.** An exact `sps` and a 1000 ppm error agree at
    every beta (−42.64 vs −43.60 dB at `beta = 0.35`).
- **Not a bias.** Over a settled tail the timing error's mean is ~−1.2e−3 at
    every setting measured — the loop is unbiased, and the floor is the
    variance, not an offset. An early reading of this as a large static bias
    came from sampling `timing_error` once instead of over a window (§7).

**Re-measured per-block and unchanged** (−32.61 / −43.47 / −58.00 dB at
`bn = 0.01`): this is per-symbol jitter, not slow drift, so the §1 artifact
does not touch it.

Note that the timing-error std barely moves with loop bandwidth (0.1425 at
`bn = 0.02` against 0.1372 at `bn = 0.002`) while EVM improves 9 dB across the
same range. That is consistent: the std being measured is the TED's own
data-dependent output noise, and what the loop bandwidth governs is how much
of it reaches the sampling instant.

______________________________________________________________________

## 3. Loop bandwidth — the default sits short of the knee

| `bn` | 0.020     | 0.010     | 0.005     | 0.002     |
| ---- | --------- | --------- | --------- | --------- |
| EVM  | −38.14 dB | −43.50 dB | −47.18 dB | −47.70 dB |

**Re-measured per-block and unchanged** (−38.17 / −43.47 / −47.20 dB, within
0.03 dB of the row above).

Monotone and saturating around −47.7 dB. At the default `beta = 0.35`,
narrowing `bn_timing` from 0.01 to 0.005 buys **3.7 dB** and costs only
settling time — 5/Bn symbols per the standard rule, so 1000 symbols rather
than 500. Past 0.005 there is nothing left to buy.

______________________________________________________________________

## 4. TED choice — DTTL is better wherever it is valid

`RateSync` already exposes `ted`; `MpskReceiver` hardcodes Gardner.

| `beta` | Gardner   | DTTL      | DTTL advantage |
| ------ | --------- | --------- | -------------- |
| 0.15   | −32.47 dB | −41.05 dB | **8.6 dB**     |
| 0.35   | −43.60 dB | −47.62 dB | **4.0 dB**     |
| 1.00   | −58.06 dB | −58.19 dB | 0.1 dB         |

**Re-measured per-block and unchanged** (Gardner/DTTL −32.61/−41.08 at
`beta = 0.15` and −43.47/−47.62 at 0.35 — the advantage holds at 8.5 and
4.2 dB). The decision below rests on the re-measured numbers.

The advantage is largest exactly where Gardner is weakest and vanishes at full
excess bandwidth, which is the same excess-bandwidth story from the other
side: a decision-directed detector still has timing information where a
transition-sample detector has run out.

!!! note "Decision: DTTL is the default TED for BPSK/QPSK"

    Taken on the measurements above. DTTL's hard-decision device is only valid
    for constellations with independent, rectangular I/Q decision boundaries —
    BPSK and QPSK — so **8PSK keeps Gardner**. That makes the TED a per-M
    default rather than a fixed one, which is the part
    [MPSK Receiver](mpsk.md) §5 previously deferred as "a follow-up, not a
    drop-in default". It is a follow-up; this is the number that justifies it.

    Not yet implemented. `MpskReceiver` passes `RATESYNC_TED_GARDNER` as a
    literal so the TED specialises branch-free, so the change is a per-M
    selection at construction, not a runtime switch.

______________________________________________________________________

## 5. `num_phases` — inert on I&D, and saturating early on RRC

The bank arm is the fractional delay (`arm = ph >> (32 - log2_phases)`,
nearest-arm truncation, low bits discarded), so `num_phases` reads like a
straightforward resolution knob. It is not.

**On I&D it is inert across its whole range.** Per-block, standalone, on a
clean rectangular stream:

| `P` | 2         | 32        | 128       | 1024      |
| --- | --------- | --------- | --------- | --------- |
| EVM | −60.04 dB | −60.04 dB | −59.98 dB | −59.94 dB |

Every setting is at the bound. Stronger than that, the recovered symbols are
**bit-identical** from P = 2 to P = 64: the I&D pulse response is a rectangle —
a 0/1 function — so shifting an arm changes a tap only when it moves a sample
across the rectangle's edge, and adjacent arms are literal duplicates. Above
128 the streams do differ, but by an amount that never reaches the bound.

!!! warning "An earlier revision claimed P = 1024 was 14.5 dB worse than P = 32 here"

    It was measured on 8000-symbol windows and was entirely the rotation-fit
    artifact of §1. Per-block the whole range is flat. `num_phases` is not
    harmful on I&D; it is simply inert, and the argument against 1024 is cost
    (§8), not quality.

**On RRC every arm differs and the resolution is real.** Five seeds at
`m_out = 8`, per-block, quoted against the saturated value (P = 1024,
−43.69 dB):

| `P`                 | 16    | 32    | 64        | 256   |
| ------------------- | ----- | ----- | --------- | ----- |
| vs saturation       | +0.95 | +0.22 | **+0.05** | −0.02 |
| seed-to-seed spread | 0.11  | 0.07  | 0.12      | 0.15  |

**The spread is the resolution, and it is ~0.15 dB.** So P = 64 is
*indistinguishable from saturated*, P = 32 costs a marginal ~0.2 dB that sits
barely above the noise, and only P = 16 and below are clearly short. An earlier
revision called P = 64 "0.06 dB short of the knee", which was a single-seed
number quoted an order of magnitude below what the measurement can resolve.

A single-seed sweep at `m_out = 4` put its knee at P = 64 as well, which would
make the requirement scale with `m_out` rather than being a constant `P · m_out`
— but that is two points at 0.15 dB resolution, so it is a hint and not a law.
**Quote P = 64 at `m_out = 8` as the measured saturation point and leave the
scaling open.**

______________________________________________________________________

## 6. The I&D bank is an ideal rectangle; real rectangles are not

`_pulse_h` builds the I&D bank from a mathematically exact rectangle. A real
rectangular stream has been through a transmit chain and an anti-alias filter
and arrives bandlimited, so the bank is matched to a pulse that cannot
physically exist. Measured standalone, lowpassing the stimulus (Nyquist-
normalised `fpass/fstop`, no resampling anywhere):

| bandlimit    | none      | 0.8/0.9   | 0.5/0.6   | 0.3/0.4   |
| ------------ | --------- | --------- | --------- | --------- |
| EVM          | −60.00 dB | −39.91 dB | −30.62 dB | −24.24 dB |
| timing error | −6.9e−4   | −6.5e−3   | −1.2e−2   | −4.1e−2   |

**Re-measured per-block and unchanged** (−60.04 / −30.63 / −24.24 dB).

The loss is entirely in this pair — the same sweep through the full receiver
reads −39.87 / −30.50 / −24.18 dB — and it costs *timing accuracy* as well as
amplitude, so it is a matched-filter mismatch rather than a scaling error.
`num_phases` cannot recover any of it, because the arms are duplicates (§5).

**In perspective, before anyone optimises it:** this is a high-SNR effect.
Through the receiver at Es/N0 = 20 dB the whole 0.3/0.4 bandlimit costs
1.2 dB, and at 12 dB it costs 0.3 dB — noise dominates it everywhere a real
link operates. It matters for a high-SNR instrument, not for a demodulator
near its anchor.

**Open:** whether `pulse = "iandd"` should match the *received* pulse rather
than the ideal one — an excess-bandwidth parameter, or a third pulse model.
That would also un-duplicate the arms and make `num_phases` meaningful for
I&D, where today it is inert.

______________________________________________________________________

## 7. Measuring this without fooling yourself

Five traps, all of which produced a confident wrong answer during this
characterisation before being caught. The first is the one that invalidated
published results twice.

- **A long EVM window cannot separate slow rotation from noise.** `ber_evm_db`
    fits ONE constellation rotation across the window it is given, so any slowly
    varying phase — a carrier loop's residual walk, a timing loop's accumulating
    offset — is charged to EVM as though it were per-symbol error. This produced
    both a fictitious 12.2 dB carrier-loop cost and a fictitious 14 dB timing
    drift (§1). **Report a median over short blocks** (500 symbols here) and use
    the long window only when the quantity under test is genuinely stationary.
    Where the two disagree, the difference is drift and should be measured and
    named as drift, not folded into an EVM number.
- **Do not impose the sample-clock offset by resampling the stimulus.**
    Running a rectangular stream through a `Resampler` to add a rate error
    reads a 22 dB penalty that is entirely the resampler shaping a full-band
    pulse. Impose the offset by constructing the receiver with an `sps` that
    differs from the stimulus.
- **Choose the `sps` offset per question, and always state the window.** The
    two options are not interchangeable and neither is safe by default. An
    exact `sps` parks the timing loop on one bank arm, turning arm quantisation
    into a fixed bias rather than the jitter it is, and hides every effect in
    §5 — but it is stationary, so it is the only basis for an absolute floor or
    an attribution. A rate error exercises the arms, but the standalone path
    then drifts by 14 dB over 30 000 symbols (§1.1), so a single window under a
    rate error says nothing about any other window. Use a rate error for
    arm-sensitivity questions, an exact `sps` for floors, and report the window
    either way.
- **`timing_error` is one instantaneous TED sample, not a statistic.** Read it
    over a settled tail. A single sample read −0.276 where the mean is
    −1.2e−3, which reads as a large static bias that does not exist.
- **Recompute the measurement window for the loop bandwidth under test.** A
    window fixed at the default `bn`'s settling time measures inside the
    transient as soon as `bn` narrows — which made `bn = 0.002` look 14 dB
    worse than `bn = 0.005` instead of marginally better.

The standalone harness is otherwise small:

```text
RateSync(sps=<stimulus sps * (1 + offset)>, pulse=..., beta=..., span=...,
         m=<outputs per symbol>, num_phases=..., bn=..., ted=...)
    .steps(x)  ->  one output per symbol (already the strobe stream)
```

`steps()` emits one sample per symbol, not `m` — it is the strobe, not the
oversampled stream, so it is scored directly with no phase selection.

______________________________________________________________________

## 8. Decisions and open items

- **DTTL default for BPSK/QPSK, Gardner for 8PSK** — decided (§4), not
    implemented.
- **`num_phases` = 1024 buys nothing measurable** (§5) — inert on I&D, and
    within 0.06 dB of saturation at P = 64 on RRC. The argument against it is
    memory and construction cost, not quality; an earlier revision claimed a
    quality penalty and was wrong. Behaviour change; not taken.
- **`bn_timing` default sits 3.7 dB short of its knee** at the default beta
    (§3). Costs settling time to close. Not taken.
- **The I&D pulse model versus a real bandlimited rectangle** (§6) — open, and
    the one item here that is a design question rather than a default.
- **The carrier loop's residual phase walk** — ~35–45° over 30 000 symbols on a
    zero-offset input (§1). It costs ~0.1 dB per block and is invisible to
    detection, so it is a curiosity rather than a defect, but it is what made
    the long-window measurements lie and it has not been explained.
- **Every number here is one seed at one operating point**, re-measured
    per-block after the §1 retraction. The eliminations in §2 and the TED
    comparison in §4 are robust — each moved a control across a wide range and
    survived the re-measurement unchanged. The absolute floors are not
    multi-seed and should not be quoted as specifications.
