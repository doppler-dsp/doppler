# Corr / Corr2D / Detector / Detector2D

`Corr` cross-correlates a reference against a stream of CF32 frames and
coherently accumulates over `dwell` frames before dumping — trading
latency for SNR gain. `Detector` wraps `Corr` and applies a CFAR
threshold; `Corr2D` / `Detector2D` extend both to 2-D template matching.

The figure below shows all four classes in one run against a BPSK PN
reference at SNR ≈ −6 dB:

- **Left** — 8 coherent integrations push peak/mean from 4.0 → 7.0,
    pulling the lag-17 peak cleanly out of the noise floor.
- **Centre** — `Corr2D` finds the (row=3, col=5) shift of a 2-D template
    in a single call.
- **Right** — `Detector.push()` emits one test-stat per dwell; signal
    cycles sit well above the threshold=5 gate while noise-only cycles stay below.

![Corr / Corr2D / Detector demo](../assets/corr_demo.png)

## Corr — coherent integrate-and-dump

`execute()` accumulates frames and returns output on the `dwell`-th call;
all other calls return `None`. Use as a context manager to ensure cleanup:

```python
from doppler.spectral import Corr
import numpy as np

rng = np.random.default_rng(42)
N, LAG, DWELL = 64, 17, 8
SNR = 10 ** (-6 / 20)   # −6 dB amplitude SNR

ref = rng.choice(np.array([-1., 1.], dtype=np.float32), size=N).astype(np.complex64)

def frame():
    """One received frame: ref delayed by LAG chips + noise."""
    sig  = np.roll(ref, LAG) * np.float32(SNR)
    re   = rng.standard_normal(N).astype(np.float32)
    im   = rng.standard_normal(N).astype(np.float32)
    return sig + (re + 1j * im).astype(np.complex64) * np.float32(1 / np.sqrt(2))

with Corr(ref, dwell=DWELL) as c:
    output = None
    for _ in range(DWELL):
        output = c.execute(frame())   # None for first DWELL-1 calls, array on last

peak_lag = int(np.argmax(np.abs(output)))
print(f"peak at lag={peak_lag}  (true={LAG})")
```

```text
peak at lag=17  (true=17)
```

## Detector — streaming CFAR detector

`push(block)` accepts arbitrary-length blocks and yields
`(lag, peak_mag, noise_est, test_stat)` for each dwell that fires:

```python
from doppler.spectral import Detector
import numpy as np

rng = np.random.default_rng(42)
N, LAG, DWELL = 64, 17, 8
SNR = 10 ** (-6 / 20)

ref = rng.choice(np.array([-1., 1.], dtype=np.float32), size=N).astype(np.complex64)

def signal_block(n_dwells):
    frames = []
    for _ in range(n_dwells * DWELL):
        re = rng.standard_normal(N).astype(np.float32)
        im = rng.standard_normal(N).astype(np.float32)
        noise = (re + 1j * im).astype(np.complex64) * np.float32(1 / np.sqrt(2))
        frames.append(np.roll(ref, LAG) * np.float32(SNR) + noise)
    return np.concatenate(frames)

det = Detector(ref, dwell=DWELL, noise_lo=LAG + 4, noise_hi=N - 1, threshold=5.0)
for lag, peak_mag, noise_est, test_stat in det.push(signal_block(4)):
    print(f"detection  lag={lag}  stat={test_stat:.2f}")
```

```text
detection  lag=17  stat=6.58
detection  lag=17  stat=8.04
detection  lag=17  stat=7.11
detection  lag=17  stat=7.90
```

!!! note

    Actual `test_stat` values vary with the random seed; the lag is
    deterministic for this RNG seed.

## Corr2D — 2-D template matching

```python
from doppler.spectral import Corr2D
import numpy as np

rng = np.random.default_rng(0)
NY, NX = 8, 8
ROW_SHIFT, COL_SHIFT = 3, 5

ref2d = rng.standard_normal((NY, NX)).astype(np.complex64)

# Signal: ref2d shifted by (ROW_SHIFT, COL_SHIFT)
sig2d = np.roll(np.roll(ref2d, ROW_SHIFT, axis=0), COL_SHIFT, axis=1)

with Corr2D(ref2d, dwell=1) as c:
    out = c.execute(sig2d.ravel())

surf = np.abs(out).reshape(NY, NX)
peak_row, peak_col = np.unravel_index(surf.argmax(), (NY, NX))
print(f"peak at (row={peak_row}, col={peak_col})  (true=({ROW_SHIFT}, {COL_SHIFT}))")
```

```text
peak at (row=3, col=5)  (true=(3, 5))
```

Run the full demo:

```bash
python examples/python/corr_demo.py   # → corr_demo.png
```
