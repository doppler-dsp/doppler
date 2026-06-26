# DSSS acquisition: stateless, parallel, dynamics-capable

**Status:** draft / for discussion
**Scope:** the receive-side acquisition *architecture* — beyond today's
all-coherent streaming `Acquirer`. This is theory, trade-space, and a phased
roadmap; for how to *use* the current engine see the
[DSSS Burst Acquisition guide](../guide/dsss-acquisition.md).

______________________________________________________________________

## 1. Context

`doppler.dsss.Acquirer` (`native/src/acq/acq_core.c`) acquires a DSSS burst —
repeated BPSK PN epochs — under unknown code phase and Doppler. It frames the
stream into `(ny, nx)` (one PN epoch per row), FFTs down the columns for Doppler,
correlates each row against the code, and coherently integrates `dwell` frames
before a CFAR gate. It works, and it is the right tool for a static, low-Doppler
signal.

It also sits in **one corner** of the acquisition design space, with three gaps:

| Gap                  | Today                                                                                                                             | Why it matters                                                                                      |
| -------------------- | --------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| **Stateful**         | Owns a private ring, a `corr2d` coherent accumulator (`accum`/`count`), and a stream offset — none expressible as explicit carry. | Cannot fan out across processes/pods; the engine *is* the unit of parallelism, not the work.        |
| **All-coherent**     | The slow-time FFT and the `dwell` accumulate are both coherent. No magnitude (`non-coherent`) integration.                        | Coherent time is bounded (below); weak signals beyond that bound are simply unreachable.            |
| **Constant-Doppler** | The slow-time FFT assumes a linear phase ramp across segments.                                                                    | Platform **dynamics** (Doppler rate) make the phase quadratic → the FFT smears → acquisition fails. |

This document defines the problem and terms precisely, frames every acquisition
method as one **cross-ambiguity function (CAF)**, lays out the trade space, and
proposes a **stateless, parallel, dynamics-capable** architecture with a phased
build. **Large Doppler and dynamics are hard requirements.**

______________________________________________________________________

## 2. Glossary

| Term                      | Definition (doppler units)                                                                                        |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Chip / chip-rate `Rc`     | PN code element; `Rc` in chips/s.                                                                                 |
| Code / PN / `sf` = `L`    | Spreading code; spreading factor `sf` = code length `L` in chips.                                                 |
| Epoch `T_epoch`           | One code period = `L / Rc` s = `nx` samples.                                                                      |
| `spc` vs `sps`            | `spc` = samples per **chip** = `fs/Rc`; `sps` = samples per **symbol** (not in the acq grid). `nx = sf·spc`.      |
| Segment                   | One epoch of fast-time samples = one slow-time row.                                                               |
| Sub-block                 | An epoch chopped into `K` pieces; segment = `epoch/K`.                                                            |
| Code phase / delay        | Circular shift of the code = propagation delay; fast-time axis `0 … nx-1`.                                        |
| Doppler                   | Carrier frequency offset `f`; slow-time axis.                                                                     |
| Doppler rate              | `rdot` = d`f`/d`t` (Hz/s); quadratic carrier phase from acceleration.                                             |
| Code Doppler              | Chip-rate dilation `Rc·(1+v/c)`; code phase walks over long integration.                                          |
| Fast-time / slow-time     | Within an epoch (code phase) / across epochs (Doppler).                                                           |
| CAF / delay-Doppler map   | The cross-ambiguity surface over `(delay × Doppler [× rate])`.                                                    |
| Coherent / non-coherent   | Sum **complex** surfaces (gain `10·log10(M)`, phase-fragile) / sum squared **magnitude** (robust, squaring loss). |
| Dwell / look              | One coherent dump; a "look" is one such dump fed to non-coherent integration.                                     |
| `T_coh`                   | Coherent integration time = `ny · T_epoch`.                                                                       |
| Pfa / Pd                  | False-alarm / detection probability; Bonferroni `pfa_cell = 1-(1-pfa)^(1/N)`, `N = ny·nx`.                        |
| Processing gain           | Coherent `10·log10(M·N)`; non-coherent adds about `5·log10(N_nc)` in the weak limit.                              |
| Squaring / combining loss | Non-coherent's few-dB shortfall versus ideal coherent.                                                            |
| CFAR                      | Constant-false-alarm-rate gate; `test_stat = peak / noise_est`.                                                   |

