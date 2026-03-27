# Continuously Variable Resampler

- Any rate change: from imperceptible to several orders of magnitude
- 32-bit resolution


# Architecture

## Interpolator  (r = Fout/Fin ≥ 1, output-driven)

- NCO frequency = 1/r (overflows once per input sample consumed)
- Every output tick: select polyphase branch from NCO phase,
  dot-product with delay line, emit one output sample
- On NCO overflow: push next input sample into delay line

```
                                  ┌────────────────────────────┐
                                  │   Polyphase filter bank    │
                                  │                            │
                    *** phase ────|────────► h[0..N-1]         │
                                  └─────────────┬──────────────┘
                                                │ h
                                                ▼
x[n] ──► push ──► [ delay line, N taps ] ──dot(ptr, h)──► y[k]
           ▲                                    │
           │                                    │
           └──── on overflow ◄── NCO (freq=1/r) ┘
                                  │
                                  └──► phase ***
```

**Per output tick:**
1. Advance NCO → `(phase, overflow)`
2. Look up branch: `h = bank[phase >> (32 − log₂L)]`
3. Compute: `y[k] = Σ delay[j] · h[j]`,  j = 0 … N−1
4. Emit `y[k]`
5. If overflow: `delay.push(x[next])`; advance input pointer


## Decimator  (r = Fout/Fin < 1, input-driven, transposed form)

- NCO frequency = r (overflows once per output sample emitted)
- Every input tick: scalar x[n] × all N branch coefficients,
  products accumulate in N integrate-and-dump (I&D) registers
- On NCO overflow: dump all N I&D values into a transposed tapped
  delay line, shift the line, emit one output sample, reset I&D

```
     ┌────────────────────────────┐
     │   Polyphase filter bank    │
     │                            │
     │  phase ──► h[0..N-1]       │
     └─────────────┬──────────────┘
                   │ h (reversed, scaled by r)
                   ▼
         x[n] ──► (×) ──► [ N integrate-and-dump registers ]
                   │                     │
                   │            on overflow: dump + reset
                   │                     │
                   │                     ▼
                   │       d[N-1]     d[N-2]       d[1]       d[0]
                   │         │  ┌───┐   │           |   ┌───┐   │
                   │         └─►│ T ├─►(+)─► ··· ─►(+)─►│ T ├─►(+)──► y[k]
                   │            └───┘                   └───┘
                   │
              NCO (freq=r) ──► phase
```

**Per input tick:**
1. Advance NCO → `(phase, overflow)`
2. Look up branch: `h = bank[phase >> (32 − log₂L)]`
3. Accumulate: `iad[j] += x[n] · h[j]`,  j = 0 … N−1
4. If overflow:
   - `d = iad;  iad = 0`  (dump and reset)
   - Shift transposed delay line and emit:
     ```
     y[k]     = d[0] + s[0]
     s[0]     = d[1] + s[1]
       ⋮
     s[N−3]   = d[N−2] + s[N−2]
     s[N−2]   = d[N−1]
     ```
   - Emit `y[k]`

# Testing

- Python reference implementation made with native components
- Reference validation
    - Passband flatness
    - Stopband / alias attenuation


## Interpolator Test Procedure

- Purpose: validate passband flatness and image / artifact suppression.
- Rate change = Fout/Fin = r = 2.0333
- Method: two complex tones at 0.1·Fin and 0.4·Fin, both of which
  should appear in the output unmolested
- Measure
    - Frequency of each tone: f/r ± measurement error
    - Amplitude: ±0.1 dB
    - Relative level of largest non-tone peak: ≤ −60 dBc


## Decimator Test Procedure

- Purpose: validate passband flatness and alias rejection.
- Rate change = Fout/Fin = r = 0.50333
- Method: two complex tones at 0.4·Fout and 0.6·Fout — tone 1 lands
  in the passband; tone 2 is above Nyquist_out and must be rejected
  by the anti-alias filter before it can fold back
- Measure
    - Frequency of tone 1: 0.4 ± measurement error
    - Amplitude of tone 1: ±0.1 dB
    - Relative level of largest non-tone peak: ≤ −60 dBc


