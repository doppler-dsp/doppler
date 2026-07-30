# Python Capture I/O API — Reader / Writer

`Reader` and `Writer` are duals: one turns `complex64` samples into a capture
file, the other turns a capture file back into `complex64` samples. Both are
CPython extension types over the C cores, so all parsing, deinterleaving and
rescaling happens in C — there is no Python fast path to fall off.

```python
from doppler.wfm import Reader, Writer, write_blue_header
```

| Symbol                                       | Direction | Use when                                                             |
| -------------------------------------------- | --------- | -------------------------------------------------------------------- |
| [`Writer`](#writer)                          | out       | Serialise samples to raw / CSV / BLUE type-1000 / SigMF              |
| [`Reader`](#reader)                          | in        | Open a capture — yours or someone else's — and stream unit-scale I/Q |
| [`write_blue_header`](#module-level-helpers) | out       | Write a **detached** BLUE header beside a payload written separately |

Source:
[`src/doppler/wfm/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/__init__.py)

The narrative versions of both sides are [Writing
captures](../guide/wfm-io/writing.md) and [Reading
captures](../guide/wfm-io/reading.md); for *generating* the samples in the first
place see [Python: Waveform Generator](python-wfmgen.md).

______________________________________________________________________

## A round trip

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
    assert (r.file_type, r.sample_type) == ("blue", "ci16")
    assert (r.fs, r.fc) == (2.4e6, 1.2e9)
    back = r.read(r.num_samples)

assert len(back) == len(x)
```

Both types are context managers, and both also expose `close()` (aliased
`destroy()`) for callers that manage the lifetime themselves. Closing a
`Writer` is what finalises the file — a BLUE header's sample count and a SigMF
sidecar are both written at close, so a `Writer` that is never closed leaves an
incomplete capture.

______________________________________________________________________

## `Writer`

`Writer(path, file_type="raw", sample_type="cf32", endian="le", fs=1e6, fc=0.0, total=0, headroom=0.0)`

The file type decides how much metadata the result can carry. `raw` and `csv`
are headerless — `fs`, `fc` and the sample type are written nowhere, so a reader
has to be told them. `blue` and `sigmf` are self-describing.

```python
# headerless: fs/fc are accepted but cannot be stored
with Writer(tmp / "capture.raw", file_type="raw", sample_type="ci16") as w:
    w.write(x)

# self-describing: fs and fc survive the round trip
with Writer(tmp / "capture.blue", file_type="blue", sample_type="ci16",
            fs=2.4e6, fc=1.2e9) as w:
    w.write(x)
```

A `sigmf` writer needs a path ending in **`.sigmf-data`** — the two halves of a
SigMF capture are found by name, so the name is part of the format. Anything
else is refused with a message saying so, rather than emitting a pair no SigMF
reader will locate:

```python
with Writer(tmp / "cap.sigmf-data", file_type="sigmf", sample_type="ci16",
            fs=2e6, fc=1.2e9) as w:
    w.write(x)
assert (tmp / "cap.sigmf-meta").exists()      # sidecar written at close
```

### Clipping

Integer sample types map ±1.0 to ±max-code, so content with PAPR above 0 dBFS
clips. `track_clipping()` turns on the counters, and `clip_fraction` /
`peak_dbfs` / `clipped` report what happened — measured on the way out, at no
cost when tracking is off:

```python
with Writer(tmp / "clip.raw", file_type="raw", sample_type="ci8") as w:
    w.track_clipping()
    w.write(x * 4.0)                  # deliberately over full scale
    assert w.clipped and w.clip_fraction > 0.0
```

`headroom` (dB) attenuates on the way out instead, so a scene with known peaks
lands under full scale without rescaling it yourself. See [Levels &
SNR](../guide/wfmgen/levels.md#scaling-to-the-wire-and-headroom).

### BLUE keywords

`add_keyword(tag, type, value)` appends a keyword to the extended header. The
`type` is the BLUE type code (`A` for ASCII, `B`/`I`/`L` integer, `F`/`D`
floating point, and so on), and the value may be a scalar or a sequence:

```python
with Writer(tmp / "kw.blue", file_type="blue", fs=1e6) as w:
    w.add_keyword("MISSION", "A", "doppler-demo")
    w.add_keyword("GAIN_DB", "D", -3.5)
    w.write(x[:64])

with Reader(tmp / "kw.blue") as r:
    assert r.keywords["MISSION"] == "doppler-demo"
    assert r.keywords["GAIN_DB"] == -3.5
```

______________________________________________________________________

::: doppler.wfm.compose.Writer

______________________________________________________________________

## `Reader`

`Reader(path, sample_type="cf32", endian="le")`

The constructor arguments are **hints, used only for a headerless file type**.
The actual file type comes from the file's content — BLUE magic at byte 0, a
`.sigmf-data` name with its required sidecar, a first line that scans as `I,Q`,
otherwise raw. So a CSV named `capture.dat` reads as CSV, and a BLUE file named
`capture.csv` reads as BLUE.

`read(count)` returns up to `count` samples as `complex64` at unit scale
whatever the wire type was, and an empty array at end of file — which makes the
block loop a `while`:

```python
with Reader(tmp / "capture.blue") as r:
    total = 0
    while len(block := r.read(4096)):
        total += len(block)
assert total == len(x)
```

Pass `out=` to read into a buffer you own instead of allocating per call.

### Two properties that exist because the obvious answer is ambiguous

`fc == 0.0` does not mean baseband — it also means "nothing in this file
declared a centre frequency". **`fc_source`** is what separates them, reporting
the tag that answered (`"FREQ"`, `"RF_FREQ"`, `"CENTER_FREQ"`, `"F_C"`,
`"core:frequency"`) or `"none"`:

```python
with Reader(tmp / "capture.blue") as r:
    assert r.fc_source == "FREQ"      # not a default — the file says 1.2 GHz
with Reader(tmp / "capture.raw", sample_type="ci16") as r:
    assert r.fc_source == "none"      # headerless: fc is a default, not a reading
```

**`trailing_bytes`** is the payload left over after the last whole sample. A
wrong `sample_type` on a headerless capture cannot fail — nothing in the file
can contradict it — so a non-zero remainder is the only signal that the hint is
wrong or the capture is truncated:

```python
with Writer(tmp / "short.raw", file_type="raw", sample_type="ci8") as w:
    w.write(x[:5])                             # 5 samples x 2 bytes = 10 bytes

with Reader(tmp / "short.raw", sample_type="cf32") as r:
    assert r.trailing_bytes == 2               # 10 bytes = one cf32 + 2 over
```

It cannot catch a wrong hint that happens to divide evenly (a `ci16` file read
as `ci8`), and it never false-alarms.

### BLUE introspection

`header` is the whole 512-byte header control block as a dict under the format's
own field names; `keywords` merges both keyword blocks into one `{tag: value}`
dict. Both are empty for a non-BLUE file type.

```python
with Reader(tmp / "capture.blue") as r:
    assert r.header["type"] == 1000            # type-1000, the I/Q workhorse
    assert r.header["xdelta"] == 1.0 / 2.4e6   # xdelta is 1/fs

tmpdir.cleanup()
```

______________________________________________________________________

::: doppler.wfm.compose.Reader

______________________________________________________________________

## Module-level helpers

`write_blue_header` writes a standalone BLUE header, for a **detached** capture
whose samples were written separately (a `.det` payload beside a `.hdr`
header). `Reader` opens either half and resolves the other.

::: doppler.wfm.write_blue_header

______________________________________________________________________

## Related pages

<!-- related-pages:start -->

**Gallery** — [type="symbols" — Bring Your Own Constellation](../gallery/symbols.md), [Composing a Scene — `.sum()`, `.add()`, and Headroom](../gallery/wfm-composition.md), [Waveform I/O — One Capture, Four File Types](../gallery/wfm-io.md), [Waveform Write — Compose, Write, Read Back](../gallery/wfm-write.md), [wfmgen — One Engine, Every Waveform](../gallery/wfmgen.md)
**Guides** — [Capture I/O](../guide/wfm-io/index.md), [Reading captures](../guide/wfm-io/reading.md), [Writing captures — output & file types](../guide/wfm-io/writing.md), [Levels & SNR](../guide/wfmgen/levels.md), [Python API](../guide/wfmgen/python.md)
**Contributing** — [Release Checklist](../dev/release.md)

<!-- related-pages:end -->
