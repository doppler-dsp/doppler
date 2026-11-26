---
description: "the feedforward symbol-timing estimate: timing, rate and a quality figure from a fixed number of symbols, with no loop; what it assumes, what bounds it, and what its characterization has to measure under noise, clock offset and Doppler"
---

# The feedforward symbol-timing estimator

**Scope:** why this object exists, what it computes, what it is predicted
to do, and what is not known yet. It is the acquisition half of symbol
timing: the tracking half is a loop
([RateSync's timing](ratesync-timing.md)). Nothing here is built; this page
is the object's first phase, and its numbers are predictions until its own
report measures them.

______________________________________________________________________

## 1. The problem: a loop's pull-in is not bounded

A timing loop that starts at an arbitrary offset has to walk the error
out, and how long that takes depends on where it started. Half a symbol
off is the detector's unstable zero, where the restoring force is nothing
and the loop waits for noise to move it. Modeled on RateSync's two
detectors at Es/N0 4 dB and a loop bandwidth of 0.005 of the symbol rate,
the median pull-in from half a symbol is 148 to 282 symbols for Gardner
and 460 to 945 for the derivative detector, and on one grid the second
did not arrive in 4000
([the loop model](ratesync-timing.md#closed-and-on-inputs-with-no-whole-number-of-samples-per-symbol)).
A receiver that declares its symbols valid on a fixed schedule cannot
build on that.

A feedforward estimate takes a fixed number of symbols whatever the
starting offset, has no unstable point, and hands the loop a timing it
already tracks. Acquisition time is then a design constant.

It runs first. The carrier has not been recovered when timing is
acquired, so the estimate may not depend on the carrier's phase and has
to hold with a frequency offset present.

## 2. Who uses it

| consumer                                                                | what it hands the estimator                                   | what it does with the answer                           |
| ----------------------------------------------------------------------- | ------------------------------------------------------------- | ------------------------------------------------------ |
| `RateSync`                                                              | its matched bank's output at `K` timing hypotheses per symbol | seeds its loop's phase and rate state, then tracks     |
| a caller with a matched-filtered stream at 3 or more samples per symbol | the stream                                                    | resamples at the estimate, or seeds a loop of its own  |
| a burst demodulator                                                     | one burst's stream                                            | one timing for the whole burst, with no loop behind it |

The DSSS receivers' code loop already forms the same statistic from its
partial sums (the Dll's symbol aid picks the boundary hypothesis with the
most power). That is this estimator's peak-picking special case, and it is
left as it is here.

## 3. What it computes

The input is a stream of complex samples `x[n]` taken after a matched
filter, `N` samples per symbol, `N` any real number of 3 or more. The
power of such a stream is periodic at the symbol rate, with its maximum at
the eye center. Over a block of `L` symbols the estimator forms the
Fourier coefficient of the power at the symbol rate:

```text
c       = sum_n |x[n]|^2 exp(-j 2 pi n / N)
tau_hat = -(1 / 2 pi) arg c              symbols, in [-1/2, 1/2)
quality = |c| / sum_n |x[n]|^2           in [0, 1]
```

`tau_hat` is where the eye center sits relative to the block's first
sample. `n / N` is carried by a phase accumulator, the same 32-bit word
and conversion the NCO uses, so a non-integer `N` needs nothing special.
This is the estimator of Oerder and Meyr (1988).

Two consecutive blocks give the symbol rate's error from the motion of the
estimate:

```text
eps_hat = (tau_hat_2 - tau_hat_1, wrapped) / L       symbols per symbol
```

It is by-value: a block in, one estimate out, and no state that outlives
a block except the accumulator's phase, which is what makes consecutive
estimates comparable.

**The general object.** It is a spectral-line estimator on a power
stream. The application's numbers (the pulse, `N`, `L`, how the stream is
formed) are arguments; nothing about a rectangle, a bank or a receiver is
in its shape. `RateSync` is one configuration: `N = K = 4` hypotheses per
symbol, formed by its bank.

## 4. What it is predicted to do

### 4.1 Noise

The estimate's variance has three parts, from the pulse's own data
pattern, from signal against noise, and from noise against noise, with
`gamma` the Es/N0:

```text
var(tau_hat) = (1 / L) * ( k_ss + k_sn / gamma + k_nn / gamma^2 )
```

The three constants depend on the pulse after the matched filter and on
`N`. They are computed, not fitted: the object's validator evaluates them
from the pulse, and the measurement is held to them.

One point exists already, from the model of RateSync's bank with a
rectangular pulse and `K = 4` at Es/N0 4 dB: an error of 0.030 to 0.036
symbols rms from 64 symbols and 0.020 to 0.027 from 128, which is 1.1 to
1.5 times `sqrt(F / L)` for the tracking detector's figure `F`.

### 4.2 Clock offset

If the symbols' true period is `N (1 + eps)` samples, the power line
rotates by `eps * L` cycles across the block. The coefficient then adds up
with a loss of `sinc(eps * L)` in amplitude, the estimate refers to the
middle of the block, and the rate estimate reads `eps` as long as
`|eps * L| < 1/2`. Past that the line has moved out of the estimator's one
bin.

So `L` is bounded above by the largest clock offset to be acquired, and
below by the noise. Whether one bin is enough for the consumers, or a
small set of rate hypotheses is needed (the two-dimensional search the
polynomial-phase estimator runs for frequency), is an unknown of §6.

### 4.3 Doppler

Doppler reaches this object two ways, and they are separate.

- **The carrier.** `|x|^2` does not see carrier phase, so a frequency
    offset costs only what it costs the matched filter ahead of the
    estimator: the symbol's amplitude falls as `sinc(f * T)`. The
    estimator's own statistic is unchanged.
- **The time base.** A range rate dilates the symbol clock by the same
    fraction as the carrier, so a Doppler of `f_d` on a carrier `f_c` is a
    clock offset of `eps = f_d / f_c`, and a Doppler rate is a clock
    offset that changes across the block. §4.2 covers the first. The
    second is a chirp of the power line, and what it costs is not derived.

### 4.4 The quality figure

With no signal, `c` is a sum of independent terms and `quality` has a
known distribution that narrows as `1 / sqrt(N * L)`. A threshold on it
at a stated false-alarm probability says "there is a timing here", and
its detection probability at the design Es/N0 follows from §4.1's
constants. Both are to be derived and then measured.

## 5. What it composes

- the NCO's phase word and conversions, for `n / N`;
- nothing else. It allocates nothing after create and keeps no history.

It does not filter, resample, or decide symbols. Whoever calls it owns the
matched filter, which is why the pulse appears here only through the three
constants.

## 6. Unknowns

- **`k_ss`, `k_sn`, `k_nn`** for the rectangle through each planned
    cascade and for the root-raised cosine, at `N` of 3 to 8.
- **How small `N` may be.** The power line needs the symbol rate inside
    the band of the squared stream. For a pulse of excess bandwidth
    `alpha` that is `N > 2 (1 + alpha)`; whether 3 holds for the
    band-limited rectangle is to be measured.
- **The bias**, if any, from a pulse that is not symmetric after the
    cascade, and from a non-integer `N` over a short block.
- **One rate bin or several.** The consumers' largest clock offset times
    the `L` their noise needs.
- **The cost of a Doppler rate** across the block.
- **The quality figure's distributions**, with and without signal.
- **Its behavior through a stretch with no transitions**, where the power
    has no line: what `quality` reads, and that the estimate is refused
    and not wrong.

## 7. What the characterization measures

Each is a measurement beside §4's prediction, with a tolerance from the
run's own standard error, on stimulus from the library's generator
(`wfm_synth`), noise from `awgn`, and Doppler from `doppler_channel`.

**A carrier offset is present on every row.** The estimator runs before
carrier recovery, so there is no measurement here with the carrier
removed: each run draws an offset and a starting phase, and the row
without one is the control, not the case.

| axis            | swept over                                                                                                                  | held against                                                                            |
| --------------- | --------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| noise           | Es/N0 0 to 14 dB; `L` 32 to 512; `N` 3 to 8, whole and not; rectangle and root-raised cosine                                | §4.1's variance; zero mean error                                                        |
| clock offset    | `eps * L` from 0 to past 1/2, both signs                                                                                    | the `sinc` loss; the rate estimate's mean and variance; where it stops working          |
| Doppler         | carrier offset to a tenth of the symbol rate; time-base dilation with it, as `doppler_channel` applies both; a Doppler rate | §4.3: no effect from the carrier beyond the matched filter's; the dilation read as rate |
| quality         | noise alone; signal at each Es/N0; a run with no transitions                                                                | the false-alarm and detection probabilities, with intervals                             |
| starting offset | 64 offsets across one symbol                                                                                                | the error does not depend on it                                                         |

The gate is the fast subset of this: one seeded row an axis, each with
its carrier offset, in the
validator's `--check`, each shown red on a named wrong case.

## 8. The phases this page starts

| #   | phase      | what it produces here                                                                          |
| --- | ---------- | ---------------------------------------------------------------------------------------------- |
| 1   | Why        | this page                                                                                      |
| 2   | Declare    | the manifest: a by-value estimator with a block method and a named result record               |
| 3   | Implement  | the header, with its claims, and the C core                                                    |
| 4   | Pin        | the C tests, each with its failing case                                                        |
| 5   | Bind       | the Python class and record, from the manifest                                                 |
| 6   | Instrument | the phase word in the state blob; no telemetry of its own (one estimate a block is its output) |
| 7   | Explore    | the validator of §7, C first                                                                   |
| 8   | Certify    | the report, its limits from §7's rows                                                          |
| 9   | User story | a guide section, a C and a Python example, a gallery decision, both benchmarks                 |
| 10  | Land       | the changelog fragment; an issue for anything left                                             |

See also: [RateSync's timing](ratesync-timing.md),
[the polynomial-phase estimator](ppe.md), which is the same idea for
frequency, [the NCO](nco.md).
