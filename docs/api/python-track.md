# Python Loop Filter API

The `doppler.track` module provides `LoopFilter` — a **second-order
proportional-integral loop filter**, the shared engine of every tracking loop
(Costas/PLL, DLL, symbol timing). An error `e` goes in, a control value comes
out (`control = integ + kp·e`), and the integrator advances `integ += ki·e`, so
the integrator holds the running frequency/rate estimate and `kp·e` is the
instantaneous (phase) correction.

Source:
[`src/doppler/track/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/__init__.py)

See the [DSSS despreader gallery page](../gallery/dsss-despread.md), which uses
two of these — one for the carrier loop, one for the code loop.

______________________________________________________________________

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

______________________________________________________________________

## Examples

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

______________________________________________________________________

## Costas — carrier-tracking loop

`Costas` is the first loop built on `LoopFilter`: a continuous BPSK
carrier-recovery loop. Per sample it de-rotates the input with the integer-phase
[`source.LO`](python-nco.md) NCO (carrier wipe-off); every `tsamps` samples it
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
from doppler.track import Costas

# bn_fll > 0 adds the FLL assist for large/fast-moving residuals
c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16, bn_fll=0.03)
symbols = c.steps(rx)        # one complex prompt symbol per tsamps samples
f_est   = c.norm_freq        # tracked residual carrier (cycles/sample)
locked  = c.lock_metric      # |Re P|/|P| EMA, ~1.0 when phase-locked
```

______________________________________________________________________

## Dll — code-tracking loop

`Dll` is the code-loop counterpart to `Costas`: a delay-lock loop that tracks
the phase of a continuous, repeating spreading code (PN / Gold sequence) on a
**carrier-wiped** sample stream. Per sample it correlates the input against
three taps of the local code — early (`+spacing` chips), prompt, late
(`-spacing` chips) — accumulating an integrate-and-dump over one code period;
per period it runs the non-coherent envelope discriminator
`(|E| - |L|) / (|E| + |L|)`, filters it through an embedded `LoopFilter`, and
steers the code rate and phase. The half-chip discriminator is steep, so the
loop bandwidth is small (a few thousandths); `Dll` is data-insensitive (it works
on envelopes, so BPSK data flips don't matter).

In a full receiver the carrier loop (`Costas`) wipes the carrier and the `Dll`
wipes the code; a channel composes the two.

```python
from doppler.track import Dll

# code: 0/1 chips for one period; sps samples per chip
d = Dll(code, sps=4, init_chip=0.0, bn=0.005, zeta=0.707, spacing=0.5)
symbols = d.steps(rx)        # one prompt symbol per code period
phase   = d.code_phase       # tracked code phase (chips)
rate    = d.code_rate        # tracked chip rate (~1.0 + code Doppler)
```

______________________________________________________________________

## Channel — full tracking channel

`Channel` is the receiver that composes the loops: a `Costas` carrier loop and a
`Dll` code loop on a **single shared per-sample integrate-and-dump**. Per sample
it wipes the carrier (integer-NCO) and feeds the de-rotated sample to the DLL's
early/prompt/late correlators; per code period it dumps the prompt and updates
both loops — the code loop on the early/late envelopes, the carrier loop on the
same prompt. `steps()` emits one despread prompt symbol per code period;
`bits()` turns the prompts into hard data bits.

`bn_fll > 0` enables FLL-assisted carrier pull-in. When a data bit spans
`nav_period` code periods (GPS C/A: 20), `bits()` **bit-syncs** — it histograms
the prompt sign-flip positions to find the bit boundary (`bit_phase`), then
coherently sums `nav_period` prompts per bit. The channel is seeded by
acquisition (coarse carrier frequency + code phase) and tracks the residual.

See the [tracking channel gallery page](../gallery/channel.md) for the full
receiver acquiring and despreading end to end.

```python
from doppler.track import Channel

ch = Channel(code, sps=8, init_norm_freq=0.0, init_chip=0.0,
             bn_carrier=0.05, bn_code=0.005, bn_fll=0.03,
             zeta=0.707, spacing=0.5, nav_period=1)
symbols = ch.steps(rx)   # one despread prompt per code period
bits    = ch.bits(rx)    # hard data bits (bit-synced when nav_period > 1)
```

______________________________________________________________________

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

______________________________________________________________________

::: doppler.track.LoopFilter

______________________________________________________________________

::: doppler.track.Costas

______________________________________________________________________

::: doppler.track.Dll

______________________________________________________________________

::: doppler.track.Channel

______________________________________________________________________

::: doppler.track.SymbolSync
