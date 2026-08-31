# Writing captures — output & file types

The sample **type** (the datatype), the **file type** (the file format), and the
**byte order** are three orthogonal choices. This page is the write side of
capture I/O; [Reading captures](reading.md) is the read side, and its
[metadata table](reading.md#what-each-file-type-actually-carries) is the one to
check before assuming a field survives the round trip.

## Output parameter reference

| Flag              | Values                    | Default | Meaning                                                                     |
| ----------------- | ------------------------- | ------- | --------------------------------------------------------------------------- |
| `--sample-type`   | `cf32 cf64 ci32 ci16 ci8` | `cf32`  | wire type; integers are full-scale ±1.0                                     |
| `--file-type`     | `raw csv blue sigmf`      | `raw`   | file type (below)                                                           |
| `--endian`        | `le be`                   | `le`    | byte order (raw/BLUE only; csv is text)                                     |
| `--output` / `-o` | path *(or `nats://…`)*    | stdout  | sink                                                                        |
| `--record`        | path                      | —       | write a JSON record of the resolved run (see [Scenes](../wfmgen/scenes.md)) |

The integer sample types map ±1.0 → ±max-code (and can clip on PAPR > 0 dB
content); see [Levels & SNR → Scaling to the wire](../wfmgen/waveforms.md#scaling-to-the-wire-and-headroom).

Reading these back — and what metadata each file type actually preserves — is
[Reading captures](reading.md).

______________________________________________________________________

## File Types

| `--file-type` | Output                                        | Notes                                                      |
| ------------- | --------------------------------------------- | ---------------------------------------------------------- |
| `raw`         | interleaved I/Q in the chosen `--sample-type` | the SDR default; honors `--endian`                         |
| `csv`         | one `I,Q` line per sample                     | `%0.9f` cf32, `%0.17g` cf64, `%d` integer; text, no endian |
| `blue`        | **X-Midas / REDHAWK BLUE type-1000**          | *(`wfmgen` only)* self-describing 512-byte header          |
| `sigmf`       | `<base>.sigmf-data` + `<base>.sigmf-meta`     | *(`wfmgen` only)* one annotation per segment               |

**BLUE type-1000** writes a complete 512-byte X-Midas/REDHAWK Header Control
Block so one file is fully self-describing: `data_rep`←`--endian`, `format`
(`CB`/`CI`/`CL`/`CF`/`CD`)←`--sample-type`, and `xdelta = 1/fs`. Add
**`--detached`** to split it into a header + data pair — `<out>.hdr` (the HCB,
with `detached=1` and `data_start=0`) and `<out>.det` (the raw samples). Detached
output requires `--output` and a finite (non-`--continuous`) run; attached mode
keeps whatever extension you give `-o` (`.blue`/`.prm`/`.tmp`).

**SigMF** writes the samples as `raw` into `<base>.sigmf-data` and a JSON sidecar
`<base>.sigmf-meta` with `core:datatype`/`core:sample_rate`, a capture at `--fc`,
and one annotation per composer segment (frequency edges, label, `wfmgen:*`
params).

### raw and CSV keep their metadata beside them

`raw` and `csv` have nowhere in the file to record `fs`, `fc` or `t0` — and the
`Writer` takes all three. It used to discard them, handing back a capture that
not even its author could interpret an hour later. A path-opened writer now
writes them to a sidecar instead:

```pycon
>>> import json, pathlib, tempfile
>>> import numpy as np
>>> from doppler.wfm import Writer
>>> tmp = tempfile.TemporaryDirectory()
>>> p = pathlib.Path(tmp.name) / "capture.raw"
>>> with Writer(p, fs=2.4e6, fc=1.2e9) as w:
...     w.write(np.zeros(64, dtype=np.complex64))
64
>>> doc = json.loads((p.parent / "capture.raw.sigmf-meta").read_text())
>>> doc["global"]["core:sample_rate"], doc["captures"][0]["core:frequency"]
(2400000, 1200000000)
>>> tmp.cleanup()

```

Four things to know about it:

- **The name is appended, not swapped** — `capture.raw` →
    `capture.raw.sigmf-meta`. Swapping would make `capture.raw` and a genuine
    `capture.sigmf-data` in one directory fight over `capture.sigmf-meta`, so
    writing one capture would silently retype the other.
- **It is SigMF-*shaped*, not a SigMF capture.** The spec pairs `.sigmf-data`,
    so nothing conformant goes looking for this file. For `csv`,
    `core:datatype` names the value domain the samples were quantised to
    rather than a byte layout.
- **Only what you stated is written.** An unspecified `fs`, `fc` or `t0` is an
    absent key, never a confident zero — the file exists to stop a capture
    asserting things nobody said.
- **`sidecar=False` opts out**, for when an extra file beside the capture would
    break a downstream glob. `blue` never gets one (its header already carries
    all three); `sigmf` always does and cannot turn it off, because there the
    sidecar *is* half the capture.

```sh
# 16-bit big-endian into a self-describing BLUE file
wfmgen --type qpsk --count 200000 --sample-type ci16 --endian be \
       --file-type blue -o capture.blue

# a SigMF pair (capture.sigmf-data + capture.sigmf-meta)
wfmgen json-template > scenario.json      # or bring your own scene spec
wfmgen --from-file scenario.json --sample-type ci16 --file-type sigmf -o capture

# --fc records the RF centre the baseband was taken at. It changes no
# sample -- it is the one number that says where in the spectrum this was.
wfmgen --type tone --freq 1e5 --fs 1e6 --fc 2.4e9 --count 4096 \
       --sample-type ci16 --file-type sigmf -o tuned
python3 -c "
import json
print(json.load(open('tuned.sigmf-meta'))['captures'][0])"
#   {'core:sample_start': 0, 'core:frequency': 2400000000}
```

### SigMF sidecar schema

The `.sigmf-meta` JSON is SigMF 1.0.0 with one **annotation per source per
segment**, so a multi-segment / multi-source scene becomes a self-labelling
ground-truth capture. The exact shape `wfmgen` (and `Composer.to_sigmf`) emit —
see `native/src/wfm_writer/wfm_writer_core.c`:

```json
{
  "global": {
    "core:datatype": "ci16_le",
    "core:sample_rate": 1000000,
    "core:version": "1.0.0",
    "core:description": "doppler wfmgen",
    "core:author": "doppler wfmgen"
  },
  "captures": [
    { "core:sample_start": 0, "core:frequency": 2400000000.0 }
  ],
  "annotations": [
    {
      "core:sample_start": 0,
      "core:sample_count": 4096,
      "core:freq_lower_edge": -62500.0,
      "core:freq_upper_edge": 62500.0,
      "core:label": "qpsk",
      "wfmgen:snr": 20.0,
      "wfmgen:snr_mode": "esno",
      "wfmgen:sps": 8,
      "wfmgen:seed": 1,
      "wfmgen:pn_length": 7,
      "wfmgen:pn_poly": 0
    }
  ]
}
```

- `core:datatype` is `<sample_type>_<endian>` (`cf32_le`, `ci16_be`, …).
- `captures[0].core:frequency` is `--fc` (the RF centre); annotation frequency
    edges are **baseband** offsets from it — a chirp spans `f_start..f_end`, a
    modulated source is roughly `±fs/(2·sps)` wide, a tone is a point.
- `core:label` is the source type; the `wfmgen:*` keys carry the generator
    parameters so the capture round-trips to the spec that made it.

`Composer.to_sigmf(sample_type="cf32", endian="le", fc=0.0)` returns this
document as a string; pair it with a `Writer(..., file_type="sigmf")` data file.

`fs` is **derived from the segments** when you don't pass one — they already
carry a rate, and the annotation edges above are computed from it, so the
document states the rate it was built with instead of staying silent about it.
Segments that disagree leave `core:sample_rate` out (no single rate is true of
that stream), and an explicit `fs=` always wins, for rendering a scene at a
resampled rate.

______________________________________________________________________

## Sinks

| `--output`                 | Result                                                                                            |
| -------------------------- | ------------------------------------------------------------------------------------------------- |
| *(omitted)*                | binary stream to **stdout** (pipe it)                                                             |
| `file.iq`                  | write to a file                                                                                   |
| `nats://127.0.0.1:4222/iq` | *(`wfmgen` only)* publish to a **NATS PUB** endpoint (SIGS wire format); requires a `nats-server` |

```sh
wfmgen --type tone --count 1000000 | other-tool                    # pipe via stdout
wfmgen --type tone --continuous --output nats://127.0.0.1:4222/iq  # stream forever to NATS
```

A `dp_sub_*` subscriber (e.g. `native/examples/spectrum_analyzer`) reads the NATS
stream. For pacing a live stream to the true sample rate, see
[Streaming](../wfmgen/scenes.md#streaming-real-time-pacing).

______________________________________________________________________

## Reading a capture back

The `raw` file type is **interleaved** I/Q in the chosen `--sample-type`, so a
naive `np.fromfile` gets the layout (and, for integers, the scale) wrong.
`Reader` does the right thing entirely in C — deinterleaving and rescaling any
wire type to unit-scale `complex64` — and auto-detects BLUE/SigMF/CSV/raw,
recovering `fs`/`fc`/sample-type from metadata:

<!-- docs-snippet: skip=illustrative: reads an I/Q capture file you supply -->

```python
from doppler.wfm import Reader

with Reader("capture.iq", sample_type="ci16") as r:  # hint for headerless raw
    iq = r.read(r.num_samples)                        # → complex64, ±1.0
with Reader("capture.blue") as r:                     # file type auto-detected
    print(r.file_type, r.fs, r.num_samples)
    x = r.read(r.num_samples)                          # or block-wise: r.read(4096)
```

`generate → Reader.read` is bit-faithful. See
[Type System → Reading interleaved I/Q](../../types.md#reading-interleaved-iq-in-python)
and the [Python API](../wfmgen/python.md) page.

______________________________________________________________________

## See also

- [Gallery: Waveform I/O](../../gallery/wfm-io.md) — round-tripping one capture
    through all four file types, visually confirmed lossless.
- [Gallery: Waveform Write](../../gallery/wfm-write.md) — a minimal
    Composer → Writer → Reader walkthrough.
