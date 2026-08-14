# The Loop Filter

Every tracking loop in doppler — carrier, code, symbol timing, gain — is the
same three pieces: a discriminator that says *how wrong*, a filter that turns
a noisy sequence of those into a stable estimate, and an oscillator or
interpolator that applies it. The discriminators are all different, because
what "wrong" means is different every time. The oscillators are the NCO and
the resampler. **The filter is one object, embedded by value in seven of
them.**

This page is the **why**: what the filter promises, what its one parameter
`bn` actually means, the condition under which that promise holds — which is
the part that is easy to violate and invisible when you do — and what it
deliberately does not do.

The contract lives in `native/inc/loop_filter/loop_filter_core.h` and the
C-level evidence in `native/tests/test_loop_filter_core.c`. This page does
not restate either; it explains the reasoning they assume.

Related: [The Exponential Moving Average](ema.md), [The NCO](nco.md),
[RateSync — timing](ratesync-timing.md), [MPSK Receiver](mpsk.md).

**Status.** This page is written at the *start* of the loop filter's
certification, not the end, and it is deliberate that it says so. Sections
1–5 describe the shipped object and the reasoning behind its present form.
Section 6 states the boundaries the header declares. **Section 8 is the list
of things this object claims and nobody has measured** — it is the input to
the characterization, written down before the sweep exists so that the sweep
cannot be designed to confirm a decision already made.

______________________________________________________________________

## 1. What it is for

One question, asked once per loop update: **given a noisy sequence of "how
wrong am I" measurements, what is my best running estimate of the error's
rate of change, and what correction should I apply right now?**

Two answers are wanted at once, and that is why the filter is second order
rather than a smoother:

- **the accumulated estimate** — a carrier offset, a symbol rate, a code
    rate. This is memory: it must persist when the error goes to zero,
    because zero error is exactly what a *correct* estimate produces. A loop
    whose correction decays to nothing when it stops seeing error cannot
    hold a frequency.
- **the instantaneous nudge** — the immediate response to this update's
    error, which is what gives the loop its speed and its damping.

The integrator is the first, `kp*e` is the second, and `control = integ + kp*e` is the sum. That is the whole object.

Seven objects embed it, at nine `loop_filter_init()` call sites:

| object             | what its integrator holds      | update period          |
| ------------------ | ------------------------------ | ---------------------- |
| `costas`           | carrier frequency offset       | one symbol             |
| `carrier_mpsk`     | carrier frequency offset       | one symbol             |
| `carrier_nda`      | carrier frequency offset       | one discriminator look |
| `dll`              | code rate                      | one code period        |
| `symsync`          | symbol timing rate             | one symbol             |
| `ratesync`         | symbol timing rate             | one symbol             |
| `burst_despreader` | carrier **and** code, one each | per burst update       |

`mpsk_receiver` embeds it twice more through `mpsk_rx_loops_t`, once for
carrier and once by way of `ratesync_loop_t`.

Seven consumers, one recursion — for the same reason the EMA is one
function. A property established for one private copy says nothing about
the others, and a defect fixed in one silently leaves the rest wrong; that
history is written up in [The NCO](nco.md), which had three copies of a
phase-word conversion with one of them fixed.

______________________________________________________________________

## 2. Theory of operation

The recurrence is two lines:

```text
integ   += ki * e
control  = integ + kp * e
```

The gains are not free parameters. They are derived from a **loop noise
bandwidth** `bn`, a damping factor `zeta` and an update period `t`, so that
a caller specifies the loop's behaviour in units it can reason about rather
than by tuning two numbers with no physical meaning.

### 2.1 Where the gains come from

For the continuous-time second-order loop, the noise bandwidth and the
natural frequency are related by the standard result

```text
Bn = wn * (zeta + 1 / (4*zeta)) / 2
```

`loop_filter_init()` **inverts** it — that single line is the entire reason
the parameter is called a noise bandwidth:

```text
wn = 8 * zeta * bn / (4*zeta^2 + 1)
```

The discrete gains then follow the canonical bilinear-mapped form, with
`th = wn*t` and `den = 4 + 4*zeta*th + th^2`:

```text
kp = 8 * zeta * th / den
ki = 4 * th^2      / den
```

### 2.2 It is the textbook form, written differently

That last pair does not look like the form in the standard reference
(Rice, *Digital Communications: A Discrete-Time Approach*, App. C), which
is usually quoted as

```text
theta = 4 * zeta * Bn * T / (4*zeta^2 + 1)
kp    = 4 * zeta * theta        / (1 + 2*zeta*theta + theta^2)
ki    = 4 * theta^2             / (1 + 2*zeta*theta + theta^2)
```

The two **are the same expression**. doppler's `th` is exactly `2*theta`,
and substituting it through turns one into the other identically — not
approximately, and not only for small bandwidths. Confirmed to machine
precision across `bn` from 0.001 to 0.2.

