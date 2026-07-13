# Correlation and Detection

![Corr / Corr2D / CorrDetector demo](../assets/corr_demo.png)

## What you're seeing

**Left — `Corr` coherent integration.** BPSK PN reference, lag=17,
SNR ≈ −6 dB. With a single frame the peak/mean ratio is ~4.0 —
barely distinguishable from noise. After 8 coherent dwells
(`dwell=8`) it rises to ~7.0, pulling the lag-17 peak cleanly above
the noise floor. Coherent integration improves SNR by `10 log₁₀(M)`
dB.

**Centre — `Corr2D` 2-D template match.** An 8×8 complex template
shifted by (row=3, col=5) is recovered in a single FFT2 call. The
surface peak lands exactly on the injected shift.

**Right — `CorrDetector.push()` stream.** Four signal dwells fire above
`threshold=5`; noise-only dwells stay below it. Each dot is one
dwell's test statistic: peak magnitude divided by local noise
estimate.

## How it works

`execute()` accumulates frames and returns output only on the
`dwell`-th call; all other calls return `None`.

```python
--8<-- "src/doppler/examples/corr_demo.py:setup"

--8<-- "src/doppler/examples/corr_demo.py:integrate"
```

`CorrDetector` wraps this loop and applies a CFAR threshold so you get
`(lag, peak_mag, noise_est, test_stat)` tuples directly from
`det.push(block)` without managing the dwell counter yourself.

`Corr2D` accumulates the `dwell` integration in the **frequency domain** — it
sums the per-frame cross-spectra and inverts once on the dump rather than once
per frame. The result is identical by linearity of the inverse FFT, but the
single inverse amortizes over the dwell (~1.7× cheaper per frame at `dwell=8`,
`bench_corr2d.py`). This holds only for *coherent* (complex-sum) integration;
see the [2-D Acquisition gallery](detection2d.md) for the details.

### `CorrDetector` — streaming CFAR

`push(block)` accepts arbitrary-length blocks and yields
`(lag, peak_mag, noise_est, test_stat)` for each dwell that fires above the
threshold; the noise estimate is taken from lags `[noise_lo, noise_hi]`:

```python
# One coherent-dwell block of the same shifted-PN + noise frames.
block = np.concatenate([noisy_frame() for _ in range(DWELL)])

det = CorrDetector(ref1d, dwell=DWELL, noise_lo=LAG + 4, noise_hi=N - 1,
                   threshold=THRESHOLD)
for lag, peak_mag, noise_est, test_stat in det.push(block):
    print(f"detection  lag={lag}  stat={test_stat:.2f}")
# detection  lag=17  stat=7.34   (stat varies with noise; lag is
#                                 deterministic)
```

### `Corr2D` — standalone 2-D match

```python
--8<-- "src/doppler/examples/corr_demo.py:match2d"
```

`CorrDetector2D` wraps `Corr2D` with the same CFAR gating for a full 2-D
acquisition search — see the [2-D Acquisition gallery](detection2d.md).

```bash
python src/doppler/examples/corr_demo.py   # → corr_demo.png
```
