# AsyncDsssReceiver: a signal's lifecycle

![State and symbol lock, the lock metric, and the carrier the receiver handed over and tracked, through noise, arrival, a fade, a departure and a return](../assets/async_dsss_receiver_lifecycle.png)

One [`dsss.AsyncDsssReceiver`](../api/python-dsss.md) follows one continuous DSSS
emitter from the first code epoch it searches to the moment the emitter leaves,
and it acquires the emitter again when the emitter returns. The receiver is fed
one code epoch per push, with its telemetry attached. It reports its own state,
its code and symbol lock decisions, its lock metric, every symbol it emits and the
Doppler it handed over, and a lossless capture keeps every record. The checks and
the figure read only those records.

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
- **Lock-up time and fade survival vary by draw.** This run keeps tracking and
    the symbol lock through a 10 ms fade. Lock-up time varies between noise draws.
    How often a fade is ridden out is measured in
    [the measurement record](../design/async-dsss-parallel-measurements.md#31-symbol-lock-through-a-dropout),
    under that record's own conditions. This setup is not that measurement, and
    this page does not restate its rate.
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
The noise is [`source.AWGN`](../api/python-nco.md), with the per-component
sigma that [`wfm_awgn_amplitude`](../api/python-wfmgen.md) gives for the Es/N0.

```python
--8<-- "src/doppler/examples/async_dsss_receiver_lifecycle.py:signal"
```

The receiver is built with the caller's rates, carrier uncertainty and release
interval. `set_telemetry(tlm, "rx")` registers every probe the receiver and its
loops publish, under `rx.`, and a
[`MemoryCapture`](../api/python-telemetry.md#capture-and-memorycapture-losslessness-you-can-prove)
keeps every record. `set_now()` stamps each push with its first sample. The caller
resets the receiver when it reports `lost`. Each probe reports at its own rate, so
`per_push()` turns the records into one row a push: a probe's last value at or
before the push, carried over pushes it wrote nothing in.

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

## Reading it when it does not behave

Read the probes in this order:

1. `rx.state`: searching (0), refining (1), tracking (2) or lost (4).
1. `rx.code.locked`: the code loop's lock detector.
1. `rx.sym.locked`: the symbol lock.
1. `rx.sym.lock`: the lock metric, the value the symbol lock thresholds.

Searching that never ends means no detection at this uncertainty. Refining that
never ends means the carrier estimate has not converged, and `rx.handover_hz`
says what the refine settled on when it did. Tracking with `rx.sym.locked` low
means the symbol lock has not come up yet, which is the lock-up time varying by
draw. The receiver's header lists every probe it registers
(`dp_async_dsss_receiver_set_telemetry()`).

The carrier probes are the loop's own view, not Hz: `rx.car.freq` (the loop's
frequency memory) and `rx.car.nco` (its whole command) are cycles a sample at
the input rate, so multiply by the sample rate, `chip_rate * spc`, for Hz. That
is how the figure's bottom panel draws the carrier. The live carrier in Hz is
`status().doppler_hz`.

To keep a run's record and look at it outside the script, open a file capture in
place of the memory one. The file and its JSON sidecar are the whole record:

```python
from doppler.dsss import AsyncDsssReceiver
from doppler.telemetry import Capture, Telemetry
from doppler.wfm import SampleClock

rx = AsyncDsssReceiver(
    code=GOLD,
    chip_rate=CHIP_RATE,
    symbol_rate=SYM_RATE,
    spc=SPC,
    cn0_dbhz=CN0_DBHZ,
    doppler_uncertainty=CARRIER_UNCERTAINTY,
)
tlm = Telemetry()
rx.set_telemetry(tlm, "rx")
x = make_timeline()[: 600 * TE]  # the noise, the arrival and its lock-up
with Capture(tlm, TE, "rx.tlm", SampleClock(FS)) as cap:
    for k in range(len(x) // TE):
        tlm.set_now(k * TE)
        rx.steps(x[k * TE : (k + 1) * TE])
assert cap.count > 0 and cap.dropped == 0
```

From a doppler checkout, `scripts/plot_capture.py` draws any capture, one panel
a probe on a shared time axis:

<!-- docs-snippet: no-exec=draws the rx.tlm the Python fence above writes, with the repository's own script -->

```sh
python scripts/plot_capture.py rx.tlm --time \
    rx.state rx.sym.locked rx.sym.lock rx.car.nco
```

A name matches by suffix, so `car.nco` alone would draw the demodulator's
carrier loop, `rx.demod.car.nco`, beside the receiver's; the full names pick
one each.

In C the same record is `dp_tlm_capture_open (tlm, TE, "rx.tlm", &clk)` in place
of `dp_tlm_capture_open_memory()`; the
[C twin](https://github.com/doppler-dsp/doppler/blob/main/native/examples/async_dsss_receiver_lifecycle.c)
shows the attach, the stamping and the read-back.

## What you're seeing

1. **State and symbol lock** (top). The black line is the state: searching,
    refining, tracking, or lost. The green fill is the symbol lock. The grey spans
    mark the signal-off intervals. The lock stays up through the fade and falls once
    the emitter has gone. The state moves to `lost` only after the confirm interval,
    and the reset takes it back to searching before it tracks the returning emitter.
1. **The lock metric** (middle). The metric is the SNR-weighted running mean of
    cos(2φ) over the symbols, the value the lock flag reads. The panel shows how far
    the metric dipped in each gap, which the binary flag does not.
1. **The carrier** (bottom). The orange step is `rx.handover_hz`. While the
    receiver refines it reads the search's coarse seed, which can sit outside
    this panel's range; from the hand-over on it reads the refined Doppler the
    tracker was built from. The purple trace is the carrier loop's command in
    Hz, which pulls the hand-over in to the emitter's carrier, the dotted line.
    The loop reports once per carrier update, and it makes none while it holds
    after the departure or while the receiver searches, so the trace breaks
    there.

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
