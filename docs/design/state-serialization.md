# State Serialization — the standard bytes interface

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
is owned **once**, centrally, in [`native/inc/dp_state.h`](../c-api/index.md).

______________________________________________________________________

## The two layers

| Layer                           | Owns                                                                       | Where                                                                                   |
| ------------------------------- | -------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| **Bytes interface** (universal) | the envelope, cursors, validation, the ABI contract, the Python/test faces | `native/inc/dp_state.h`, `native/inc/dp_state_pyhelp.h`, `native/tests/dp_state_test.h` |
| **Serialization** (per-module)  | which fields to pack, in what order                                        | each `native/src/<obj>/<obj>_core.c`                                                    |

A module's `get_state`/`set_state` stamps the standard header (one call), then
packs or unpacks **its own** fields through the cursor helpers. The build system
asks `jm` to generate the *Python binding* for this triplet, never the C bodies
— `jm` can't see the runtime, config-dependent sizes (`num_taps`, ring capacity)
that live only in `create()`.

______________________________________________________________________

## The ABI triplet

Every serializable C object exposes exactly three functions (sibling to
`reset`), plus the optional pure-transducer `run`:

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

## Cursors

Hand-packing is a few bounds-checked calls on a writer/reader cursor, not raw
pointer arithmetic. The cursors use a **sticky-error** model: an overrun sets
`err` and subsequent operations no-op, so call sites stay flat.

```c
void
lo_get_state (const lo_state_t *s, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, lo_state_bytes (s));
  dp_w_hdr (&w, LO_STATE_MAGIC, LO_STATE_VERSION, lo_state_bytes (s));
  dp_w_f64 (&w, s->phase);          /* pack the module's own fields */
  dp_w_f64 (&w, s->phase_inc);
}

int
lo_set_state (lo_state_t *s, const void *blob)
{
  int rc = dp_state_validate (blob, lo_state_bytes (s),
                              LO_STATE_MAGIC, LO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;                       /* wrong object / version / size → reject */
  dp_reader_t r = dp_reader_init (blob, lo_state_bytes (s));
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
(e.g. a `loop_filter_state_t` by value) is captured automatically, so a
composition of by-value POD members is still one `DP_DEFINE_POD_STATE`.

```c
/* native/src/loop_filter/loop_filter_core.c */
DP_DEFINE_POD_STATE(loop_filter, loop_filter_state_t,
                    LOOP_FILTER_STATE_MAGIC, LOOP_FILTER_STATE_VERSION)
```

### Field-wise — `DP_GET_OPEN` / `DP_SET_OPEN`

When the struct owns heap buffers, pack only the *running* fields and let
`create()` re-derive the buffers/config. `DP_GET_OPEN(MAGIC, VER, BYTES)` stamps
the envelope and opens a writer `_w`; `DP_SET_OPEN(MAGIC, VER, BYTES)` validates
and opens a reader `_r` positioned past the header (early-returns
`DP_ERR_INVALID` on a bad blob). The body is just `dp_w_*`/`dp_r_*` calls. The
function parameters **must** be named `s` (the state) and `blob`.

```c
size_t delay_state_bytes(const delay_state_t *s)
{ return sizeof(dp_state_hdr_t) + sizeof(uint64_t)
         + 2 * s->capacity * sizeof(double _Complex); }

void delay_get_state(const delay_state_t *s, void *blob)
{
  DP_GET_OPEN(DELAY_STATE_MAGIC, DELAY_STATE_VERSION, delay_state_bytes(s));
  dp_w_u64(&_w, s->head);
  dp_w_bytes(&_w, s->buf, 2 * s->capacity * sizeof(double _Complex));
}

int delay_set_state(delay_state_t *s, const void *blob)
{
  DP_SET_OPEN(DELAY_STATE_MAGIC, DELAY_STATE_VERSION, delay_state_bytes(s));
  s->head = (size_t)dp_r_u64(&_r);
  dp_r_bytes(&_r, s->buf, 2 * s->capacity * sizeof(double _Complex));
  return DP_OK;
}
```

> A borrowed/owned **pointer** that `create()` re-establishes (e.g. a code
> table) is config, not state. Don't serialize its *address* — it differs across
> instances and makes the blob non-canonical. Either skip it (field-wise) or, if
> you snapshot the whole struct, NULL it in the serialized copy and preserve the
> live value in `set_state`. See `dll_get_state`.

### Composition — `DP_W_CHILD` / `DP_R_CHILD`

Nest each serializable child as a self-validating sub-blob. `state_bytes` sums
`<child>_state_bytes(child_ptr)`; `get`/`set` then writes/reads each child via
the reserve cursors. `child_ptr` may be a pointer member or the address of an
embedded-by-value member (`&s->lf`). `DP_R_CHILD` returns `DP_ERR_INVALID` from
the enclosing `set_state` if a child rejects, so a composite restore is
atomic-by-validation.

```c
size_t mpsk_receiver_state_bytes(const mpsk_receiver_state_t *s)
{ return sizeof(dp_state_hdr_t) + carrier_nda_state_bytes(&s->car)
         + symsync_state_bytes(&s->sync) + fir_state_bytes(s->mf)
         + /* running scalars … */; }

void mpsk_receiver_get_state(const mpsk_receiver_state_t *s, void *blob)
{
  DP_GET_OPEN(MPSK_RECEIVER_STATE_MAGIC, MPSK_RECEIVER_STATE_VERSION,
              mpsk_receiver_state_bytes(s));
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
    `<OBJ>_STATE_MAGIC`/`_VERSION` in the header (`#include "dp_state.h"`).
    Serialize only the *running* state — config is restored by `create()`. Pick
    the macro for the shape (see [Helper macros](#helper-macros--the-three-serializer-shapes)):

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
    post-block **state blob itself** is the resume observable.

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
[Enforcement](#enforcement--the-gate-it-cant-rot)) holds the line going forward —
a new object must declare `serializable = "true"` or opt out as stateless.

Covered: generators + loops (`LO`/`NCO`/`AWGN`/`PN`/`Costas`/`CarrierMpsk`/
`CarrierNda`/`LoopFilter`), `FIR`/`CIC`/`DDC`/`Ddcr`/`RateConverter`/`Resampler`/
`HalfbandDecimator`/`Acquisition`, the POD set (`Farrow`/`AGC`/`ADC`/the four
`acc_*` accumulators/the four `f32_to_*` quantizers), the field-wise set
(`delay`/`acc_trace`/`hbdecim_q15`), the compositions (`Dll`/`SymbolSync`/
`Channel`/`MpskReceiver`/`wfm_synth`), and the correlator/detector/analyzer
family (`corr`/`corr2d`/`detector`/`detector2d`/`despreader`/`psd`/`specan` —
opaque FFT plans + work buffers rebuilt by `create`; ring/pending buffers
zero-padded to a fixed capacity so blobs stay canonical).

### Open work

- **Orchestrator pod-handoff** — `CoarseChannel.get_state`/`set_state` composing
    its DDC + Acquisition blobs (the elastic payoff).
- **Rust FFI** — expose `get_state`/`set_state` over the C triplet (mechanical).
