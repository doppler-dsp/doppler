# Digital Down-Converter

A DDC shifts a signal from a carrier frequency to DC and optionally
decimates it.  The doppler C library provides `dp_ddc_t` plus the
building blocks to assemble custom pipelines.  This page documents
the practical architectures, the trade-offs between them, and
measured throughput so you can pick the right one.

---

## Signal chain overview

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
60 dB rejection).  A halfband stage can be inserted before or after to
trade filter cost against DPMFS work.

---

## Architecture A — Plain DDC (default)

```
CF32 in ──► NCO ──► DPMFS (0.4/0.6, M=3 N=19, rate r) ──► CF32 out
```

`dp_ddc_create(norm_freq, num_in, rate)` with default coefficients.
No design step required.  One allocation, no intermediate buffers.

**Best for:** prototype, any decimation rate, single-stage simplicity.

---

## Architecture B — Halfband → DDC (complex input)

```
CF32 in ──► HB ÷2 ──► NCO ──► DPMFS (0.4/0.6, M=3 N=19, rate 2r) ──► CF32 out
```

The halfband (N=19, 60 dB) decimates by 2 first.  The DPMFS then
runs on half the samples at twice the effective rate (same filter
coefficients, same total output rate).

The halfband cost (~427 MSa/s for N=19) is far below the DPMFS cost,
so the pipeline time is dominated by DPMFS work on N/2 input samples.

**Best for:** complex IQ input, decimation ≥ 2×.  Dominant choice.

---

## Architecture C — DDC (wide-band) → Halfband

```
CF32 in ──► NCO ──► DPMFS (0.2/0.8, M=3 N=7, rate 2r) ──► HB ÷2 ──► CF32 out
```

A 0.2/0.8 DPMFS (3× wider transition band) needs only N=7 taps
instead of N=19.  But it runs at rate 2r — double the output — and
those extra samples must pass through the halfband.  The wider
intermediate buffer erases the tap-count savings in practice.

```
MAC analysis (per input sample):
  Architecture A  0.4/0.6 at r:     (M+1) × N × r   = 4 × 19 × r = 76·r
  Architecture C  0.2/0.8 at 2r:    (M+1) × N × 2r  = 4 ×  7 × 2r = 56·r
                                                                 ─ 26 % fewer MACs
  … but memory traffic for 2× intermediate buffer negates the saving.
```

**Best for:** not recommended over Architecture B for complex input.

---

## Architecture D — Real input: NCO → HB → DPMFS

```
Real in ──► NCO ──► HB ÷2 ──► DPMFS (0.4/0.6, M=3 N=19, rate 2r) ──► CF32 out
```

When the ADC produces real samples (not complex IQ), the DDC chain
is different in two important ways.

### NCO is 2× cheaper

| Input type | NCO multiply | MACs/sample |
|---|---|---|
| Complex IQ | (I+jQ)(cos+j·sin) | 4 mul + 2 add |
| **Real** | x·cos + j·x·sin | **2 mul** |

The NCO must come first to convert real → complex.

### HB does dual duty: decimation + image rejection

A real input signal has conjugate-symmetric spectrum: X(−f) = X\*(f).
After NCO mixing at frequency f_n, the desired signal lands at DC but
a mirror image appears at −2f_n.  The halfband's stopband
(≥ 3·fs/8 for a standard 60 dB Kaiser design) naturally covers this
image whenever f_n > fs/8, which is almost always satisfied.
No separate image-rejection filter is needed.

For complex IQ input there is no real-input image — HB is only
decimating.  For real input it is decimating *and* filtering, which
means **the stage always earns its keep regardless of decimation rate**.

### Additional 2× saving in the HB (not yet implemented)

After the NCO, the I and Q channels are independently real:

```
I[n] = x[n] · cos(2πf_n·n)
Q[n] = x[n] · sin(2πf_n·n)
```

Applying the complex HB to this pair is identical to running two
separate real halfbands.  A real halfband costs N/4 MACs per output
sample (halfband symmetry reduces N taps to N/2 effective, pre-summing
reduces again to N/4).

```
Complex HB on general IQ:          2 × (N/2) real MACs = N   MACs/output
Two real HBs on NCO(real) output:  2 × (N/4) real MACs = N/2 MACs/output
                                                         ─ 2× cheaper
```

This optimisation requires a `dp_hbdecim_r2cf32_t` variant (real-pair
input, complex output) that does not yet exist in the library.

