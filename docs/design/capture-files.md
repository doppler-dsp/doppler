# Capture files — one reader, one writer, four containers

A capture has to survive leaving the process. `wfm_writer` and
`wfm_reader` are the two halves of that: cf32 in, bytes on disk, cf32
back. [Reading and writing captures](../guide/wfm-io/index.md) is how to
use them; this page is why they are shaped as they are.

Four containers, and the split that matters is not the file format:

|                    | self-describing | carries                                                            |
| ------------------ | --------------- | ------------------------------------------------------------------ |
| **BLUE** type-1000 | yes             | rate, byte order, sample type, length, keywords, a timecode        |
| **SigMF**          | yes             | rate, centre frequency, datatype, a start time — in a JSON sidecar |
| **raw**            | no              | nothing. Interleaved I/Q and a filename                            |
| **CSV**            | no              | nothing, and it is text                                            |

Everything below follows from that column: a self-describing capture can
be handed to someone, and a headerless one cannot be interpreted even by
the process that wrote it, ten minutes later.

______________________________________________________________________

## 1. The file type is decided by CONTENT

`wfm_reader_create` looks at the bytes: the BLUE magic at byte 0, a first
line that scans as `I,Q`, otherwise raw. A CSV called `capture.dat` reads
as CSV and a BLUE file called `capture.csv` reads as BLUE, so misnaming a
capture costs nothing.

**Two suffixes are decided by name instead, and only because they have no
content that identifies them:**

- `.det` — a detached BLUE payload, headerless by construction. Its
    header lives in a sibling file (§3).
- `.sigmf-data` — half of a pair whose *other* half carries the datatype.

That is the whole exception list, and it is short deliberately: extension
sniffing is how a file's name comes to outrank its bytes.

**Nothing is refused for looking unfamiliar.** An unrecognised file opens
as raw at the caller's `sample_type` hint, because a truncated or partial
recording is a real thing and a reader that rejects it is useless. What
you get instead of a refusal is §5.

______________________________________________________________________

## 2. One keyword codec, both directions

BLUE's extended header is a packed sequence of tag/value entries
(Midas BLUE 1.1 §3.3.1). `wfm/wfm_keywords.h` is the single codec:
`wfm_writer` encodes with it, `wfm_reader` decodes with it. **Two
implementations of one wire format is how the two halves come to disagree
about whether a `D` is eight bytes**, so there is one.

The same decision is made once more, one level in: the reader carries
**every field of the 512-byte header control block as a
`wfm_keyword_t`** too, under the name the format itself uses
(`data_start`, `ext_size`, `xdelta`). So `Reader.header` and
`Reader.keywords` share one tag/value marshaller rather than growing a
second that could turn a double into a Python object differently.

A reader advances by each entry's `lkey`, which is what lets a keyword of
an unrecognised type be **stepped over intact** rather than aborting the
parse. Metadata must never cost you the samples: a malformed keyword
region yields whatever decoded cleanly before it.

______________________________________________________________________

## 3. BLUE is two shapes, and detached is the interesting one

**Attached**: a 512-byte HCB then the payload, one file.

**Detached**: the header is `<base>.hdr` (BLUE 3.1.1.4 also allows
`.tmp`/`.prm`) and the payload is `<base>.det`. The HCB's `detached`
field decides, not the extension — and either half can be opened by name,
because the reader resolves the sibling.

That shape has a property worth naming, because §8 turns out to depend on
it: **the header is written last.** `wfmgen`'s detached path streams the
payload and only then writes the header with the real count, so a killed
run leaves a payload with no sibling — which is a *legible* failure. The
attached path cannot do that; it must go back and patch.

Only `format` modes `C` (interleaved I/Q) and `S` (real) are supported.
Every other Midas mode is **rejected at open** rather than reinterpreted
as interleaved I/Q, which is the failure that would look like working.

______________________________________________________________________

## 4. Provenance is a value, not a docstring caveat

`fc == 0.0` is a legitimate answer — a genuine baseband capture — and it
is also what a reader reports when it could not find a centre frequency
at all. Those are different facts and a caller acts differently on them,
so the reader returns **which metadata it read**, not just the number:

| accessor    | says                                                                 |
| ----------- | -------------------------------------------------------------------- |
| `fc_source` | the keyword's own tag (`FREQ`, `RF_FREQ`, …), SigMF's key, or `none` |
| `fs_source` | BLUE `xdelta`, SigMF `core:sample_rate`, or `none`                   |
| `t0_source` | BLUE `timecode`, or `none`                                           |

`t0_source` is the one that bites. BLUE stores time as a J1950 timecode,
and **doppler's own writer leaves the field zero** — so a zero there
means *unset*, never 1950-01-01. A reader that skips the check dates
every capture this library writes to 1950. `none` is the common answer,
not an edge case, which is exactly why it has to be visible.

`fc` also shows why BLUE needs keywords at all: **type-1000 has no header
field for centre frequency.** The adjunct carries `xstart`/`xdelta`,
which describe the abscissa, not the RF. So an RF capture conveys it as a
keyword, and `FREQ` is X-Midas convention rather than anything BLUE 1.1
mandates — the spec defines `FREQ` only as a type-6000 *column* name,
under a heading saying those are not keyword names. It is nonetheless
what real captures carry, so it is what the reader looks for first.

