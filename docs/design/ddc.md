---
description: "Why the down-converters are built the way they are: the signal chain, the candidate architectures and their measured cost, and the decision guide."
---

# DDC Architecture

The usage guide is [Down-Conversion](../guide/down-conversion.md).

A DDC shifts a signal from a carrier frequency to DC and optionally
decimates it. This section documents the practical architectures, the
trade-offs between them, and measured throughput so you can pick the
right one.

## Signal chain overview

```
                    ┌─────────────────────────────────────────────┐
  in (fs_in) ──────►│  LO mix ──► [HB ÷2] ──► polyphase resample │──► out (fs_out)
                    └─────────────────────────────────────────────┘
```

Three stages, each optional or reorderable:

| Stage              | C type            | Purpose                                        |
| ------------------ | ----------------- | ---------------------------------------------- |
| LO mix             | `dp_lo_state_t`   | Multiply by e^{j2πf_n·t} — shift carrier to DC |
| Halfband ÷2        | `hbdecim_state_t` | Cheap factor-of-2 decimation                   |
| Polyphase resample | `resamp_state_t`  | Continuously-variable rate conversion          |

`dp_ddc_create(norm_freq, rate)` chains the LO and a RateConverter with built-in
Kaiser coefficients (passband ≤ 0.4·fs_out, stopband ≥ 0.6·fs_out, 60 dB
rejection); `dp_ddc_create_matched(norm_freq, rate, pulse, …)` puts a
matched-filter bank on the terminal stage instead — see
[Matched mode](../guide/down-conversion.md#matched-mode).

## Architecture A — Plain DDC (default)

```
CF32 in ──► LO ──► RateConverter (0.4/0.6, rate r) ──► CF32 out
```

`dp_ddc_create(norm_freq, rate)` with built-in Kaiser bank. No design step
required. The RateConverter already picks a CIC + halfband + polyphase
cascade for large ratios, so this is the halfband-first structure below
whenever it is cheaper.

**Best for:** prototype, any decimation rate, single-stage simplicity.

## Architecture B — Halfband → DDC (complex input)

```
CF32 in ──► HB ÷2 ──► LO ──► polyphase resample (0.4/0.6, rate 2r) ──► CF32 out
```

The halfband (N=19, 60 dB) decimates by 2 first. The resampler then runs
on half the samples. Architecture B wins at every decimation rate.

**Best for:** complex IQ input, decimation ≥ 2×. Dominant choice.

## Architecture D2 — Real input: zero-multiply band capture + fine NCO

```
Real in ──► Modified HB (fs/4 shift embedded) ──► Fine LO (at fs/2) ──► resample ──► CF32 out
              zero extra multiplications              arbitrary carrier tune
```

This is the optimal architecture for any real ADC input. Mixing by
fs/4 then decimating by 2 is a lossless real-to-complex conversion —
the fs/4 mix multiplies by `{1, −j, −1, +j, …}` (sign negations only,
no multiplications) and is embedded into the halfband tap weights at
construction time.

**Fine NCO frequency convention:** `norm_freq = -(2*f_tone + 0.5)`
The 0.5 cancels the halfband's embedded fs/4 shift.

**Cost vs Architecture D (NCO → complex HB → polyphase resample):**

| Stage            | Arch D             | Arch D2                  |
| ---------------- | ------------------ | ------------------------ |
| Full-rate NCO    | 2 MACs             | —                        |
| Halfband         | N/2 MACs (complex) | N/4 MACs (real modified) |
| Fine NCO at fs/2 | —                  | 1 MAC (effective)        |
| **Total (N=19)** | **≈ 11.5 MACs**    | **≈ 5.75 MACs**          |

That is the MAC count; measured, the front end gains far less (1.04–1.40×,
see above), because multiplies are not what this path pays for. `Ddcr`
implements Architecture D2.

## Architecture E — Coarse/fine LO split (high decimation)

For decimation > 38×, embedding the coarse LO into the polyphase filter
taps and running only a fine correction LO at the output rate becomes
worthwhile.

**Break-even:** D ≈ 38× decimation.

| Decimation | Arch B MACs/input | Arch E MACs/input |          Δ |
| ---------: | ----------------: | ----------------: | ---------: |
|        10× |               9.6 |              15.8 |     B wins |
|        38× |               4.0 |               4.2 | break-even |
|       100× |               2.8 |               1.6 | **E +43%** |

Implementation requires a complex-coefficient polyphase variant — planned;
not yet in the library.

## Measured cost

Architecture B is the fastest of the three candidates at every rate measured,
from 2x to 100x decimation. The measurements, and the third candidate (C)
that B beats everywhere, are in the [DDC measurement record](ddc-measurements.md).

## Decision guide

```
Is your input real (single ADC channel)?
  YES ─► Ddcr / Architecture D2
  │        the real-input path (cheaper in a whole receiver, not ~2×)
  │        └─ Decimation > 38× after the HB?
  │              ─► Architecture E (planned): embed LO into polyphase taps
  │
  NO (complex IQ)
  │
  ├─ Total decimation = 1× ─► DDC without resampler (plain NCO mix)
  ├─ Total decimation 2× – 38× ─► DDC / Architecture B (dominant)
  └─ Total decimation > 38× ─► Architecture E (planned)
```

### Architecture A — one call

```c
#include <doppler/ddc/ddc_core.h>

int main(void)
{
  float _Complex in[4096]  = { 0 };   /* fill with your samples */
  float _Complex out[4096];

  dp_ddc_state_t *ddc = dp_ddc_create(-0.1, 0.25);
  size_t n = dp_ddc_execute(ddc, in, 4096, out, 4096);
  (void)n;
  dp_ddc_destroy(ddc);
  return 0;
}
```

### Architecture B — halfband then DDC

`dp_hbdecim_create` needs real Kaiser-designed halfband taps (see
`native/benchmarks/HalfbandDecimator/bench_HalfbandDecimator_core.c` for a
C composition — tap design is Python-side, via `kaiser_window`), so this
sketch omits that
step to keep the shape of the composition visible:

<!-- docs-snippet: skip=illustrative composition sketch; dp_hbdecim_create needs Kaiser-designed taps (Python-side), omitted here to keep the composition shape visible -->

```c
hbdecim_state_t *hb  = dp_hbdecim_create(num_taps, h);   /* h: Kaiser-designed taps */
dp_ddc_state_t     *ddc = dp_ddc_create(norm_freq, rate * 2.0);

float _Complex mid[num_in / 2 + 32];
float _Complex out[num_in];

size_t n_mid = dp_hbdecim_execute(hb, in, num_in, mid,
                               sizeof mid / sizeof mid[0]);
size_t n_out = dp_ddc_execute(ddc, mid, n_mid, out, num_in);

dp_hbdecim_destroy(hb);
dp_ddc_destroy(ddc);
```