This matters for a practical reason: it means the implementation can be
checked against an *independently written* expression rather than against a
transcription of itself. A test that re-types
`8*zeta*th/(4 + 4*zeta*th + th*th)` beside the implementation's own
`8*zeta*th/(4 + 4*zeta*th + th*th)` proves only that the file was copied
correctly. Asserting the Rice parameterisation instead is a real check,
because a sign or factor error in one form does not reproduce in the other.

______________________________________________________________________

## 3. The promise holds only in the loop's own units

`bn` is the noise bandwidth **of the closed loop**, and the derivation above
assumes the rest of the loop has unit gain: that the discriminator returns
one unit of error per unit of the quantity being tracked, and the oscillator
applies one unit of correction per unit of control. In the textbook that is
the condition `Kd * K0 = 1`.

Nothing in `loop_filter_core.h` can enforce this — the filter never sees the
discriminator or the oscillator. **It is the caller's obligation, and it is
the single most consequential thing about this object.** A discriminator
whose slope is 4 delivers a loop four times wider than the `bn` its
constructor was handed, and nothing anywhere reports the discrepancy: the
loop still tracks, just with the wrong bandwidth, the wrong settling time
and the wrong noise.

This is why doppler's discriminators normalise by their own contribution,
and it is worth seeing that the rule was arrived at independently three
times before it was recognised as one rule:

- `carrier_nda_disc()` divides out its own `|z|^M`, so its slope does not
    depend on the amplitude reaching it;
- `ratesync` scales by `symsync_ted_slope()` — the TED's *only*
    normalisation is its own slope, which is [MPSK Receiver](mpsk.md) §6.1;
- the AGC exists in front of both so the amplitude a discriminator sees is
    one.

Read together, those are not three tuning decisions. They are three
instances of making `Kd * K0 = 1` true so that `bn` means what it says.

The corollary is the failure mode to look for first when a loop is wider or
narrower than it was configured to be: suspect the discriminator's slope
before suspecting these six lines of arithmetic.

______________________________________________________________________

## 4. The embedding contract

The state struct is **public**, and that is a design decision rather than an
oversight. A tracker holds one by value — often two — and a heap allocation
per loop inside an object that already owns its own allocation buys nothing
and costs an indirection in the hot path. `loop_filter_create()` exists for
the Python binding, and is `calloc` plus `loop_filter_init()`.

The contract that follows from embedding is small and sharp:

**`loop_filter_init()` does not touch `integ`.** That is what makes it double
as a retune (§5) — but it also means an embedder is responsible for the
integrator's initial value, and a `loop_filter_state_t` on the stack starts
with whatever was there.

Every embedder in the tree honours this, and — audited rather than assumed —
they do it in three different ways:

| how                                             | who                                                                |
| ----------------------------------------------- | ------------------------------------------------------------------ |
| `loop_filter_reset()` after `init`              | `dll`, `ratesync`, `mpsk_receiver`, `burst_despreader` (code loop) |
| assign a **seeded** value outright              | `costas`, `carrier_mpsk`, `burst_despreader` (carrier loop)        |
| `memset` the whole enclosing struct before init | `symsync`                                                          |

The second row is the interesting one, and the reason `init` must not zero:
`costas` and `carrier_mpsk` seed the integrator to
`init_norm_freq * 2*pi * tsamps`, because the integrator *is* the frequency
estimate (§1) and a caller who already knows roughly where the carrier is
should not make the loop rediscover it. Zeroing on init would throw that
away.

`dll_core.c` is the one that documents the hazard in place, having evidently
met it — its `loop_filter_reset()` carries the comment that an embedded state
"would otherwise start with a garbage code rate".

So the contract is real, currently honoured everywhere, and **unpinned by any
test** — which makes it exactly the kind of thing that holds until the eighth
embedder arrives. The object could remove the hazard by zeroing in `init` and
giving the retune its own entry point, but that would break both the seeding
pattern above and the retune-preserves-lock property `ratesync` depends on.
The split is deliberate; the burden is real; the answer is to pin it.

______________________________________________________________________

## 5. Two ways to change a running loop, and they are not the same

- **`loop_filter_configure(bn, zeta, t)`** recomputes the gains and leaves
    the integrator alone. This is the acquisition-to-tracking transition: run
    wide to pull in, then narrow to reduce noise, **without losing the
    frequency estimate you just spent the acquisition earning**. `ratesync`
    retunes exactly this way.
- **`loop_filter_reset()`** zeroes the integrator and leaves the gains alone.
    This is dropping lock: the estimate is now believed to be wrong, the
    bandwidth is still right, start again.

They are orthogonal on purpose, and every consumer uses both. Calling the
wrong one is a real bug with a quiet signature — a `reset` where a
`configure` was meant throws away a good frequency estimate and looks like a
loop that will not hold lock.

______________________________________________________________________

## 6. The boundaries the header declares

`loop_filter_init()`'s documentation constrains two of its three parameters:
`bn >= 0` and `t > 0`. `zeta` is described as "typically 0.707" without a
stated range.

