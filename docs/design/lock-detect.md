# Lock Detection — the reasoning

**Scope:** why lock detection in doppler has the shape it does. Not how to
use it — that is the [guide](../guide/lock-detection.md), the
[gallery page](../gallery/lockdet.md) and the
[API reference](../api/python-detection.md). This page is the argument
underneath them, and it exists because the argument had been written down
inside one object's design note while the code it justifies became shared.

______________________________________________________________________

## 1. Why a lock decision is two things

A tracking loop that computes a lock statistic still has to decide when the
statistic is high enough, for long enough. Those are different problems with
different owners.

The **statistic** is the object's. Only `SymbolSync` knows what an eye-opening
ratio reads on a pulse of a given rolloff; only the carrier loop knows what an
M-th power retains once noise has rotated the sample. That knowledge does not
generalise, and trying to make it generalise is how you get a lock detector
per object.

The **decision rule** is nobody's in particular. "Declare after `n` looks
above a threshold, drop after `m` below a lower one" is the same rule
whatever the statistic means, which is why eleven consumers share one
implementation of it.

**A consequence that is easy to forget once the lamp is on the dashboard:** a
decision of this shape reports on the STATISTIC and on nothing else. It has no
input carrying loop convergence, settling time or frequency error, so the
instant it declares is not evidence about any of them. The carrier case makes
that concrete — §4's H1 mean is a function of Es/N0 alone
(`E[cos(Mθ)] = exp(−M²/(4ρ))`), so a caller reading "locked" as "the estimate
has converged" is reading a quantity the detector never saw. Convergence is
asked for directly: an acquisition instant plus a settling budget.

**The interface between them is three numbers**: the statistic's H0 law, its
H1 mean at the operating point, and whether successive looks are independent.
A new loop does not write a lock detector — it characterises those three and
calls `lockdet_init`. Everything else follows arithmetically.

The reason to insist on that split is that the two halves fail differently. A
wrong statistic produces a detector that is honestly measuring the wrong
thing. A wrong decision rule produces one that is dishonestly measuring the
right thing — and only the first is visible in a plot.

## 2. Why the sizing is derived rather than tuned

A threshold picked by eye is a number nobody can defend under a changed
operating point, and the failure is silent: it will look fine at the SNR it
was tuned at. Stating the operating point instead — *the Es/N0 I must still
work at, the Pd I need, the false-alarm rate I can afford* — turns every
downstream number into a consequence, and makes the assumptions arguable.

That is why `lock_thresh` is an output here, not an input, and why the same
spec fixes the averaging length and the verify counts too: they trade against
each other, and letting a caller set them independently is letting them state
a Pd and a Pfa that the averaging cannot deliver.

**One consequence worth stating plainly:** if a spec is unreachable, the
right behaviour is to say so at construction. An 8PSK carrier indicator at a
4 dB floor needs ~321,000 symbols to declare (§4). A detector that quietly
averages toward that looks identical, for a long time, to one that is
working.

### Why a magic constant is not the same as a correct one

`SymbolSync`'s sizing carried a bare `8` where a variance belonged. It was
*safe* — both targets were met — but only because 8 happened to be ~6× the
statistic's real variance, not because it was derived. Replacing it with the
measured variance then missed the pfa target by 13×, which looked like the
measurement being wrong and was actually a convention: the formula was
written in `erfcinv` where the classic derivation uses `Q⁻¹`, and
`Q⁻¹(p) = √2·erfcinv(2p)`, so the √2 cancels in the threshold and its square
absorbs into the dwell. The two forms are the same formula.

The lesson that generalises is not about that formula. It is that a constant
which happens to work is a debugging lead, not a fix, and that when algebra
and measurement disagree the answer is usually a convention neither of them
stated.

## 3. Why independence is the precondition, and why it keeps failing

