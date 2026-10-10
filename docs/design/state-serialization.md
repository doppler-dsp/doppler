# State Serialization — the standard bytes interface

This is the internals doc — the envelope, the cursor helpers, and the CI
gate. For the practitioner's path (the basic pattern, composed receivers,
elastic pod hand-off) see the
[Checkpoint & Resume guide](../guide/state-serialization.md).

Every stateful doppler object can hand its running state to a fresh instance and
resume **bit-for-bit** — across a thread, a process, or a pod. A decimator
serialized at sample 1100 and restored into a brand-new decimator produces the
exact same next sample it would have produced uninterrupted. This is the
*elastic* face: scale a pipeline out, checkpoint it, migrate it, and the DSP
doesn't notice.

The design rests on one distinction:

> **Serialization is module-specific; the bytes interface is not.**

Only `lo` knows it holds a phase; only `fir` knows it holds a delay line; only
`acq` knows it holds a sample ring and a non-coherent surface. *What* to pack is
the module's business. But the envelope around those bytes — the type tag, the
version, the validation, the language faces — is identical for every object, and
is owned **once**, centrally, in [`native/inc/doppler/dp_state.h`](../c-api/index.md).

______________________________________________________________________

## The two layers

| Layer                           | Owns                                                                       | Where                                                                                                   |
| ------------------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| **Bytes interface** (universal) | the envelope, cursors, validation, the ABI contract, the Python/test faces | `native/inc/doppler/dp_state.h`, `native/inc/doppler/dp_state_pyhelp.h`, `native/tests/dp_state_test.h` |
| **Serialization** (per-module)  | which fields to pack, in what order                                        | each `native/src/<obj>/<obj>_core.c`                                                                    |

A module's `get_state`/`set_state` stamps the standard header (one call), then
packs or unpacks **its own** fields through the cursor helpers. The build system
asks `jm` to generate the *Python binding* for this triplet, never the C bodies
— `jm` can't see the runtime, config-dependent sizes (`num_taps`, ring capacity)
that live only in `create()`.

______________________________________________________________________

## The ABI triplet

Every serializable C object exposes exactly three functions (sibling to
`reset`), plus the optional pure-transducer `run`:

<!-- docs-snippet: skip=API signature sketch (design spec), not a compilable usage example -->

```c
size_t obj_state_bytes(const obj_state_t *s);          /* serialized size      */
void   obj_get_state  (const obj_state_t *s, void *blob); /* serialize          */
int    obj_set_state  (obj_state_t *s, const void *blob); /* restore: DP_OK / DP_ERR_INVALID */
```

`set_state` **always** opens with `dp_state_validate()`, so a blob from a
different object, a different format version, a foreign endianness, or a
different configuration is *rejected* (`DP_ERR_INVALID`) — never silently
reinterpreted. This closed a real latent bug: before the standard, leaf objects
had no envelope and accepted any blob of the right length, corrupting state.

______________________________________________________________________

## The envelope

Every blob begins with a 16-byte self-describing header:

<!-- docs-snippet: skip=struct layout illustration (design spec), not a compilable usage example -->

```c
typedef struct
{
  uint32_t magic;   /* per-object FourCC type tag, e.g. DP_FOURCC('A','C','Q','R') */
  uint16_t version; /* per-object blob format version                             */
  uint8_t  endian;  /* DP_STATE_ENDIAN at serialize time                          */
  uint8_t  flags;   /* reserved; 0                                                */
  uint32_t bytes;   /* total blob size; equals obj_state_bytes()                  */
  uint32_t _pad;    /* reserved; 0                                                */
} dp_state_hdr_t;
```

16 bytes keeps a following `double`/`uint64_t` naturally 8-aligned. The `magic`
**is** the type identity (a human-readable FourCC in a hex dump); `bytes` is the
one size invariant, and it agrees with the Python exact-size gate.

### Layout

```
leaf:        [ dp_state_hdr_t ] [ module payload ]
composition: [ dp_state_hdr_t ] [ extra? ] [ child blob ] [ child blob ] ...
             state_bytes = sizeof(hdr) + sizeof(extra) + Σ child_state_bytes
```

