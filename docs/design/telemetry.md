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

<!-- docs-snippet: skip=illustrative excerpt (state->tlm is undeclared here), not standalone -->

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

<!-- docs-snippet: skip=struct layout illustration (design spec), not a compilable usage example -->

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
    (timing error, dB gains, lock metrics in `[0, 1]`); a double would double
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
#include <telemetry/telemetry.h>

int main(void)
{
  dp_tlm_t *tlm = dp_tlm_create (1 << 14);           /* records, pow2 */
  int id = dp_tlm_probe (tlm, "agc.gain_db", 1);     /* decim = 1     */
  (void) id;
  dp_tlm_destroy (tlm);
  return 0;
}
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

<!-- docs-snippet: skip=struct layout illustration (design spec), not a compilable usage example -->

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

<!-- docs-snippet: skip=illustrative excerpt from agc_core.c, not standalone (needs the full agc_state_t/agc_core.h context) -->

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

<!-- docs-snippet: skip=illustrative excerpt (state->tlm is undeclared here), not standalone -->

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

<!-- docs-snippet: skip=pseudocode (comment bodies stand in for real code), not compilable -->

```c
if (!state->tlm.ctx)
  { /* pristine specialised loops — the pre-telemetry code, verbatim */ }
else
  { /* instrumented loops: ... if (step (...)) symsync_tlm_flush (state); */ }
```

Hoisting the check is legal because attach/detach is setup-path-only on
the producer thread (the SPSC contract): `tlm.ctx` cannot change inside a
block.

When the block loop is too large to duplicate textually (the DLL carries
two ~40-line correlator variants), the same split is expressed as a
**literal parameter on a forced-inline kernel** — `dll_steps_impl(..., int tlm_on)` called with a literal `0`/`1` from the hoisted branch. The
`tlm_on == 0` instantiation dead-code-eliminates the flush call site
entirely, so it compiles to the pre-telemetry loop verbatim; this is the
same constant-folding mechanism as `symsync_step_ted`'s literal TED
selector.

**4. Serialization** — swap the POD-state macro for the TLM-aware variant and
bump the object's state version (the struct grew):

<!-- docs-snippet: skip=usage excerpt (real macro invocation, but not a standalone compilable program) -->

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

<!-- docs-snippet: skip=illustrative excerpt (tlm is undeclared here), not standalone -->

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
Every tracking loop is instrumented (see the probe table in
[docs/api/python-telemetry.md](../api/python-telemetry.md)); the AGC was
the first:

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

## Egress — NATS `tlm_sink`

Cross-process consumers read the same records over NATS via the
`dp_tlm_sink_*` helper (`telemetry/tlm_sink.h`) — the exact `wfm_sink`
split: the implementation lives in the **optional `libdoppler_stream`
component** (it publishes through the vendored nats.c), and
`telemetry_core` itself stays dependency-free. Each pump drains the ring
and publishes the records as **`TLM16`** frames (a `dp_sample_type_t`
appended for the purpose: SIGS header, `num_samples` counts records,
payload is packed `dp_tlm_rec_t`):

<!-- docs-snippet: skip=illustrative excerpt (tlm undeclared; needs a live NATS server + the optional libdoppler_stream component) -->

```c
dp_tlm_sink_t *sink = dp_tlm_sink_open ("nats://127.0.0.1:4222/tlm");
int n = dp_tlm_sink_pump (sink, tlm);  /* consumer thread, non-blocking */
dp_tlm_sink_close (sink);
```

The pump is a *consumer* of the SPSC ring — run it on the (single)
consumer thread, never the DSP thread — and the path stays lossy
end-to-end by design: ring overruns are counted, a failed publish drops
that batch and returns the error. On the receive side any `dp_sub_*`
reader gets the frames; the Python `Subscriber` decodes a TLM16 frame
directly into the same structured array `Telemetry.read()` returns (and
a Python producer can symmetrically publish `read()` output through
`Publisher(ep, TLM16)`). File dump still falls out of Python for free
(`recs.tofile(...)`).

## v2 — convenience layer (designed, not yet built)

v1 is powerful and clunky. The clunkiness is structural, not sloppiness:
`src/doppler/examples/mpsk_telemetry_capture_demo.py` is expert-written and
still spends more lines on telemetry ceremony than on DSP. The friction,
measured across the three telemetry examples:

