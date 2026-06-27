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

Two cases need more than the flag:

- **Sacred fragments.** An object whose `_ext_<obj>.c` fragment is hand-owned
    (a bespoke property, a custom `execute`) is not regenerated by `jm apply`, so
    the triplet is *hand-added* to match `jm`'s form (the flag still generates the
    `.pyi`). `DDC` (its gh-219 independent-array execute) and `RateConverter` (its
    `stages` property and self-pinned view execute) are the two such objects.
- **Handle modules.** A `kind="handle"` module (e.g. `ddc_fn`'s `Ddcr`) is
    generated glue that `serializable` does not yet cover — see
    [Open work](#open-work).

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

## Status

| Type                              | C triplet | Python ser/des | Binding                       |
| --------------------------------- | --------- | -------------- | ----------------------------- |
| `LO`, `CIC`, `FIR`, `Acquisition` | ✅        | ✅             | `jm`-auto from the flag       |
| `DDC`, `RateConverter`            | ✅        | ✅             | hand-added to sacred fragment |
| `Ddcr` (`ddc_fn`)                 | ✅        | ⛔             | blocked on a `jm` feature     |

### Open work

- **`jm` handle serializable** — honor `serializable` on `kind="handle"` modules
    so `Ddcr`'s existing C triplet gets a Python face automatically.
- **`jm` sacred-fragment transplant** — inject the triplet into hand-owned
    fragments the way `jm` already transplants docstrings, retiring the `DDC` /
    `RateConverter` hand-adds.
- **More C triplets** — `NCO`, `AWGN`, `Farrow`, `HBDecimQ15`, `Despreader` are
    not yet serializable; each needs a hand C serializer, then the flag.
