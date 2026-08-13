# Arbitrary-Rate Symbol Recovery

![Arbitrary-rate matched filtering and timing recovery](../assets/ratesync_demo.png)

[`track.RateSync`](../api/python-track.md) recovers symbols from a stream whose
sample clock has **no integer relationship to the symbol clock** — here 17.33389
samples per symbol, and 200 ppm off even that.

It gets there by inverting the usual arrangement. Where
[`SymbolSync`](symsync.md) runs a matched FIR and then a separate
[`Farrow`](farrow.md) interpolator steered by an integer NCO, `RateSync` owns a
[`MatchedRateConverter`](../api/python-resample.md) whose **terminal stage carries the
pulse**. The cascade's last dot product *is* the matched filter, and the
polyphase arm that dot product selects *is* the fractional timing delay. One
filter, no Farrow, no separate matched-filtering pass — and because that
stage's accumulator is a `double`, `sps` is a `double`.

## What you're seeing

**Top — Recovered symbols.** The real part per symbol index: a short pull-in
while the loop acquires, then clean rails — at a fractional `sps`, with the
clock 200 ppm off nominal.

The noise is quoted as **Es/N0**, not as an "SNR": at 17.33 samples per symbol
those differ by more than 12 dB, so the same number would describe two very
different links. The noise is complex, N0 total — a real receiver's baseband is
complex and its Q channel carries noise even when the modulation is real.

At the matched-filter output the error vector *is* that complex noise, of total
variance N0, against a reference of energy Es. So on the I/Q plane

$$
\\mathrm{EVM}^2 = \\frac{N_0}{E_s}
\\qquad\\Longleftrightarrow\\qquad
\\mathrm{EVM},[\\mathrm{dB}] = -\\bigl(E_s/N_0\\bigr)[\\mathrm{dB}]
$$

(The familiar factor of two belongs to an **I-only** measurement, which
discards the Q channel. EVM is a plane quantity unless it says otherwise.)

Measured, ten seeds per point:

| Es/N0 | bound    | measured  | offset   |
| ----- | -------- | --------- | -------- |
| 10 dB | −10.0 dB | −10.02 dB | −0.02 dB |
| 15 dB | −15.0 dB | −15.01 dB | −0.01 dB |
| 20 dB | −20.0 dB | −19.97 dB | +0.03 dB |

So the fused matched filter **is on the bound**, across a 10 dB span — fusing
it into the resampler's polyphase bank costs nothing in detection performance.
It is the same filter, evaluated at a steered instant.

(One 375-symbol window carries ~0.3 dB of estimator noise, so a single seed
lands either side. The script asserts ±1.5 dB at three operating points rather
than pretending one measurement is tighter than it is.)

**Middle — Tracked clock.** `RateSync.rate` pulling off the nominal it was
constructed with (dotted) onto the stream's true rate (dashed). The loop is
tracking an *asynchronous, non-integer* clock; `rate` reports the truth, which
is what a caller disciplining a clock reads. It is taken from the loop
**integrator**, not the instantaneous control — the integrator is the rate
memory, and reading the noisy total instead biases the estimate.

**Bottom — Matched-filter cost.** Taps per arm against input samples per
symbol. Applied at the input rate (red) a root-raised-cosine matched filter
grows linearly with `sps` — 4225 taps per arm at 256 samples/symbol, some 35 MB
of bank. Riding the cascade's terminal stage (blue) it is **flat**: the HB/CIC
stages ahead of it have already done the bulk decimation, at no multiplies, so
the filter is sized by the *post*-decimation rate. The single 34 → 40 step is
the CIC droop compensator folding into the bank once the planner picks a CIC; a
halfband cascade has no droop to correct.

## How it works

The receiver is one object and one call. `sps` is the *nominal* rate — the loop
finds the true one:

```python
--8<-- "src/doppler/examples/ratesync_demo.py:signal"
```

`RateSync` asks its `MatchedRateConverter` for `rate = m/sps` and lets the planner
decide the shape. At `sps = 17.33389` with `m = 2` that is `CIC(8)` followed by
a `Resampler(0.923, rrc)`: the CIC throws away the bulk of the rate for free,
and the fractional remainder lands on the stage that carries the pulse. Ask for
`sps = 64` instead and the plan becomes `CIC(32)` + `Resampler(1.0, rrc)` — the
same bank, a 32× cheaper front end.

That terminal stage always exists when a pulse is selected, even when the rate
divides exactly. It is not there to correct the rate; it is the matched filter
*and* the timing element, and a cascade that ended in a bare `CIC(32)` would
have nothing steerable at the end.