# Performance Optimizations

## Current numbers (cf32, AVX-512, -march=native)

| filter       | rate   | mode   | MSa/s |
|--------------|--------|--------|-------|
| 80 dB, L=4096| 4.0×   | interp |  48.5 |
| 80 dB, L=4096| 0.25×  | decim  | 145.4 |
| 65 dB, L=1024| 4.0×   | interp |  47.8 |
| 65 dB, L=1024| 0.25×  | decim  | 126.2 |

The 80 dB filter's coefficient table is 304 KB — spills past L2.
Branch strides at rate=0.25× jump 1024 rows per input sample,
guaranteeing L2/L3 misses on every iad_madd.  The 65 dB table
(76 KB) fits in L2 but not L1, so it's slower at low rates where
branch locality is bad.  Getting the table into L1 is the dominant
leverage point.


## Idea 1 — DPMFS: Dual Phase Modified Farrow Structure

Reference: M. T. Hunter and W. B. Mikhael, "A Novel Farrow
Structure with Reduced Complexity," ICASSP 2009.

Instead of a large phase table, compute coefficients on-the-fly
as polynomials in the fractional phase.  The target structure is
the **DPMFS** (J=2, M=3), which combines two prior improvements:

- **MFS** (Modified Farrow): exploits linear phase symmetry →
  ~½ the multiplications of standard Farrow
- **GFS** (Generalized Farrow): uses J-fold oversampling to
  reduce polynomial order for a given frequency response

The DPMFS unifies both.  J=2 splits the fractional interval μ
into a 1-bit coarse select and a fine residual:

```
j   = ⌊2μ⌋        ∈ {0, 1}   — selects polyphase branch
μ_J = 2μ − j      ∈ [0, 1)   — fine fractional offset
```

Each of the two subfilters c_m(p, j) is odd-length and symmetric
(linear phase), so each requires only ~½ the multiplications of a
non-symmetric filter.  Both savings compound.

### Complexity comparison (70 dB, 0.3/0.7 transition — Table 2)

| Structure        | J | M | Multipliers | Coeff storage |
|------------------|---|---|-------------|---------------|
| Standard FS      | 1 | 3 | ~44         | ~44 floats    |
| MFS              | 1 | 4 |  32         |  28 floats    |
| GFS              | 8 | 2 |  32         | 240 floats    |
| **DPMFS**        | 2 | 3 | **27**      |  46 floats    |

DPMFS beats every prior structure: **~39% fewer multiplications
than naive cubic Farrow**, 15% fewer than MFS or GFS, and
coefficient storage of 46 floats = 184 bytes — a few cache lines,
fully L1-resident.

### Per output sample (runtime)

1. NCO gives raw phase word → μ = phase / 2³²
2. j = top bit of fractional phase (1 shift); μ_J = 2μ − j (1
   MAC)
3. Horner evaluation: 3 MACs (M=3 polynomial in μ_J)
4. Dot product with symmetric subfilter c_m(p, j): ~P/2
   multiplications + P additions (symmetry folds the tap pairs)
5. Emit output sample

### Design tool

`doppler.polyphase` builds the Kaiser prototype.  A new
`to_dpmfs_coeffs()` method would:
1. Design optimal symmetric PBF with J=2 and odd length
2. Polyphase decompose: c_m(p, j) = d_m(2p + j)
3. Return two coefficient arrays (j=0, j=1) ready for the C
   runtime


## Idea 2 — Vectorize across output samples (interpolator)

For rate ≈ N (near-integer), consecutive outputs share the same
delay-line window — the window only advances on NCO overflow, once
every ~N outputs.  Those N outputs are fully independent (different
branch / μ, same signal data).

Current: N sequential dot products, each reloading the delay line.
Proposed: load delay window once, compute N dot products in parallel.

```
delay window (19 taps, 76 bytes — stays in registers):
  output k+0: dot(window, h(μ₀))   ─┐
  output k+1: dot(window, h(μ₁))    ├─ all independent
  output k+2: dot(window, h(μ₂))    │
  output k+3: dot(window, h(μ₃))   ─┘
```

