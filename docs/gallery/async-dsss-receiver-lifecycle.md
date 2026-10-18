# AsyncDsssReceiver: a signal's lifecycle

![State and symbol lock, and the lock metric, through noise, arrival, a fade, a departure and a return](../assets/async_dsss_receiver_lifecycle.png)

One [`dsss.AsyncDsssReceiver`](../api/python-dsss.md) follows one continuous DSSS
emitter from the first code epoch it searches to the moment the emitter leaves,
and it acquires the emitter again when the emitter returns. The receiver is fed
one code epoch per push. After every push the page reads the receiver's state
from its properties (`tracking`, `refining`, `lost`) and its symbol-lock flag, and
it takes the symbols from `steps()`.

The setup is one emitter, CCSDS Gold-1023 at 3.069 Mcps, asynchronous BPSK at
2700 bps, 1500 Hz off nominal, at 10 dB Es/N0. The caller bounds the carrier at
±5 kHz, which is the receiver's one-sided `doppler_uncertainty`: the search covers
that half-range in either direction, and the caller sets it from the uncertainty
it actually has.

The timeline has six parts:

1. **Noise alone.** The receiver searches and emits no symbols.
1. **Arrival.** The emitter comes on. The receiver acquires it, refines the
    carrier estimate and starts tracking. Symbols begin only then.
1. **A short fade.** The emitter drops for 10 ms, and the page checks that the
    lock is up before it.
1. **Back on the air.** The emitter returns for 100 ms.
1. **Departure.** The emitter leaves for 0.3 s. Both lock detectors fall, and
    after the confirm interval the receiver goes to `lost`. The caller resets it.
1. **Return.** A returning emitter comes on. The reset receiver searches,
    acquires it and tracks it.

## What the receiver does at each stage

- **Searching and refining emit nothing.** The first symbols come once the
    receiver is tracking.
- **A fade shorter than the lock detectors' memory is ridden out.** In this run
    the state stays tracking and the symbol lock stays up through the 10 ms fade.
    That is one draw. How often a 10 ms fade is ridden out is a rate, measured
    over many draws at 10 dB in
    [the measurement record](../design/async-dsss-parallel-measurements.md#31-symbol-lock-through-a-dropout),
    and this page does not restate it.
- **The release rule is armed.** `lost_confirm_s` is the interval both lock
    detectors must stay down before the receiver gives up on the emitter. The
    setup sets it to 100 ms. Once it passes, the state is `lost`, the loops stop,
    and samples are discarded until `reset()`.
- **Reset returns the receiver to searching.** A receiver that has locked cannot
    be reset onto the same signal. It searches again, and a returning emitter is
    acquired afresh.

The script prints the intervals it measured: the arrival to tracking, the release
after departure, and the re-acquisition after the return. Those are this run's
numbers. The script asserts each claim on this page, so a change that breaks one
fails the example rather than the prose.

## How it works

The emitter is [`wfm.Synth`](../api/python-wfmgen.md) in continuous DSSS mode,
with its own PRBS payload on the Gold code. Each off span is zeroed after
synthesis, so the noise floor is the same before, during and after every cut.
The noise is [`source.AWGN`](../api/python-wfmgen.md), with the per-component
sigma that [`wfm_awgn_amplitude`](../api/python-wfmgen.md) gives for the Es/N0.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:signal"
```

The receiver is built with the caller's rates, carrier uncertainty and release
interval. It is fed one epoch per push. The state is read after each push, and
the caller resets the receiver when it reports `lost`.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:receiver"
```

The checks run on those rows. Where a bound is physical, the check asserts it: the
receiver cannot release an emitter before its confirm interval has passed, and it
cannot emit a symbol before it tracks. The other bounds are windows around this
run, and they hold the order of the events.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:validate"
```

## What you're seeing

1. **State and symbol lock** (top). The black line is the state: searching,
    refining, tracking, or lost. The green fill is the symbol lock. The grey spans
    mark the signal-off intervals. The lock stays up through the fade and falls once
    the emitter has gone. The state moves to `lost` only after the confirm interval,
    and the reset takes it back to searching before it tracks the returning emitter.
1. **The lock metric** (bottom). The metric is the SNR-weighted running mean of
    cos(2φ) over the symbols, the value the lock flag reads. The panel shows how far
    the metric dipped in each gap, which the binary flag does not.

## Reproduce

```sh
python -m doppler.examples.async_dsss_receiver_lifecycle out.png
```

The C twin is
[`async_dsss_receiver_lifecycle.c`](https://github.com/doppler-dsp/doppler/blob/main/native/examples/async_dsss_receiver_lifecycle.c).
It runs the same timeline and checks the same physics. It uses its own noise draw,
so its numbers differ from this page's, and it shows the buffers and the reset that
the binding hides.

The design, including the state machine and the release rule, is in
[the async DSSS receiver design](../design/async-dsss-receiver.md).

Source: [`async_dsss_receiver_lifecycle.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/async_dsss_receiver_lifecycle.py).