______________________________________________________________________

## 3. The cross-ambiguity function — one surface

Every method here computes, or approximates, the same object: the **CAF**, a
correlation surface over delay (code phase) and Doppler, extended by a Doppler-
rate axis when there are dynamics.

```
                 Doppler  f
                   ▲
                   │        ┌───────────────┐
                   │       ╱               ╱│
        rate rdot  │      ╱   CAF tile    ╱ │   peak = (delay*, f*, rdot*)
            (depth) │     ╱  |R(τ, f)|²   ╱  │
                   │    └───────────────┘   │
                   │    │               │  ╱
                   └────┼───────────────┼─╱──────►  delay τ (code phase)
                        └───────────────┘
```

The methods differ only in **how they sweep Doppler** and **how they handle
non-constant phase** (dynamics). They are not competing algorithms; they are
decompositions of one surface, each efficient in a different regime.

______________________________________________________________________

## 4. Computational decompositions — placement verdicts

| Method                              | Wins when                                          | Doppler resolution | Stateless | Verdict                     |
| ----------------------------------- | -------------------------------------------------- | ------------------ | --------- | --------------------------- |
| **Mixer + slow-time FFT** (current) | Doppler within one slow-time Nyquist; low dynamics | `1/(ny·nx)` (fine) | after P0  | **IN — P0 coherent kernel** |
| **2-D spectral roll**               | single-epoch coarse sweep, no integration          | `1/nx` (coarse)    | yes       | **OUT — documented dual**   |
| **Sub-block chop**                  | wide Doppler with integration intact               | `1/(ny·nx)` (same) | yes       | **IN — P2 widener**         |
| **Direct mix-and-correlate**        | reference truth                                    | any                | yes       | **OUT — test ground truth** |
| **Doppler-rate de-chirp grid**      | acceleration / long `T_coh`                        | adds rate bins     | yes       | **IN — P3 dynamics axis**   |

**Mixer + slow-time FFT — the core.** Reframe `(ny, nx)`, FFT down the columns
for Doppler, correlate rows against the code, integrate `dwell` frames. It is the
most code-correlation-efficient method per Doppler bin: one correlation set yields
all `ny` Doppler bins via the slow-time FFT. The whole architecture generalizes
*around* this kernel.

**2-D spectral roll — OUT, but worth understanding.** Rolling `conj(FFT(code))`
by `k` integer bins and IFFT-ing against the received spectrum tests Doppler
hypothesis `k`. But rolling by `k` bins is *exactly* mixing by `k/nx` — the
integer-bin special case of the mixer. The mixer bank does everything the roll
does **plus** a finer (half-window) step **plus** the multi-epoch slow-time
integration the roll lacks; sub-block chopping widens the span the same way with
integration intact and at the same resolution. The roll is **strictly dominated**
on every axis except raw single-epoch simplicity, so it stays a documented dual,
not a kernel. (Revisit only if profiling ever shows a batched roll-IFFT beats the
per-channel mixer at very wide spans — a micro-optimization, not architecture.)

**Sub-block chopping — the wide-Doppler widener.** Chop each epoch into `K`
sub-blocks; the slow-time rate rises to `K/T_epoch`, so the native span widens to
`±K/(2·T_epoch)` (one long FFT of length `ny·K`) at the **same** resolution
`1/(ny·T_epoch)`, and the within-segment rotation limit loosens `K×`. The cost is
**partial-correlation loss** (each sub-block sees `1/K` of the code → weaker
autocorrelation and worse cross-correlation sidelobes) plus a larger FFT. For
moderate `K` (≤ 8) the loss is a few dB. It is the principled alternative to a
very wide mixer bank.