______________________________________________________________________

## 5. What the reader cannot know, and says so

A wrong `sample_type` hint on a headerless file **does not fail**. It
returns plausible garbage at the wrong stride, and nothing in the samples
says so. There is exactly one tell:

`trailing_bytes` — payload bytes left over after the last whole sample.
Zero for a capture whose declared type and mode match its content. Non-
zero means one of two things and **the reader cannot tell them apart**:
the hint is wrong for a headerless type, or the capture is truncated.
Either way the leftovers are dropped.

That it reports the symptom without diagnosing the cause is the honest
shape. Guessing between the two would be a diagnosis the file cannot
support.

______________________________________________________________________

## 6. Metadata with nowhere to go

Raw and CSV take `fs`, `fc` and `t0` at construction and have nowhere to
put them — and until recently discarded them, handing back a capture not
even its author could interpret afterwards. The containers have no room;
a sidecar is room.

So a **path-opened** raw or CSV writer emits `<path>.sigmf-meta`
alongside. Three deliberate choices in that:

- **SigMF-*shaped*, not a SigMF capture.** Documented as a sidecar,
    never advertised as SigMF.
- **The name is APPENDED, not swapped** (`cap.raw` → `cap.raw.sigmf-meta`).
    Swapping would give `cap.raw` and `cap.sigmf-data` in one directory the
    same sidecar name, so writing one would silently describe the other.
    Appending keeps it 1:1 with the file it describes.
- **BLUE gets none.** Its header already carries all three, and a second
    copy is only somewhere for them to drift.

An `fp`-opened writer has no name to derive one from, so the caller owns
it — which is why the sidecar is a `create()`-only guarantee.

______________________________________________________________________

## 7. The quantiser, and a known defect

Full scale is ±1.0 per axis. Integer wire types scale to their maximum
(`ci32` 2³¹−1, `ci16` 32767, `ci8` 127) and saturate there; float types
never clip but are still tracked.

- **Peak is always on** — a fused max in the write loop, free.
- **The clip *fraction* is opt-in** (`track_clipping`), because a
    per-component compare is the one extra per-sample cost.
- **`headroom` is a single scale** applied before quantisation, so it
    changes no power ratio — SNR is invariant, only the absolute level
    moves. `0 dB` is a bit-exact no-op.

Peak is also the remedy, not just the diagnosis: `ceil(peak_dbfs)` dB of
headroom is exactly what makes the capture fit.

**Defect, open:** `qz()` is `return (long)(v * scale)` — a C cast, which
**truncates toward zero**. Every integer capture therefore reads back up
to one LSB low, with a −0.5 LSB DC bias. Rounding is the same cost. This
is the same defect class as the `norm_freq → phase_inc` conversion that
was rounding in `lo_core.c` and truncating in `nco_core.c`; found while
building a bit-exactness harness, and filed rather than fixed in passing.

______________________________________________________________________

## 8. The writer streams, and that decides everything about endings

`wfm_writer_write` may be called any number of times; the capture is the
concatenation. Nothing buffers the whole thing, because a capture may be
larger than memory.

Two consequences, and both land at `close()`:

- **Keywords are written after the data.** BLUE §3.3 recommends exactly
    this for a stream, since the total data size is unknown until the end.
    `ext_start`/`ext_size` are patched into the HCB at the same moment.
- **`data_size` is patched from the actual count**, which is why
    `total_samples` at open is a *hint* and `0` is fine.

So **a capture that is never closed is incomplete**, and for an attached
BLUE capture, incomplete in a way that reads as empty rather than as
short: a killed writer leaves the placeholder `data_size`, and the reader
enforces it as a bound.

That is where this subsystem meets the wait contract.
[Ending a Capture](end-of-capture.md) is that story — following a capture
while it is still being written, and stopping both halves cleanly — and
it is a *behaviour of this design* rather than a separate one. The
`0 → N` transition of `data_size` at close is the end-of-capture marker
precisely because §8 puts it there.

______________________________________________________________________

## 9. Layering

```text
   wfm/wfm_keywords.h   the tag/value codec        ← both halves
   wfm/wfm_path.h       sidecar name derivation    ← both halves
   wfm/wfm_time.h       J1950 ⇄ UNIX               ← both halves
        │
   wfm_writer_core ──── wfm_filetype_t, the HCB writer
        │                     ▲
   wfm_reader_core ──────────-┘  (reader includes the writer's header, and
                                  links it: its C test round-trips through
                                  the writer it then reads back)
```

The reader depending on the writer is deliberate and is a *test* coupling
made structural: a round-trip test that writes with anything other than
the shipped writer proves less. jm has no test-only dependency, so the
writer objects land in the reader module too — a few KB, and the price of
keeping the round-trip honest.

______________________________________________________________________

## 10. What this does not do

- **No format conversion.** The reader emits `complex64` at unit scale
    whatever the wire type was; converting a capture from one container to
    another is a caller composing a reader and a writer.
- **No repair.** A truncated capture is *reported* (§5, §8), never
    reconstructed.
- **No SigMF `core:datetime` parsing.** It is an ISO 8601 string and the
    reader has no parser, so such a capture reports `t0_source = none`
    rather than a guess.
- **No indexing or seeking by time.** `reset()` rewinds to the first
    sample; that is the whole of the random access on offer.