Both halves of the chain assume successive looks are independent. Averaging
`n` looks shrinks the H0 spread as `1/n` only for uncorrelated looks;
`p^n` is a product only for independent events. **Neither assumption is
checked anywhere, by construction — a detector cannot know what fed it.**

This is the failure that recurs, because a correlated statistic produces a
detector that is wrong in the direction of *declaring too easily*, which
looks like a detector that works well:

- Reusing a verify count of 8 on `CarrierNda`'s fast EMA gave a **13%
    false-lock rate** against noise-only input. Sixty-four was the smallest
    count that reliably eliminated false locks over 300 trials. The physical
    quantity was independent look to look; the EMA of it was not.
- The MPSK carrier statistic's `sd_H0` is derived for independent looks,
    which holds on the one-per-symbol strobe and fails on a tap reading every
    terminal output, where adjacent looks share most of their matched-filter
    support. The same threshold therefore means different false-alarm rates at
    different taps — which is why that receiver computes its indicator on the
    strobe regardless of what steers the loop ([MPSK design](mpsk.md)).

**The rule:** an `N_eff`-look EMA correlates its output over roughly `N_eff`
looks, so a verify count shorter than that is counting one look several
times. Before reusing a count or a dwell across two statistics, establish
that the new one sees independent samples.

### An asymptote presented as an identity

`p^n_up` is the `p → 0` limit of the false-declare rate, not the rate. The
exact per-look rate is `p^n_up·(1−p)/(1−p^n_up)`, whose reciprocal is the
mean looks to a declare. Measured against the shipped detector on noise-only
looks (`native/validation/lockdet_verify.c`):

| p   | n_up | `p^n_up` | exact    | measured | error of the approximation |
| --- | ---- | -------- | -------- | -------- | -------------------------- |
| 0.5 | 4    | 0.062500 | 0.033333 | 0.033917 | **+87.5%**                 |
| 0.2 | 3    | 0.008000 | 0.006452 | 0.006509 | +24.0%                     |
| 0.1 | 2    | 0.010000 | 0.009091 | 0.008897 | +10.0%                     |

At the `p` a detector is actually sized for — 1e-3, 1e-5 — the correction is
~`p`, and it errs toward over-provisioning. The approximation was never
wrong as a budget. It was wrong as a stated identity in the one place a
reader would go to check it, which is the kind of error that survives
indefinitely because everything built on it still works.

## 4. Why the carrier statistic is the well-founded one

Of the five statistics in this codebase, the carrier lock metric is the only
one whose H0 law and H1 mean are both *derived* rather than measured-and-fitted,
and it is worth understanding why, because it is a design choice rather than
luck.

**H0 is exact because the sample is limited before the M-th power.**
`Re((z/|z|)^M)` is bounded in ±1 and its phase is uniform under noise, giving
variance exactly 1/2 — for *every* M. That is what lets one threshold mean one
Pfa at every constellation order, and it is a stronger footing than any
measured variance, which is what every other statistic here rests on. The
limiter costs H1 at low SNR and buys this; the trade was measured and is
strongly favourable above ~6 dB.

**H1 derives too.** At high SNR the phase error has variance `1/(2ρ)`, so
`E[cos(Mθ)] = exp(−M²/(4ρ))` with **no free constant**. Measured against the
shipped discriminator (`native/validation/carrier_nda_lock.c`, 2e6
symbols/point) it holds to 0.039 absolute across the usable range and is exact
to four decimals above 14 dB — a closed form where `SymbolSync`'s equivalent
is an empirical fit.

Looks to declare at `pd = 0.99`, `pfa = 1e-5`:

| M   | Es/N0 4 dB     | 10 dB       | 20 dB      |
| --- | -------------- | ----------- | ---------- |
| 2   | 0.634 → 55     | 0.900 → 27  | 0.990 → 23 |
| 4   | 0.217 → 462    | 0.660 → 50  | 0.961 → 24 |
| 8   | 0.008 → 321103 | 0.204 → 524 | 0.851 → 30 |

