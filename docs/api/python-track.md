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
from doppler.wfm import Synth

rx = Synth(type="qpsk", sps=16, snr=20, freq=0.01).steps(4096)  # received IQ

# bn_fll > 0 adds the FLL assist for large/fast-moving residuals
c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=16, bn_fll=0.03)
symbols = c.steps(rx)        # one complex prompt symbol per tsamps samples
f_est   = c.norm_freq        # tracked residual carrier (cycles/sample)
locked  = c.lock_metric      # |Re P|/|P| EMA, ~1.0 when phase-locked
```

______________________________________________________________________

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
([`mpsk.mpsk_diff_demap`](python-mpsk.md)) or a sync word; this loop only
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

______________________________________________________________________

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
locked = c.lock              # M-th-power lock metric (→ lock_scale when locked)
```

______________________________________________________________________

## MpskReceiver — pulse-shaped M-PSK modem

`MpskReceiver` is a complete per-sample M-PSK demodulator that composes the
tracking primitives on one shared sample loop: a `CarrierNda` carrier loop
(per-sample integer-NCO wipe-off + non-data-aided M-th-power acquisition), an
owned **matched filter** on the de-rotated stream (`pulse="iandd"` integrate-and-
dump boxcar by default, or `pulse="rrc"` root-raised-cosine for band-limited
links), and a `SymbolSync` Gardner timing loop. Carrier recovery follows the
project rule — **predetection de-rotation** (always) and **postdetection
discrimination**: the NDA loop acquires with no data and no symbol timing, then,
when `auto_handover=1` (opt-in) and the loop has locked, the receiver hands the
shared NCO to a lower-jitter **decision-directed** loop on the recovered symbols
(essential for 8PSK, whose M-th-power phase noise would otherwise cross the
±π/8 margins). The loop locks to one of `m` phases (M-fold ambiguity); resolve it
with `bits(..., differential=1)` or a sync word. `steps()` returns the recovered
symbols; `bits()` returns hard Gray bits (coherent, or rotation-invariant
differential). A DSSS-MPSK receiver is `Dll(segments) → MpskReceiver`. All
constructor parameters are keyword-capable with defaults. See the
[MPSK receiver gallery](../gallery/mpsk-receiver.md) and the
[MPSK receiver design](../design/mpsk.md).

```python
from doppler.track import MpskReceiver
from doppler.wfm import Synth

iq = Synth(type="qpsk", sps=8, snr=20).steps(4096)  # received IQ

# QPSK, 8 samples/symbol, I&D matched filter; NDA acquisition + opt-in handover
rx = MpskReceiver(m=4, sps=8, n=4, pulse="iandd",
                  bn_carrier=0.01, bn_timing=0.01,
                  auto_handover=1, lock_thresh=0.4)
sym  = rx.steps(iq)          # recovered symbols (~ len(iq) / sps)
bits = rx.bits(iq)           # hard Gray bits (LSB-first per symbol)
f    = rx.norm_freq          # tracked carrier (cycles/sample)
lk   = rx.lock               # carrier lock metric (-> + at lock, every M)
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
import numpy as np
from doppler.track import Dll
from doppler.wfm import Synth

code = np.random.default_rng(1).integers(0, 2, 127).astype(np.uint8)
rx = Synth(type="pn", pn_length=7, sps=8).steps(127 * 8 * 4)  # PN-spread IQ

# code: 0/1 chips for one period; sps samples per chip
d = Dll(code, sps=4, init_chip=0.0, bn=0.005, zeta=0.707, spacing=0.5)
symbols = d.steps(rx)        # one prompt symbol per code period
phase   = d.code_phase       # tracked code phase (chips)
rate    = d.code_rate        # tracked chip rate (~1.0 + code Doppler)
```

**Sub-epoch partials for an asynchronous symbol clock (`segments`).** When the
data-symbol rate is on the order of the code-epoch rate but *asynchronous* to it,
a coherent full-epoch despread straddles data transitions and collapses. Set
`segments > 1` to split each epoch into that many sub-epoch **partial
correlations**: `steps()` then emits `segments` partial prompts per period — a
stream at ~`segments` samples/symbol (since symbol ≈ epoch) for a downstream
symbol matched filter + `SymbolSync` — and the code is tracked **non-coherently**
across the partials (`(Σ|E| − Σ|L|)/(Σ|E| + Σ|L|)`), which a data flip cannot
collapse. `segments=1` (default) is the plain coherent DLL above; choose `≥ 2`
for symbol-timing recovery. This `segments` mode is the **streaming despreader**:
its job is to remove the PN code and output samples. Because the code loop is
non-coherent it is **carrier-blind** — it locks with a residual carrier still on
the samples, and (a short partial window being carrier-tolerant) the residual
just rides out on the partials. **Carrier recovery (`Costas`) and symbol
extraction (`SymbolSync`) are downstream**, fed from this output. See the
[streaming async despreader gallery](../gallery/async-despread.md) and the
[async despreader design note](../design/async-symbol-despreader.md).

```python
# 4 partial correlations per epoch -> non-coherent (carrier-blind) code tracking
# + an oversampled async-BPSK stream; carrier + symbol recovery are downstream.
d = Dll(code, sps=8, bn=0.002, zeta=0.707, spacing=0.5, segments=4)
partials = d.steps(rx)       # 4 partial prompts per code epoch (PN removed)
# downstream: Costas(...).steps(partials) -> SymbolSync(...).steps(...) -> bits
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

::: doppler.track.LoopFilter

______________________________________________________________________

::: doppler.track.Costas

______________________________________________________________________

::: doppler.track.CarrierMpsk

______________________________________________________________________

::: doppler.track.CarrierNda

______________________________________________________________________

::: doppler.track.Dll

______________________________________________________________________

::: doppler.track.Channel

______________________________________________________________________

::: doppler.track.SymbolSync
