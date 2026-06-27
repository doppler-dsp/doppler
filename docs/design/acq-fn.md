# Design — pure-functional acquisition kernel (elastic fleet)

**Status:** model + naming locked; build sequenced below. Companion to
[`dsss-acquisition.md`](dsss-acquisition.md) (roadmap) and the
[acquisition guide](../guide/dsss-acquisition.md).

## Mental model — two kernels, two faces

```
source ─► ddc_fn (mix f_coarse [+decimate]) ─► acq_fn (acquire) ─► hits
```

Two streaming kernels chained; one `(ddc_fn, acq_fn)` pipeline per coarse Doppler
channel; channels are the unit of parallel work. **Every kernel is a pure
transducer** — `f(config, scratch, state_in, input) → (state_out, output)`,
nothing retained between calls. That purity is what makes the fleet **elastic**:
the shipped unit of work is `(descriptor, state_in, input_block)`, and *any* pod
can continue a channel from it — time-split, restart, rebalance, un-rewindable
live socket.

**Two faces over the one engine, both first-class:**

| face                                                              | what it is                                                                                | for                                                 |
| ----------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | --------------------------------------------------- |
| **OO object** (`Acquisition`, `Ddcr`)                             | stateful wrapper holding `(config, scratch, carry)`, threading it across `push`/`execute` | the simple single-stream case; unchanged public API |
| **pure kernel** (`acq_*`, `ddcr_*` tile fns + serializable state) | `f(config, scratch, state_in, input) → (state_out, hits)`                                 | the orchestrator, pods, Rust FFI — anything elastic |

The OO object is the pure kernel + an owned state blob. Keeping it is the point;
the pure kernel is **added underneath and exposed alongside**, never a
replacement.

## Three roles (so "knows nothing" ≠ "rebuild plans every call")

- **config** — immutable; built **once per pod** from a small **descriptor** (PN
    replica, FFT plans, thresholds, grid sizing). A `const` input to every call,
    never mutated → purity holds. Rebuilding pffft plans per call would be absurd;
    the descriptor→config build is the once-per-pod step, cached by descriptor.
- **carry (state)** — the serializable flat **POD** that *is* "everything a fresh
    pod needs." Threaded in → out each call.
- **scratch** — per-worker workspace; carries no meaning (reused, never state).

## State blobs (flat, versioned POD)

- **`acq_fn` carry:** ring tail (≤ `n−1` leftover samples) + ring length +
    running sample offset (the code-phase anchor — carried so a resumed pod keeps a
    continuous phase reference) + `nc_surface[n]` + `nc_count`. Header stamps
    `magic`/`version`/`n` for validation; a mismatched version is rejected, never
    reinterpreted.
- **`ddc_fn` state:** NCO phase + every filter's delay line (FIR history, CIC
    integrator/comb, halfband, resampler fractional phase). Heterogeneous — the
    harder serialization.

## C API shape (acq)

```c
typedef struct acq_config  acq_config_t;   /* immutable; replica + plans + sizing */
typedef struct acq_scratch acq_scratch_t;  /* per-worker workspace, no state      */

acq_config_t  *acq_config_create (/* the physics descriptor: code, code_len, reps,
                                     spc, chip_rate, cn0_dbhz, doppler_uncertainty,
                                     pfa, pd, noise_mode, max_noncoh */);
void           acq_config_destroy (acq_config_t *);
acq_scratch_t *acq_scratch_create (const acq_config_t *);
void           acq_scratch_destroy (acq_scratch_t *);

size_t         acq_carry_bytes (const acq_config_t *);
void           acq_carry_init  (const acq_config_t *, void *carry);

/* Pure: one frame (n samples) -> at most one hit; threads carry. */
int            acq_tile (const acq_config_t *, acq_scratch_t *,
                         void *carry, const float complex *frame /*[n]*/,
                         acq_result_t *out);

/* Pure: arbitrary input -> hits; ring tail lives in carry (no dp_f32 ring). */
size_t         acq_run  (const acq_config_t *, acq_scratch_t *,
                         void *carry, const float complex *in, size_t n_in,
                         acq_result_t *hits, size_t max_hits);
```

`Acquisition` (the object) owns one `(config, scratch, carry)` and forwards:
`push` → `acq_run`, `reset` → `acq_carry_init`, getters → `config`. Bit-identical
to today.

## Elastic fan-out

- shard = one coarse channel = a `(ddc_fn, acq_fn)` pure pipeline.
- threads: share `const config`, per-worker scratch + carry.
- processes/pods: ship `(descriptor, carry, input_block)`. The node rebuilds
    config from the descriptor (cached), applies carry, runs, returns
    `(carry_out, hits)`. Only POD + samples cross a boundary — never plan pointers.
- non-coherent integration is **inside** `acq_fn`'s carry; no external merge.

## Build sequence

1. **(done — PR #259)** physics sizing API; the foundation.
1. **`acq_fn` pure kernel + carry** — split `acq_state_t` into config/scratch/
    carry; `acq_tile`/`acq_run`; flat-POD carry (ring tail folded in, replacing the
    `dp_f32` ring). `Acquisition` rewrapped. **Acceptance: bit-exact vs today +
    carry round-trip** (serialize mid-stream → fresh config+carry → resume →
    identical hits). *Clean half.*
1. **`ddc_fn` pure kernel + state** — same shape; heterogeneous filter delay-line
    serialization is the work.
1. **Orchestrator** over the two pure kernels (mixer bank, fan-out, hit dedupe).

The OO objects (`Acquisition`, `Ddc`/`Ddcr`) are preserved at every step.
