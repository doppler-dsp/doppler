# Doppler Channel — Clock Doppler as a Propagation Impairment

![DopplerChannel: carrier offset linear in ppm, code slip accumulating at 61.4 chips/s, a Doppler ramp integrating to 500 Hz/s, and a 550 km LEO pass no ramp can express](../assets/doppler_channel_demo.png)

A Doppler shift is not a frequency offset. Relative motion rescales the
**entire received time base**, so every clock in the signal changes together —
carrier, chip rate, symbol rate, frame rate. Modelling only the carrier is the
usual shortcut, and it silently deletes the one error a delay-lock loop exists
to track.

`impairment.DopplerChannel` applies both halves of the effect from a single
parameter, so they cannot disagree with each other.

## What you're seeing

**Left — the carrier offset is linear in ppm.** Doppler is specified in parts
per million of the nominal time base, which is what makes it
carrier-frequency agnostic. At the 2.5 GHz carrier of
[the async DSSS receiver spec](../design/async-dsss-spec.md), that spec's ±50 kHz frequency
uncertainty is exactly **±20 ppm**. Measured FFT peaks sit on the `fc·d` line
across the sweep.

**Centre-left — the time base dilates.** This is the panel a carrier-only model gets
wrong: it would be flat on the dotted zero line. The real channel accumulates
code phase at `Rc·d` — **61.4 chips per second** at 20 ppm on a 3.069 Mcps
code. Over the 256 ms plotted, that is 16 chips the receiver's code loop has to
make up. The trace is quantised to an eighth of a chip because slip is counted
in whole samples at `spc=8`.

**Centre-right — a Doppler ramp is the integral of the rate.** With
`doppler_rate_ppm_s = 0.2` (SPEC.md's 500 Hz/s at 2.5 GHz), the instantaneous
offset climbs as `fc·ḋ·t`. The dotted orange line is the natural wrong
implementation — accumulating `t·d(t)` instead of `∫d dt` double-counts the
ramp and lands at exactly twice the truth. It is the one error that passes every
static-Doppler check, which is why both the C and Python test suites assert
against it specifically.

**Right — a real pass is not a ramp.** The first three panels are all driven by
the create-time pair `(doppler_ppm, doppler_rate_ppm_s)`, which is a straight
line. A satellite pass is not one. This panel takes the Doppler from circular
orbit geometry for a 550 km overhead pass — the law of cosines for slant range,
differentiated — and it comes out an S-curve: **+23.3 to −23.3 ppm** (±58.2 kHz
at 2.5 GHz), steepest at closest approach and flattening toward the horizon.
The dotted orange line is the best straight line through it, and the profile
departs from it by **21% of its own range**, so no choice of Doppler rate
reproduces this.

That is what `execute_profile()` is for: one Doppler value per waveform sample,
handed to the same resampler the scalar form drives. The blue points are the
channel's own `offset_hz` read back as the pass plays, sitting on the curve it
was given.

One number in that panel is worth more than it looks. Over the whole pass the
**net time dilation is +0.000 ppm** — the record is compressed while the
satellite closes and stretched by the same amount while it opens, because an
overhead pass is antisymmetric about closest approach. The instantaneous rate
error reached 23.3 ppm and the totals cancel, which is precisely why a receiver
has to *track* a pass rather than fit one clock offset to the capture.

## How it works

The dilation is a resampling of the whole stream at output/input ratio
`1/(1+d)`, which is what makes it apply to every clock at once rather than to
each one separately. It reuses `resample.Resampler`'s per-sample rate control
(`resamp_execute_ctrl`), whose double-precision accumulator tracks a Doppler
*ramp* exactly instead of approximating it with a piecewise-constant ratio
re-set once per block. No resampling math is reimplemented.

The carrier is then `exp(j·2π·fc·excess(t))`, where `excess(t) = ∫d dt` is the
same dilation integral the resampler ratio came from — one number, so the code
rate and the carrier can never drift apart.

!!! note "`carrier_hz` is load-bearing here, not metadata"

    Everywhere else in this codebase `--fc` is a SigMF annotation that never
    touches a sample. In `DopplerChannel` it is DSP input, and unavoidably so:
    Doppler is dimensionless ppm, and `fc` is the only thing that converts it
    into Hz. Setting it to `0` still dilates the clocks correctly but leaves the
    carrier stationary — permitted, because it is occasionally useful for
    isolating a code loop under test, but not what a real channel does.

What is deliberately **not** plotted: under a Doppler rate the code slips
quadratically, `Rc·½·ḋ·t²`. At SPEC.md's 0.2 ppm/s that is 0.08 chips over the
whole half-second run — a fraction of a single sample, below what sample
counting can resolve. The carrier effect of a ramp is first order and plainly
visible; the code effect is second order and, over a realistic dwell,
negligible.

```python
--8<-- "src/doppler/examples/doppler_channel_demo.py:channel"
```

## Reproduce

```sh
python -m doppler.examples.doppler_channel_demo doppler_channel_demo.png
```

Source: [`src/doppler/examples/doppler_channel_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/doppler_channel_demo.py)