Every `m`-th terminal output is the on-time strobe and the output `m/2` back is
the Gardner transition gate, so the oversampled stream and the symbol stream
come out of the same dot products. A half-symbol error in that assignment *is*
an equilibrium of the detector — but an **unstable** one: each parity's S-curve
has one zero at the eye centre with negative slope and one at the T/2 point with
positive slope, so the loop runs away from the wrong one unaided. No eye-sign
detector, no counter flip, and no second bank.

The cost claim in the third panel is measured, not asserted:

```python
--8<-- "src/doppler/examples/ratesync_demo.py:cost"
```

## Reading the object honestly

Two habits worth forming, both of which cost real debugging time to learn:

**Judge lock by `lock_stat` / `locked`, not by an error-vector magnitude.** A
single cycle slip during acquisition drags a windowed EVM by 20 dB while the
eye is wide open at +0.75. The eye statistic is the honest indicator; EVM is
only meaningful once the loop has settled.

**Check `clipped` at least once against real input.** The cascade inherits its
CIC's ±1.0 input bound, and an overdriven front end costs about 25 dB of EVM
with a perfectly healthy lock — nothing in the timing metrics reveals it. That
is exactly why the flag exists.

One tuning note: use `m >= 4` with `pulse="iandd"`. The rectangle is one symbol
wide, so at `m = 2` its matched filter is a two-tap sum and the eye barely
opens — measured on an NRZ stream, `m = 2` does not clear the lock detector's
own declare threshold while `m = 4` clears it comfortably, with tens of dB of
EVM between them. The rule rests on that separation, not on a particular pair
of `lock_stat` values: those move with `sps` and with the stream. The RRC spans
many symbols and is unaffected.

## Choosing a detector

`ted` is the one knob here whose two settings are both correct, so it is a
choice rather than a default. The demo recovers the same stream with each:

```python
--8<-- "src/doppler/examples/ratesync_demo.py:ted"
```

|                               | `gardner` (default)                   | `dttl`                                                                                            |
| ----------------------------- | ------------------------------------- | ------------------------------------------------------------------------------------------------- |
| kind                          | blind                                 | decision-directed                                                                                 |
| constellations                | any                                   | BPSK/QPSK only                                                                                    |
| self-noise near lock          | pays on every non-transition symbol   | gates those out — rests ~6x closer to the eye centre on a noiseless stream                        |
| does `bn` mean one bandwidth? | yes — flat across roll-off            | **no** — varies ~8x over beta 0.1–0.9 ([#669](https://github.com/doppler-dsp/doppler/issues/669)) |
| level error costs             | `A²` — a 2x error is 4x the loop gain | `A¹` — a 2x error is 2x                                                                           |

At a realistic Es/N0 the noise dominates and the two reach the same EVM, which
is what the demo asserts. DTTL's advantage is a *self-noise* advantage, so it
shows where self-noise is what is left. Start with `gardner`.

## The C composition API

RateSync splits into a cascade and a timing loop so a receiver can inline the
per-sample step into its own loop rather than calling a block API — which is
exactly what `MpskReceiver` does, steering the same accumulator through its
DDC's `rate_ctrl` port. The object form is the same call from C:

```c
#include <ratesync/ratesync_core.h>
#include <complex.h>
#include <stdio.h>

int main(void)
{
    /* 17.33389 samples/symbol -- a free-running ADC clock against the
       symbol clock -- RRC beta 0.35 span 8, m = 2 outputs/symbol, a
       1024-arm bank, bn 0.01, zeta 0.707, blind Gardner detector. */
    ratesync_state_t *rs = ratesync_create(17.33389, RATESYNC_PULSE_RRC,
                                           0.35, 8, 2, 1024, 0.01, 0.707,
                                           RATESYNC_TED_GARDNER);
    if (!rs)
        return 1;

    float complex x[4096];
    for (int i = 0; i < 4096; i++)
        x[i] = 0.0f;  /* your baseband goes here */

    /* One input in, at most one symbol out. This is the call a receiver
       inlines; ratesync_steps() is the block form over the same body. */
    float complex sym;
    long got = 0;
    for (int i = 0; i < 4096; i++)
        if (ratesync_step(rs, x[i], &sym))
            got++;

    printf("%ld symbols, rate %.5f, locked %d\n", got,
           ratesync_get_rate(rs), ratesync_get_locked(rs));
    ratesync_destroy(rs);
    return 0;
}
```

`SymbolSync` is unchanged and remains the right answer when the matched filter
is not one this family builds, or when the front end is already at a small
integer `sps` and a Farrow interpolator is the cheaper shape.

## Reproduce

```sh
uv run python src/doppler/examples/ratesync_demo.py ratesync_demo.png
```

Source: [`src/doppler/examples/ratesync_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/ratesync_demo.py)
