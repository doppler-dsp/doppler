# AsyncDsssReceiver: a signal's lifecycle

![State and symbol lock, and the lock metric, through noise, arrival, a fade, a departure and a return](../assets/async_dsss_receiver_lifecycle.png)

One [`dsss.AsyncDsssReceiver`](../api/python-dsss.md) follows one continuous DSSS
emitter from the first code epoch it searches to the moment the emitter leaves,
and it acquires the emitter again when the emitter returns. The receiver is fed
one code epoch per push. After every push the page reads its
[`status()`](../api/python-dsss.md): the state, the symbol-lock flag and the lock
metric. Symbols come from [`steps()`](../api/python-dsss.md).

The setup is one emitter, CCSDS Gold-1023 at 3.069 Mcps, asynchronous BPSK at
2700 bps, 1500 Hz off nominal, at 10 dB Es/N0. The caller bounds the carrier at
±5 kHz, which is the receiver's one-sided `doppler_uncertainty`: the search covers
that half-range in either direction, and the caller sets it from the uncertainty
it actually has.

The timeline has six parts:

1. **Noise alone.** The receiver searches and emits no symbols.
1. **Arrival.** The emitter comes on. The receiver acquires it, refines the
    carrier estimate and starts tracking. Symbols begin only then.
1. **A short fade.** The emitter drops for 10 ms. Tracking and the symbol lock
    ride it out.
1. **Departure.** The emitter leaves for 0.3 s. The receiver goes to `LOST`, and
    the caller resets it.
1. **Return.** The emitter comes back. The reset receiver searches, acquires it
    again and tracks it.

## What the receiver does at each stage

- **Searching emits nothing.** The receiver emits no symbols while it searches or
    refines. The first symbols come once it is tracking.
- **Tracking holds through a fade.** A fade shorter than the lock detectors'
    memory leaves the state `TRACKING` and the symbol lock up.
- **The release rule is armed.** `lost_confirm_s` is the interval both lock
    detectors must stay down before the receiver gives up on the emitter. The
    setup sets it to 100 ms. Once the interval passes, the state is `LOST`, the
    loops stop, and samples are discarded until `reset()`.
- **Reset returns the receiver to searching.** A receiver that has locked cannot be
    reset onto the same signal. It searches again, and a returning emitter is
    acquired afresh.

The script prints the intervals it measured: the arrival to tracking, the release
after departure and the re-acquisition after the return. It does not claim a
lock-up time, which depends on the noise draw. It waits for the lock to be up
before the fade and measures only what happens after.

## How it works

The emitter is synthesised by [`wfm.Synth`](../api/python-wfmgen.md) in continuous
DSSS mode. Each off span is zeroed after synthesis, so the noise floor is the same
before, during and after every cut. The noise is the library's own unit-noise
`Synth`, scaled by [`wfm_awgn_amplitude`](../api/python-wfmgen.md).

Every event is cut while the data is transitioning. The payload rides on the code
throughout, so no event falls in a code-only run. A check on the clean emitter
asserts this stimulus precondition: the data flips at about half the symbol
boundaries, and within twenty symbols of every cut.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:signal"
```

The receiver is built with the caller's rates, carrier uncertainty and release
interval. It is fed one epoch per push. The status is read after each push, and
the caller resets the receiver when it reports `LOST`.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:receiver"
```

The checks run on those rows. Where a bound is physical, the check asserts it: the
receiver cannot release an emitter before its confirm interval has passed, and it
cannot emit a symbol before it tracks. The other bounds are windows around one
run, and they hold the order of the events.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:validate"
```

## What you're seeing

1. **State and symbol lock** (top). The black line is the state: searching,
    tracking, or lost. The green fill is the symbol lock. The grey spans mark the
    signal-off intervals. The lock stays up through the fade, falls once the
    emitter has gone, and the state moves to `LOST` only after the confirm
    interval. The reset takes it back to searching, and it tracks the returning
    emitter.
1. **The lock metric** (bottom). The metric is the SNR-weighted running mean of
    cos(2φ) over the symbols, the value the lock flag reads. The panel shows how far
    the metric dipped in each gap, which the binary flag does not.

## Reproduce

```sh
python -m doppler.examples.async_dsss_receiver_lifecycle out.png
```

The C twin is
[`async_dsss_receiver_lifecycle.c`](https://github.com/doppler-dsp/doppler/blob/main/native/examples/async_dsss_receiver_lifecycle.c).
It runs the same timeline and checks the same physics, and it shows the buffers
and the reset that the binding hides.

The design, including the state machine and the release rule, is in
[the async DSSS receiver design](../design/async-dsss-receiver.md).

Source: [`async_dsss_receiver_lifecycle.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/async_dsss_receiver_lifecycle.py).
