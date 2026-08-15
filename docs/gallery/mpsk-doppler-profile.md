# M-PSK Receiver — Doppler Profile and Loop Stress

![Carrier tracking a step, a ramp up and a ramp down, with the phase error each one costs](../assets/mpsk_doppler_profile_demo.png)

A [`track.MpskReceiver`](../api/python-track.md) is handed a carrier that steps,
then ramps up, then ramps down, then holds — and the figure records what each
of those costs the carrier loop.

The answer is not the same for the step and the ramps, and the difference is
the point of the page.

## A step is free; a ramp is not

The carrier loop filter is **type 2** — a PI filter feeding a phase
accumulator, so two integrators around the loop. Against a frequency **step**
its steady-state phase error is zero *regardless of loop gain*: the integrator
absorbs the whole offset and the discriminator settles back to nothing. You can
see it in the bottom panel, where the trace returns to zero after acquisition
and again after the ramps stop.

Against a frequency **ramp** it holds a **constant** phase error, and that
constant has a closed form:

$$
\theta_{ss} = \frac{2\pi r}{\omega_n^2},
\qquad
\omega_n = \frac{8\zeta\,b_n}{4\zeta^2+1} = 1.8857\,b_n \ \ (\zeta = 0.707)
$$

with $r$ the Doppler **rate** in cycles/symbol². At the settings in the figure
($b_n = 0.005$/symbol, $r = 2\times10^{-6}$) that is **0.1414 rad**, and the
measured mean sits within 1% of it on both ramps — positive on the way up,
negative on the way down.

The loop breaks when $\theta_{ss}$ leaves the M-th-power discriminator's linear
range, about $\pi/2M$ — the dotted lines. That is the real Doppler-rate limit
of a given `bn_carrier`, and because $\theta_{ss}$ goes as $1/\omega_n^2$, the
maximum trackable rate goes as $b_n^2$: doubling the loop bandwidth buys four
times the rate.

## Why this matters beyond the picture

**A test that only ever applies a frequency step cannot size a loop.** Since a
type-2 loop nulls a step whatever its gain, a loop running several times
narrower than configured passes every step test unchanged. That is not
hypothetical — it is exactly how `freq_scale` under-drove every non-`strobe`
NDA tap in this library until a ramp measurement found it
([gh-765](https://github.com/doppler-dsp/doppler/issues/765)). The ramp is what
charges the loop, so the ramp is what reveals its size.

`native/validation/rx_nda_tap.c` now gates that closed form on every tap.

## The middle panel is a readback subtlety, not an error

`rx.car.freq` is [`mpsk_rx_freq_est`](../c-api/mpsk__rx__loops_8h.md) — the
loop filter's **integrator alone**, deliberately, because the integrator is the
frequency memory. The frequency the LO actually applies is `integ + kp*e`.

On a ramp the phase error sits at a constant $\theta_{ss}$, so the proportional
term contributes a constant that the integrator-only readback does not carry —
which is the fixed ±33 Hz gap in the middle panel. The example asserts this by
its *ratio*: the gap divided by the phase error is the same constant on both
ramps to within 0.5%, which is what distinguishes "this is the proportional
term" from "the loop is lagging". `get_nco_freq()` is the readback that
includes it, and its own docstring says the mean tracks a ramp with no lag.

## Telemetry, not reconstruction

Every trace is a probe the receiver already publishes, captured losslessly at
`decim=1` through [`Telemetry`](../api/python-telemetry.md) and a
`MemoryCapture` — `rx.car.e` for the phase error, `rx.car.freq` for the
estimate, `rx.lock` for the lock metric. Nothing here re-derives a loop
internal from the output symbols.

```python
--8<-- "src/doppler/examples/mpsk_doppler_profile_demo.py:profile"
```

## The step has to be one a cold loop can find

`F0_SYM` is 0.001 cycles/symbol, which is 0.8 of the `bn/M` seeding bound. Past
that bound an M-th-power NDA loop still acquires, but pull-in time grows as
$\Delta f^2/b_n^3$ — the first draft of this example used a step 6.4× the bound
and the receiver had not locked by the end of the record. If a cold acquisition
is taking implausibly long, that ratio is the first thing to check.

## Related pages

- [M-PSK Receiver](mpsk-receiver.md) — the constellation, lock and BER story.
- [M-PSK Receiver: Performance](mpsk-receiver-performance.md) — EVM, SER and
    lock time over random geometries.
- [Design note](../design/mpsk.md) — the two-clock architecture, the NDA taps,
    and where the discriminator reads.
