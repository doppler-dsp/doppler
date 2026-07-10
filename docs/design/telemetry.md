# Telemetry — zero-cost scalar taps for running pipelines

Doppler's loops already *compute* every diagnostic worth watching — the
symbol-sync timing error, the Costas lock metric, the DLL code phase, the AGC
gain — as named fields in their state structs, refreshed every event. What was
missing is a way to watch them **as time series from a live pipeline** without
perturbing the signal path. `dp_tlm` (`native/inc/telemetry/telemetry.h`) is
that tap: a probe registry plus a lock-free record ring, designed around one
budget:

> **Detached: one predicted-not-taken branch per event. Attached: one 16-byte
> ring write per record. Never a lock, never an allocation, never a stall.**

______________________________________________________________________

## The cost model

An instrumented object holds a `dp_tlm_t *ctx` that is NULL by default. Every
probe site compiles to:

```c
DP_TLM (state->tlm.ctx, state->tlm.id_gain, state->gain_db);
```

| Mode               | Per-event cost                              | How                                                              |
| ------------------ | ------------------------------------------- | ---------------------------------------------------------------- |
| Detached (default) | 1 pointer load + predicted-not-taken branch | `dp_tlm_emit` opens with `if (!t) return;`                       |
| Attached           | decim counter + 16-byte SPSC write          | VM-mirrored ring (`buffer/buffer.h`), release-store, no locks    |
| Compiled out       | literally zero                              | consumer builds with `-DDP_TLM_DISABLE` → `DP_TLM` is `(void) 0` |

Two properties keep the "next to nothing" claim honest:

- **Probe sites sit at event rate, not sample rate.** The AGC emits per gain
    update (already amortized by `gain_update_period`); a symbol sync emits per
    recovered symbol. No probe lives inside a per-sample inner loop.
- **The producer never waits for the consumer.** `dp_tlmr_write` is the lossy
    SPSC write from `buffer/buffer.h`: on overrun the record is dropped and
    counted (`dp_tlm_dropped`), so a slow — or entirely absent — reader can
    never backpressure the DSP thread.

There is deliberately **no callback mechanism**. The doppler C API is a handle
model with readable state structs and explicit drains; a telemetry callback
would invert that (arbitrary user code running inside a hot loop) for no
gain. The ring *is* the interface.

______________________________________________________________________

## The record

Sixteen bytes, one ring slot, 8-aligned:

```c
typedef struct
{
  uint64_t n;     /* caller-stamped sample index                */
  float    value; /* the scalar, narrowed to float              */
  uint16_t probe; /* probe id (index into the context registry) */
  uint16_t flags; /* reserved; 0                                */
} dp_tlm_rec_t;
```

- **`value` is float.** ~7 significant digits is ample for diagnostics
    (timing error, dB gains, lock metrics in [0, 1]); a double would double
    ring bandwidth for no plotting benefit. `flags` reserves room for a future
    wide-record class if one is ever needed.
- **`n` is caller-maintained.** Whoever owns the pipeline's sample clock
    stamps it once per block — `dp_tlm_set_now (tlm, clk->n)` — and every
    record emitted during that block carries it. Objects do not track absolute
    sample counts and should not start to. If never stamped, `n` stays 0 and
    consumers index by record order (fine for per-symbol series).
- **There is no per-record sequence number.** The SPSC ring preserves order;
    losses are visible as `dp_tlm_dropped()` (global) reconciled against
    `dp_tlm_emitted(id)` (per probe).

## The registry

Probes are registered at **setup time** — never on the hot path — and named
with dotted paths so a consumer can build a channel map once:

```c
dp_tlm_t *tlm = dp_tlm_create (1 << 14);           /* records, pow2 */
int id = dp_tlm_probe (tlm, "agc.gain_db", 1);     /* decim = 1     */
```

- Registration is **idempotent by name**: re-registering returns the same id
    (and updates the decimation), so an object can re-attach after a reset
    without churning ids.
- **Decimation lives in the registry**, per probe: `decim = N` emits every
    N-th event, and the phase is primed so the *first* event after
    registration always emits (you see the series start immediately). The
    counter lives next to the data the emit already touches — instrumented
    objects carry no decimation state of their own.
- The table is fixed-capacity (`DP_TLM_MAX_PROBES` = 64 per context,
    `DP_TLM_NAME_MAX` = 32 chars) — generous for a receiver chain, and keeps
    the context a flat POD with zero hot-path indirection.

## Threading

The ring is single-producer / single-consumer, and the contract follows from
that:

- **One context per producer thread.** All objects attached to one `dp_tlm_t`
    must step on the same thread (true of any doppler pipeline). Multiple
    pipelines → multiple contexts.
- `dp_tlm_read` may run concurrently on **one** consumer thread; that
    cross-thread hand-off is what the ring's acquire/release discipline
    provides. It is non-blocking — it returns whatever is available and never
    spins.
- Registration and attach complete **before** the producer starts stepping;
    the probe table is written unlocked at setup.

______________________________________________________________________

## Instrumenting an object

Instrumentation is a four-part, ~15-line pattern. Using the AGC as the
canonical example:

**1. Attachment member** — a small POD tail on the state struct
(`native/inc/agc/agc_core.h`):

```c
typedef struct
{
  dp_tlm_t *ctx;     /* NULL = detached                    */
  int32_t   id_gain; /* probe id from a successful attach  */
  int32_t   _pad;
} agc_tlm_t;

/* ... last member of agc_state_t: */
agc_tlm_t tlm; /* live attachment; zeroed in blobs */
```

**2. Attach function** — registers the object's probes under a caller prefix
(setup path, in `agc_core.c`):