| friction                       | where                                                  | cost                                          |
| ------------------------------ | ------------------------------------------------------ | --------------------------------------------- |
| ring size is a guess           | `Telemetry(1 << 14)` in all three                      | silent drops unless you remember to assert    |
| hand-rolled drain loop         | `set_now` / `steps` / `read` / `concatenate` per block | get the cadence wrong and data is gone        |
| per-probe filtering reinvented | `recs[recs["probe"] == tlm.probe_id(n)]["value"]`      | **three copies**, one per example             |
| id→name inversion by hand      | `{v: k for k, v in probes.items()}`                    | every consumer redoes it                      |
| no time axis                   | plots use `np.arange(v.size)` labelled "symbol index"  | can't plot seconds even though `n` is stamped |
| capture is not self-describing | `np.save(recs)` / `recs.tobytes()`                     | **probe names live only in the live context** |

The last one is a correctness defect, not an inconvenience: the file the
example writes cannot be interpreted without the process that wrote it.

### The time base belongs to the DATA, not the process

The central design point, and the one that is easy to get wrong. A record
carries `n` (sample index) and no time. Time is **derived** from two
quantities the caller supplies:

```text
t(record) = t0 + n / fs
```

- **`fs`** — sample rate; converts index to elapsed seconds.
- **`t0`** — absolute epoch of sample `n == 0`, belonging to *whatever
    produced the samples*.

`now` is merely one possible `t0`, correct only for a live capture and
**never a silent default**. Processing a 2019 file would otherwise stamp its
telemetry as today — worse than no timestamp, because it looks
authoritative. This is the same error as stamping wall-clock at `read()`
time, which attributes a whole drained batch to the drain instant; neither
is acceptable.

A replay implies a third obligation: the `n` stamped via `set_now` must be
in the **source's** index space. Start reading at sample 1,000,000 and
telemetry must say 1,000,000, or the capture will not line up against the
recording it came from.

Since doppler already parses the source's time base, lift it:

<!-- docs-snippet: skip=proposed v2 API (capture/recording/read_dict do not exist yet), by definition not runnable -->

```python
r = Reader("capture_2019.tmp")
tlm = capture(rx=rx, source=r)      # fs from r.fs, t0 from the BLUE timecode
```

Three honest degradations, each a recorded state in the capture file rather
than a fabricated number:

| condition                             | consequence                         |
| ------------------------------------- | ----------------------------------- |
| `r.fs == 0.0` (raw/CSV carry no rate) | ordinal only, no elapsed seconds    |
| no timecode in the source             | relative seconds, no absolute epoch |
| `set_now` never called (`n == 0`)     | ordinal only                        |

**Prerequisite primitive.** BLUE `timecode` is J1950 seconds and nothing in
the tree converts it — `wfm_reader_core.c` reads it as a raw double and
passes it through. The converter lives **once**, beside the BLUE code in
`wfm`; telemetry consumes epochs and must not learn what J1950 is.

**Say where the time base came from.** `fs == 0.0` and `t0 == 0.0` are
ambiguous exactly as `fc == 0.0` is, and doppler already solved that once:
`Reader.fc_source` names the metadata the value was read from so a caller
can tell "genuinely zero" from "not found". `fs` and `t0` need the same
disambiguation, so a capture records *where* its time base came from rather
than silently degrading to an ordinal axis.

**Do not invent a sidecar format for raw/CSV.** A metadata-free capture does
need its rate and epoch to travel by other means, but doppler already reads
two containers that carry them — a SigMF `.sigmf-data` resolves its
`.sigmf-meta` (`core:sample_rate`, `core:datetime`), and BLUE resolves a
detached HEADER — and `Writer` already emits both (`file_type` is
`raw`/`csv`/`blue`/`sigmf`). A bespoke `_fs_t0.csv` naming convention would
be a third mechanism for a job two shipped ones do, and a weaker one:
encoding field order in a filename gives no units, no precision statement,
no room to grow, and is destroyed by a rename. **Raw is metadata-free by
definition — the fix is to stop choosing raw when metadata is needed**, not
to bolt a convention onto it. Where an external tool can only emit raw,
generate a `.sigmf-meta` beside it.

**Wall-clock-at-write is provenance, not a time base.** Three quantities sit
close together and must not merge:

| quantity | meaning                              | role                    |
| -------- | ------------------------------------ | ----------------------- |
| `t0`     | epoch of sample `n == 0`             | **the time base**       |
| `fs`     | sample rate                          | index → elapsed seconds |
| `tnow`   | wall clock when the file was written | **provenance only**     |

