# Capture I/O

A **capture** is a file of IQ samples plus whatever metadata the file type can
carry alongside them. `doppler.wfm` writes one with `Writer` and reads one back
with `Reader`, in either direction, entirely in C.

This section is the two halves of that:

| Page                           | What it covers                                                                       |
| ------------------------------ | ------------------------------------------------------------------------------------ |
| [Writing captures](writing.md) | Sample types, the four file types, byte order, sinks, the SigMF sidecar schema.      |
| [Reading captures](reading.md) | `Reader`, content-based file-type detection, and what metadata each file type keeps. |

Reading a capture has nothing to do with generating one, so neither page assumes
you produced the file with doppler — `Reader` opens captures it did not write,
which is the case that matters most.

______________________________________________________________________

## A round trip, end to end

Write a scene to a self-describing BLUE file, then read it back and recover the
metadata:

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

with Reader(tmp / "capture.blue") as r:      # file type detected from content
    assert (r.file_type, r.sample_type) == ("blue", "ci16")
    assert (r.fs, r.fc) == (2.4e6, 1.2e9)    # BLUE carries both
    back = r.read(r.num_samples)

assert len(back) == len(x)
tmpdir.cleanup()
```

Nothing about the *name* `capture.blue` is what made that work — the file type
is detected from the file's content. See
[The file type is detected from content](reading.md#the-file-type-is-detected-from-content-not-from-the-name).

______________________________________________________________________

## Three orthogonal choices

| Choice          | Values                    | What it decides                                      |
| --------------- | ------------------------- | ---------------------------------------------------- |
| **sample type** | `cf32 cf64 ci32 ci16 ci8` | the datatype on the wire; integers are full-scale ±1 |
| **file type**   | `raw csv blue sigmf`      | what wraps the samples, and what metadata survives   |
| **byte order**  | `le be`                   | byte order for the binary file types                 |

They compose freely — a `ci16` big-endian BLUE file and a `cf32` little-endian
raw file are both one `Writer(...)` call. The full flag reference is in
[Writing captures](writing.md#output-parameter-reference).

**The file type is the consequential one**, because it decides how much of the
second half of a capture — the metadata — can exist at all. `raw` and `csv` are
headerless: `fs`, `fc` and the sample type are things you must already know and
pass in. `blue` and `sigmf` are self-describing and hand them back to you. The
row-by-row table is in
[What each file type actually carries](reading.md#what-each-file-type-actually-carries).

______________________________________________________________________

## Two things that bite

Both come from the same root — a headerless file cannot contradict you — and
both are covered in [Reading captures](reading.md):

- **A wrong `sample_type` on a headerless capture does not raise.** It returns
    plausible garbage at the wrong stride.
    [`trailing_bytes`](reading.md#wrong-hints-and-truncation) is the signal, and
    the only one there is.
- **`fc == 0.0` is ambiguous** — a genuine baseband capture and a capture whose
    centre frequency could not be found report the same number.
    [`fc_source`](reading.md#centre-frequency-and-why-fc_source-exists) is what
    separates them.

______________________________________________________________________

## See also

- [Python: Capture I/O (Reader / Writer)](../../api/python-wfm-io.md) — the API
    reference for both classes and `write_blue_header`.
- [Gallery: Waveform I/O](../../gallery/wfm-io.md) — one capture round-tripped
    through all four file types, confirmed lossless.
- [Gallery: Waveform Write](../../gallery/wfm-write.md) — a minimal
    Composer → Writer → Reader walkthrough.
- [Waveform Generator (`wfmgen`)](../wfmgen/index.md) — generating the samples
    in the first place, and the CLI that writes them.
- [Type System → Reading interleaved I/Q](../../types.md#reading-interleaved-iq-in-python)
    — why a naive `np.fromfile` gets raw captures wrong.
