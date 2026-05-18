# C API Reference

The entire doppler DSP library is implemented in C99. Every Python and Rust
binding is a thin wrapper over this ABI — one implementation, tested once,
callable from any language.

Include the umbrella header for most use cases:

```c
#include <doppler.h>
```

For streaming-only consumers:

```c
#include <stream/stream.h>
```

---

## DSP modules

| Module | Header | Description |
|--------|--------|-------------|
| NCO | [`nco/nco_core.h`](nco__core_8h.md) | Raw uint32 phase accumulator |
| LO | [`lo/lo_core.h`](lo__core_8h.md) | IQ phasor generator (2¹⁶-entry LUT) |
| FIR filter | [`fir/fir_core.h`](fir__core_8h.md) | Complex FIR; CI8/CI16/CF32 inputs; AVX-512 |
| FFT | [`fft/fft_core.h`](fft__core_8h.md) | 1-D FFT; pocketfft or FFTW backend |
| FFT 2D | [`fft2d/fft2d_core.h`](fft2d__core_8h.md) | 2-D FFT |
| DDC | [`ddc/ddc_core.h`](ddc__core_8h.md) | Digital down-converter (complex and real input) |
| Resampler | [`Resampler/Resampler_core.h`](Resampler__core_8h.md) | Polyphase resampler (4096-phase × 19-tap Kaiser) |
| Halfband decimator | [`HalfbandDecimator/HalfbandDecimator_core.h`](HalfbandDecimator__core_8h.md) | Fixed 2:1 halfband decimation |
| Ring buffer | [`buffer/buffer.h`](buffer_8h.md) | Double-mapped ring buffers, lock-free SPSC |
| Delay line | [`delay/delay_core.h`](delay__core_8h.md) | Circular delay buffer |
| Accumulator (F32) | [`acc_f32/acc_f32_core.h`](acc__f32__core_8h.md) | Running sum / dot product |
| Accumulator (CF64) | [`acc_cf64/acc_cf64_core.h`](acc__cf64__core_8h.md) | Complex accumulator for I&D |

## Streaming

| Topic | Header | Description |
|-------|--------|-------------|
| Wire protocol | [`stream/stream.h`](stream_8h.md) | `dp_header_t`, magic, PUB/SUB, PUSH/PULL, REQ/REP |

## Reference pages

| Page | Contents |
|------|----------|
| [Modules](modules.md) | All Doxygen module groups |
| [Functions](functions.md) | Complete function index |
| [Types](annotated.md) | All structs and typedefs |
| [Files](files.md) | Per-header documentation |
| [Type groups](group__types.md) | `dp_cf32_t`, `dp_cf64_t`, sample-type enums |
| [Error codes](group__errors.md) | Return values and `dp_err_t` |