---

## Architecture D2 — Real input: zero-multiply band capture + fine NCO

```
Real in ──► Modified HB (fs/4 shift embedded) ──► Fine NCO (at fs/2) ──► DPMFS ──► CF32 out
              zero extra multiplications              arbitrary carrier tune
              entire [0, fs/4] band preserved         1 MAC per original input sample
```

This is the optimal architecture for **any** real ADC input, not a
special case.  It beats Architecture D in two places simultaneously and
requires no fixed IF.

### Step 1 — Band capture: real → complex via modified halfband

Mixing by fs/4 then decimating by 2 is a **lossless real-to-complex
conversion**.  A real signal at sample rate fs has its unique content in
`[0, fs/2]`; the fs/4 shift maps `[0, fs/4]` to `[−fs/4, +fs/4]`, and
the halfband passes the whole thing.  No information is discarded —
every carrier in `[0, fs/4]` is present in the complex output at fs/2.

The fs/4 mix multiplies by:

```
e^{−j·(π/2)·n}  =  { 1,  −j,  −1,  +j,  1,  −j,  −1,  +j, … }
```

No multiplications — only sign negations.

### Embedding in the halfband (no extra coefficient loads)

The combined mix-then-halfband output at decimation index m:

```
z[m] = Σ_k  h[k] · x[2m−k] · e^{−j·(π/2)·(2m−k)}

      = e^{−jπm} · Σ_k  [h[k] · e^{j·(π/2)·k}]  · x[2m−k]
            ↑                       ↑
      output correction        g[k] = h[k] · e^{j·(π/2)·k}
      (±1 alternating, free)   precomputed at construction
```

The per-tap rotation applied to the halfband polyphase branches:

| Branch | Taps | Rotation | Result |
|---|---|---|---|
| FIR branch | even k = 0, 2, 4, … | `{+1, −1, +1, −1, …}` | sign-flip every other tap, encoded once |
| Delay branch | odd k = 1, 3, 5, … | `{+j, −j, +j, …}` | pure imaginary → **Q channel** |

The delay branch has only one non-zero tap (centre, gain 0.5) by the
halfband property.  So Q is a single delayed real sample — one multiply.
I is the FIR branch with alternating-sign coefficients — N/4 real MACs.

**No extra coefficient loads at runtime.**  The sign flips are baked into
`h_fir_modified[]` at construction.  The output correction e^{−jπm} = (−1)^m
is one sign flip per output — free on any SIMD unit.

### Step 2 — Fine NCO at fs/2 (arbitrary carrier)

The complex output of the modified halfband contains the full `[0, fs/4]`
band centered around DC.  A conventional NCO running at **fs/2** shifts
any carrier within that band to DC:

```
fine_freq = carrier_freq / (fs/2)     ← normalised to output rate
```

This NCO is complex-input/complex-output (4 MACs + 2 adds per sample),
but it runs at **fs/2**, costing the equivalent of **2 MACs per original
input sample** — the same as Architecture D's real-input NCO but at half
the rate, so **1 MAC/original sample** effective.

### Cost comparison: D vs D2

Per original input sample (N=19 halfband FIR branch):

| Stage | Arch D | Arch D2 |
|---|---|---|
| Full-rate NCO | 2 MACs (real→complex) | — |
| Halfband | N/2 MACs (complex HB on NCO output) | N/4 MACs (real modified HB) |
| Fine NCO at fs/2 | — | 2 MACs × ½ = **1 MAC** |
| **Total** | **2 + N/2 ≈ 11.5** | **N/4 + 1 ≈ 5.75** |

Architecture D2 is approximately **2× cheaper** than Architecture D for
real input, regardless of carrier frequency or decimation rate.

### What a `dp_hbdecim_r2cf32_t` would do

```
Construction:
  h_fir_modified[k] = h_fir[k] · (−1)^k      ← sign-flip every other tap

Per output sample:
  I[m] = Σ_k  h_fir_modified[k] · x[2m − 2k]   ← N/4 real MACs
  Q[m] = 0.5 · x[2m − centre_offset]            ← 1 multiply
  apply (−1)^m to both I and Q                  ← 1 sign flip each

Output: CF32 at fs/2, full [0, fs/4] band centered near DC
```

Followed by a fine NCO (at fs/2) then DPMFS for remaining decimation.

---

## Architecture E — Coarse/fine NCO split (high decimation)

