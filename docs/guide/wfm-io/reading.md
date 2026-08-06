# Reading captures

`doppler.wfm.Reader` is the dual of [`Writer`](writing.md): it opens a capture,
works out what file type it is, and streams unit-scale `complex64` samples out
of it.

```python
import pathlib
import tempfile

from doppler.wfm import Composer, Reader, Segment, Writer

tmpdir = tempfile.TemporaryDirectory()
tmp = pathlib.Path(tmpdir.name)
x = Composer([Segment("qpsk", sps=8, snr=15, fs=2.4e6,
                      num_samples=8192)]).compose()
with Writer(tmp / "capture.blue", file_type="blue", sample_type="ci16",
            fs=2.4e6, fc=1.2e9) as w:
    w.write(x)

with Reader(tmp / "capture.blue") as r:
    print(r.file_type, r.sample_type, r.fs, r.fc)
    total = 0
    while len(block := r.read(4096)):
        total += len(block)
assert total == len(x)
```

______________________________________________________________________

## What each file type actually carries

A capture is samples plus the metadata needed to interpret them, and the
file types differ enormously in how much of that second part they can hold.
This table is the honest state of it — read down your file type's column
before assuming a field will survive a round trip.

|                          | `raw` | `csv` |         `blue`          | `sigmf` |
| ------------------------ | :---: | :---: | :---------------------: | :-----: |
| samples                  |   ●   |   ●   |            ●            |    ●    |
| `sample_type`            |   ○   |   ○   |            ●            |    ●    |
| `endian`                 |   ○   |  n/a  |            ●            |    ●    |
| `fs`                     |   ○   |   ○   |            ●            |    ●    |
| `fc`                     |   ○   |   ○   |            ●            |    ●    |
| `num_samples`            |   ●   |   ●   |            ●            |    ●    |
| `mode` (real vs I/Q)     |   ○   |   ○   |            ●            |    ○    |
| keywords / free metadata |   ○   |   ○   |            ●            |    ○    |
| one file or two          |  one  |  one  | one *(two if detached)* | **two** |

● carried by the file · ○ not carried — supplied by you, or lost

