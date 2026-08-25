# Detection Sizing — the four laws behind one prefix

**Scope:** why `detection` has the shape it does. Not how to use it — that is
the [guide](../guide/lock-detection.md), the
[gallery](../gallery/detection-curves.md) and the
[API reference](../api/python-detection.md). This page is the argument
underneath them.

It is a sibling of [Lock Detection](lock-detect.md), which owns the *decision
rule* — declare after `n` looks above a threshold, drop after `m` below a
lower one. This page owns what goes into that rule: **where the threshold
belongs and how long to integrate**, for each of the statistics doppler
actually thresholds.

______________________________________________________________________

## 1. Why the sizing is one module and not one per object

Every object in this tree that declares anything faces the same two questions.
`Acquisition` asks them of a 2-D correlation surface, `LockDet` of a scalar
lock metric, `Dll` of a code-lock statistic, `BurstDespreader` of an in-phase
power ratio, `BerMeter` of a bit-error count. The *statistic* is each object's
own — only the carrier loop knows what an M-th power retains once noise has
rotated the sample — but the arithmetic that turns a statistic's null
distribution into a threshold and a dwell is nobody's in particular.

So it lives here once, and the objects call it. The alternative was measured
rather than imagined: before this module existed, three objects carried three
sizings of the same Gaussian statistic, and they had already drifted.

The consequence worth stating plainly: **a change here moves every detector in
the library at once.** That is the point — a correction reaches all of them —
and it is also why this module carries a certification report, and why its
limits are asserted on every push rather than reasoned about.

______________________________________________________________________

## 2. The statistic decides the law, not the object

There is one rule on this page that matters more than the rest:

> **Pick the family by the H0 distribution of the statistic you are actually
> thresholding — never by which function is nearest to hand.**

Five families ship here, and they are not interchangeable:

| family                    | the statistic                 | H0 law         | threshold from                                   |
| ------------------------- | ----------------------------- | -------------- | ------------------------------------------------ |
| **envelope ratio**        | `peak_mag / noise_est`        | Rayleigh(1)    | `det_threshold` — exact, `sqrt(-2 ln pfa)`       |
| **non-coherent**          | `sqrt(Σ\|z_k\|² / noise)`     | χ²(2M)         | `det_threshold_noncoherent` — Marcum-Q bisection |
| **power**                 | `\|R[0]\|² / mean(\|R[τ]\|²)` | Exponential(1) | `det_threshold_power` — exact, `-ln pfa`         |
| **Gaussian**              | a block-averaged lock metric  | N(0, σ²)       | `det_q_inv` — the normal quantile                |
| **estimated-noise ratio** | `sqrt(n · ΣRe² / ΣIm²)`       | F(n, n)        | `det_threshold_f` — regularized incomplete beta  |

The envelope and power families are the same detector in different units — a
power SNR `s` is an amplitude SNR `sqrt(s)`, and the Q₁ arguments match — so
they agree on Pd at every SNR. That equivalence is asserted, not assumed.

The other three are genuinely different distributions, and reaching for the
wrong one does not fail loudly. It returns a plausible number.

### The 4.9409 trap

At `pfa = 5e-6`, `det_threshold` returns **4.9409** and `det_q_inv` returns
**4.4172**. Both are small numbers near 5. Only one of them is a count of
standard deviations.

`det_threshold` inverts `Pfa = exp(-η²/2)`, the envelope law. A caller who
block-averages a lock metric until the CLT applies has a *Gaussian* statistic,
and wants `det_q_inv(pfa) · sd_H0`. Using the envelope threshold on it prices
a 4.94-sigma gate as though it were 4.42 — a false-alarm rate wrong by about
an order of magnitude, in the safe direction here and the unsafe direction
under a sign flip.

Nothing in the type system distinguishes them: both take a probability and
return a double. The defence is this page, the warning in the header, and the
fact that `det_q_inv` is **signed** — it returns a negative quantile above the
median, which makes `det_dwell_gauss`'s `Q_inv(pfa) - Q_inv(pd)` a *sum* of
two tails rather than a difference. Clamping that to zero, which reads as
defensive, silently halves every dwell it sizes.

