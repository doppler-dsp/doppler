# Output & containers

The sample **type** (the datatype), the **container** (the file format), and the
**byte order** are three orthogonal choices.

## Output parameter reference

| Flag              | Values                    | Default | Meaning                                                           |
| ----------------- | ------------------------- | ------- | ----------------------------------------------------------------- |
| `--sample-type`   | `cf32 cf64 ci32 ci16 ci8` | `cf32`  | wire type; integers are full-scale ±1.0                           |
| `--file-type`     | `raw csv blue sigmf`      | `raw`   | container (below)                                                 |
| `--endian`        | `le be`                   | `le`    | byte order (raw/BLUE only; csv is text)                           |
| `--output` / `-o` | path *(or `nats://…`)*    | stdout  | sink                                                              |
| `--record`        | path                      | —       | write a JSON record of the resolved run (see [Scenes](scenes.md)) |

The integer sample types map ±1.0 → ±max-code (and can clip on PAPR > 0 dB
content); see [Levels & SNR → Scaling to the wire](levels.md#scaling-to-the-wire-and-headroom).

______________________________________________________________________

## Containers

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

```sh
# 16-bit big-endian into a self-describing BLUE file
wfmgen --type qpsk --count 200000 --sample-type ci16 --endian be \
       --file-type blue -o capture.blue

# a SigMF pair (capture.sigmf-data + capture.sigmf-meta)
wfmgen --from-file scenario.json --sample-type ci16 --file-type sigmf -o capture
```

### SigMF sidecar schema

The `.sigmf-meta` JSON is SigMF 1.0.0 with one **annotation per source per
segment**, so a multi-segment / multi-source scene becomes a self-labelling
ground-truth capture. The exact shape `wfmgen` (and `Composer.to_sigmf`) emit —
see `native/src/wfm/wfm_writer.c`:

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

`Composer.to_sigmf(sample_type="cf32", endian="le", fs=1e6, fc=0.0)` returns this
document as a string; pair it with a `Writer(..., file_type="sigmf")` data file.

______________________________________________________________________

## Sinks

| `--output`                 | Result                                                                                            |
| --------------------------- | --------------------------------------------------------------------------------------------------- |
| *(omitted)*                | binary stream to **stdout** (pipe it)                                                              |
| `file.iq`                  | write to a file                                                                                     |
| `nats://127.0.0.1:4222/iq` | *(`wfmgen` only)* publish to a **NATS PUB** endpoint (SIGS wire format); requires a `nats-server`   |

```sh
wfmgen --type tone --count 1000000 | other-tool                    # pipe via stdout
wfmgen --type tone --continuous --output nats://127.0.0.1:4222/iq  # stream forever to NATS
```

A `dp_sub_*` subscriber (e.g. `examples/c/spectrum_analyzer`) reads the NATS
stream. For pacing a live stream to the true sample rate, see
[Streaming](streaming.md).

______________________________________________________________________

## Reading a capture back

The `raw` container is **interleaved** I/Q in the chosen `--sample-type`, so a
naive `np.fromfile` gets the layout (and, for integers, the scale) wrong.
`read_iq` does the right thing — a zero-copy complex view for the float types, a
SIMD rescale to ±1.0 for the integer types; the container-aware `Reader` also
auto-detects BLUE/SigMF/CSV/raw and recovers `fs`/`fc`/sample-type from metadata:

<!-- docs-snippet: skip=illustrative: reads an I/Q capture file you supply -->

```python
from doppler.wfm import read_iq, Reader

iq = read_iq("capture.iq", sample_type="ci16")   # → complex64, ±1.0
with Reader("capture.blue") as r:                 # container auto-detected
    print(r.file_type, r.fs, r.num_samples)
    x = r.read(r.num_samples)                       # or block-wise: r.read(4096)
```

`generate → read_iq` is bit-faithful. See
[Type System → Reading interleaved I/Q](../../types.md#reading-interleaved-iq-in-python)
and the [Python API](python.md) page.
