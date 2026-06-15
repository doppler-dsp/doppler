# Power Spectra & Measurements

doppler computes spectral metrics through **one pipeline**:

```
time data  →  averaging PSD (one core)  →  measurements
```

A single FFT-and-window-and-average implementation — the **`PSD`** estimator in
`doppler.spectral` — produces the power spectrum, and every measurement analyzer
in `doppler.measure` (`ToneMeasure`, `IMDMeasure`, `NPRMeasure`) consumes that
same averaged spectrum. There is no second copy of the window→FFT→power maths.

This guide is the usage walk-through. For the metric *equations* and IEEE Std
1241 conventions see the [Measurement Suite design guide](../design/measurement-suite.md);
for the full method/attribute reference see the
[spectral](../api/python-spectral.md) and [measure](../api/python-measure.md) API
pages.

!!! tip "The 30-second version"

    ```python
    import numpy as np
    from doppler.spectral import PSD
    from doppler.measure import ToneMeasure

    fs, n = 100e6, 1 << 14
    x = np.cos(2 * np.pi * 10.017e6 * np.arange(8 * n) / fs).astype(np.float32)

    # display spectrum: 8 averaged segments, one-sided, in dBFS
    w = PSD(n=n, fs=fs, window="kaiser", beta=12.0)
    w.accumulate_real(x)
    psd = w.psd_db()                       # averaged power spectrum, dB

    # ADC/tone metrics from the *same* averaged-spectrum engine
    r = ToneMeasure(n=n, fs=fs, beta=12.0).analyze(x)
    r.enob, r.sfdr_dbc, r.fund_dbfs        # named ToneMetrics result
    ```

______________________________________________________________________

## The averaging PSD — `PSD`

`PSD` is a stateful Welch-method (averaging) power-spectral-density estimator. It
owns a window, a zero-padded FFT and a per-bin averager; you feed it frames and
read the averaged spectrum.

### Create

```python
from doppler.spectral import PSD

w = PSD(
    n=4096,            # segment / window length (samples)
    fs=100e6,          # sample rate (Hz)
    window="kaiser",   # "hann" or "kaiser"
    beta=12.0,         # Kaiser shape (ignored for Hann)
    pad=2,             # zero-pad factor → nfft = next_pow2(n * pad)
    full_scale=1.0,    # amplitude that reads 0 dBFS in the dB getters
    bits=0,            # bits>0 sets full_scale = 2**(bits-1) (ADC dBFS)
    mode="mean",       # "mean" | "exp" | "maxhold" | "minhold"
    alpha=0.1,         # EMA factor (exp mode only)
)
w.n        # 4096   — segment length (sets the resolution bandwidth)
w.nfft     # 8192   — zero-padded transform length (sets the bin spacing)
```

- **`n`** is the *segment* length — it sets the resolution bandwidth
    (`rbw ≈ enbw · fs / n`). Longer segments resolve closer tones.
- **`pad`** zero-pads each segment to `nfft = next_pow2(n · pad)`. Padding
    interpolates the spectrum (finer bin spacing, smoother peaks) without
    improving resolution — it does **not** sharpen two close tones, it just draws
    the lobe with more points.
- **`full_scale`** / **`bits`** set the 0-dBFS reference for the **dB** getters.
    With the default `full_scale=1.0`, a unit-amplitude tone peaks at 0 dB. For an
    ADC, pass **`bits=B`** — it sets `full_scale = 2**(B-1)` so codes read
    directly in dBFS. `bits` is the single definition of that conversion: the
    measurement analyzers and [`Specan`](../api/python-analyzer.md) take the same
    `bits`/`full_scale` and read the reference back from this core.

### Feed data — real or complex, segmented and averaged

A capture longer than `n` is split into `floor(len / n)` non-overlapping
segments; each is windowed, (zero-padded,) FFT'd and folded into the running
average. Pass more data to average more segments — averaging trades frequency
snapshots for a smoother, lower-variance noise floor.

