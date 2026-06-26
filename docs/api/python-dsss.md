# Python DSSS API

The `doppler.dsss` module provides the two halves of a DSSS receiver:
**`Acquisition`** — the streaming burst-acquisition engine that finds an unknown
code phase and Doppler — and **`Despreader`** — the tracking receiver that locks
and despreads the payload once acquired.

Source:
[`src/doppler/dsss/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/__init__.py)

See the [DSSS acquisition & despreading gallery page](../gallery/dsss-despread.md)
for the full acquire → track → despread chain with plots.

______________________________________________________________________

## `Acquisition` — streaming burst acquisition

`Acquisition` searches a streamed cf32 signal for a repeated BPSK PN burst over the
joint (Doppler × code-phase) grid, auto-configuring its CFAR threshold and
coherent dwell from a target `(pfa, pd, min_snr)` using `doppler.detection`. Push
arbitrary-length blocks; it yields one record per detection — `(doppler_bin, code_phase, peak_mag, noise_est, test_stat, snr_est)` — whose `(doppler_bin, code_phase)` seed the `Despreader`. See the
[DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) for the search-space
sizing and a worked example.

::: doppler.dsss.Acquisition

______________________________________________________________________

## `Despreader` — tracking receiver

Seeded with a coarse frequency and code-phase estimate (from the
`Corr2D`/`Detector2D` acquisition engine or `Acquisition`), the `Despreader` locks
the signal with a code-tracking **delay-locked loop** and a carrier-tracking
**Costas loop**, despreads the payload, and emits symbols.

______________________________________________________________________

## How it works

Every dimension is a run-time parameter — spreading code, spreading factor
(`sf`), samples-per-chip (`sps`), loop bandwidths. Per input sample the
despreader wipes the carrier (an inline NCO driven by the Costas loop), then
correlates against early / prompt / late replicas of the code. Once per code
period it dumps the three accumulators:

- the **prompt** accumulator is the despread symbol — its sign is the BPSK
    decision, its phase/magnitude the soft information;
- the **non-coherent early-minus-late** envelope drives the DLL
    (`track.LoopFilter`) → code phase/rate;
- the **decision-directed** product drives the Costas loop → carrier
    frequency/phase.

**Seeding from acquisition.** `init_norm_freq` is the carrier frequency in
cycles/sample and `init_chip_phase` the code phase in chips; the caller converts
the detector's `(Doppler bin, code-phase chip)` into those units (the bin→Hz map
depends on the search grid, so it stays application-side).

**Distinct acquisition vs data codes.** Real bursts use a long acquisition code
for the preamble and a different (often shorter) data code for the payload.
`set_acq(acq_code, acq_reps)` enables **preamble-aided pull-in** — track the
unmodulated, repeated acquisition preamble coherently (a full ±π discriminator,
so even a wide residual pulls in), then switch to the data code at the payload.
Omit it for payload-only operation (seeded from acquisition).

Tracking state is readable: `norm_freq` (carrier estimate), `code_phase`,
`lock_metric` (0–1), `snr_est`. The cf32 symbol output chains over the `stream`
module's `dp_header_t` framing like any other DSP block.

______________________________________________________________________

## Examples

### Despread a payload seeded from acquisition

```python
import numpy as np
from doppler.dsss import Despreader

# data_code: 0/1 spreading chips; seed from the acquisition peak
d = Despreader(data_code, sf=32, sps=2,
               init_norm_freq=acq_freq, init_chip_phase=acq_chip)
symbols = d.steps(rx)        # complex64 prompt symbols
bits    = d.bits(rx)         # or hard BPSK bits (0/1)
round(d.lock_metric, 2)      # ~1.0 once locked
```

### Preamble-aided pull-in with a distinct acquisition code

```python
d = Despreader(data_code, sf=32, sps=2)
d.set_acq(acq_code, acq_reps=5)   # 5-rep preamble pulls the loops in
symbols = d.steps(burst)          # preamble emits nothing; payload follows
```

______________________________________________________________________

::: doppler.dsss.Despreader