______________________________________________________________________

## 3. Coherent depth is free; non-coherent looks are not

This is the asymmetry that surprises callers, and the reason
`det_threshold_noncoherent` takes the look count as an argument at all.

**Coherent integration** over `M` samples raises the non-centrality to
`a = sqrt(2M)·snr` and leaves the threshold alone: `det_threshold` depends on
`pfa` and nothing else. Doubling `M` is a clean 3 dB.

**Non-coherent integration** over `n` looks accumulates magnitude-squared, so
it survives data-modulation sign flips that would destroy a coherent sum — but
the H0 law widens from Rayleigh to χ²(2n), and the threshold **grows with the
look count**. Some of the SNR bought is handed straight back.

`det_n_noncoh` is the honest inverse: it iterates the look count and
recomputes the threshold at each step, rather than sizing against a fixed one.
That loop is not an implementation detail — a closed form that held the
threshold constant would under-size every result it returned.

### The cell count is the other half of the price

The same effect reaches acquisition from a second direction, and the two
compound. A detector searching `C` cells must price its per-cell false-alarm
rate at `pfa/C` to keep the *system* rate at `pfa` — so a search grid that
grows also raises the threshold on every cell in it.

Measured, on `BurstAcquisition` sweeping preamble repetitions
(`src/doppler/examples/dsss_burst_demo.py`): the detection threshold improves
by **2.26 / 2.50 / 3.12 dB** per doubling of coherent depth against the ideal
3.01, and the shortfall is exactly this — more coherent depth is more Doppler
bins, so the Bonferroni threshold rises from 3.98 to 4.30 across those arms.
The gain is real and it is not 3 dB per doubling. A caller budgeting link
margin on the ideal number will be short.

______________________________________________________________________

## 4. Estimating the noise fattens the tail — the 41× that `det_threshold_f`

exists to remove

Every family in §2 except the last prices a statistic normalised by a **known**
noise power. `BurstDespreader` cannot do that: it has one burst, and it
estimates the noise from the same burst it is testing — the quadrature sum
`ΣIm²` against the in-phase sum `ΣRe²`.

A ratio of two estimates is not a ratio to a constant. Its exact H0 law is
`R² = n · F(n, n)`, whose tail is fatter than the chi-square gate's, and the
gap is not a rounding error. Re-derived from the shipped API at `pfa = 1e-3`:

| n   | χ² gate | correct F gate | pfa actually realized | ratio     |
| --- | ------- | -------------- | --------------------- | --------- |
| 4   | 4.6167  | 53.4358        | 8.38e-02              | **83.8×** |
| 8   | 3.2656  | 12.0455        | 5.71e-02              | **57.1×** |
| 16  | 2.4533  | 5.2048         | 4.10e-02              | **41.0×** |
| 32  | 1.9527  | 3.0923         | 3.14e-02              | **31.4×** |
| 64  | 1.6362  | 2.1931         | 2.55e-02              | **25.5×** |

A detector priced at one false alarm in a thousand is delivering one in
twenty-four. The error shrinks as the noise estimate hardens — the last column
falls monotonically — but it never becomes negligible over any dwell a burst
actually affords.

**A degrees-of-freedom note, because getting it wrong reproduces a plausible
wrong answer.** The comparator above is `det_threshold_noncoherent(pfa, n/2)`,
not `(pfa, n)`. `R` is asymptotically `sqrt(χ²(n))` — `n` real degrees of
freedom, one per prompt — while `det_threshold_noncoherent(pfa, M)` prices
`χ²(2M)`, because a non-coherent *look* is a complex magnitude and carries
two. Pricing at `(pfa, n)` yields 4.8× at n = 16: still a warning, off by
almost ten.

`det_threshold_f` solves `I_{1/(1+g)}(n/2, n/2) = pfa` on the regularized
incomplete beta, which is exact for every `n ≥ 1`, odd included — a
requirement, not a nicety, since a burst's prompt count is whatever the burst
contained.

______________________________________________________________________

## 5. What is exact, what is a budget, and which way each errs

