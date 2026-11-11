---
description: "one class, `Correlator`, for every FFT correlation: maps, delay-Doppler maps, streaming detectors and the continuous and burst searches (acquisition)"
---

# The Correlator

`doppler.correlation.Correlator` is the library's one FFT correlator. It
compares what you received against a reference you know, at every lag at
once, and tells you where the reference sits in the input and how strongly.
That one job comes in seven shapes: a correlation map, a 2-D map, a
delay-Doppler map, a streaming detector over either map, and the two
searches a receiver runs before anything tracks a signal (also called
**acquisition** when it is part of a detector or receiver).

There is one class for all seven. You do not pick a class; you describe the
job with the constructor's knobs, and the knobs pick the **flavor**. The
`flavor` property then tells you which one you built.

## Which knobs pick which flavor

| `framing`      | `ref`     | `doppler_rows` | `detect` | `flavor`          | the one call      |
| -------------- | --------- | -------------- | -------- | ----------------- | ----------------- |
| `"none"`       | 1-D       | 0              | 0        | `"1d"`            | `execute(frame)`  |
| `"none"`       | 1-D       | 0              | 1        | `"detector_1d"`   | `push(chunk)`     |
| `"none"`       | 2-D       | 0              | 0        | `"2d"`            | `execute(frame)`  |
| `"none"`       | 2-D       | 0              | 1        | `"detector_2d"`   | `push(chunk)`     |
| `"none"`       | 1-D       | > 0            | 0        | `"delay_doppler"` | `execute(stream)` |
| `"continuous"` | 1-D chips | -              | implied  | `"continuous"`    | `push(chunk)`     |
| `"burst"`      | 1-D       | -              | implied  | `"burst"`         | `push(chunk)`     |

`ref` is the reference **as samples**, complex64. A 2-D `ref` of shape
`(1, n)` is the 1-D flavor. The continuous search takes its code as chips,
NRZ (`0 → +1`, `1 → -1`), one sample per chip; anything not exactly `±1`
is refused.