```python
import numpy as np

# complex baseband (cf32)
for frame in cf32_frames:           # each frame: any multiple of n samples
    w.accumulate(frame)

# real input (f32) — folded to a one-sided spectrum
w.accumulate_real(real_capture)     # averages floor(len/n) segments

w.count                             # number of segments folded in so far
w.reset()                           # discard the average; re-seed on next accumulate
```

### Read the spectrum

```python
# display spectra (DC-centred, in dBFS w.r.t. full_scale)
psd_db   = w.psd_db()               # averaged power spectrum, dB     (None if empty)
psd_dbhz = w.psd_dbhz()             # PSD, dB/Hz (ENBW / fs normalised)

# raw linear power (cg²-normalised; full_scale NOT applied)
two = w.power_twosided()            # length nfft, DC-centred
one = w.power_onesided()            # length nfft//2 + 1, folded to [0, fs/2]

# band / level statistics
per   = w.band_power(np.array([-2e5, -1e5, 1e5, 2e5]))   # dB per [lo,hi] band
total = w.total_band_power(np.array([-2e5, -1e5, 1e5, 2e5]))
obw   = w.occupied_bw(0.99)         # occupied bandwidth, Hz
nf    = w.noise_floor()             # median dB level
snr   = w.snr(-1e5, 1e5)            # peak-in-band minus noise floor, dB
sfdr  = w.sfdr(min_db=-120.0)       # spurious-free dynamic range, dB
```

All spectra are DC-centred (fftshift), matching `find_peaks_f32`'s
bin → frequency convention, so peaks compose directly:

```python
from doppler.spectral import find_peaks_f32
peaks = find_peaks_f32(w.psd_db(), n_peaks=5, min_db=-60.0)
```

!!! note "dB getters vs. linear accessors"

    `psd_db`/`psd_dbhz`/`band_power`/… apply `full_scale` (they read in dBFS).
    `power_onesided`/`power_twosided` return the **raw** coherent-gain-normalised
    linear power — they ignore `full_scale`. The measurement analyzers consume
    the linear accessors and apply their own references, which is why one `PSD`
    engine serves both the display and the metrics.

______________________________________________________________________

## The measurement suite

`ToneMeasure`, `IMDMeasure` and `NPRMeasure` are **single-shot in API** — you
call `analyze(capture)` once — but each one drives the same `PSD` engine
internally, so **they consume an averaged spectrum**. The capture you pass sets
how much averaging happens:

- `len(capture) == n` → one segment (a single periodogram).
- `len(capture) == k·n` → `k` averaged segments → a tighter noise floor and
    more stable SNR/SINAD/ENOB/NPR.

`n` is the segment length; pass a longer capture to buy averaging without
changing the resolution bandwidth.

### Single-tone ADC metrics — `ToneMeasure`

```python
import numpy as np
from doppler.measure import ToneMeasure

fs, n = 100e6, 1 << 14
# 8 segments worth of capture → 8-way averaged spectrum
x = np.cos(2 * np.pi * 10.017e6 * np.arange(8 * n) / fs).astype(np.float32)

m = ToneMeasure(window="kaiser", n=n, fs=fs, beta=12.0, n_harmonics=8)
r = m.analyze(x)                    # named ToneMetrics result
r.enob, r.sinad, r.sfdr_dbc, r.thd  # attribute access
r.fund_freq, r.fund_dbfs, r.worst_spur_freq

# the analyzer's own display spectrum (same window/nfft/average as the metrics):
spectrum = m.spectrum_dbfs(x)       # DC-centred dBFS, length nfft
```

For an ideal *B*-bit converter, characterise it by quantising through
[`doppler.cvt.ADC`](../gallery/cvt-quantization.md) and reading `enob ≈ B`:

```python
from doppler.cvt import ADC

bits = 12
adc = ADC(bits, 0.0, 0)                         # 0 dBFS at amplitude 1.0
codes = adc.steps((0.999 * np.sin(2 * np.pi * 1234.567 * np.arange(n) / n))
                  .astype(np.float32)).astype(np.float32)
r = ToneMeasure(n=n, beta=14.0, bits=bits).analyze(codes)   # bits sets dBFS
assert abs(r.enob - bits) < 0.3
```