A composition embeds each child as a **self-contained sub-blob** — the child
carries its own header, so it is independently validatable, and migrating a leaf
(which changes its size) propagates automatically because the parent sums
`child_state_bytes`. For example a `ddcr` blob is
`[hdr][ddcr_extra{rate}][r2c][lo][rc]`, where `r2c`, `lo`, and `rc` are each a
full leaf/sub blob with its own envelope.

______________________________________________________________________

## What goes in the blob — a mutator's value is state

A blob holds everything the next sample depends on that `create()` cannot
rebuild from its own arguments. Four rules decide each field (#2022):

1. **Running state is packed:** a phase, a delay line, an accumulator, an
    RNG, a loop's integrator.
1. **A mutator's value is state.** A value any post-create mutator can change
    travels in the blob. A mutator is a setter, a writable property,
    `configure*`, `retune`, `reconfigure` or `reseed`. `set_state` checks the
    value with that mutator's own predicate, one helper both call. Only the
    config no mutator reaches is restored by `create()`.
1. **Create-time config that sizes the blob, or changes what it means, is a
    reject key:** `sf`, `sps`, a mode, a rate. `set_state` refuses a blob whose
    key differs from the target's. It decodes into a temporary, checks it
    whole, then commits, so a refused blob changes nothing.
1. **A blob that gains a field bumps the object's `*_STATE_VERSION`,** so
    a blob of the old layout is refused rather than read as the new one.

A mutator whose value **sizes the blob** makes the blob target-sized: it
restores only into a target the same mutator gave the same size.
`BurstDespreader.set_acq()` is the precedent (#2041). Its acq code travels,
and a blob taken with a 127-chip acq code restores only into a despreader
whose acq code is also 127 chips; any other target refuses it before reading
anything.

Never in a blob:

- **Handles to the outside world:** telemetry sinks, event logs, thread
    counts, refill callbacks.
- **Addresses:** a pointer `create()` re-establishes.
- **The wall clock.**

**A blob is a function of the object.** Two objects built and fed the same
give byte-identical blobs: no uninitialised padding, and no unused buffer
tail. A blob that varies cannot be judged by a restore (#2052, #2076).

### The gate

`src/doppler/tests/test_mutator_state.py` holds every serializable object to
these rules, with nothing to register.

**What it finds:**

- every public member of the class Python gets, wherever it was bound: a
    writable property, and every method but the lifecycle (`reset`, `close`,
    `destroy`, the state triplet). Nothing is called a reader for its return
    type: a reader is a call the probe sees change nothing;
- each member's signature, from the class's `.pyi` stub;
- every C setter Python cannot call, from the headers.

**How it probes each member.** It takes an instance and a feed from this
page's Python matrix (`test_state_serialization.CASES`). It picks values the
member accepts and that change what the object does. It then restores one
value's blob into a default target, a target at the same value, targets at
other values, and targets beside it: a scalar a hair either side, an array
of the same length with other content. Every target that accepts the blob
must then reproduce the source: its readback (every property), its
continuation output and its blob. That gives these verdicts:

| verdict            | meaning                                                                               |
| ------------------ | ------------------------------------------------------------------------------------- |
| `TRAVELS`          | passes: the default target and one at another value take the blob and match           |
| `KEYED`            | passes: the default target refuses it, and the same-value target takes it and matches |
| `READS`            | passes: a call that hands back a value and changes nothing                            |
| `LOST`             | fails: a target took the blob and then differed                                       |
| `UNPROBED`         | fails: the probe could not show it either way, and the list says why                  |
| `NONDETERMINISTIC` | fails: two identical builds differed                                                  |
| `CRASHES`          | fails: the probe kills the process, so a child process confirms it                    |

**The list.** Anything that does not pass is listed once in
`scripts/.mutator-state-exempt`, with exactly its verdict and a reason. That
list only shrinks. An entry that now passes, names no mutator, or carries
another verdict is red, and `make tests-ssot` refuses a key the merge base
did not hold. Two more verdicts appear only there. `C_ONLY` marks a setter
with no Python face (a C-side twin is #2077). `NO_RECIPE` marks a class with
no matrix row (#2078). Each family of known violations has its issue
(#2079–#2084), and its fix deletes its own lines.

______________________________________________________________________

## Cursors

Hand-packing is a few bounds-checked calls on a writer/reader cursor, not raw
pointer arithmetic. The cursors use a **sticky-error** model: an overrun sets
`err` and subsequent operations no-op, so call sites stay flat.

<!-- docs-snippet: skip=illustrative excerpt (param names simplified from the real lo_core.c for exposition), not standalone -->

```c
void
dp_lo_get_state (const dp_lo_state_t *s, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, dp_lo_state_bytes (s));
  dp_w_hdr (&w, LO_STATE_MAGIC, LO_STATE_VERSION, dp_lo_state_bytes (s));
  dp_w_f64 (&w, s->phase);          /* pack the module's own fields */
  dp_w_f64 (&w, s->phase_inc);
}

int
dp_lo_set_state (dp_lo_state_t *s, const void *blob)
{
  int rc = dp_state_validate (blob, dp_lo_state_bytes (s),
                              LO_STATE_MAGIC, LO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;                       /* wrong object / version / size → reject */
  dp_reader_t r = dp_reader_init (blob, dp_lo_state_bytes (s));
  r.off = sizeof (dp_state_hdr_t);
  s->phase     = dp_r_f64 (&r);
  s->phase_inc = dp_r_f64 (&r);
  return DP_OK;
}
```

The writer/reader pairs cover `u32`/`u64`/`f64`/`cf32`/`f32`/raw `bytes`, plus
`dp_w_reserve`/`dp_r_reserve` for handing a region to a child's get/set.

## Helper macros — the three serializer shapes

Almost every serializer is one of three shapes, and each has a macro so the
triplet is a few lines, not a hand-rolled envelope. All live in `dp_state.h`.

### POD — `DP_DEFINE_POD_STATE(pfx, STATE_T, MAGIC, VERSION)`

A **pointer-free** struct *is* its own state — snapshot it whole. Defines all
three functions; place it once beside `reset`. Restoring the config fields is a
harmless no-op into an identically-built instance. An embedded **POD child**
(e.g. a `dp_loop_filter_state_t` by value) is captured automatically, so a
composition of by-value POD members is still one `DP_DEFINE_POD_STATE`.

<!-- docs-snippet: skip=usage excerpt (real macro invocation, but not a standalone compilable program) -->

```c
/* native/src/loop_filter/loop_filter_core.c */
DP_DEFINE_POD_STATE(dp_loop_filter, dp_loop_filter_state_t,
                    LOOP_FILTER_STATE_MAGIC, LOOP_FILTER_STATE_VERSION)
```

### Field-wise — `DP_GET_OPEN` / `DP_SET_OPEN`

When the struct owns heap buffers, pack the running fields and every
mutator's value ([What goes in the blob](#what-goes-in-the-blob-a-mutators-value-is-state)),
and let `create()` re-derive the buffers and the config no mutator reaches. `DP_GET_OPEN(MAGIC, VER, BYTES)` stamps
the envelope and opens a writer `_w`; `DP_SET_OPEN(MAGIC, VER, BYTES)` validates
and opens a reader `_r` positioned past the header (early-returns
`DP_ERR_INVALID` on a bad blob). The body is just `dp_w_*`/`dp_r_*` calls. The
function parameters **must** be named `s` (the state) and `blob`.

<!-- docs-snippet: skip=illustrative excerpt (design pattern), not a standalone compilable program -->

```c
size_t dp_delay_state_bytes(const dp_delay_state_t *s)
{ return sizeof(dp_state_hdr_t) + sizeof(uint64_t)
         + 2 * s->capacity * sizeof(double _Complex); }

void dp_delay_get_state(const dp_delay_state_t *s, void *blob)
{
  DP_GET_OPEN(DELAY_STATE_MAGIC, DELAY_STATE_VERSION, dp_delay_state_bytes(s));
  dp_w_u64(&_w, s->head);
  dp_w_bytes(&_w, s->buf, 2 * s->capacity * sizeof(double _Complex));
}

int dp_delay_set_state(dp_delay_state_t *s, const void *blob)
{
  DP_SET_OPEN(DELAY_STATE_MAGIC, DELAY_STATE_VERSION, dp_delay_state_bytes(s));
  s->head = (size_t)dp_r_u64(&_r);
  dp_r_bytes(&_r, s->buf, 2 * s->capacity * sizeof(double _Complex));
  return DP_OK;
}
```

> A borrowed/owned **pointer** that `create()` re-establishes (e.g. a code
> table) is config, not state. Don't serialize its *address* — it differs across
> instances and makes the blob non-canonical. Either skip it (field-wise) or, if
> you snapshot the whole struct, NULL it in the serialized copy and preserve the
> live value in `set_state`. See `dp_dll_get_state`.

> A **wall-clock or other non-deterministic quantity** (a `dp_sample_clock_t`
> anchor, a running sample counter kept only for cross-call bookkeeping) is
> runtime state, not resumable DSP state — never part of the blob. Bit-exact
> resume means "the exact same next *sample*," not "the exact same wall-clock
> reading" — a restored object re-observes or re-derives timing fresh, the same
> way a borrowed pointer is re-established by `create()` rather than carried in
> the blob.
>
> **That rule does not reach a counter that is compared with a child's.** A
> parent count that is diffed against a child's serialized position is in the
> child's timebase whether it is serialized or not. `DsssReceiver` kept such a
> count, `samples_fed`, and subtracted it from its acquisition's hit offset. It
> was left out of the blob as runtime state, and `configure_search_raw()` reset
> the child but not the count, so after a restore or a reconfigure the two
> timebases parted and the hand-off read outside its input (#2042). Read the
> child's own position instead (`dp_acq_position()`), and keep no parallel
> counter.

### Composition — `DP_W_CHILD` / `DP_R_CHILD`

Nest each serializable child as a self-validating sub-blob. `state_bytes` sums
`<child>_state_bytes(child_ptr)`; `get`/`set` then writes/reads each child via
the reserve cursors. `child_ptr` may be a pointer member or the address of an
embedded-by-value member (`&s->lf`). `DP_R_CHILD` returns `DP_ERR_INVALID` from
the enclosing `set_state` if a child rejects, so a composite restore is
atomic-by-validation.

<!-- docs-snippet: skip=illustrative excerpt (design pattern), not a standalone compilable program -->

```c
size_t dp_mpsk_receiver_state_bytes(const dp_mpsk_receiver_state_t *s)
{ return sizeof(dp_state_hdr_t) + dp_carrier_nda_state_bytes(&s->car)
         + dp_symsync_state_bytes(&s->sync) + dp_fir_state_bytes(s->mf)
         + /* running scalars … */; }

void dp_mpsk_receiver_get_state(const dp_mpsk_receiver_state_t *s, void *blob)
{
  DP_GET_OPEN(MPSK_RECEIVER_STATE_MAGIC, MPSK_RECEIVER_STATE_VERSION,
              dp_mpsk_receiver_state_bytes(s));
  DP_W_CHILD(&_w, carrier_nda, &s->car);   /* embedded by value      */
  DP_W_CHILD(&_w, symsync,     &s->sync);
  DP_W_CHILD(&_w, fir,         s->mf);      /* pointer member         */
  /* … then dp_w_* the running scalars … */
}
```

## The `run` transducer

For a single-`execute` object, `DP_DEFINE_RUN(pfx, STATE_T, IN_T, OUT_T)`
generates the identical pure-transducer wrapper
`pfx_run(state_in, state_out, in, n_in, out, max_out)`: optionally restore
`state_in`, run one `execute`, optionally emit `state_out`. (Frame/push shapes
like `acq` keep a hand-written `run`.)

______________________________________________________________________

## The Python face

For a plain object, one manifest flag is the whole story:

```toml
# objects/<obj>.toml
serializable = "true"
```

`jm apply` then generates the Python binding triplet — `state_bytes() -> int`,
`get_state() -> bytes`, `set_state(bytes) -> None` (size-mismatch / rejected-blob
→ `ValueError`, non-`bytes` → `TypeError`) — and the matching `.pyi` stubs, over
the C ABI below the envelope. No hand-binding.

```python
import numpy as np
from doppler.resample import RateConverter

a = RateConverter(0.5)
a.execute(np.ones(2048, dtype=np.complex64))
blob = a.get_state()          # bytes; len(blob) == a.state_bytes()

b = RateConverter(0.5)        # a fresh, identically-built instance
b.set_state(blob)             # resume from a's exact state
```

As of **jm 0.20.0** the flag is the whole story for the two harder kinds too —
no hand-binding anywhere:

- **Sacred fragments.** An object whose `_ext_<obj>.c` fragment is hand-owned
    (a bespoke property, a custom `execute`) is not regenerated, so `jm apply`
    **transplants** the triplet into it — injecting the wrappers + `PyMethodDef`
    rows idempotently, leaving every hand-written binding intact (gh-404).
    `DDC` and `RateConverter` are such objects.
- **Handle modules.** A `kind="handle"` module (`ddc_fn`'s `Ddcr`) generates the
    triplet over its opaque handle when `serializable = "true"` is set on
    `[module.<name>]` (gh-403).

______________________________________________________________________

## Testing

The C and Python faces are tested by **shared harnesses**, so each new
serializable type subscribes to the same invariants rather than re-deriving
them.

- **C** — `DP_STATE_ROUNDTRIP_TEST(pfx, a, b)` in `native/tests/dp_state_test.h`:
    `get_state(a)` → `set_state(b)` is `DP_OK`, then a magic-clobbered blob is
    `DP_ERR_INVALID`. Each `test_<obj>_core.c` also splits a real stream and
    asserts bit-exact resume.
- **Python** — `src/doppler/tests/test_state_serialization.py`, a parametrized
    matrix over every block-`execute` type (LO, CIC, FIR, DDC, RateConverter)
    asserting bit-exact elastic resume across a mid-stream split and the
    self-validating rejects (short / long / clobbered → `ValueError`, non-`bytes`
    → `TypeError`).

______________________________________________________________________

## Portability

Blobs are **native-endian POD** for same-machine / same-architecture resume
(thread, process, pod) — the realistic deployment for elastic scaling. The
`endian` byte is stamped and rejected on mismatch; there is deliberately *no*
cross-endian byte-swap. The format is not promised across doppler versions: a
`version` bump (or any size/layout change) is caught by `dp_state_validate`, so a
stale blob fails loudly instead of corrupting state.

______________________________________________________________________

## Adding a serializable object

When you build a new object with **just-makeit** (see the workflow in
`CLAUDE.md`), serialization is a required step for anything stateful — every
object that carries running state between calls must speak this interface, so the
whole library stays uniformly resumable. The rule of thumb: *if it has a `reset`
that does more than nothing, it needs the triplet.* Stateless objects (pure
converters, FFT plans, by-value analyzers) are exempt.

1. **Write the C triplet** beside `reset` in `<obj>_core.c`, with a per-object
    `<OBJ>_STATE_MAGIC`/`_VERSION` in the header (`#include "doppler/dp_state.h"`).
    Serialize the running state and every mutator's value; `create()` restores
    only the config no mutator reaches, and a key that sizes the blob is checked
    ([What goes in the blob](#what-goes-in-the-blob-a-mutators-value-is-state)). Pick
    the macro for the shape (see [Helper macros](#helper-macros-the-three-serializer-shapes)):

    - **pointer-free POD** → `DP_DEFINE_POD_STATE(...)` (one line).
    - **owns heap buffers** → field-wise with `DP_GET_OPEN`/`DP_SET_OPEN` + the
        `dp_w_*`/`dp_r_*` cursors; skip pointers (re-derived by `create()`).
    - **composition** → `DP_W_CHILD`/`DP_R_CHILD` over each serializable child.

    Add `DP_DEFINE_RUN(...)` for the pure `<obj>_run` transducer if it's a
    single-`execute` object.

1. **Flip the flag** — `serializable = "true"` in `objects/<obj>.toml`, then
    `jm apply`. As of **jm 0.20.0** the flag is the entire Python story for every
    object kind: jm generates the `state_bytes`/`get_state`/`set_state` binding +
    `.pyi`, **transplanting** the triplet into a hand-owned (sacred) `_ext_<obj>.c`
    fragment when one exists (gh-404), and generating it over the handle for a
    `kind="handle"` module (gh-403). **clang-format the touched fragment** (jm
    emits 4-space; doppler is GNU 2-space).

1. **Test both faces** — a C round-trip + reject in `test_<obj>_core.c` (the
    `DP_STATE_ROUNDTRIP_TEST` macro, plus a buffer/field equality check for
    field-wise/composition shapes), and an entry in the parametrized Python
    matrix `src/doppler/tests/test_state_serialization.py`. The matrix `feed`
    returns an array the continuation compare checks bit-for-bit; for an
    output-less object, return `np.frombuffer(o.get_state(), np.uint8)` so the
    post-block **state blob itself** is the resume observable. That row is also
    what [the mutator gate](#the-gate) probes your object's mutators with.

1. **Drop it from the burn-down** — remove `<obj>` from
    `scripts/.serializable-ignore`.

## Enforcement — the gate (it can't rot)

`scripts/check_serializable.py` (wired into the CI `docs` job) makes the stance
**mandatory**: every object in `objects/*.toml` must resolve to exactly one of —

- `serializable = "true"` in its TOML, or
- listed in `scripts/.serializable-stateless` — a reviewed permanent opt-out for
    objects with **no resumable state** (pure converters, FFT plans, by-value
    analyzers). It lives in a sidecar file, not the TOML, because jm's manifest
    dumper only round-trips keys it knows.

An object that declares neither **fails CI** — unless it is still on the
rollout burn-down list `scripts/.serializable-ignore`, which shrinks to empty as
objects are completed. A stale ignore entry (now resolved) also fails, keeping
the list honest. Net effect: a new stateful object cannot ship without making a
conscious, reviewed choice.

That gate decides **whether** an object has a blob. What goes **in** it is
held by the mutator gate: [The gate](#the-gate), under "What goes in the blob".

______________________________________________________________________

## Status

As of **jm 0.20.0**, `serializable = "true"` is the entire Python binding for
every object kind — regenerable, sacred-fragment (jm transplants the triplet,
gh-404), and `kind="handle"` (jm generates it over the handle, gh-403):

| Type                                          | C triplet | Python ser/des | Binding                          |
| --------------------------------------------- | --------- | -------------- | -------------------------------- |
| `LO`, `CIC`, `FIR`, `Acquisition`, generators | ✅        | ✅             | `jm`-auto from the flag          |
| `DDC`, `RateConverter`, compositions, loops   | ✅        | ✅             | `jm` transplant into sacred frag |
| `Ddcr` (`ddc_fn`, `kind="handle"`)            | ✅        | ✅             | `jm`-auto over the handle        |

**The rollout is complete: every stateful object is serializable** (the gate's
burn-down list is empty). A CI gate (`scripts/check_serializable.py`, see
[Enforcement](#enforcement-the-gate-it-cant-rot)) holds the line going forward —
a new object must declare `serializable = "true"` or opt out as stateless.

The coverage spans every stateful family — generators and tracking loops
(a phase, an RNG), filters and resamplers (delay lines, integ/comb
chains), the pointer-free POD set (snapshot whole), the field-wise set
(structs with pointers pack running fields and skip them), compositions
(delegate to their children's triplets), and the
correlator/detector/analyzer family, whose opaque FFT plans and work
buffers are rebuilt by `create()` while ring/pending buffers are
zero-padded to a fixed capacity so blobs stay canonical. The
authoritative per-object roster is the code, not this page: every
`objects/*.toml` either declares `serializable = "true"` or is listed in
`scripts/.serializable-stateless` — the gate fails CI on any object that
does neither.

### The payoff — elastic pod hand-off

The orchestrator cashes it in: `CoarseChannel.get_state`/`set_state` (and the
bank-level `Acquirer.get_state`/`set_state`) compose their children's blobs (DDC
mixer/decimator + the `BurstAcquisition` or `BurstCapture` search) behind a small Python envelope. So a
running acquirer is the documented `(descriptor, state, block)` triple —
checkpoint a bank mid-stream, rebuild it from its descriptor on another pod,
restore the blob, and the search continues **detection-for-detection identical**
to an uninterrupted run (`test_bank_pod_handoff_resumes_bit_exact`).

### Language faces

The bytes interface is reached from every binding doppler ships, all over the
same C triplet:

- **C** — the ABI itself (`<obj>_state_bytes`/`get_state`/`set_state`).
- **Python** — `serializable = "true"` → jm generates `state_bytes()` /
    `get_state() -> bytes` / `set_state(bytes)` (size/clobber/non-bytes rejects
    raise `ValueError`/`TypeError`).
- **Rust** — `ffi/rust`'s `impl_serializable!` macro exposes `state_bytes()` /
    `get_state() -> Vec<u8>` / `set_state(&[u8]) -> Result<(), StateError>` on
    `Lo`/`Nco`/`Fir`/`AccF32`/`AccCf64`.

The rollout is complete — there is no remaining open work on the standard
itself.
