# ADC Quantisation — 3–8 Bits

![ADC quantisation demo](../assets/adc_demo.png)

## What you're seeing

A real sinusoid at **-10 dBFS** is passed through `doppler.cvt.ADC` at six
bit depths (3–8 bits) with no dithering. The decoded output is shown in
both time and frequency domains.

**Top — Time domain (3 cycles).** The float32 reference (grey) overlays the
decoded signal at each bit depth. The quantisation staircase is coarse and
clearly stepped at 3 bits; it becomes nearly indistinguishable from the
reference at 8 bits. Each additional bit halves the step size.

**Bottom — One-sided spectrum (Blackman-Harris, N = 8192).** The tone stays
fixed at −10 dBFS across all bit depths. The wideband quantisation noise
floor descends by ≈ 6 dB with each additional bit, following the theoretical
relationship:

```
noise floor ≈ −(6.02 × bits + 1.76) dBFS
```

| Bits | Theoretical noise floor | Theoretical SNR |
| ---- | ----------------------- | --------------- |
| 3    | −19.8 dBFS              | 19.8 dB         |
| 4    | −25.8 dBFS              | 25.8 dB         |
| 5    | −31.9 dBFS              | 31.9 dB         |
| 6    | −37.9 dBFS              | 37.9 dB         |
| 7    | −43.9 dBFS              | 43.9 dB         |
| 8    | −50.0 dBFS              | 50.0 dB         |

The dashed horizontal lines mark the theoretical noise floor for each bit
depth. At low bit depths (3–4 bits) harmonic spurs from deterministic
quantisation distortion are visible above the noise floor; they fade toward
the noise at 7–8 bits.

## The ADC object

`doppler.cvt.ADC` models a signed two's-complement ADC with configurable
full-scale reference and optional TPDF dither.

```python
--8<-- "src/doppler/examples/adc_demo.py:adc"
```

### Parameters

| Parameter   | Default | Description                                                                                                           |
| ----------- | ------- | --------------------------------------------------------------------------------------------------------------------- |
| `bits`      | `16`    | ADC resolution, 1–64 bits                                                                                             |
| `dbfs`      | `-10.0` | Full-scale input level in dBFS. A signal at this amplitude fills the converter — `scale = 2^(bits-1) × 10^(-dbfs/20)` |
| `dithering` | `0`     | `0` = off; non-zero = TPDF dither added before rounding                                                               |

### Properties

| Property   | Type    | Description                                                                   |
| ---------- | ------- | ----------------------------------------------------------------------------- |
| `.scale`   | `float` | Precomputed scale factor — divide `int64` output by this to reconstruct float |
| `.bits`    | `int`   | ADC resolution                                                                |
| `.clipped` | `bool`  | Sticky flag — set if any sample saturated since last `reset()`                |

```bash
python src/doppler/examples/adc_demo.py   # → adc_demo.png
```