```c
int
agc_set_telemetry (agc_state_t *s, dp_tlm_t *t, const char *prefix,
                   uint32_t decim)
{
  if (!t) /* detach */
    {
      s->tlm.ctx = NULL;
      return DP_OK;
    }
  char name[DP_TLM_NAME_MAX];
  (void) snprintf (name, sizeof (name), "%s.gain_db", prefix);
  int id = dp_tlm_probe (t, name, decim);
  if (id < 0)
    return id; /* full table / bad name: attach fails whole */
  s->tlm.id_gain = id;
  s->tlm.ctx     = t; /* set last: emit sites gate on ctx */
  return DP_OK;
}
```

**3. Emit sites** — one line at each event, guarded by the macro:

```c
DP_TLM (state->tlm.ctx, state->tlm.id_gain, state->gain_db);
```

The inline `DP_TLM` form is right when the event site is already outside
the per-sample inner loop (the AGC's gain update is amortised by
`gain_update_period`). **When the event fires inside a force-inlined
per-sample step — a symbol sync emitting per recovered symbol — do NOT
put emits (or any call site) in the step body.** Two compiler effects,
both measured at ~20-30% detached slowdown on the 64k-block bench even
though no telemetry code ever executed:

- *Inlined emit bloat*: each `dp_tlm_emit` expansion enlarges the inlined
    step body; several of them spill the register-cached loop state.
- *Extern-call aliasing poison*: any extern call site inside the block
    loop forces the compiler to assume every state field may be clobbered
    per iteration, reloading the NCO/interpolator hot state from memory
    each sample — even when the call is behind a never-taken branch.

The pattern that benchmarks at parity with the untouched baseline
(symsync/mpsk_receiver): an **out-of-line flush function** per object
(`symsync_tlm_flush` — reads the state fields, emits every probe) and an
**attachment check hoisted to block-loop entry**, so the detached loops
contain no call site at all:

```c
if (!state->tlm.ctx)
  { /* pristine specialised loops — the pre-telemetry code, verbatim */ }
else
  { /* instrumented loops: ... if (step (...)) symsync_tlm_flush (state); */ }
```

Hoisting the check is legal because attach/detach is setup-path-only on
the producer thread (the SPSC contract): `tlm.ctx` cannot change inside a
block.

**4. Serialization** — swap the POD-state macro for the TLM-aware variant and
bump the object's state version (the struct grew):

```c
DP_DEFINE_POD_STATE_TLM (agc, agc_state_t, AGC_STATE_MAGIC,
                         AGC_STATE_VERSION, tlm)
```

### Why `DP_DEFINE_POD_STATE_TLM` exists

The [state-serialization standard](state-serialization.md) snapshots
pointer-free POD structs whole (`DP_DEFINE_POD_STATE`). A telemetry
attachment breaks that premise in both directions: `get_state` would leak a
live heap address into the blob (nondeterministic bytes, useless on restore),
and `set_state` would clobber the *receiving* instance's attachment with the
sender's stale pointer. The TLM variant (`native/inc/dp_state.h`) fixes both:
the named member is **zeroed in the serialized copy** — so blobs are
deterministic and attachment-independent — and **preserved across restore** —
so a live attachment survives a state hand-off. Telemetry is observation; it
is not part of the DSP state that migrates.

Compositions that embed instrumented children by value (e.g. `mpsk_receiver`
holding a `symsync_state_t`) inherit this automatically through the children's
triplets, and forward their attach with a prefixed name
(`"rx.sync.timing_err"`).

______________________________________________________________________

## Consuming

The v1 consumer face is pull-only: `dp_tlm_read` drains into caller storage,
non-blocking, from any single consumer thread.

```c
dp_tlm_rec_t recs[512];
size_t n = dp_tlm_read (tlm, recs, 512);
for (size_t i = 0; i < n; i++)
  printf ("%s @ %llu = %f\n",
          dp_tlm_probe_name (tlm, recs[i].probe),
          (unsigned long long) recs[i].n, (double) recs[i].value);
```

The Python face — `doppler.telemetry.Telemetry`, a hand-owned `no_generate`
module like `buffer` and `stream` — reads the same ring as a numpy
structured array (`dtype: n u8 | value f4 | probe u2 | flags u2`): one
`read()` returning everything since the last drain, plus the probe-name map
(`probe_names()`), per-probe `emitted()` and the `dropped` counter. Its
`_capsule` property exposes the `dp_tlm_t *` that instrumented objects'
jm-generated `set_telemetry` bindings unwrap (they also accept the
`Telemetry` object itself, duck-typed through `_capsule` — jm gh-432).
The AGC is the first instrumented object:

```python
import numpy as np

from doppler.agc import AGC
from doppler.telemetry import Telemetry

tlm = Telemetry(1 << 14)
agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
agc.set_telemetry(tlm, "agc", decim=1)

x = np.full(4096, 0.125 + 0j, dtype=np.complex64)
agc.steps(x)

recs = tlm.read()
gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]["value"]
assert len(gain) == 4096 // agc.decim  # one record per control update
assert gain[-1] > gain[0]  # quiet input: commanded gain rises
assert tlm.dropped == 0
```

## Future work (deliberately out of v1)

- **NATS egress**: a `tlm_sink` helper that drains a context into the
    existing `dp_pub_*` wire layer on a telemetry subject — the exact
    `wfm_sink` split (optional stream component; telemetry_core itself stays
    dependency-free). File dump falls out of Python for free
    (`recs.tofile(...)`).
- **Wide records** (`flags`-tagged f64 or vector payloads) if a use case
    ever outgrows f32 scalars.
- **Per-thread ring aggregation** if a multi-threaded producer pipeline ever
    exists; today's contract (one context per producer thread) covers every
    doppler pipeline.