```
Real/CF32 in ──► Complex DPMFS (coarse NCO baked in) ──► Fine NCO ──► CF32 out
                   h_rot[k] = h[k]·e^{-j2πf_coarse·k}   at fs_out
```

### The idea

A full-rate NCO multiplies every input sample.  For high decimation rates,
most of that work is discarded by the filter.  The coarse/fine split moves
almost all of the NCO work to the output rate, which can be 100× lower.

**Derivation.**  Polyphase DDC with decimation D and prototype taps h[k]:

```
y[m] = Σ_k  h[k] · x[mD-k] · e^{j2πf_c(mD-k)}

      = e^{j2πf_c·mD}  ·  Σ_k  [h[k]·e^{-j2πf_c·k}]  ·  x[mD-k]
           ↑                              ↑
     fine NCO (once per output)     h_rot: complex filter taps
     runs at fs_out = r·fs_in             precomputed, static between retunes
```

The prototype filter h[k] is rotated tap-by-tap by e^{-j2πf_c·k} to produce
complex coefficients h_rot[k].  The per-output phase correction
e^{j2πf_c·mD} is a trivial NCO at the **output** sample rate — 100× slower
than the input rate for 100× decimation.

For the coarse/fine split, let f_c = f_coarse + f_fine:

- **f_coarse** is quantized to a grid (e.g. a multiple of fs_out, or any
  value that changes only on channel reassignment).  The rotated filter
  h_rot is recomputed once per retune.
- **f_fine** is the residual — a full-precision NCO at fs_out correcting
  the quantisation error or tracking a slowly drifting carrier.

The DPMFS representation handles this directly: rotate the prototype h[k]
before calling `fit_dpmfs`.  The polynomial fit naturally absorbs the
complex rotation; the resulting c0/c1 arrays are complex float32.

### Cost analysis

Per input sample, real input, HB → DPMFS pipeline:

```
Architecture B (HB → NCO@fs_in/2 → real DPMFS):
  NCO cost:     4 MACs/sample × N_in/2  =  2 · N_in  MACs
  DPMFS cost:   76 MACs/output × N_out  = 76 · N_in · r  MACs
  Total:        2 + 76r  MACs/input

Architecture E (HB → complex DPMFS, fine NCO@fs_out):
  DPMFS cost:   152 MACs/output × N_out = 152 · N_in · r  MACs
  Fine NCO:     ~6 MACs/output          ≈  6 · N_in · r  MACs
  Total:        158r  MACs/input
```

Break-even:  **2 + 76·r = 158·r  →  r ≈ 1/38  →  D ≈ 38×**

| Decimation | Arch B | Arch E | Δ |
|---:|---:|---:|---:|
| 4× | 21.0 | 39.5 | −88% (B wins) |
| 10× | 9.6 | 15.8 | −65% (B wins) |
| 38× | 4.0 | 4.2 | ≈break-even |
| 50× | 3.5 | 3.2 | +10% (E wins) |
| **100×** | **2.8** | **1.6** | **+43% (E wins)** |

### Retune cost

Retuning means recomputing h_rot[k] = h[k]·e^{-j2πf_coarse·k} and
re-fitting c0/c1.  For M=3 N=19 this touches (M+1)×N = 76 complex
multiplies + the polynomial fit — a one-time cost amortised over every
block until the next retune.

For continuous fine tuning (AFC, Doppler tracking) use f_coarse as a
coarse channel assignment that rarely changes, and let f_fine absorb
all rapid variation at output rate.

### Implementation status

Requires a complex-coefficient variant of the DPMFS resampler.  The
existing `dp_resamp_dpmfs_create` takes `const float *c0, *c1`; a
`dp_resamp_dpmfs_cf32_create` taking `const dp_cf32_t *c0, *c1` would
complete this architecture.  The design tooling (Python `fit_dpmfs`) works
on complex-valued banks today — the C runtime is the missing piece.

---

## Performance (Release build, x86-64, WSL2)

### Complex IQ input — Architecture A vs B vs C

Block = 65536 samples · 200 iterations · M=3 N=19 (A, B) or N=7 (C).