**They are enforced at `loop_filter_create()` and deliberately nowhere
else** ([gh-740](https://github.com/doppler-dsp/doppler/issues/740)). That
split is the design, so it is worth stating why rather than leaving it to
look like an omission:

- **`create()` is the untrusted boundary.** `LoopFilter(bn, zeta, t)` hands
    a Python caller's arbitrary doubles straight to it. Before the guard,
    `t = 0` produced `kp = ki = 0` — a dead loop indistinguishable from the
    legitimate frozen `bn = 0` below — and `t = inf`, or a NaN in any
    argument, produced NaN gains, which poison every subsequent update
    permanently. Both were one line away in Python. It now returns NULL for
    `bn < 0`, `zeta <= 0`, `t <= 0` or any non-finite argument, and the
    binding raises `ValueError` with the component's own message.
- **`init()` is an internal guarantee.** All seven embedders pass a
    validated `bn` and a compile-time-constant `zeta`; the only
    runtime-computed `t` in the tree is `mpsk_receiver`'s `1.0/upd`, safe
    because `m_out >= 2` is checked in its own constructor. Guarding that is
    the error handling for impossible scenarios this project does not write.

Validating in `create()` also makes the arithmetic **total**. With
`bn >= 0` and `zeta > 0` the intermediate `th` is non-negative, so
`den = 4 + 4*zeta*th + th^2 >= 4` — which closes the one genuinely
pathological corner: for `zeta >= 1` a sufficiently negative `bn` drives
`den` exactly through zero and both gains to infinity.

`bn = 0` remains **inside** the domain and is accepted: it means a frozen
loop, and that is how a loop is held open.

`t` is an update *period in samples*, not a rate, and `bn` is normalised to
**cycles per sample of the update clock** — so a loop updating once per
symbol at 8 samples/symbol and a loop updating every sample do not mean the
same thing by `bn = 0.01`. This is the units seam that
[MPSK Receiver](mpsk.md) §7 exists to pin down, and where `mpsk_receiver`
passes `1.0/upd` rather than `1.0`.

______________________________________________________________________

## 7. State

The filter is a pointer-free POD, so its serialized state is a whole-struct
snapshot (`DP_DEFINE_POD_STATE`) rather than a hand-packed field list. The
practical consequence is that **the blob carries the configuration as well as
the memory** — `bn`, `zeta`, `t`, `kp` and `ki` travel with `integ`.

For the documented use — restore into an identically-built instance — that is
exactly right and costs nothing. What it means when the instance is *not*
identically built is [section 8](#8-what-is-not-yet-measured)'s question.

______________________________________________________________________

## 8. What is not yet measured

Written before the characterization exists, so that the sweep answers these
rather than confirming them:

1. **Is `bn` the loop's noise bandwidth?** §2 derives the gains by inverting
    the analog relation and §2.2 confirms the algebra is the textbook's, but
    *no measurement anywhere in this repository closes the loop and asks what
    bandwidth came out.* Every consumer sizes settling as `5/bn` off this
    promise. The expectation is agreement for small `bn*t` and growing
    deviation as `th` approaches 1 — an expectation the sweep can falsify.
1. ~~**What happens outside the declared domain.**~~ **Answered, and the
    answer became a fix.** `t = 0` produced a dead loop indistinguishable
    from the legitimate frozen `bn = 0`, and `t = inf` or any NaN produced
    NaN gains that poison the object permanently — both reachable from
    Python in one line. `create()` now rejects them (§6, gh-740); `init()`
    is deliberately still unguarded.
1. **`loop_filter_steps()` against `loop_filter_step()`.** The block entry
    point claims per-element equivalence, integrator carry across calls, and
    that `out` may alias `x`. It has no C-level caller and no C-level test.
1. **Does the retune actually preserve lock?** §5's property is asserted as
    "`integ` is unchanged", which is arithmetic. Whether a mid-track retune
    is *transient-free* in the tracking sense is a different claim and an
    unmeasured one.
1. **Restore into a differently-configured instance** — §7.

______________________________________________________________________

## 9. What it deliberately is not

- **It is not a smoother, and an EMA is not a substitute.** The distinction
    is load-bearing and conflating the two costs a transient; that is
    [The Exponential Moving Average](ema.md) §7, written as the diagnosis of
    a measured defect in the AGC's decimated loop.
- **There is no anti-windup.** The integrator accumulates without bound. A
    loop driven by a discriminator that is saturated or meaningless will wind
    up, and recovering takes as long as the winding did. Every consumer that
    needs bounded behaviour must bound its own discriminator or its own
    control.
- **There is no output limit,** and adding one is not free: a bound on the
    control can stop the oscillator, and a stopped oscillator in a
    strobe-driven loop never receives the update that would restart it. That
    is [The NCO](nco.md)'s cycle-slip argument, and it is why
    `nco_steer_scale` exists without a production caller.
- **It does not know its own update rate.** `t` is a number it was handed. A
    loop whose update clock changes — a handover from a fixed-rate
    acquisition tap to a symbol-rate decision-directed one — must re-`init`,
    and forgetting to is [MPSK Receiver](mpsk.md) §10's open item.