**Doppler-rate de-chirp — the dynamics axis.** Multiply by `exp(-jπ·rdot·t²)` per
rate hypothesis before the slow-time FFT to remove the quadratic phase. It wraps
the coherent kernel as an independent outer search axis; grid sizing falls out of
the `T_coh` budget (§8).

______________________________________________________________________

## 5. The trade space

`T_coh` (coherent integration time) is the master knob: it adds gain linearly
(`10·log10(M·N)`), but is **bounded** by four ceilings, whichever bites first —
(a) Doppler uncertainty (`f·T_coh < ~1/4` cycle, the within-segment sinc), (b)
data-bit period, (c) oscillator coherence, (d) **Doppler rate**
(`rdot·T_coh² < O(1)`). Pushed past the binding ceiling, coherent gain reverses
into loss. **Non-coherent integration** (`N_noncoh` magnitude-summed looks) picks
up there: robust to inter-look phase (Doppler walk, bit flips, oscillator drift),
gaining about `5·log10(N_nc)` in the weak limit — the **squaring/combining loss**
is the price of phase-robustness.

| Axis            | Raises                             | Costs                              | Coupled to                    | Parallel       |
| --------------- | ---------------------------------- | ---------------------------------- | ----------------------------- | -------------- |
| `T_coh` (`M`)   | gain `10·log10(M·N)`               | latency; bounded by f/bit/osc/rate | rate-grid (`∝T_coh²`), `N_nc` | within shard   |
| `N_noncoh`      | weak-signal reach                  | squaring loss (`~5·log10`)         | the `T_coh` ceiling           | tree-reduce    |
| Sub-block `K`   | Doppler span (`K×`), rate headroom | partial-corr loss, FFT `ny·K`      | within-segment limit          | within shard   |
| Coarse channels | Doppler span (tiling)              | linear compute                     | step / edge loss              | **shard axis** |
| Rate hypotheses | dynamics tolerance                 | linear compute                     | `1/T_coh²`                    | **shard axis** |
| `spc`           | code-phase resolution, anti-alias  | compute `∝nx`                      | `nx`, `fs`                    | —              |
| `Pfa↓ / Pd↑`    | confidence                         | larger `M·N_nc`                    | threshold `eta`               | —              |

**The coherent ↔ non-coherent split** is the central design choice: take `M`
(coherent depth) as large as the binding `T_coh` ceiling allows, then take `N_nc`
(looks) to close the remaining `Pd` gap. The detection module already has the
primitives — `det_threshold(pfa)` → `eta`, `det_pd(snr, dwell, eta)` (coherent,
order 1), and crucially `marcum_q(m, a, b)` with **arbitrary integer order `m`**,
which *is* the non-coherent detector for `m = N_nc` looks (`native/inc/detection/`).
The only missing piece is a packaged `det_pd_noncoherent(snr, n_coh, n_noncoh)`
(plus its dwell/look inverse) so the engine can **auto-split** `(M, N_nc)` from
`(Pfa, Pd, min_snr)` and the ceilings.

______________________________________________________________________

## 6. Stateless kernel + carry contract

The smallest reusable unit is one coherent CAF **tile**:

> compute one coherent CAF surface for one `(coarse-Doppler, rate)` shard over one
> time-block of `ny` segments, given carry-in, returning the updated coherent
> accumulator and count as carry-out.

It is a **pure function**: no internal ring, no hidden accumulator, no stored
offset. Everything that survives a call is *carry*; everything immutable and
shared is *config*; everything per-shard and recomputable is *scratch*.

| Class                       | Contents                                                                                                                                               | Sharing                                                 |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------- |
| **Config** (immutable)      | code/`ref` replica, `ny`/`nx`/`sf`/`spc`/`K`, coarse + rate grids, `eta`, `M`, `N_nc`, CFAR mode/cells, **FFT plans** (pocketfft plans are read-only). | build once; broadcast to every shard/pod.               |
| **Scratch** (shard-private) | slow-time FFT buffers, `corr2d` work buffers, magnitude + noise scratch, de-chirp LUT.                                                                 | allocate per shard; cheap to rebuild; never serialized. |
| **Carry** (explicit in/out) | coherent accumulator + `count`; non-coherent surface + `look_count`; leftover samples + stream offset `n0`. **No pointers.**                           | passed every call; serialize for processes/pods.        |

