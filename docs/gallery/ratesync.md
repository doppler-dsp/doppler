# Arbitrary-Rate Symbol Recovery

![Arbitrary-rate matched filtering and timing recovery](../assets/ratesync_demo.png)

[`track.RateSync`](../api/python-track.md) recovers symbols from a stream whose
sample clock has **no integer relationship to the symbol clock** — here 17.33389
samples per symbol, and 200 ppm off even that.

It gets there by inverting the usual arrangement. Where
[`SymbolSync`](symsync.md) runs a matched FIR and then a separate
[`Farrow`](farrow.md) interpolator steered by an integer NCO, `RateSync` owns a
[`RateConverter`](../api/python-resample.md) whose **terminal stage carries the
pulse**. The cascade's last dot product *is* the matched filter, and the
polyphase arm that dot product selects *is* the fractional timing delay. One
filter, no Farrow, no separate matched-filtering pass — and because that
stage's accumulator is a `double`, `sps` is a `double`.

## What you're seeing

**Top — Recovered symbols.** The real part per symbol index: a short pull-in
while the loop acquires, then clean rails — at a fractional `sps`, with the
clock 200 ppm off nominal.

The noise is quoted as **Es/N0**, not as an "SNR", because at 17.33 samples per
symbol those differ by 12 dB and the same number would describe two very
different links. At Es/N0 = 15 dB a matched filter cannot do better than
EVM² = 1/(2·Es/N0), i.e. −18.0 dB. This receiver measures **−18.0 dB averaged
over twelve seeds — 0.03 dB from the bound**. Fusing the matched filter into the
resampler's polyphase bank costs nothing in detection performance; it is the
same filter, evaluated at a steered instant.

(A single run's 375-symbol window carries about 0.4 dB of estimator noise, so
one seed can land either side of the bound. The script asserts ±1.5 dB — three
sigma — rather than pretending a single measurement is tighter than it is.)

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

`RateSync` asks its `RateConverter` for `rate = m/sps` and lets the planner
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
wide, so at `m = 2` its matched filter is a two-tap sum and the eye barely opens
(`lock_stat` −0.34 at m=2 against +0.95 at m=4 on the same NRZ stream). The RRC
spans many symbols and is unaffected.

`SymbolSync` is unchanged and remains the right answer when the matched filter
is not one this family builds, or when the front end is already at a small
integer `sps` and a Farrow interpolator is the cheaper shape.

## Reproduce

```sh
uv run python src/doppler/examples/ratesync_demo.py ratesync_demo.png
```

Source: [`src/doppler/examples/ratesync_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/ratesync_demo.py)
