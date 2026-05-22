# Python Examples

## LO — complex phasor generator

```python
from doppler.source import LO
import numpy as np

# Free-running quarter-rate tone
lo = LO(0.25)
iq = lo.steps(8)
print(iq)
# [ 1.+0.j  0.+1.j -1.+0.j  0.-1.j  1.+0.j  0.+1.j -1.+0.j  0.-1.j]

# FM control port — per-sample frequency deviation
ctrl = (0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024))).astype(np.float32)
lo2 = LO(0.1)
iq = lo2.steps_ctrl(ctrl)

# Phase continuity: successive calls resume where the last left off
lo3 = LO(0.25)
a = lo3.steps(4)
b = lo3.steps(4)   # seamlessly continues from sample 4
```

## NCO — raw phase accumulator

```python
from doppler.source import NCO
import numpy as np

nco = NCO(0.25)

# Raw uint32 phase values
ph = nco.steps_u32(16)

# Overflow carry: 1 at each accumulator wrap (every 4 samples for 0.25)
carry = nco.steps_u32_ovf(16)
# carry is 1 at indices 3, 7, 11, 15

# Scaled into [0, nmax) — no division, fixed-point multiply
nco2 = NCO(0.25, nmax=1000)
scaled = nco2.steps_u32_scaled(16)   # values in [0, 1000)
```

---

## FFT

### Per-instance FFT (preferred API)

Each `FFT` object owns one plan. Multiple instances of different sizes
or signs coexist with no global state.

```python
from doppler.spectral import FFT, FFT2D
import numpy as np

# CF32 (~2× faster than CF64)
x32 = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
f = FFT(1024)
X32 = f.execute(x32)          # complex64 in → complex64 out
assert X32.dtype == np.complex64

# CF64
x64 = np.random.randn(1024) + 1j * np.random.randn(1024)
X64 = f.execute(x64)          # complex128 in → complex128 out

# In-place
f.execute_inplace(x32)
```

### Reusing a plan (repeated transforms)

```python
from doppler.spectral import FFT
import numpy as np

x = (np.random.randn(1024) + 1j * np.random.randn(1024)).astype(np.complex64)
f = FFT(1024, nthreads=4)     # plan once

for _ in range(1000):
    out = f.execute(x)        # out-of-place
    # f.execute_inplace(x)    # or in-place
```

### 2-D FFT

```python
from doppler.spectral import FFT2D
import numpy as np

x = (np.random.randn(64, 64) + 1j * np.random.randn(64, 64)).astype(np.complex64)
f2 = FFT2D(64, 64)
out = f2.execute(x)
```

---

## Ring buffers

High-performance double-mapped ring buffers for producer/consumer pipelines:

```python
from doppler import F32Buffer, F64Buffer, I16Buffer
import numpy as np
import threading

# F64 (double-complex) — minimum 256 samples (page-alignment constraint)
buf = F64Buffer(256)

def producer():
    data = np.ones(256, dtype=np.complex128)
    buf.write(data)   # non-blocking; returns False on overflow

def consumer():
    view = buf.wait(256)   # blocks until 256 samples available
    process(view)          # zero-copy view into buffer memory
    buf.consume(256)       # release for reuse

threading.Thread(target=consumer).start()
threading.Thread(target=producer).start()
```

| Type | NumPy dtype | Min samples | Notes |
| ---- | ----------- | ----------- | ----- |
| `F32Buffer` | `complex64` | 512 | float IQ pairs |
| `F64Buffer` | `complex128` | 256 | double IQ pairs |
| `I16Buffer` | `int16, shape=(n,2)` | 1024 | col 0 = I, col 1 = Q |

---

## Correlation and detection

### Corr / Corr2D / Detector / Detector2D

`Corr` cross-correlates a reference against a stream of CF32 frames and
coherently accumulates over `dwell` frames before dumping — trading
latency for SNR gain.  `Detector` wraps `Corr` and applies a CFAR
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

```python
from doppler.spectral import Corr, Corr2D, Detector, Detector2D
import numpy as np

rng = np.random.default_rng(42)
N, LAG, DWELL = 64, 17, 8

ref = rng.choice(np.array([-1., 1.], dtype=np.float32), size=N).astype(np.complex64)

# Coherent integration: dwell=8 lifts SNR by √8
with Corr(ref, dwell=DWELL) as c:
    for _ in range(DWELL - 1):
        c.execute(frame())        # accumulate
    output = np.abs(c.execute(frame()))   # dump on final frame

# Streaming detector with CFAR threshold
det = Detector(ref, dwell=DWELL, noise_lo=LAG+4, noise_hi=N-1, threshold=5.0)
for lag, row, col, test_stat in det.push(signal_block):
    print(f"detection at lag={lag}  stat={test_stat:.2f}")
```

Run the full demo:

```bash
python examples/python/corr_demo.py   # → corr_demo.png
```

---

### Detection theory curves

`doppler.detection` implements the closed-form Marcum Q functions used
to set thresholds and predict detection probability without running
Monte Carlo.  `det_pd` gives P_d for a given SNR and dwell;
`det_dwell` inverts it to find the minimum dwell that meets a P_d
target.

![Detection theory curves](../assets/detection_curves.png)

The right panel shows a key design insight: at P_fa = 1e-5 and P_d = 0.9,
every 3 dB of extra SNR roughly halves the required dwell (coherent
integration gain vs SNR).

```python
from doppler.detection import det_pd, det_dwell, det_threshold
import numpy as np

PFA = 1e-5
eta = det_threshold(PFA)   # CFAR threshold parameter

# Pd at 0 dB SNR after M coherent integrations
for M in [2, 5, 9, 18]:
    snr_amp = 1.0   # 0 dB amplitude SNR
    pd = det_pd(snr_amp, M, eta)
    print(f"dwell={M:2d}  Pd={pd:.3f}")

# Minimum dwell to reach Pd ≥ 0.9 at each SNR
for snr_db in [0, 3, 6, 10]:
    snr_amp = 10 ** (snr_db / 20)
    M = det_dwell(snr_amp, pd_target=0.9, pfa=PFA, max_dwell=64)
    print(f"SNR={snr_db:+d} dB  →  min dwell M={M}")
```

```text
SNR= +0 dB  →  min dwell M=18
SNR= +3 dB  →  min dwell M=9
SNR= +6 dB  →  min dwell M=5
SNR=+10 dB  →  min dwell M=2
```

Run the full plot:

```bash
python examples/python/detection_curves.py   # → detection_curves.png
```

---

### Monte Carlo vs Marcum Q theory

`detection_sim.py` validates both the envelope detector and power
detector against Marcum Q predictions using 30 000 independent trials
per SNR point.  The left column shows empirical survival functions
overlaid on the closed-form curves; the right column plots P_d vs SNR.
MC and theory agree to within statistical noise throughout.

![Monte Carlo vs theory](../assets/detection_sim.png)

The simulation samples the matched-filter output directly from its
theoretical distribution (Rician H1, Rayleigh H0) rather than running
the full FFT pipeline, which avoids the degeneracy that appears when a
single-bin tone is correlated against itself via FFT.

```bash
python examples/python/detection_sim.py   # → detection_sim.png  (~5 s)
```
