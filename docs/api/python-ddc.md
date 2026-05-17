# DDC

Digital Down-Converter — shifts a carrier to baseband and decimates in
one call, backed by `dp_ddc_t` and `dp_ddc_real_t`.

Source:
[`src/doppler/ddc/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/ddc/__init__.py)

---

## Which class to use

| Class | Input | Cost | Use when |
|-------|-------|------|----------|
| `Ddc` | CF32 IQ | baseline | complex ADC, already at fs |
| `DDCR` | float32 real | ~2× cheaper | real ADC, direct-sampling SDR |

Both produce CF32 IQ at the decimated output rate.

---

## `Ddc` — complex input

Chains: NCO frequency shift → DPMFS polyphase decimator.
Built-in M=3 N=19 Kaiser-DPMFS bank — no filter design required.

**Frequency convention:** `norm_freq = -f_tone` shifts a tone at
`f_tone` (normalised to fs) down to DC.

```python
from doppler.ddc import Ddc
import numpy as np

# Tune to a tone at +0.1·fs, decimate 4×
ddc = Ddc(norm_freq=-0.1, num_in=4096, rate=0.25)

x = np.random.randn(4096).astype(np.complex64)
y = ddc.execute(x)          # CF32 output, len(y) ≈ 4096 * 0.25 = 1024

print(f"in={len(x)}  out={len(y)}  max_out={ddc.max_out}")
```

### Retune without reset

```python
ddc.set_freq(-0.2)          # NCO retuned; delay-line history preserved
y = ddc.execute(next_block)
```

### Phase-continuous across blocks

```python
ddc = Ddc(-0.1, 4096, 0.25)
for block in iq_stream:     # generator of 4096-sample CF32 arrays
    out = ddc.execute(block)
    process(out)
```

---

## `DDCR` — real input (Architecture D2)

Chains: halfband 2:1 decimation with embedded −fs/4 shift (free) →
fine NCO at the fs/2 intermediate rate → DPMFS decimation.

~2× cheaper than `Ddc` for any real-ADC source because the halfband
operates at half the sample rate and the embedded mix costs zero extra
multiplications.

**Frequency convention:** `norm_freq = 2*f_tone + 0.5`
The +0.5 cancels the halfband's embedded −fs/4 shift.

```python
from doppler.ddc import DDCR
import numpy as np

# Tune to a tone at f_tone=0.1·fs; real ADC at 4096 samples/block
#   norm_freq = 2*0.1 + 0.5 = 0.7
ddc = DDCR(norm_freq=0.7, num_in=4096, rate=0.25)

x = np.random.randn(4096).astype(np.float32)   # real ADC samples
y = ddc.execute(x)          # CF32 output, len(y) ≈ 4096/2 * 0.25 = 512
```

### Output size

Both classes expose `max_out` (worst-case allocation) and `nout` (actual
count from the last call).

```python
buf = np.empty(ddc.max_out, dtype=np.complex64)
y = ddc.execute(x)
assert len(y) == ddc.nout
```

---

::: doppler.ddc.DDC

---

::: doppler.ddc.DDCR

---

## DDC Architecture

A DDC shifts a signal from a carrier frequency to DC and optionally
decimates it.  This section documents the practical architectures, the
trade-offs between them, and measured throughput so you can pick the
right one.

### Signal chain overview

```
                    ┌─────────────────────────────────────────────┐
  in (fs_in) ──────►│  NCO mix ──► [HB ÷2] ──► DPMFS resample   │──► out (fs_out)
                    └─────────────────────────────────────────────┘
```

Three stages, each optional or reorderable:

| Stage | C type | Purpose |
|---|---|---|
| NCO mix | `dp_nco_t` | Multiply by e^{j2πf_n·t} — shift carrier to DC |
| Halfband ÷2 | `dp_hbdecim_cf32_t` | Cheap factor-of-2 decimation |
| DPMFS resample | `dp_resamp_dpmfs_t` | Continuously-variable rate conversion |

The default `dp_ddc_create` chains NCO + DPMFS with built-in M=3 N=19
Kaiser-DPMFS coefficients (passband ≤ 0.4·fs_out, stopband ≥ 0.6·fs_out,
60 dB rejection).

---

### Architecture A — Plain DDC (default)

```
CF32 in ──► NCO ──► DPMFS (0.4/0.6, M=3 N=19, rate r) ──► CF32 out
```

`dp_ddc_create(norm_freq, num_in, rate)` with default coefficients.
No design step required.  One allocation, no intermediate buffers.

**Best for:** prototype, any decimation rate, single-stage simplicity.

---

### Architecture B — Halfband → DDC (complex input)

```
CF32 in ──► HB ÷2 ──► NCO ──► DPMFS (0.4/0.6, M=3 N=19, rate 2r) ──► CF32 out
```

The halfband (N=19, 60 dB) decimates by 2 first.  The DPMFS then runs
on half the samples.  Architecture B wins at every decimation rate.

**Best for:** complex IQ input, decimation ≥ 2×.  Dominant choice.

---

### Architecture D2 — Real input: zero-multiply band capture + fine NCO

```
Real in ──► Modified HB (fs/4 shift embedded) ──► Fine NCO (at fs/2) ──► DPMFS ──► CF32 out
              zero extra multiplications              arbitrary carrier tune
```

This is the optimal architecture for any real ADC input.  Mixing by
fs/4 then decimating by 2 is a lossless real-to-complex conversion —
the fs/4 mix multiplies by `{1, −j, −1, +j, …}` (sign negations only,
no multiplications) and is embedded into the halfband tap weights at
construction time.

**Fine NCO frequency convention:** `norm_freq = 2*f_tone + 0.5`
The +0.5 cancels the halfband's embedded −fs/4 shift.

**Cost vs Architecture D (NCO → complex HB → DPMFS):**

| Stage | Arch D | Arch D2 |
|---|---|---|
| Full-rate NCO | 2 MACs | — |
| Halfband | N/2 MACs (complex) | N/4 MACs (real modified) |
| Fine NCO at fs/2 | — | 1 MAC (effective) |
| **Total (N=19)** | **≈ 11.5 MACs** | **≈ 5.75 MACs** |

Architecture D2 is approximately **2× cheaper** than Architecture D for
real input at any carrier or decimation rate.  This is what `DDCR`
implements.

---

### Architecture E — Coarse/fine NCO split (high decimation)

For decimation > 38×, embedding the coarse NCO into the DPMFS filter
taps and running only a fine correction NCO at the output rate becomes
worthwhile.

**Break-even:** D ≈ 38× decimation.

| Decimation | Arch B MACs/input | Arch E MACs/input | Δ |
|---:|---:|---:|---:|
| 10× | 9.6 | 15.8 | B wins |
| 38× | 4.0 | 4.2 | break-even |
| 100× | 2.8 | 1.6 | **E +43%** |

Implementation requires a complex-coefficient DPMFS variant
(`dp_resamp_dpmfs_cf32_create`) — planned; not yet in the library.

---

### Performance (Release build, x86-64)

Block = 65536 samples × 200 iterations, M=3 N=19.

| Rate | Decimation | Arch A | Arch B | Arch C |
|---|---|---:|---:|---:|
| 0.50 | 2× | 61 MSa/s | **335 MSa/s** | 70 MSa/s |
| 0.25 | 4× | 70 MSa/s | **76 MSa/s** | 62 MSa/s |
| 0.10 | 10× | 72 MSa/s | **97 MSa/s** | 74 MSa/s |
| 0.01 | 100× | 85 MSa/s | **116 MSa/s** | 80 MSa/s |

Architecture B wins at every rate.

---

### Decision guide

```
Is your input real (single ADC channel)?
  YES ─► DDCR / Architecture D2
  │        ~2× cheaper at any carrier, any decimation rate
  │        └─ Decimation > 38× after the HB?
  │              ─► Architecture E (planned): embed NCO into DPMFS taps
  │
  NO (complex IQ)
  │
  ├─ Total decimation = 1× ─► Ddc without resampler (plain NCO mix)
  ├─ Total decimation 2× – 38× ─► Ddc / Architecture B (dominant)
  └─ Total decimation > 38× ─► Architecture E (planned)
```

---

### C code examples

#### Architecture A — one call

```c
dp_ddc_t *ddc = dp_ddc_create(-0.1f, 4096, 0.25);

dp_cf32_t out[dp_ddc_max_out(ddc)];
size_t n = dp_ddc_execute(ddc, in, 4096, out, dp_ddc_max_out(ddc));

dp_ddc_destroy(ddc);
```

#### Architecture B — halfband then DDC

```c
dp_hbdecim_cf32_t *hb  = dp_hbdecim_cf32_create(N_hb, h_fir);
dp_ddc_t          *ddc = dp_ddc_create(norm_freq, num_in / 2, rate * 2.0);

dp_cf32_t mid[num_in / 2 + N_hb + 2];
dp_cf32_t out[dp_ddc_max_out(ddc)];

size_t n_mid = dp_hbdecim_cf32_execute(hb, in, num_in, mid,
                                        sizeof mid / sizeof mid[0]);
size_t n_out = dp_ddc_execute(ddc, mid, n_mid, out, dp_ddc_max_out(ddc));

dp_hbdecim_cf32_destroy(hb);
dp_ddc_destroy(ddc);
```
