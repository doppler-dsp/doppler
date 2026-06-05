# Functional DDCR API — Real Passband to Baseband

![Functional DDCR spectral demo](../assets/ddc_fn_demo.png)

## What you're seeing

Three panels share the same x-axis — normalised frequency in cycles/sample
(−0.5 to +0.5). A red dashed marker labels the strongest spectral line in
each panel.

**Top panel — input.** 16384 samples of a **real** float32 signal: a tone at
`fn = 0.18` (relative to the input sample rate `fs_in`) plus broadband noise.
Because the input is real, the spectrum is symmetric — the tone shows at both
±0.18.

**Middle panel — baseband output.** The same signal run through `ddcr_execute`
with the NCO tuned so the carrier lands at **DC**. The output is **complex64**
and decimated 4× (`rate = 0.25`), so its x-axis is normalised to `fs_out`. The
tone sits at `fn = 0.000`.

**Bottom panel — retuned output.** The LO is moved 0.04 (in `fs_in`) below the
carrier via `ddcr_set_norm_freq`. The tone shifts off DC by
`0.04 / rate = 0.16` in `fs_out` units, landing at `fn = +0.160` — exactly as
predicted, and phase-continuously (no filter-history reset).

## The two faces of the DDC

`doppler.ddc` exposes the same C down-converter two ways:

| | Object API | Functional API (this demo) |
|---|---|---|
| Import | `from doppler.ddc import DDCR` | `from doppler.ddc import ddcr_create, …` |
| State | wrapped in a Python type | opaque **PyCapsule**, passed explicitly |
| Output | allocated per call | written into a **caller-owned** buffer |
| Use when | you want a simple object | you manage your own arrays / want zero
per-call allocation in a hot loop |

## How it works

A DDCR takes a real passband signal, mixes it with a fine NCO (running at
`fs_in / 2`), low-pass filters, and **decimates** to complex baseband. To park
a real tone at carrier `f_carrier` (normalised to `fs_in`) at DC:

```python
norm_freq = -(2 * f_carrier + 0.5)
```

The functional lifecycle is explicit — the capsule is yours to keep, reuse,
and release:

```python
import numpy as np
from doppler.ddc import (
    ddcr_create, ddcr_execute, ddcr_set_norm_freq,
    ddcr_reset, ddcr_destroy,
)

state = ddcr_create(norm_freq=-(2 * 0.18 + 0.5), rate=0.25)

out = np.empty(4096, dtype=np.complex64)        # reused every block
for block in stream:                            # block: ndarray[float32]
    y = ddcr_execute(state, block, out)         # zero-copy view out[:n_out]
    ...

ddcr_set_norm_freq(state, -(2 * 0.14 + 0.5))    # retune, phase-continuous
ddcr_reset(state)                               # zero all history
ddcr_destroy(state)                             # release C resources
```

After `ddcr_destroy`, any further call on the same capsule raises
`RuntimeError`; live views of earlier output stay valid because they reference
the caller's buffer, not the state.

## Run it

```sh
python examples/python/ddc_fn_demo.py
```

Source: [`examples/python/ddc_fn_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/examples/python/ddc_fn_demo.py)