BPSK is comfortable at a 4 dB floor; 8PSK is not reachable there at all. The
residual is signed — the closed form runs ~0.03 optimistic for BPSK between 2
and 8 dB, under-sizing that dwell by ~10% — which is the direction worth
knowing before anything depends on it.

## 5. Why one noise source

Every harness that measures a **rate** draws from `awgn`. This is a rule
rather than a preference because the failure mode is invisible: a private
generator with a slightly wrong variance moves the very rate the harness
exists to measure, and no assertion fires.

It is not hypothetical here. A local Box-Muller helper scaled by the
per-quadrature sigma, while the helper was unit *total* power, put every
Es/N0 on the carrier sweep 3 dB out. Nothing failed. What caught it was a
cross-check against the phase-variance derivation, whose constant came out
exactly a factor of two wrong — i.e. the measurement was caught by the theory
it was measuring, which only works if both exist.

`native/tests/` was consolidated onto one generator for this reason, and that
header spells out which "complex Gaussian" normalisation it chose precisely
because two are defensible. `native/validation/` was not part of that pass,
and that is the corner the error came from.

## 6. Why a dead statistic drops the lock

There is a third failure direction, and it is not a level or a time: the
statistic itself can stop being a number. A ratio with a vanished
denominator, an EMA that inherited one non-finite sample, a metric read
from a stalled stage — all of them arrive as NaN, and **every comparison
against NaN is false**. So the decision rule's two tests, `x > up_thresh`
and `x < down_thresh`, both say *no*, and which way that falls out is
decided by how the predicate happens to be spelled rather than by any
argument.

Spelled the obvious way it holds the lock: a locked detector reads the NaN
as "not a miss", resets the drop run on every look, and keeps the lamp lit
forever on a metric that has died. That is the worst of the three
directions, because a lock lamp is exactly the thing a caller trusts when
they have stopped looking at the signal.

The rule is therefore stated, not inherited: **an unknown lock is not a
lock.** A non-finite look is a miss on both sides — it never advances a
declare, and while locked it advances the drop run like any other miss.

It is also not implemented in the detector. `util_core.h`'s `saturate()`
already carries this decision as its `nan_to` parameter, and its
documentation already named a lock statistic as the caller that wants the
floor — while having no such caller, so the rationale described someone who
did not exist. Routing the look through it makes the policy an argument
that can be audited at the call site (`-INFINITY`, the floor) rather than a
comparison someone has to read carefully. The AGC uses the same primitive
with the opposite end, because an unknown *level* must drive the gain down.

Mechanics and a runnable demonstration:
[Lock Detection: Verify Counts](../gallery/lockdet.md).

______________________________________________________________________

## 7. What this is built on, and what it is not

|                                  | evidence                                                                    |
| -------------------------------- | --------------------------------------------------------------------------- |
| the decision rule                | `test_lockdet_core.c`, `native/validation/lockdet_verify.c`                 |
| the sizing chain                 | `test_detection_core.c`                                                     |
| `SymbolSync`'s statistic         | `native/validation/symsync_lock.c` + values pinned in `test_symsync_core.c` |
| the carrier statistic            | `native/validation/carrier_nda_lock.c`                                      |
| `CarrierNda`                     | `src/doppler/track/tests/validation/carrier_nda/results.md`                 |
| `MpskReceiver` / `MpskReceiverR` | `src/doppler/track/tests/validation/mpsk_receiver/results.md`               |
| **`Costas`, `carrier_mpsk`**     | **not certified**                                                           |

The asymmetry this section used to record — every number in §4 describing a
statistic whose *object* had never been through validation — is closed on the
NDA side: `CarrierNda` and the MPSK receivers are both certified, alongside
`ratesync`, the timing half. `Costas` and `carrier_mpsk` remain, and are the
reason to keep reading this row rather than assuming it.