A file copied or rewritten last week carries last week's `tnow` while its
samples are years old — the replay error one step removed. If `tnow` is
surfaced it is labelled provenance and never falls back into
`t = t0 + n / fs`.

### The four pieces

**1 — `capture(**objects)` — one command to turn everything on.**
`set_telemetry` already registers *all* of an object's probes, forwarding to
children (one attach gets `MpskReceiver`'s 11). The ceremony around it is
what hurts. The keyword name becomes the prefix, so it stays explicit
instead of guessed from a class name:

<!-- docs-snippet: skip=proposed v2 API (capture/recording/read_dict do not exist yet), by definition not runnable -->

```python
tlm = capture(rx=rx, code0=ch, agc=agc)
```

Ring sizing: a generous default plus **strict overrun** — the recorder
raises on the first drop. Deriving a size from a duration hint trades one
guess for two (the event rate depends on symbol rate and decim, which the
caller rarely knows a priori); growing on demand would break the lock-free
SPSC invariant and the fixed-capacity VM mirror. Over-allocating a few MB is
the cheapest of the three.

**2 — `read_dict()` — no struct-array parsing.**
`{probe_name: values}`, or `{name: (n, values)}` with `index=True` when the
x-axis is wanted. This retires the `series()` helper each example currently
rewrites. It lives **in the C extension beside `read()`** — telemetry is a
hand-owned `no_generate` module, so that is its normal home, and it keeps
`__init__.py` re-export-only.

**3 — `to_file` — a self-describing TLM16 container, in C.**
Header + probe registry (name↔id) + time base (`fs`, `t0`, epoch kind) +
decim + `dropped` + packed 16-byte records. One implementation shared with
`dp_tlm_sink`, readable from C, with Python `to_file`/`from_file` as thin
bindings. The record itself does **not** change: 16 bytes is structurally
two ring slots, and widening it would cost hot-loop bandwidth and break the
TLM16 wire type.

**4 — `recording()` — the drain stops being the caller's problem.**
`dp_tlm_recorder_*` in `telemetry_core`, a pthread drain loop following
`dp_parallel.h`'s precedent for C-level threading, with the Python
`with tlm.recording() as rec:` as a thin binding. The recorder *is* the
single consumer, so the SPSC contract holds. Strict-on-drop belongs here:
the recorder is the one component positioned to notice an overrun when it
happens rather than at a post-hoc assert.

Together, on the capture demo:

<!-- docs-snippet: skip=proposed v2 API (capture/recording/read_dict do not exist yet), by definition not runnable -->

```python
tlm = capture(rx=rx, fs=fs, t0=t0)           # attach all 11 probes
with tlm.recording() as rec:                 # the drain is the recorder's job
    for i in range(0, iq.size, 256):
        tlm.set_now(i)                       # the pipeline still owns its clock
        rx.steps(iq[i : i + 256])
rec.to_file(path)                            # names + time base travel with it

for ax, (name, (n, v)) in zip(axes, rec.read_dict(index=True).items()):
    ax.plot(n / fs, v)                       # a real time axis, in seconds
```

`set_now` stays explicit on purpose — the pipeline owns its sample clock,
and on replay that line is exactly where the source offset enters.

### Smaller QoL, same wave

- **`set_decim(name, k)`** — per-probe decimation is *already possible* and
    undocumented: `probe()` is idempotent by name and updates the decimation,
    so `.e` can be thinned while `.locked` stays at full rate. This is a name
    for an existing capability, not new machinery.
- **`stats()`** — one call reconciling `dropped` against per-probe
    `emitted`, which is a manual cross-check today.
- **`probe_spec()`** — class-level probe discovery, so what an object *would*
    register is visible without attaching. Feeds tooling and the docs table.

### Deliberately not doing

- **Widening the record** — breaks the 16-byte/two-slot invariant, ring
    bandwidth, and TLM16.
- **Per-record wall clock** — a batch stamped at drain time is a fabricated
    number (see the time-base section).
- **Plotting helpers** — demo-tier; belongs in examples, not the library.
- **Pipeline logic in Python** — the C-first rule is unchanged; every piece
    above is either presentation (`read_dict`) or a thin binding over C.

## Future work (deliberately out of v1)

- **Wide records** (`flags`-tagged f64 or vector payloads) if a use case
    ever outgrows f32 scalars.
- **Per-thread ring aggregation** if a multi-threaded producer pipeline ever
    exists; today's contract (one context per producer thread) covers every
    doppler pipeline.
