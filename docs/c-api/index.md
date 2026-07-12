# C API Reference

doppler exposes every algorithm through a plain C99 API — no C++, no
exceptions, no hidden allocations.  Each module follows the same
lifecycle pattern:

```c
dp_foo_t *h = dp_foo_create(/* params */);   /* heap-allocate state  */
dp_foo_execute(h, in, n, out, max_out);      /* process a block      */
dp_foo_reset(h);                             /* zero state in-place  */
dp_foo_destroy(h);                           /* free                 */
```

The master include is `doppler.h`; it pulls in every public header.
Individual headers live under `native/inc/<module>/`.

Looking for the Python API instead? See the
[**Python API Reference**](../api/index.md) — C symbol names don't map
1:1 to Python class names (e.g. `symsync_create`/`symsync_step` here vs.
`track.SymbolSync` there), so this page doesn't attempt to cross-link
by name; browse both independently.

---

## Browsing the API

- [**File list**](files.md) — every header with a one-line description
- [**Data structures**](annotated.md) — all `_state_t` structs
- [**Functions**](functions.md) — alphabetical index

---

## Key modules

| Header | What it does |
|---|---|
| `resample/resample_core.h` | `ciccompmf` — CIC droop compensator design |
| `cic/cic_core.h` | CIC decimation filter |
| `fir/fir_core.h` | Direct-form FIR (real and complex taps) |
| `nco/nco_core.h` | Numerically-controlled oscillator |
| `fft/fft_core.h` | Complex FFT (power-of-two, in-place) |
| `resamp/resamp_core.h` | Polyphase resampler |
| `hbdecim/hbdecim_core.h` | Halfband 2:1 decimator |
| `delay/delay_core.h` | Delay line (circular buffer, double-copy) |
| `agc/agc_core.h` | Log-domain automatic gain control |