With Farrow coefficients the μᵢ are computed from the NCO phase
and the h vectors are tiny — fully register-resident.  This turns
the interpolator hot path into a pure FMA chain with no memory
traffic between outputs.


## Idea 3 — Coefficient producer / FIFO pipeline

A producer thread precomputes coefficient vectors (advancing the NCO,
evaluating polynomials) and writes them into a lock-free ring buffer.
The filter thread reads coefficient vectors and does dot products.

**When this wins:** only if coefficient generation is the bottleneck
AND it can fully overlap with the filter computation.

**For our case:** with Farrow order-3, coefficient generation costs
~3N FLOPs ≈ 57 FLOPs per output.  AVX-512 executes that in ~4
cycles — far below thread synchronization overhead (~100–1000
cycles).  The bottleneck is the dot product, not the lookup.

**More useful form:** double-buffer the *input samples*.  One thread
reads from DMA / network / file into a ring buffer; the filter thread
reads from it.  This hides I/O latency without touching the inner
loop.  Standard producer/consumer on the sample stream, not the
coefficient stream.

**Verdict:** coefficient FIFO — no; input sample double-buffer — yes,
for real streaming pipelines.


## Idea 4 — Halfband cascade pre-decimation

For large decimation ratios, prepend one or more halfband stages
before the polyphase resampler.  A halfband filter:
- Cuts at fs/4 (passes 0–fs/4, rejects fs/4–fs/2)
- Exactly half its taps are zero — costs ≈ N/2 MACs/sample
- Decimates by exactly 2×

Cost of three halfband stages to pre-decimate by 8× before a fine
polyphase stage operating on the residual rate:

```
Stage 1: N/2 MACs at full rate  = N/2   MACs/input
Stage 2: N/2 MACs at fs/2       = N/4   MACs/input
Stage 3: N/2 MACs at fs/4       = N/8   MACs/input
Fine polyphase (on decimated stream): much shorter filter needed
```

The polyphase resampler then handles only the fractional part of the
rate (e.g., ÷1.0333 instead of ÷8.0333), which requires far fewer
taps for the same stopband attenuation.  Both the filter length and
the sample rate it operates at shrink simultaneously.

Best fit: integer-ratio coarse decimation followed by arbitrary-rate
fine resampling.  Halfband design belongs in `doppler.polyphase`.


## Idea 5 — Parallel phase computation / threading

The polyphase branches are independent, so the question is whether
computing them in parallel pays off.

**Per-output threading (inner loop):** for N=19 taps, one dot
product takes ~2 AVX-512 iterations ≈ a handful of cycles.  Thread
launch overhead is 100–10000× that.  Never worth it.

**Multi-output threading:** Ideas 1+2 above vectorize M outputs using
SIMD.  Using threads instead of SIMD lanes for those M outputs gives
no advantage — SIMD is zero-overhead, threads are not.

**Block-level threading:** split a large input block (e.g., 1M
samples) into B chunks, replicate the delay-line state at each
boundary using the tail of the previous chunk, process B chunks on B
cores.  Overhead is: B state copies + B thread launches + join.
Amortizes for blocks ≳ 100 K samples and B ≤ core count.

**Real-time SDR context:** single-core vectorized throughput is
already 50–150 MSa/s.  Typical SDR sample rates are 1–30 MSa/s.
We are already well ahead.  Threading adds latency and complexity
for a use case that doesn't need it yet.

**Verdict:** block-level threading is the right granularity if we
ever need it; leave the inner loop single-threaded.


## Priority order

1. **Farrow cubic (Idea 1)** — eliminates the cache-miss penalty
   that dominates at high phase counts; table shrinks from 304 KB
   to 304 *bytes*.
2. **Multi-output vectorization (Idea 2)** — once the table is
   register-resident, this removes the remaining memory traffic
   from the interpolator hot path.
3. **Halfband cascade (Idea 4)** — for large decimation ratios,
   reduces both filter length and sample rate simultaneously.
4. **Input sample double-buffer (Idea 3, revised)** — for streaming
   pipelines where I/O and compute should overlap.
5. **Block threading (Idea 5)** — only if we ever saturate a single
   core, which current benchmarks suggest won't happen at SDR rates.