### Two-tone intermodulation / TOI — `IMDMeasure`

```python
from doppler.measure import IMDMeasure

m = IMDMeasure(n=n, fs=fs, beta=12.0)
r = m.analyze(two_tone_capture)     # finds the two strongest tones automatically
r.imd3_dbc, r.imd2_dbc, r.toi_dbfs  # third/second-order products & intercept
r.f1, r.f2, r.imd3_lo_freq, r.imd3_hi_freq
```

### Notched-noise NPR — `NPRMeasure`

```python
from doppler.measure import NPRMeasure

m = NPRMeasure(n=n, fs=fs, beta=12.0, bits=bits)
# band/notch geometry (Hz) is an analyze() argument, so one estimator sweeps notches
r = m.analyze(codes, active_lo=1e6, active_hi=49e6,
              notch_lo=24e6, notch_hi=26e6, guard_hz=0.5e6)
r.npr_db, r.inband_psd_dbfs, r.notch_psd_dbfs
```

______________________________________________________________________

## Natural parameters — the `Specan`

`PSD` and the measurement analyzers speak **DSP** parameters — segment length
`n`, Kaiser `beta`, zero-pad. A spectrum-analyzer operator speaks **instrument**
parameters — **center, span, RBW, reference level**. `doppler.analyzer.Specan`
is the C-first bridge: it composes the [`DDC`](../api/python-ddc.md)
tuner/decimator and the same `PSD` PSD core, takes the instrument knobs, and
emits a ready-to-plot dB display band.

```python
import numpy as np
from doppler.analyzer import Specan
from doppler.spectral import find_peaks_f32

sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, center=0.0)
sa.n, sa.beta, sa.rbw          # the DSP grid it derived from span/rbw

for chunk in iq_stream:                       # any cf32 block size
    db = sa.execute(chunk.astype(np.complex64))
    if db is None:                            # not enough samples yet
        continue
    peaks = find_peaks_f32(db, 5, -60.0)      # bin i → center + (i−mid)·fs_out/nfft

sa.retune(50e3)                # seamless LO retune; no rebuild
```

The mapping is the one a spec-an makes for you: the **span** picks the
decimation rate (`fs_out = span·1.28`), the **RBW** picks the window length
(coarse) and the Kaiser `beta` (fine, solved so the window ENBW *is* the
requested RBW), and the display is cropped to the central ±span/2 band. So you
ask for "500 Hz RBW over a 200 kHz span" and never touch `beta` or `n`. This is
the same C object that backs the [`doppler.specan`](../specan/index.md) live
display — see the [analyzer API page](../api/python-analyzer.md) for the full
reference.

______________________________________________________________________

## Choosing parameters

| Goal                                       | Lever                                       |
| ------------------------------------------ | ------------------------------------------- |
| Resolve two close tones                    | larger **`n`** (finer `rbw`) — *not* `pad`  |
| Smoother lobes / sub-bin peak reads        | **`pad = 2`** (or more)                     |
| Lower-variance noise floor, stable SNR/NPR | feed **more segments** (`len = k·n`)        |
| Read ADC codes directly in dBFS            | **`bits = B`** (sets `full_scale=2**(B-1)`) |
| Lower sidelobes (catch small spurs)        | **`window="kaiser"`** with larger `beta`    |
| Peak-hold / min-hold display               | `mode="maxhold"` / `"minhold"`              |
| Drive by center / span / RBW, not `n`/beta | **`doppler.analyzer.Specan`** (above)       |

______________________________________________________________________

## See also

- [Python: spectral API](../api/python-spectral.md) — full `PSD` + helper reference
- [Python: measurement API](../api/python-measure.md) — `ToneMeasure` reference
- [Measurement Suite design guide](../design/measurement-suite.md) — metric equations
- [Gallery: Measurement Suite](../gallery/measure.md) and
    [IMD & NPR](../gallery/measure-imd-npr.md) — rendered end-to-end demos