Today's hidden state maps exactly: `corr2d.accum/count` → carry; the ring leftover

- head/tail → carry; the `corr2d`/`fft` work buffers → scratch; `ref`/plans/`eta`/
    `dwell` → config.

Proposed C shape (pure; all buffers caller-owned):

```
/* Layer A — pure coherent tile. Returns 1 on a coherent dump, else 0. */
int  acq_caf_tile(const acq_config_t *cfg, acq_scratch_t *scr,
                  const float complex *block,        /* ny*nx samples */
                  double f_coarse, double rate,      /* this shard */
                  float complex *accum, size_t *count,   /* CARRY in/out */
                  float complex *dump_out);              /* iff dump */

/* Layer B — non-coherent reduce (associative). */
void acq_nc_accumulate(const float *dump_mag2, size_t n,
                       float *nc_surface, size_t *look_count);
void acq_nc_merge(float *dst, const float *src, size_t n);  /* tree-reduce */
```

Peak + CFAR run **once, after** the reduce, reusing today's `_compute_stat` /
`_noise_estimate`.

**Carry representation — a flat POD, single source of truth.** The carry is plain
arrays + scalars behind a `dp_header_t`-style envelope (reuse the streaming wire
convention). Threads get a zero-copy **handle** that is just a pointer-view over
that POD (the `ddc_fn` precedent: opaque state across free functions, GIL released
via `nogil`); processes and pods serialize the **same bytes**. One representation,
two faces — no second format to maintain.

**`Acquirer` becomes a thin wrapper.** It keeps the ring (leftover + offset) and
the carry buffers; `acq_push` drains the ring into `ny·nx` blocks, calls
`acq_caf_tile` per shard, feeds dumps to `acq_nc_accumulate`, and gates after
`N_nc` looks. The hot math lives entirely in the pure kernel; the wrapper owns the
carry and the streaming glue. Today's behavior is the `N_nc = 1`, single-shard
special case — **backward compatible and bit-exact.**

______________________________________________________________________

## 7. Parallel fan-out

The shard axes are independent — embarrassingly parallel:

```
shards  =  coarse_Doppler bins  ×  rate hypotheses  ×  time blocks
                    │                    │                  │
                    └──── each: private scratch + a slice of carry ────┘
                                        │
              non-coherent |·|² accumulate  (associative + commutative)
                                        │
                              tree / hierarchical reduce
                                        │
                            peak + CFAR  (once, on the merged surface)
```

| Substrate      | Distribute                                     | Reduce                                            | Precedent                                                 |
| -------------- | ---------------------------------------------- | ------------------------------------------------- | --------------------------------------------------------- |
| **Threads**    | thread per shard; per-shard partial surfaces   | in-RAM `acq_nc_merge`; GIL released in the kernel | doppler thread-per-shard + `nogil`; `ddc_fn` (~5.4× on 8) |
| **Processes**  | fork shards; emit serialized partial surfaces  | parent merges deserialized PODs                   | flat-POD carry (§6)                                       |
| **Pods (k8s)** | distribute `(coarse, rate)` shards across pods | hierarchical merge over the wire envelope         | streaming transport seam                                  |

Orchestration is the **caller's job** (doppler has no internal thread pool;
`nthreads` is accepted-but-ignored because the FFT plans are single-threaded).
Start **Python-first** — thread-per-shard, mirroring the coarse-bank loop already
in the usage guide — and promote to Rust later (the FFI layer has no acq/detection
today; the C ABI is the parallel substrate regardless).