Three of these functions do not return the mathematically tight answer, and in
every case the direction is chosen rather than inherited:

- **`det_threshold`, `det_threshold_power`, `det_threshold_f`, `det_q_inv`**
    are exact inversions. No iteration, no tolerance.
- **`det_dwell`, `det_n_noncoh`, `det_dwell_power`** iterate and return the
    *first* value meeting the requirement — minimal by construction, and the
    minimality is asserted rather than assumed (the value one below must fail).
- **`det_verify_count`** sizes on `p_look^n ≤ p_target`, which is deliberately
    the conservative side of the exact consecutive-run law
    `p^n(1-p)/(1-p^n)`. The exact rate is lower, so sizing on `p^n`
    over-provisions the count rather than under-provisioning it. The gap is
    about `p` — negligible where a detector is really sized, 10 % at
    `p = 0.1`. Pick the count here; predict what a caller will *observe* with
    `det_verify_delay`, which is exact.

The split is the point: a budget that errs toward more integration costs
latency, and a budget that errs toward less costs false alarms in a system
that has already been told it is safe.

______________________________________________________________________

## 6. The boundaries are part of the contract

These are design-time helpers — a caller sizes a loop once, at startup, from
numbers a config file supplied. So every one of them **fails closed on
nonsense** rather than propagating a NaN into a threshold that will then be
compared against every sample for the life of the process:

| shape                                                        | returns                                                       |
| ------------------------------------------------------------ | ------------------------------------------------------------- |
| a probability outside (0, 1)                                 | `0.0` (`det_q_inv`, `det_threshold_f`, `det_threshold_gauss`) |
| a requirement that cannot be met within the search bound     | `-1` (`det_dwell`, `det_n_noncoh`, `det_dwell_gauss`)         |
| a per-look probability that can never compound to the target | `INT_MAX` (`det_verify_count`)                                |
| `p_look = 0` with a run required                             | `inf` (`det_verify_delay`)                                    |

`-1` and `INT_MAX` are both "not achievable", and they differ because one is
a count that a caller may clamp and the other is a dwell that a caller must
not. A caller that ignores the sign gets an obviously broken configuration
immediately, which is the intent.

______________________________________________________________________

## 7. What this is, and what it is not

**Stateless and thread-safe, all of it.** Nothing here holds a sample, a
history or a lock. There is no state triplet because there is no state — these
functions size a detector, they are not one. The detector is `LockDet`
(hysteresis over a metric), `Acquisition` (a CFAR surface), `BurstDespreader`
(the F-ratio gate) — each of which owns its statistic and calls here for the
arithmetic.

**It is not a substitute for measuring.** Every Pd on this page is a
semi-analytical model. Where a model is trusted, it is because a Monte-Carlo
run agreed with it — and that agreement is a measurement with a date on it,
not a property of the formula. That is what
`src/doppler/detection/tests/characterization/models/` is for, and
it is the reason this module carries a characterization subject at all.

**One inherited claim did not survive contact with that sweep.** `acq_core.h`
states, twice, that the non-coherent Pd model becomes "non-monotonic and
unreliable past a few hundred looks", and bounds the look count at
`ACQ_N_NONCOH_SAFETY_CEILING = 256` on that basis. Measured here: the model is
monotone in both threshold and Pd out to **1024** looks, and agrees with
Monte-Carlo to **under one standard error at 512** (0.2-0.6 sigma across four
cells). Neither half of the claim reproduces. Whether the ceiling should move
is `acq`'s call and not this page's — it is tracked as
[#997](https://github.com/doppler-dsp/doppler/issues/997) — but a caller
reading that sentence today is being told something the sweep contradicts.

______________________________________________________________________

## 8. Related pages

- [Lock Detection](lock-detect.md) — the decision rule these numbers feed
- [DSSS Acquisition](dsss-acquisition.md) — the largest consumer, and where
    the cell-count price of §3 is paid
- [Timing Lock Detector](timing_lock_detector.md) — a Gaussian-family caller
- [The FEC Receive Half](fec-receive.md) — node sync's two error probabilities