**The `○` rows are the ones that bite.** `raw` and `csv` are headerless: the
sample type and byte order you pass to `Reader` are *hints*, and nothing in
the file can confirm or contradict them. A wrong hint does not raise — it
returns plausible garbage at the wrong stride. See
[Wrong hints and truncation](#wrong-hints-and-truncation).

`num_samples` is now populated for every file type. A CSV has no header to
declare its length, so the first read of the property scans the file once and
counts rows exactly the way `read` parses them; later reads are free, and the
read position is untouched.

______________________________________________________________________

## The file type is detected from content, not from the name

Detection order, first match wins:

1. the **BLUE magic** at byte 0
1. a `.sigmf-data` name, whose `.sigmf-meta` sidecar is then required
1. a first line that scans as `I,Q` → CSV
1. otherwise **raw**, at the `sample_type`/`endian` you passed

So a CSV saved as `capture.dat` still reads as CSV, and a BLUE file saved as
`capture.csv` still reads as BLUE. The name only breaks ties the content
cannot: a `.det` payload (headerless by construction — its header sibling
describes it) and a CSV whose first line is a column header.

Nothing is refused for looking unfamiliar. An unrecognised file opens as raw,
because a partial or truncated recording is a real thing and a reader that
rejects it is useless.

______________________________________________________________________

## Centre frequency, and why `fc_source` exists

`0.0` is a legitimate centre frequency. A genuine baseband capture and a
capture whose frequency `Reader` could not find both report `fc == 0.0`, so
the number alone cannot be trusted — **`fc_source` is what separates them**:

```python
with Reader(tmp / "capture.blue") as r:
    if r.fc_source == "none":
        origin = "nothing declares it; fc is a default, not a reading"
    else:
        origin = f"declared as {r.fc} Hz by the {r.fc_source} keyword"
print(origin)
assert origin.startswith("declared")   # this capture carries FREQ
```

BLUE type-1000 has **no header field for centre frequency** — the adjunct's
`xstart`/`xdelta`/`xunits` describe the abscissa (time), not the RF. So an RF
capture conveys it as a keyword, and which tag it uses is X-Midas convention
rather than anything the format mandates: BLUE 1.1 §3.1.2.6.4.4 defines `FREQ`
only as a type-6000 *column* name, under a heading stating those names "are not
keyword names".

`Reader` therefore tries the conventional tags in order — `FREQ`, `RF_FREQ`,
`CENTER_FREQ`, `F_C` — and reports which one answered. Both encodings are
accepted, because captures in the wild use both:

- **ASCII, in the HCB keyword area** (§3.1.1.24.1: `KEY=VALUE\0` text at offset
    164, no type field). This is where real X-Midas captures put it.
- **Typed, in the extended header** (§3.3.1), where a `D` keeps full double
    precision.

A value that is not a bare number is left alone rather than guessed at:
`FREQ=2.4 GHz` yields `fc_source == "none"`, and the string stays visible in
`.keywords` for a caller who knows the convention. Reading it as `2.4` would
be wrong by a factor of a billion.

`Writer` writes **both** copies for a non-zero `fc`. The typed extended-header
one is §3.4-compliant and authoritative; the ASCII mirror is what an X-Midas
reader will actually look for. §3.4 reserves that 92-byte area for six standard
keywords and warns that X-Midas may *delete* a user keyword found there to make
room for `IO`/`VER` — which is exactly why it is the mirror and not the
original. `Reader` prefers the typed copy, so the pair can never be read as
disagreeing.

______________________________________________________________________

## Wrong hints and truncation

`trailing_bytes` is the payload bytes left over after the last whole sample.
It is `0` for any capture whose declared sample type and mode match its
content, and always `0` for CSV (delimited, not strided).

Non-zero means one of two things, and the reader cannot tell which:

- the `sample_type`/`endian` hint is **wrong** for a headerless file type, or
- the capture is **truncated** — cut mid-sample.

Either way the leftover bytes are dropped; `read` stops at the last complete
sample. For a headerless file type this is the only signal there is:

```python
# a ci8 capture, deliberately read back with the wrong hint
with Writer(tmp / "capture.raw", fs=1e6, file_type="raw",
            sample_type="ci8") as w:
    w.write(x[:5])                       # 5 samples, 2 bytes each = 10 bytes

with Reader(tmp / "capture.raw", sample_type="cf32") as r:
    if r.trailing_bytes:
        print(f"{r.trailing_bytes} bytes do not fit — wrong sample_type, "
              f"or the capture is truncated")
    assert r.trailing_bytes == 2         # 10 bytes is one cf32 sample + 2
```

Note it cannot catch every wrong hint: reading a `ci16` file as `ci8` gets the
stride wrong but still divides evenly, so `trailing_bytes` stays `0`. It
catches the mismatches that leave a remainder, which is most of them, and it
never reports a false alarm.

______________________________________________________________________

## BLUE specifics

**`.header`** is the whole 512-byte header control block as a dict, under the
names the format itself uses (`version`, `data_start`, `data_size`, `format`,
`keylength`, `xstart`, `xdelta`, `xunits`, …). Nothing is renamed or dropped,
so what you see is what the file holds.

**`.keywords`** merges both keyword blocks — the HCB's own area and the
extended header — into one `{tag: value}` dict, so a caller cannot tell which
block carried a key. Values follow the keyword's type: `str` for `A`, `int` or
`float` for a single-element numeric, a `list` for a multi-element one. An HCB-
area value is always a `str`, since that area has no type field.

**Detached captures** (`detached = 1`) split into a header file and a `.det`
payload. Open either one: given the header, `Reader` resolves the collocated
`.det`; given the `.det`, it looks for the header as `.hdr`, `.prm` or `.tmp`.
The extension never decides — the HCB's `detached` field does.

______________________________________________________________________

## SigMF specifics

A SigMF capture is a **pair**: `<base>.sigmf-data` holds the samples and
`<base>.sigmf-meta` holds the JSON without which they cannot be decoded. Both
halves are found by name, so the name is part of the format — `Writer` requires
a path ending in `.sigmf-data` and emits the sidecar itself at close:

```python
with Writer(tmp / "capture.sigmf-data", file_type="sigmf",
            sample_type="ci16", fs=2e6, fc=1.2e9) as w:
    w.write(x)          # capture.sigmf-meta is written on close

assert (tmp / "capture.sigmf-meta").exists()
with Reader(tmp / "capture.sigmf-data") as r:
    assert (r.sample_type, r.fs, r.fc) == ("ci16", 2e6, 1.2e9)

tmpdir.cleanup()
```

`fc` comes from `captures[0]["core:frequency"]`, and `fc_source` reports it as
`"core:frequency"`.

For a capture with per-segment **annotations** — ground truth for scoring a
detector — build it through
[`Composer`](../wfmgen/python.md), which knows the scene; a plain `Writer` has nothing to
annotate and emits an empty `annotations` array. The sidecar's schema is
documented in [Output & file types](writing.md#sigmf-sidecar-schema).