Every knob after `ref` is a keyword. The ones a flavor reads, and their
defaults, are on the [API page](../api/python-correlation.md#doppler.correlation.Correlator).

## A map: `execute`

A map flavor correlates a whole frame and returns the correlation at every
lag. With `dwell > 1` it integrates that many frames coherently before it
returns anything (`None` in between), which is how a weak signal comes up
out of the noise. The examples on this page are regions of
`src/doppler/examples/corr_demo.py`, which runs in CI; first, its setup:

```python
--8<-- "src/doppler/examples/corr_demo.py:setup"
```

The 1-D map, one frame against eight integrated:

```python
--8<-- "src/doppler/examples/corr_demo.py:integrate"
```

A 2-D `ref` builds the 2-D map, which finds a `(row, col)` shift in one
FFT2. The output is flat, row-major:

```python
--8<-- "src/doppler/examples/corr_demo.py:match2d"
```

The delay-Doppler map is a 1-D code with `doppler_rows` set: the input is
that many code periods back to back, and the map's rows are Doppler bins.
It has a guide of its own, [Delay-Doppler Search](delay-doppler.md), which
also covers its batch methods and the carrier-hypothesis bank.

## A detector: `push`

`detect=1` turns a map into a streaming detector. Push any number of
samples; it frames them, integrates `dwell` frames, divides the strongest
cell by a noise estimate taken from the bins `noise_lo … noise_hi`
(aggregated by `noise_mode`), and reports a detection when that ratio, the
test statistic, clears `threshold` (`0.0` reports every dump). The 1-D
detector:

```python
--8<-- "src/doppler/examples/corr_demo.py:detect"
```

`push` returns a NumPy structured array, one record per detection. The
fields are named, so read them by name: `row` and `col` are the cell (on
the 1-D detector `col` is the lag and `row` is 0), then `peak_mag`,
`noise_est` and `test_stat`. The 2-D detector is the same call:

```python
--8<-- "src/doppler/examples/corr_demo.py:detect2d"
```

The record array is sized for the whole chunk you pushed, so a push never
drops a detection however many frames it completes.

## A search: `push`, with the grid sized for you

The two search flavors are detectors that also search Doppler, and size the
search themselves from what you tell them about the signal: its C/N0
(`cn0_dbhz`), the Doppler span to cover (`doppler_uncertainty`), and the
error rates you will accept (`pfa`, `pd`). If that cannot be met they still
build, warn `UserWarning`, and set `underpowered`.

**The continuous search** finds a code that repeats forever. It needs the
`sample_rate` you feed it at and the `chip_rate`:

```python
import numpy as np

from doppler.acquire import bin_to_signed
from doppler.correlation import Correlator
from doppler.wfm import PN, mls_poly

code = np.asarray(
    PN(poly=mls_poly(7), seed=1, length=7).generate(127), np.uint8
)
ref = np.where(code & 1, -1, 1).astype(np.complex64)  # chips as NRZ
fs, chip_rate = 2.0e6, 1.0e6
acq = Correlator(
    ref,
    framing="continuous",
    sample_rate=fs,
    chip_rate=chip_rate,
    cn0_dbhz=60.0,
    doppler_uncertainty=2000.0,
)
assert acq.flavor == "continuous"

# The code, 2 samples a chip, 30 chips late and 1 kHz off, in noise at
# 60 dB-Hz.
rng = np.random.default_rng(1)
n = 40 * 254
x = np.roll(np.tile(np.repeat(ref, 2), 40), 60)
x = x * np.exp(2j * np.pi * 1000.0 * np.arange(n) / fs)
sigma = np.sqrt(fs / 10 ** (60.0 / 10) / 2)
x = x + sigma * (rng.standard_normal(n) + 1j * rng.standard_normal(n))

hits = acq.push(x.astype(np.complex64))
hit = hits[0]
doppler_hz = bin_to_signed(int(hit["row"]), acq.doppler_bins)
doppler_hz *= acq.doppler_res_hz
assert abs(int(hit["col"]) - 60) <= 1  # the delay, in samples
assert abs(hit["chip_phase"] - (127 - 30)) <= 1.0  # the Dll seed, in chips
assert abs(doppler_hz - 1000.0) <= acq.doppler_res_hz / 2
```

**The burst search** finds a burst of any repeated preamble, given as
samples: `reps` is how many repetitions the burst holds and `sample_rate`
their rate. [Acquiring a Repeated Preamble](acquisition.md) walks it through
on a Zadoff-Chu preamble, and
[DSSS Burst Acquisition](dsss-acquisition.md) on a PN code, including how
`BurstCapture` turns its detections into bursts.

A search's records carry more than a detector's: `cn0_dbhz_est`,
`samples_consumed` (where in the stream the hit was decided),
`chip_phase` (the seed for a code-tracking loop), `doppler_rate_hz_s` and
`test_stat_peak`. On a map or detector those fields read 0.

## What a flavor does not do is refused

The constructor never ignores a knob. A value the flavor would not honor,
set away from its default, is refused with a `ValueError`, and so is a
combination the table above does not list: `doppler_rows` with `detect`,
`ny_out` on a 1-D `ref`, a 2-D `ref` with `framing="burst"`.

The message names the knob:

```pycon
>>> import numpy as np
>>> from doppler.correlation import Correlator
>>> ref = np.ones(8, np.complex64)
>>> try:
...     Correlator(ref, framing="burst", doppler_rows=4)
... except ValueError as e:
...     print(e)
Correlator: a search sizes its own Doppler grid; doppler_rows needs framing='none'
```

The same holds for members. One class carries every flavor's methods and
properties, but a property the flavor does not serve raises
`AttributeError`, so `hasattr` reads `False`, instead of returning a
number that looks like a measurement:

```pycon
>>> c = Correlator(ref)
>>> c.flavor
'1d'
>>> hasattr(c, "eta"), hasattr(c, "nx_out")
(False, True)
```

A method the flavor does not serve raises `ValueError`, before it writes
anything; a served one that has nothing yet returns 0:

```pycon
>>> try:
...     c.surface(out=np.zeros(64, np.float32))
... except ValueError as e:
...     print(e)
surface: only the continuous search (framing='continuous') has it; Correlator.flavor says which this is
```

Ask `flavor` before reading a member when the object came from somewhere
else; the [API page](../api/python-correlation.md#doppler.correlation.Correlator)
names the flavors that serve each member.

## From C

A C caller usually names the flavor it wants and calls that flavor's
constructor (`dp_correlator_create_burst()` and its siblings). The class's
own entry point, `dp_correlator_create()`, takes every knob and a trailing
`const char **why`: on a refusal it returns NULL and points `why` at the
sentence Python raises.

```c
#include "doppler/correlator/correlator_core.h"
#include <stdint.h>
#include <stdio.h>

int main (void)
{
  float _Complex ref[4] = { 1, 0, 0, 0 };
  const char *why = NULL;
  dp_correlator_state_t *c = dp_correlator_create (
      ref, 1, 4, DP_CORRELATOR_FRAMING_NONE, 0, 1, 1, 0, 0, -1, 0.0f, 0,
      SIZE_MAX, DET_NOISE_MEAN, 0.0, 0.0, 0.0, 1, DP_CORRELATOR_CN0_NONE,
      0.0, 1e-3, 0.9, 0.0, 1, 1, 1, &why);
  if (!c || dp_correlator_flavor (c) != DP_CORRELATOR_DETECTOR_1D)
    return 1;
  dp_correlator_destroy (c);
  c = dp_correlator_create (
      ref, 1, 4, DP_CORRELATOR_FRAMING_BURST, 0, 0, 1, 0, 0, -1, 0.0f, 0,
      SIZE_MAX, DET_NOISE_MEAN, 0.0, 0.0, 0.0, 1, DP_CORRELATOR_CN0_NONE,
      0.0, 1e-3, 0.9, 0.0, 1, 1, 1, &why);
  printf ("%s\n", why);
  return c == NULL ? 0 : 1;
}
```

## State

Every flavor with running state (the dwell accumulator, a detector's carry,
a search's ring) serializes: `get_state()` returns bytes, and `set_state()`
on an object built the same way resumes it bit for bit. A blob from one
flavor is refused by another. See [Checkpoint & Resume](state-serialization.md).

## See also

- [Python: Correlation](../api/python-correlation.md) — every method and
    property, and which flavors serve it
- [Correlation and Detection](../gallery/corr.md) — the figure
    `corr_demo.py` draws
- [Delay-Doppler Search](delay-doppler.md),
    [Acquiring a Repeated Preamble](acquisition.md),
    [DSSS Burst Acquisition](dsss-acquisition.md)
- [One Python class: the step-4 design](../design/correlator-python.md) —
    why there is one class, and every ruling behind its knobs