| Total rate | Label | (A) Plain DDC | (B) HB→DDC | (C) DDC→HB |
|---|---|---:|---:|---:|
| 0.50 | 2× decim | 61 MSa/s | **335 MSa/s** | 70 MSa/s |
| 0.25 | 4× decim | 70 MSa/s | **76 MSa/s** | 62 MSa/s |
| 0.125 | 8× decim | 71 MSa/s | **102 MSa/s** | 66 MSa/s |
| 0.10 | 10× decim | 72 MSa/s | **97 MSa/s** | 74 MSa/s |
| 0.01 | 100× decim | 85 MSa/s | **116 MSa/s** | 80 MSa/s |

Architecture B wins at every rate.  The 2× case is dramatic: after the
halfband, the DDC degenerates to pure NCO bypass on N/2 samples, which
is essentially free.  Architecture C is slower than plain DDC for most
rates because the 2× intermediate buffer increases memory traffic more
than the reduced tap count saves.

`-march=native` gives negligible improvement — DPMFS is memory-latency
bound, not FLOP-bound.

### NCO-only (bypass, no resampler)

| Config | MSa/s |
|---|---|
| Complex NCO (complex IQ input) | ~1215 |
| Real NCO (real input, 2× cheaper) | ~2400 (projected) |

The NCO multiply alone runs at ~1.2 GSa/s for complex input; real input
halves the multiply count so expect ~2.4 GSa/s.

---

## Decision guide

```
Is your input real (single ADC channel)?
  YES ─► Architecture D2: modified HB + fine NCO@fs/2 + DPMFS
  │        ~2× cheaper than Arch D at any carrier, any decimation rate
  │        (zero-multiply band capture; fine NCO at half-rate)
  │        └─ Decimation > 38× after the HB?
  │              ─► Architecture E variant: embed fine NCO into complex DPMFS
  │                   fine NCO at fs_out                 (+43% at 100×)
  │
  NO (complex IQ)
  │
  ├─ Total decimation = 1× (bypass)
  │     ─► Architecture A without resampler  (plain NCO mix)
  │
  ├─ Total decimation 2× – 38×
  │     ─► Architecture B: HB → DDC          (dominant choice)
  │
  └─ Total decimation > 38×
        ─► Architecture E: complex DPMFS (coarse NCO embedded),
             fine NCO at fs_out           (+43% at 100× vs B)
```

---

## Code examples

### Architecture A — one call

```c
dp_ddc_t *ddc = dp_ddc_create(-0.1f, 4096, 0.25);  // 4× decim, shift 0.1 to DC

dp_cf32_t out[dp_ddc_max_out(ddc)];
size_t n = dp_ddc_execute(ddc, in, 4096, out, dp_ddc_max_out(ddc));

dp_ddc_destroy(ddc);
```

### Architecture B — halfband then DDC

```c
/* HB: 60 dB Kaiser, N=19 FIR branch — design with
 *   doppler.polyphase.kaiser_prototype(phases=2)            */
dp_hbdecim_cf32_t *hb  = dp_hbdecim_cf32_create(N_hb, h_fir);
dp_ddc_t          *ddc = dp_ddc_create(norm_freq, num_in / 2, rate * 2.0);

dp_cf32_t mid[num_in / 2 + N_hb + 2];
dp_cf32_t out[dp_ddc_max_out(ddc)];

size_t n_mid = dp_hbdecim_cf32_execute(hb, in, num_in, mid, sizeof mid / sizeof mid[0]);
size_t n_out = dp_ddc_execute(ddc, mid, n_mid, out, dp_ddc_max_out(ddc));

dp_hbdecim_cf32_destroy(hb);
dp_ddc_destroy(ddc);
```

### Architecture D — real input

```c
/* NCO converts real → complex at full rate.
 * dp_nco_execute_r2cf32 not yet available — use execute_cf32
 * with the real part only (Q input = 0) as an interim.      */
dp_nco_t          *nco = dp_nco_create(norm_freq);
dp_hbdecim_cf32_t *hb  = dp_hbdecim_cf32_create(N_hb, h_fir);
dp_ddc_t          *ddc = dp_ddc_create(0.0f, num_in / 2, rate * 2.0);
  /* norm_freq=0: NCO already applied; DDC used for DPMFS only */
```

!!! note "Real-input NCO"
    A dedicated `dp_nco_execute_r2cf32` (real → complex, 2 MACs/sample)
    and `dp_hbdecim_r2cf32_t` (real-pair HB, N/2 MACs/output) are
    planned.  Until then, load real samples into the `.i` field of
    `dp_cf32_t` and zero the `.q` field before passing to
    `dp_nco_execute_cf32`.
