# Python Correlation API

The `doppler.correlation` module is one class, **`Correlator`**, over the C
`correlator` core: every FFT correlator in the library, behind one
constructor whose knobs pick the flavor.

| flavor            | built by                                       | the one call      |
| ----------------- | ---------------------------------------------- | ----------------- |
| `"1d"`            | `Correlator(ref)`, `ref` 1-D                   | `execute(frame)`  |
| `"2d"`            | `Correlator(ref)`, `ref` 2-D                   | `execute(frame)`  |
| `"delay_doppler"` | `Correlator(code, doppler_rows=ny)`            | `execute(stream)` |
| `"detector_1d"`   | `Correlator(ref, detect=1)`, `ref` 1-D         | `push(chunk)`     |
| `"detector_2d"`   | `Correlator(ref, detect=1)`, `ref` 2-D         | `push(chunk)`     |
| `"continuous"`    | `Correlator(chips, framing="continuous", ...)` | `push(chunk)`     |
| `"burst"`         | `Correlator(preamble, framing="burst", ...)`   | `push(chunk)`     |

The [Correlator guide](../guide/correlator.md) is where to start: what each
flavor is for, which knobs pick it, and the refusal rule. In short, a knob the
flavor would not honor is refused with `ValueError`, and a member the flavor
does not serve raises `AttributeError` (`hasattr` reads `False`); the
`flavor` property says which flavor was built.

The two search flavors are also called **acquisition**: they find a signal
before anything tracks it. `BurstCapture` and the DSSS receivers on the
[Acquire](python-acquire.md) and [DSSS](python-dsss.md) pages compose them.

Source: `src/doppler/correlation/__init__.py` (new in the next release,
so no release link yet).

______________________________________________________________________

## Maps

A map pre-computes the reference's spectrum once, so each `execute()` costs
one forward FFT and the multiplies, plus the inverse FFT on the call that
dumps. With `dwell == 1` every call dumps; with a larger `dwell` the
accumulator integrates that many frames coherently before returning a result
(and returns `None` in between).

```python
import numpy as np
from doppler.correlation import Correlator

ref = np.exp(2j * np.pi * 0.1 * np.arange(1024)).astype(np.complex64)
corr = Correlator(ref, dwell=1)
frame = ref + 0.1 * (np.random.randn(1024) + 1j * np.random.randn(1024))
out = corr.execute(frame.astype(np.complex64))   # ndarray on a dump, else None
lag = int(np.argmax(np.abs(out)))                 # correlation peak position
```

A 2-D `(ny, nx)` reference builds the 2-D map: `execute` takes the input in
either shape and returns a flat row-major array. A 1-D code with
`doppler_rows = ny` builds the delay-Doppler map: it takes the raw stream,
`ny` code periods back to back, and returns the centered `ny_out × nx` map,
whose rows are Doppler bins `doppler_step` cycles/sample apart, with zero
Doppler on row `ny_out // 2`, and whose columns are delay lags. See the
[Delay-Doppler guide](../guide/delay-doppler.md) and
[the design section](../design/corr2d-interpolated-inverse.md#10-delaydoppler-the-slow-time-fft-in-c-2666).

## Detectors and searches

A detector (`detect=1`) or a search wraps the correlator with a
**double-mapped ring buffer** so you can push arbitrary-sized chunks. After
each integrate-and-dump it compares the peak-to-noise test statistic against
`threshold` and reports a detection when it passes (`threshold = 0.0` fires
on every dump).

`push()` returns a NumPy structured array, one record per detection, with the
fields `row`, `col`, `peak_mag`, `noise_est`, `test_stat`, `cn0_dbhz_est`,
`samples_consumed`, `chip_phase`, `doppler_rate_hz_s` and `test_stat_peak`.
The search-only fields read 0 on a detector. The array is sized from
`push_max_out(len(chunk))`, so one push never drops a detection however many
frames it completes.

```python
import numpy as np
from doppler.correlation import Correlator

ref = np.exp(2j * np.pi * 0.1 * np.arange(1024)).astype(np.complex64)
det = Correlator(ref, detect=1, dwell=4, threshold=12.0)  # 12x the noise


def stream_chunks():                              # a real CF32 source
    for _ in range(4):
        yield (np.random.randn(1024)
               + 1j * np.random.randn(1024)).astype(np.complex64)


for chunk in stream_chunks():
    for hit in det.push(chunk):
        print("detection: lag", hit["col"], "stat", hit["test_stat"])
```

The searches size their own Doppler grid and dwell from `cn0_dbhz`,
`doppler_uncertainty`, `pfa` and `pd`; the
[acquisition guide](../guide/acquisition.md) and the
[DSSS acquisition guide](../guide/dsss-acquisition.md) work through them.

______________________________________________________________________

::: doppler.correlation.Correlator

## Related pages

<!-- related-pages:start -->

**Gallery** — [CarrierAcquisition: RRC Pulse Shaping](../gallery/carrier-acq-rrc.md), [Correlation and Detection](../gallery/corr.md), [Delay-Doppler Search](../gallery/delay-doppler.md), [2-D Acquisition Grid](../gallery/detection2d.md), [DSSS Acquisition — Pd / Pfa vs Es/N0](../gallery/dsss-acq-characterization.md), [DsssBurstReceiver — the Composed Burst Chain](../gallery/dsss-burst-receiver.md), [DsssReceiver — the Composed Continuous DSSS Receiver](../gallery/dsss-receiver.md), [Gallery](../gallery/index.md)
**Guides** — [Acquiring a Repeated Preamble](../guide/acquisition.md), [The Correlator](../guide/correlator.md), [Delay-Doppler Search](../guide/delay-doppler.md), [DSSS Burst Acquisition](../guide/dsss-acquisition.md), [Guides](../guide/index.md), [Checkpoint & Resume](../guide/state-serialization.md), [Waveforms — what you can generate](../guide/wfmgen/waveforms.md)
**Design** — [Design — pure-functional acquisition kernel (elastic fleet)](../design/acq-fn.md), [The acquisition surface: the DSSS search engine](../design/acq-surface.md), [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [AsyncDsssReceiver and AsyncDsssPool — continuous DSSS, from spec to object](../design/async-dsss-receiver.md), [`BurstCapture`: acquisition's output, turned into bursts](../design/burst-capture.md), [Corr2D batches: many maps in one call](../design/corr2d-batch.md), [Corr2D hypothesis bank: operation order](../design/corr2d-hypotheses.md), [Corr2D: decoupled (interpolated) inverse length](../design/corr2d-interpolated-inverse.md), [One Python class: the step-4 design](../design/correlator-python.md), [One correlator: the step-3 design](../design/correlator.md), [Detection Sizing — the four laws behind one prefix](../design/detection.md), [`DsssBurstReceiver`: the burst chain, composed in C](../design/dsss-burst-receiver.md), [State Serialization — the standard bytes interface](../design/state-serialization.md)
**Contributing** — [DSSS Primary Use Cases for Code Acquisition Design](../dev/contributing/dsss-use-cases.md)

<!-- related-pages:end -->