**Compute model** (1 Mcps, `L=1000`, `spc=2` → `nx=2000`, `ny=10`): a coherent
tile is a couple of `(ny×nx)` FFT pairs plus the code multiply — order a few
Mflop. A 10 ms dump across **400 coarse channels** is roughly a Gflop; real-time
single-rate is order `~100 Gflop/s`, well within a multicore CPU and trivially
shardable. Per-shard memory is tiny — the accumulator and non-coherent surfaces
are `ny·nx·4 B ≈ 80–160 kB` each, so thousands of shards fit in RAM.

______________________________________________________________________

## 8. Large Doppler + dynamics — worked case

Assume a LEO-like downlink: `Rc = 1 Mcps`, `L = 1000` (`T_epoch = 1 ms`),
`spc = 2` (`fs = 2 Msps`, `nx = 2000`), Doppler **±100 kHz**, Doppler rate
**±5 kHz/s**.

**Wide Doppler.** Native span is `±500 Hz`. Either a **coarse mixer bank**
(primary): 50%-overlap step `500 Hz` → **400 channels** for the 200 kHz range at
\<1 dB edge loss (or abutting `1 kHz` → 200 channels at ~4 dB edge); or
**sub-block** `K = 4` → span `±2 kHz` per long FFT (length `ny·K = 40`), cutting
the bank ~4× to **~100 channels** at a few-dB partial-correlation loss. Pick by
the loss budget.

**Coherent depth.** `ny = 10` → `T_coh = 10 ms`, resolution `100 Hz`.

**Doppler-rate grid (the dynamics sizing).** A residual rate `rdot` leaves an edge
phase `π·rdot·(T_coh/2)²`. Holding it under `1/4` cycle:

```
rdot_res ≤ 2 / T_coh²            (¼-cycle edge tolerance)
Δrdot    ≈ 4 / T_coh²            (worst residual = half a grid step)
R        = rate_span / Δrdot
```

| `ny` | `T_coh` | `Δrdot`   | `R` for ±5 kHz/s      |
| ---- | ------- | --------- | --------------------- |
| 10   | 10 ms   | 40 kHz/s  | **1** (dynamics free) |
| 50   | 50 ms   | 1.6 kHz/s | **~7**                |
| 100  | 100 ms  | 400 Hz/s  | **~25**               |

The key insight: at 10 ms coherent this case needs **no rate search** — the
dynamics are absorbed. It is *pushing `T_coh`* (for coherent dB on weak signals)
that forces a rate grid, and its size grows as `T_coh²`. So the auto-splitter
should cap `T_coh` at the rate ceiling and spend the rest on `N_nc`, unless the
SNR genuinely demands coherent dB only a rate-searched long `T_coh` can give. The
worst row above is `400 coarse × 25 rate ≈ 10 000` independent tiles per 100 ms —
squarely fan-out territory.

**Code Doppler** (chip-rate dilation) walks the code phase by `(v/c)·Rc·T_coh`
chips; for `T_coh ≤ 10 ms` at modest `v/c` this is sub-chip and tolerable. It
becomes a real axis (a code-NCO/resample stretch) only for long codes or very high
velocity — deferred (§11).

______________________________________________________________________

## 9. Honest tradeoffs

- **Statelessness vs the shipped API.** P0 moves the hot path into a pure kernel.
    We hold the `Acquirer` Python API and bit-exactness as a hard contract, so the
    refactor is invisible to users — at the cost of carrying the wrapper.
- **Squaring loss.** Non-coherent integration buys phase-robustness and weak-signal
    reach, but only `~5·log10(N_nc)` versus `10·log10` coherent. It is a fallback
    for when coherence runs out, not a free sensitivity dial.
- **Sub-block partial-correlation loss.** Wider native Doppler for fewer channels,
    paid in code-correlation gain and sidelobe level — a few dB for `K ≤ 8`.
- **Mixer-bank channel count.** Simple and lossless, but linear in span: ±100 kHz
    is hundreds of channels. The fan-out makes that cheap; the orchestration is the
    real work.

______________________________________________________________________

## 10. Phased roadmap

Priority: **sensitivity → span → dynamics.**

