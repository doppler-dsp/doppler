---
description: "Track carrier, code and symbol timing: the shared PI loop filter, the Costas and M-PSK carrier loops, the M-PSK and BPSK receivers, the DLL, symbol sync and rate sync."
---

# Tracking Loops

[`doppler.track`][doppler.track] provides `LoopFilter` — a **second-order
proportional-integral loop filter**, the shared engine of every tracking loop
(Costas/PLL, DLL, symbol timing). An error `e` goes in, a control value comes
out (`control = integ + kp·e`), and the integrator advances `integ += ki·e`, so
the integrator holds the running frequency/rate estimate and `kp·e` is the
instantaneous (phase) correction.

Source:
[`src/doppler/track/__init__.py`](https://github.com/doppler-dsp/doppler/blob/v0.86.0/src/doppler/track/__init__.py)

## How it works

The gains are derived from a loop **noise bandwidth** `bn` (normalized,
cycles/sample), a damping factor `zeta` (0.707 = critically damped), and the
update period `t` (samples):

```
wn = 8·zeta·bn / (4·zeta² + 1)
theta = wn·t
kp = 8·zeta·theta / (4 + 4·zeta·theta + theta²)
ki = 4·theta²     / (4 + 4·zeta·theta + theta²)
```

`configure(bn, zeta, t)` recomputes the gains while preserving the integrator
(so a tracker can retune mid-stream without losing lock); `reset()` zeroes the
integrator. The state struct is public C, so trackers embed it **by value** and
drive it with the same kernel — there is no per-update allocation.

### Drive a loop with a constant error

```python
from doppler.track import LoopFilter

lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
lf.step(1.0)                 # integ += ki; returns integ + kp
round(lf.integ, 6)           # == ki (one update of unit error)
```

### Retune without losing the estimate

```python
lf.configure(0.05, 0.707, 1.0)   # wider bandwidth; integ preserved
lf.reset()                       # zero the integrator
```

## StateFilter — a loop filter for a drifting carrier

`LoopFilter` nulls a frequency *step* but not a frequency *ramp*: against a
carrier whose frequency drifts at `r` cycles per update per update, a
second-order loop settles to a steady phase error of `r / ki`, and `ki` falls
as the square of the bandwidth, so the narrower the loop, the further it lags.
`StateFilter` is the third-order sibling for exactly that case. It keeps a
rate estimate in cascade with the frequency estimate, so a frequency ramp is
tracked with zero steady phase error, at the same noise bandwidth, from the
same two numbers: `bn` and `t`. There is no damping argument and no other
coefficient to pick.

```pycon
>>> from doppler.track import StateFilter
>>> sf = StateFilter(bn=0.01, t=1.0)
>>> round(sf.step(0.25), 8)         # rate += kr*e; integ += rate + ki*e
0.00597554
>>> round(sf.integ, 8)              # the frequency estimate
7.016e-05
>>> round(sf.rate, 12)              # its slope, per update per update
4.11883e-07
```

`rate` is the change of `integ` per update, so a Doppler rate of `R` Hz/s at
an update period of `T` seconds is `rate = R * T**2` in cycles per update per
update (set it with `sf.rate = ...` to start a loop on a known ramp; the
[design page](../design/loop-filter.md) derives it). The class and its
members are in the Python reference, `doppler.track.StateFilter`.

Close both loops on a ramp and read the steady error:

```python
--8<-- "src/doppler/examples/state_filter_ramp_demo.py:ramp"
```

The second-order error is the closed form `r / ki`; the third-order one is
zero. Seed `integ` and `rate` when the frequency and its rate are already
known, and use `configure(bn, t)` to widen to acquire and narrow to track
without losing either estimate.

### What the loop needs of the rest

A third-order loop is conditionally stable in the loop gain `K`, the product
of the discriminator's slope and the oscillator's gain. `StateFilter` is
stable from about a quarter of nominal `K` upward, and the sampled loop has
an upper edge that falls with the detector's delay. `bn*t` is limited to
0.12, the largest value for which the loop is stable at every gain from 0.5
to 2 and every detector delay up to two updates; `StateFilter(...)` raises
`ValueError` beyond it. The object cannot see `K` or the delay, so it
answers for the ones you know:

```python
--8<-- "src/doppler/examples/state_filter_ramp_demo.py:exact"
```

`bandwidth(delay, gain)` is the loop's exact delivered noise bandwidth in
cycles per update, `NaN` where the loop is not stable; `stable(delay, gain)`
is the yes or no. Both are exact for an integer `delay`. The design page,
[The Loop Filter](../design/loop-filter.md), derives them and states what
the second-order loop's bandwidth is by the same expression. The C example
is `native/examples/state_filter_demo.c`.

## KalmanStateFilter — a loop sized from the signal

`StateFilter` is sized from a bandwidth. If you know the detector's noise and
how fast the carrier's rate can change, give those two figures instead and
get the bandwidth that is optimal for them: `KalmanStateFilter` is the same
third-order loop with its gains computed each update from a covariance
recursion. It takes the rms detector error when the loop is locked
(`sigma_e`) and the rms change of the rate per update (`sigma_a`), both in the
loop's own units, or `bn` and `t` alone. Nothing in it knows a detector: a
caller with a carrier-to-noise density and a jerk figure makes the two
figures in one line, as an example and not an API:

```python
--8<-- "src/doppler/examples/state_filter_kalman_demo.py:figures"
```

Started at its steady state, the gains are constant: a third-order loop at
the optimal gains for those figures. They are not `StateFilter`'s Butterworth
gains; the two differ by 0.7% to 9% of `kr` between `bn*t` 0.01 and the bound
(the design page, Part III, measures it), and `KalmanStateFilter` has its own
bounds, `bn*t` from 1e-6 to 0.12. An initial frequency uncertainty `sigma_f0`,
between `sigma_f` (the steady state's own deviation, a property) and 5 times
it (less at `bn*t` above 0.04, where the allowed start falls to 1.2 at the
bound), starts the covariance larger, so the gains rise for a while and the
loop settles to a frequency step about 1.7 times as fast as the fixed-gain loop of
the same final bandwidth:

```python
--8<-- "src/doppler/examples/state_filter_kalman_demo.py:start"
```

Both are runnable as they stand: `python -m doppler.examples.state_filter_kalman_demo`
and, after `make build`, `./build/native/examples/state_filter_kalman_demo`.
The C example does the same pull-in and shows the lifecycle a C caller
manages: create from the figures, a start of four times `sigma_f`, the step
response against the fixed-gain loop, a start far past the allowed one refused
with `NULL`, `configure()` with every argument 0 ending the start early (the
gains go to the steady state at once and the estimates carry), and destroy:

```c
--8<-- "native/examples/state_filter_kalman_demo.c"
```

A larger start is refused, because the gains it would produce along the way
leave the region where the exact stability test covers every loop gain from
0.5 to 2 and every detector delay up to two updates. `bandwidth(delay, gain)`
and `stable(delay, gain)` answer for the gains in force now, as if they
stopped changing; they are not a statement about the converging loop. The
state round-trips through `get_state()` and `set_state()`, and the blob
carries the covariance. The C type embeds the fixed-gain struct, so an
embedder holds it by value (`native/inc/doppler/state_filter_kalman/`).

## Costas — carrier-tracking loop

`Costas` is the first loop built on `LoopFilter`: a continuous BPSK
carrier-recovery loop. Per sample it de-rotates the input with the integer-phase
[`source.LO`][doppler.source.LO] NCO (carrier wipe-off); every `tsamps` samples it
dumps the coherent integrate-and-dump accumulator, runs a decision-directed
Costas phase discriminator, filters the error through an embedded `LoopFilter`,
and steers the NCO frequency and phase. It tracks the small **residual** carrier
offset left after FFT acquisition removes the bulk Doppler — an offset larger
than the per-symbol integration bandwidth must be removed upstream, not by the
loop. Because the steering NCO is integer-phase, the carrier phase is bounded and
exactly reproducible (no `double`-accumulator drift).

**FLL assist.** Setting `bn_fll > 0` enables a frequency-lock-loop assist: a
data-wiped cross-product frequency discriminator over consecutive prompts whose
linear range is far wider than the phase discriminator's. It pulls the loop's
frequency integrator onto a large or fast-moving residual the bare PLL cannot
acquire, then the PLL refines phase (an FLL-assisted PLL). `bn_fll = 0` (the
default) is a pure Costas PLL.

See the [carrier loop stress gallery page](../gallery/costas.md) for the bare
PLL stalling on a large residual while the FLL assist pulls it in.

```python
import numpy as np
from doppler.track import Costas
from doppler.wfm import Synth

bpsk = Synth(type="bpsk", sps=16, snr=20).steps(16000)  # BPSK, no offset
n = np.arange(len(bpsk))
rx = (bpsk * np.exp(2j * np.pi * 0.01 * n)).astype(np.complex64)  # 0.01 off

# The bare PLL cannot pull in a residual this large; the FLL assist can.
pll = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16)
pll.steps(rx)
assert not pll.locked

c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16, bn_fll=0.03)
symbols = c.steps(rx)        # one complex prompt symbol per tsamps samples
f_est   = c.norm_freq        # tracked residual carrier (cycles/sample)
assert abs(f_est - 0.01) < 5e-4 and c.locked
```

### Order 3 — a carrier that ramps

A Doppler ramp leaves the default loop a steady phase lag of `x/ki`, which
grows as the loop narrows.

**When to switch.** For a carrier ramping at `r` cycles per sample² (`R / fs²` for `R` Hz/s), the ramp per update is `x = 2π·r·tsamps²` radians per
update², and order 2's steady phase error is `x / ki`, with `ki` the loop
filter's own gain at the loop's `bn` and `zeta`. Switch to order 3 when that
error is more than the phase budget the link allows. It grows as `1/bn²`:

````python
import math
from doppler.track import LoopFilter

tsamps, r = 16, 2e-8                     # r: cycles/sample^2
x = 2 * math.pi * r * tsamps**2          # the ramp per update, rad/update^2
lag = {bn: math.degrees(x / LoopFilter(bn, 0.707, 1.0).ki)
       for bn in (0.05, 0.01)}
assert 0.2 < lag[0.05] < 0.25            # 0.22 degrees at bn 0.05
assert 5.0 < lag[0.01] < 5.5             # 5.3 degrees at bn 0.01
``` `set_order(3, bn)` switches a live loop to a
[`StateFilter`](#statefilter-a-loop-filter-for-a-drifting-carrier), which
also tracks the carrier's rate and holds a ramp with no steady lag. The
frequency estimate carries across the switch. The rate starts at zero, or
is seeded through `norm_rate` in cycles per sample² (`R / fs²` for `R`
Hz/s). The FLL assist and order 3 do not combine: `set_order(3, ...)`
refuses while `bn_fll > 0`. The loop is the same object, and its blob
carries the order. [Costas's design](../design/costas.md#3-the-second-loop-order)
has what a switch carries.

```python
--8<-- "src/doppler/examples/costas_demo.py:order3"
````

## CarrierMpsk — M-PSK carrier-tracking loop

`CarrierMpsk` is the M-ary generalization of `Costas`: the same integer-NCO
wipe-off, coherent integrate-and-dump, embedded `LoopFilter`, and FLL assist,
but with a **decision-directed M-PSK** phase discriminator instead of the BPSK
one. Each symbol it slices the prompt to the nearest constellation point
`ahat` and forms `e = Im(P · conj(ahat)) / |P|` (the sine of the residual phase
error near lock). `m` selects the constellation — `2` (BPSK), `4` (QPSK), or
`8` (8PSK); **at `m = 2` it is byte-for-byte the `Costas` loop** (same prompt
stream, same tracked frequency), which is the loop's validation anchor.

The loop locks to **one of `m` phases** — an M-fold ambiguity on absolute
phase. Resolve it downstream with differential demapping
([`mpsk.mpsk_diff_demap`][doppler.mpsk.mpsk_diff_demap]) or a sync word; this loop only
recovers the carrier and emits the prompts. The FLL assist (`bn_fll > 0`)
matters more as `m` grows: 8PSK's phase discriminator is linear only over
±π/8, so a sizeable residual needs the wide cross-product frequency
discriminator to pull in before the PLL can refine phase.

```python
from doppler.track import CarrierMpsk

# QPSK carrier loop, 16 samples/symbol, FLL-assisted; all params keyword-capable
c = CarrierMpsk(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16, bn_fll=0.01, m=4)
symbols = c.steps(rx)        # one complex prompt symbol per tsamps samples
f_est   = c.norm_freq        # tracked residual carrier (cycles/sample)
locked  = c.lock_metric      # Re(P conj ahat)/|P| EMA, ~1.0 when phase-locked
# resolve the M-fold ambiguity downstream, e.g. mpsk_diff_demap(mpsk_demap(...))
```

## CarrierNda — non-data-aided carrier loop

`CarrierNda` is the **non-data-aided** carrier-recovery loop — the cold-start
counterpart to `CarrierMpsk`. Per sample it de-rotates with the integer `lo` NCO;
it filters the de-rotated samples through a free-running I/Q **boxcar moving
average of `sps/n` samples** (one output per input sample — no rate change), and
on **every sample** runs an **M-th-power** phase discriminator
(`z²`/`z⁴`/`z⁸` by repeated squaring). Raising the arm sample to the Mth power
strips the M-PSK data, so the loop acquires the carrier **with no symbol timing
and no data present** — a bare/unmodulated carrier, or modulated data before
timing settles. `phase_error = Im(z^M)` (gain-normalized to a slope-2 S-curve for
every M); `lock` is the M-th-power lock metric. It locks to one of `m` phases
(M-fold ambiguity, resolved downstream). `steps()` returns the de-rotated sample
stream. See the [NDA carrier gallery](../gallery/carrier-nda.md) and the
[MPSK receiver design](../design/mpsk.md).

```python
from doppler.track import CarrierNda

# QPSK NDA loop, 8 samples/symbol, sps/n = 2-sample boxcar arm; keyword-capable
c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, m=4)
derot  = c.steps(rx)         # de-rotated samples (one per input sample)
f_est  = c.norm_freq         # tracked carrier (cycles/sample)
locked = c.lock              # M-th-power lock metric (normalised: ~1.0 at lock)
```

## MpskReceiver — pulse-shaped M-PSK modem

`MpskReceiver` is a complete M-PSK demodulator that owns no filter, no NCO and no
interpolator of its own: it is a **matched down-converter with two loops closed
around its two control ports**. A `MatchedDDC` mixes, decimates and
matched-filters in the dot products it was already doing (`pulse="iandd"`
integrate-and-dump by default, or `pulse="rrc"` root-raised-cosine for
band-limited links) — its terminal polyphase stage's **bank is the matched
filter** and the **arm it selects is the fractional symbol-timing delay**. A
carrier loop steers the LO (`freq_ctrl`); `RateSync`'s own timing loop, reused
rather than copied, steers the terminal accumulator (`rate_ctrl`).

Carrier recovery follows the project rule — **predetection de-rotation** (in the
LO, at the front of the chain) and **postdetection discrimination** (on the
matched-filtered symbols at the end of it). **One discriminator** does the work:
the NDA M-th-power error on the on-time strobe, needing no data and no symbol
timing, running from the first symbol to the last. Nothing gates it — `lock` and
`locked` are indicators a caller reads, not inputs the loop obeys. The loop locks
to one of `m` phases (M-fold ambiguity); resolve it with
`bits(..., differential=1)` or a sync word.

There used to be a second, decision-directed discriminator behind an opt-in
`acq_to_track`, on the reasoning that 8PSK's ±π/8 margin needs the lower-jitter
error. Measured, it was worth 0.09 dB at the 8PSK anchor while moving ~99% of the
recovered samples, so it is gone
([#877](https://github.com/doppler-dsp/doppler/issues/877)).

Because the front end plans its own cascade, **`sps` is a `float`** — an
irrational samples-per-symbol (a free-running ADC clock against the symbol clock)
is no harder than an integer one. `steps()` returns the recovered symbols;
`bits()` returns hard Gray bits (coherent, or rotation-invariant differential). A
DSSS-MPSK receiver is `Dll(segments) → MpskReceiver`. All constructor parameters
are keyword-capable with defaults. See the
[MPSK receiver gallery](../gallery/mpsk-receiver.md) and the
[MPSK receiver design](../design/mpsk.md).

```python
from doppler.track import MpskReceiver
from doppler.wfm import Synth

iq = Synth(type="qpsk", sps=8, snr=20).steps(4096)  # received IQ

# QPSK, 8 samples/symbol, I&D matched filter; NDA from the first strobe
rx = MpskReceiver(m=4, sps=8, m_out=4, pulse="iandd",
                  bn_carrier=0.005, bn_timing=0.01,
                  lock_thresh=0.4)
sym  = rx.steps(iq)          # recovered symbols (~ len(iq) / sps)
bits = rx.bits(iq)           # hard Gray bits (LSB-first per symbol)
f    = rx.norm_freq          # tracked carrier (cycles/sample)
lk   = rx.lock               # carrier lock metric (-> + at lock, every M)
```

!!! warning "Two parameters changed meaning in the cascade rebuild"

    - **`n` is now `m_out`**, and it means something different. `n` sized a
        separate NDA arm (window = `sps/n`); that arm no longer exists. `m_out` is
        the terminal stage's **outputs per symbol** (even, 2–8, default 8), which
        sets the Gardner strobe/gate geometry. The default is 8 because that is
        where an I&D matched filter reaches the coherent bound: measured on QPSK
        at `sps = 8` against `EVM_dB = -(Es/N0)_dB`, `m_out = 8` lands 0.41 dB
        off the bound at 18 dB Es/N0 where `m_out = 4` loses 3.11 dB. **Never
        pair 2 with `pulse="iandd"`** — the rectangle is one symbol wide, so at 2
        its matched filter degenerates to a two-tap sum (measured lock statistic
        −0.34 at 2 against +0.95 at 4) and acquisition itself fails about half
        the time.
    - **`bn_carrier` is normalised to the symbol rate**, like `bn_timing`, rather
        than to the input sample rate. At `sps = 8` the same number is now an 8×
        wider loop.

    Outputs are also **no longer bit-identical** to releases before the rebuild
    (polyphase bank instead of a dense FIR, bank arm instead of a Farrow).
    Detection performance is unchanged; exact-output pins are not.

!!! tip "Pull-in range — set by the loop, not by a tap"

    An M-th-power discriminator updating at rate `F` can only observe
    `|Δf| < F/(2M)`. It reads the **on-time strobe**, so `F = Rs` and the range
    is `Rs/(2M)` — measured `0.050·Rs` for QPSK at `sps=8` with the default
    `m_out=8`, and `0.010·Rs` at `m_out=4`.

    `nda_tap` used to select among three nodes and is **gone**
    ([#832](https://github.com/doppler-dsp/doppler/issues/832)); the strobe won
    on every axis measured on the receiver's own waveform. See
    [the design](../design/mpsk.md#33-where-the-discriminator-reads-the-strobe-and-why-the-menu-closed)
    for the numbers and for the measurement trap that nearly enshrined the
    wrong answer.

## One discriminator, and nothing waits

**There is no handover, no warmup, no lock gate and no timing gate.** The NDA
M-th-power error steers the LO from the first output to the last. That is a
reliability argument rather than a simplicity one: there is no state in which
the receiver can be wrong about which mode it is in, because there is one — no
declaring on garbage, no drop-back that never fires, and no metric that has to
be trusted before the loop is allowed to act. See
[MPSK Receiver §2.1](../design/mpsk.md).

`ContinuousMpskReceiver` used to be a separate view here, existing only to pin
`acq_to_track = 0`. With the handover deleted it pinned nothing and was a
duplicate of `MpskReceiver`, so it is gone
([#877](https://github.com/doppler-dsp/doppler/issues/877)) — as is
`configure_lock`, which retuned the handover's detector and never the lock
indicator's, so it desynced the two detectors it appeared to configure.

```python
from doppler.track import MpskReceiver

# Continuous BPSK at 8 samples/symbol. The caller states the link,
# not the loops.
rx = MpskReceiver(m=2, sps=8.0, bn_carrier=0.02, bn_timing=0.01)
sym = rx.steps(iq)          # recovered symbols
f = rx.norm_freq            # tracked carrier (cycles/sample)
```

**`lock` and `locked` are indicators, not gates.** `lock` is the M-th-power
statistic `Re((z/|z|)^M)` smoothed by an EMA; `locked` is a threshold test on
it with hysteresis — 8 consecutive symbols above `lock_thresh` to declare, 32
below `lock_drop_thresh` to withdraw. Neither steers a loop or gates an
output, so a wrong reading costs a caller their measurement window and costs
the demodulator nothing.

That statistic measures **phase coherence, not frequency error**, so the
instant `locked` declares says nothing about how converged the carrier
estimate is — do not read it as "the estimate is good now". `lock_time` (the
symbol of the first declaration) plus a settling budget is the question that
actually asks about convergence.

```python
rx.m_out, rx.num_phases, rx.lock_thresh   # 8, 64, 0.4999 — all derived
```

The M-fold phase ambiguity is **permanent** — no decision-directed stage
anywhere pins the absolute phase — so a caller wanting bits rather than
symbols needs either `bits(..., differential=1)` or a downstream sync word.
Coherent demapping with neither is a misconfiguration, not a choice.

## BpskReceiver — stated in the units a capture comes with

`BpskReceiver` is a **view** over the same core, and what makes it worth having
is what it does *not* ask for. A caller holding a capture knows its sample
rate, its symbol rate and its carrier frequency, in Hz. They do not know `sps`:
that is `fs / Rs`, a ratio the library computes for its own use in planning a
cascade. Requiring it makes the caller derive an internal quantity — and it
does not stop at one parameter, because with `sps` in the constructor
`init_norm_freq` has to be cycles per **sample**, so stating a carrier offset
needs `sps` *and* `fs` together while the loop bandwidth on the next line is
normalised to the **symbol** rate. One constructor, two normalisations, and the
conversion between them is the caller's problem.

```python
from doppler.track import BpskReceiver

rx = BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6)
rx.m        # 2   — the type says it, so it is not a parameter
rx.sps      # 8.0 — computed from the two rates
rx.m_out    # 8   — derived, not chosen
```

Two required arguments against `MpskReceiver`'s fifteen. `m` is carried by
the class name; `sps`, `m_out`, `num_phases` and `bn_agc_ratio` are
internal choices the object makes for itself; and `carrier_freq_hz` defaults to
0 for complex baseband. Everything a caller has a real reason to pin — the
pulse, both loop bandwidths, `differential`, `agc` — is still there as a
keyword.

**`m_out` deriving rather than defaulting is not cosmetic.** Pinning `m_out=4`
against the default I&D pulse is measured **3.11 dB** off the coherent bound
where the derived 8 is 0.41 dB off — the rectangle's matched filter is an
`m_out`-tap sum, and four taps sample that integral too coarsely. A parameter
nobody needed was a way to lose most of the link's margin quietly.

An impossible geometry is refused at construction rather than approximated: a
non-positive rate makes `sps` meaningless, and a carrier outside Nyquist is a
mis-stated capture rather than a tuning request. Both raise `ValueError`.

<!-- docs-snippet: raises=ValueError -->

```python
from doppler.track import BpskReceiver

# The carrier is past Nyquist for that sample rate.
BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6, carrier_freq_hz=9e6)
```

## MpskReceiverR — the real-input face

`MpskReceiverR` is `MpskReceiver` for a **real IF**: `steps()` and `bits()` take
`float32` samples of a real bandpass signal instead of complex baseband, and a
`MatchedDdcr` front end tunes and converts it to complex internally. Every loop,
discriminator, handover rule and demapper is the *same implementation* shared
with the complex face — only the front end and the two rate conversions its
halfband forces differ.

It is a **view** over `MpskReceiver`, like `BpskReceiver` above: one
core, one state, one set of loops, reached through a second constructor. It was
a separate class until just-makeit#1012, because a view shared its parent's
methods verbatim and `steps()` takes a different dtype here; a view may now
bind its own C symbol under the parent's Python name and declare its own
signature. So the type/flavor rule is unchanged — a difference in
**constructor** is a flavor, a difference in **method signature** is a separate
type — and the dtype difference now sits on the flavor side of it because jm
can express it.

Its one extra constraint is **`sps > 2 * m_out`** — the cascade behind the R2C
halfband runs at twice the overall rate. `init_norm_freq` and `norm_freq` are
both in cycles/sample at the **real input** rate; the tuning law and the
intermediate-rate conversion are handled internally.

```python
import numpy as np
from doppler.track import MpskReceiverR

fc = 0.10                                    # real IF, cycles/sample
rng = np.random.default_rng(3)
syms = np.exp(2j * np.pi * rng.integers(0, 4, 2000) / 4)
bb = np.repeat(syms, 24)                     # QPSK, 24 samples/symbol
n = np.arange(len(bb))
rf = (bb * np.exp(2j * np.pi * fc * n)).real.astype(np.float32) * 0.5

rx = MpskReceiverR(m=4, sps=24.0, m_out=4, init_norm_freq=fc, bn_carrier=0.002)
sym = rx.steps(rf)           # recovered symbols
lk  = rx.lock                # carrier lock metric
```

## Dll — code-tracking loop

`Dll` is the code-loop counterpart to `Costas`: a delay-lock loop that tracks
the phase of a continuous, repeating spreading code (PN / Gold sequence) on a
**carrier-wiped** sample stream. Per sample it correlates the input against
three taps of the local code — early (`+spacing` chips), prompt, late
(`-spacing` chips) — accumulating an integrate-and-dump over one code period;
per period it runs the power discriminator `0.5·(|E|² − |L|²) / |P|²`, filters
it through an embedded `LoopFilter`, and steers the code rate and phase. The
half-chip discriminator is steep, so the loop bandwidth is small (a few
thousandths). Taking powers makes the loop blind to the carrier's phase and to
the data sign of a whole period; a data flip *inside* a period is what
`segments`, below, is for.

In a full receiver the carrier loop (`Costas`) wipes the carrier and the `Dll`
wipes the code; [`dsss.Despreader`][doppler.dsss.Despreader] composes the two.

```python
import numpy as np
from doppler.track import Dll
from doppler.wfm import PN

code = np.asarray(PN(length=7, seed=1).generate(127)).astype(np.uint8)
# the same code at 4 samples per chip, 800 periods, as carrier-wiped BPSK
rx = np.tile(np.repeat(np.where(code & 1, -1.0, 1.0), 4), 800)
rx = rx.astype(np.complex64)

# code: 0/1 chips for one period; sps samples per chip; the replica starts
# a quarter chip off
d = Dll(code, sps=4, init_chip=0.25, bn=0.005, zeta=0.707, spacing=0.5)
symbols = d.steps(rx)        # one prompt symbol per code period
phase   = d.code_phase       # tracked code phase (chips)
rate    = d.code_rate        # tracked chip rate (~1.0 + code Doppler)

# The offset decays as exp(-zeta wn n), wn = 8 zeta bn / (4 zeta^2 + 1) =
# 0.0094 per period: 800 periods leave about 1e-3 chip of the 0.25.
assert abs(phase) < 0.01
```

**Sub-epoch partials for an asynchronous symbol clock (`segments`).** When the
data-symbol rate is on the order of the code-epoch rate but *asynchronous* to it,
a data transition lands inside most code periods, and a coherent full-epoch
despread that straddles one collapses. Set `segments > 1` to split each epoch
into that many sub-epoch **partial correlations**: `steps()` then emits
`segments` partial prompts per period — a stream at ~`segments` samples/symbol
(since symbol ≈ epoch) for a downstream `SymbolSync` — and the loop steers on
coherent sums over the one-period window its look-back finds free of a
transition. `segments=1` (default) is the plain coherent DLL above; choose `≥ 2`
for symbol-timing recovery. This `segments` mode is the **streaming
despreader**: its job is to remove the PN code and output samples.

**Wipe the carrier first.** The loop's sums are coherent over its window, so it
is blind to the carrier's phase but not to a residual frequency, which costs a
window of `T` seconds `sinc²(Δf·T)` of its power. The carrier estimate that
wipes the carrier, divided by the carrier frequency, is also the code-rate aid
(`set_rate_aid`): the same motion dilates the code. Symbol extraction
(`SymbolSync`) is downstream, fed from this output. See the
[streaming async despreader gallery](../gallery/async-despread.md) and the
[async despreader design](../design/async-dsss-receiver.md) §3.

```python
# 4 partial correlations per epoch from a carrier-wiped input: an oversampled
# async-BPSK stream with the PN removed. Symbol recovery is downstream.
d = Dll(code, sps=8, bn=0.002, zeta=0.707, spacing=0.5, segments=4)
partials = d.steps(rx)       # 4 partial prompts per code epoch (PN removed)
# downstream: SymbolSync(...).steps(partials) -> bits
```

## SymbolSync — symbol timing recovery

`SymbolSync` recovers the symbol clock of an **asynchronous** data stream (a
symbol rate not locked to the sample clock). It is a Gardner timing-error
detector closing a PI loop around an **integer timing NCO** and a `Farrow`
interpolator: the NCO's post-wrap value is the interpolation fraction µ (free, no
floating-point timing phase), so timing stays exact while only the interpolation
is floating point. Two interpolants per symbol (on-time + mid) are derived from
the phase value, and the loop steers the NCO **frequency** only — slip-free, so
the strobe count never drifts.

`steps()` emits one timing-corrected symbol per recovered instant; `rate` is the
tracked samples/symbol; `order` picks the Farrow interpolator. See the
[symbol-timing gallery page](../gallery/symsync.md) for the loop locking and
tracking an asynchronous clock end to end.

```python
from doppler.track import SymbolSync

ss = SymbolSync(sps=4, bn=0.01, zeta=0.707, order="cubic")
symbols = ss.steps(rx)   # timing-corrected symbols
ss.rate                  # recovered samples/symbol
```

## RateSync — matched filtering and timing in one dot product

`RateSync` solves the same problem as `SymbolSync` and shares its detectors and
loop, but it is built the other way round. Instead of a matched FIR followed by
a Farrow interpolator steered by a timing NCO, it owns a
[`MatchedRateConverter`][doppler.resample.MatchedRateConverter] whose **terminal stage carries the
pulse** — so the cascade's last dot product *is* the matched filter, and the
polyphase arm that dot product selects *is* the fractional timing delay. One
filter, no Farrow, no separate matched-filtering pass.

Two things follow from that, and they are the reason to reach for it:

- **`sps` is a `double`.** 4, 17.33389, an irrational ratio, or a slowly
    drifting clock all work by construction, because the terminal stage's
    accumulator is a double and the loop only has to steer the strobe. That is
    the real case whenever the ADC clock free-runs against the symbol clock.
- **A high input rate is nearly free.** The cascade's HB/CIC stages do the bulk
    decimation at no multiplies, so the matched-filter bank is sized by the
    *post-decimation* rate. The bank is the same size at 4 samples per symbol and
    at 256 — where filtering at the input rate would need thousands of taps per
    arm.

```python
from doppler.track import RateSync

rx_sync = RateSync(sps=17.33389, pulse="rrc", beta=0.35, span=8, m=2, bn=0.01)
symbols = rx_sync.steps(rx)   # one symbol per recovered instant
rx_sync.rate                  # tracked samples/symbol -- the clock estimate
rx_sync.locked                # verify-counted timing-lock decision
```

`locked` is decided once per block of 133 symbols. It declares after two
consecutive blocks clear the threshold and drops after six fall below it, so
the earliest declaration is 266 symbols in; both counts derive from a
per-decision false-transition budget of 1e-6, and
`configure_lock_raw()` sets others.

Judge lock by `lock_stat` / `locked` rather than by an error-vector magnitude:
a single cycle slip during acquisition drags a windowed EVM by 20 dB while the
eye is wide open. And check `clipped` at least once against real input — the
cascade inherits its CIC's ±2.0 input bound, and overdriving it costs ~25 dB
with a perfectly healthy lock.

Use `m >= 4` with `pulse="iandd"`: the rectangle is one symbol wide, so at
`m = 2` its matched filter is a two-tap sum and the eye barely opens. The RRC
spans many symbols and is unaffected.

`SymbolSync` remains the answer when the matched filter is one this family does
not build, or when the front end is already at a small integer `sps` and a
Farrow interpolator is the cheaper shape.