| Phase                                     | Deliverable                                                                                                                                               | Acceptance criteria                                                                                                                                                                                                            |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **P0 — stateless kernel + carry**         | Extract `acq_caf_tile` pure kernel + flat-POD carry (`accum`+`count`+leftover+`n0`); re-express `Acquirer` as a thin wrapper owning the carry.            | Refactored `Acquirer` is **bit-identical** to today on the existing C + Python tests; a **carry round-trip** (split a stream, serialize the carry, resume in a fresh kernel) yields identical hits; no ring inside the kernel. |
| **P1 — non-coherent + order-M detection** | `acq_nc_accumulate`/`acq_nc_merge`; `det_pd_noncoherent(snr, n_coh, n_noncoh)` + inverse in `doppler.detection`; auto-split `(M, N_nc)`.                  | The helper matches a Monte-Carlo `Q_{N_nc}` reference to \<1%; a weak-signal case unreachable coherent-only is now detected at the target `(Pfa, Pd)`; measured non-coherent gain is within the predicted `~5·log10(N_nc)`.    |
| **P2 — sub-block / wide Doppler**         | The `K` knob (segment = `epoch/K`, long `ny·K` FFT); document the roll as the integer-bin dual.                                                           | Span = `±K/(2·T_epoch)` on a swept-Doppler signal; resolution invariant in `K`; partial-correlation loss matches prediction; channel-count reduction vs the mixer bank demonstrated.                                           |
| **P3 — Doppler-rate search**              | A `rate` de-chirp axis in `acq_caf_tile`; grid auto-sizer `Δrdot = 4/T_coh²`.                                                                             | A chirped burst that smears at `rate = 0` is recovered with the grid; grid size matches the `T_coh²` table; `(coarse × rate)` shards are order-independent (any merge order → same surface).                                   |
| **P4 — orchestration**                    | Python thread-per-shard (GIL released in the kernel) + process/pod POD-carry fan-out with hierarchical `acq_nc_merge`; reuse the streaming wire envelope. | Thread scaling ≥ 4–5× on 8 (the `ddc_fn` precedent); the process/pod path reduces serialized partial surfaces to a result **bit-identical** to single-process; the compute model (§7) validated against measured throughput.   |
| *(P5 — later)*                            | Code-Doppler dilation (code-NCO stretch); Tong / sequential verification dwell (declare → confirm) to cut false alarms.                                   | —                                                                                                                                                                                                                              |

______________________________________________________________________

## 11. Open questions / risks

1. **POD-first carry as the single source of truth.** Recommended (handle = view
    over the POD), but it locks the carry to pointer-free arrays. Fine for
    acquisition; confirm before P0 freezes the layout.
1. **Orchestration layer.** Python-first (thread-per-shard, mirrors the guide's
    coarse-bank loop), Rust later — confirmed direction; no C-level scheduler now.
1. **Primary wide-Doppler method.** Mixer bank primary (in hand, lossless,
    linear); sub-block as a profile-driven P2 option — confirmed.
1. **`det_pd_noncoherent` home.** The `detection` module (it composes only
    `marcum_q`), not `acq` — recommended.
1. **Code-Doppler scope.** Deferred to P5; revisit if a target needs long codes or
    very high velocity within one coherent window.
1. **2-D roll re-litigation.** Verdict is OUT (dominated); reopen only if P2/P4
    profiling shows a batched roll-IFFT wins at very wide spans on some hardware.

______________________________________________________________________

## 12. See also

- [DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) — how to use today's
    `Acquirer` (parameters, streaming, the coarse-mix widening loop).
- [Python: Detection Statistics](../api/python-detection.md) — `det_threshold` /
    `det_pd` / `det_dwell` / `marcum_q` (the order-`M` primitive).
- [Gallery: 2-D Acquisition](../gallery/detection2d.md) — the `Detector2D`
    matched-filter surface the coherent kernel builds on.
- [Streaming roadmap](streaming-roadmap.md) — the transport seam the pod fan-out
    reuses.
