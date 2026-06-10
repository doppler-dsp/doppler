# Waveform Generator — `wavegen` & `wfmgen`

doppler ships a C-first **waveform generator**: one declarative synth engine
(every algorithm in C, exactly once) exposed three ways —

- **`wavegen`** — a quick, single-waveform CLI (generated in three byte-identical
    faces: a C binary, a Python console script, and a PEP 723 script).
- **`wfmgen`** — a hand-written C composer for **multi-segment** scenarios, the
    **BLUE** / **SigMF** containers, and streaming to **ZMQ**.
- **`doppler.wfmgen`** — the same engine as a Python API.

![wfmgen engine](../assets/wfmgen_demo.png)

A single-segment `wfmgen` run is **byte-identical** to the same `wavegen` call —
they share the engine. Start with `wavegen`; reach for `wfmgen` when you need
multiple segments, BLUE/SigMF, or a ZMQ stream.

!!! tip "The 30-second version"

    ```sh
    wavegen --type qpsk --snr 12 --count 100000 -o capture.cf32   # 100k QPSK samples @ 12 dB Es/No
    wavegen --type tone --freq 1e5 --count 4096                    # a tone → stdout (cf32)
    wavegen --type pn --pn_length 9 --file_type csv -o pn.csv      # length-9 MLS as text
    ```

______________________________________________________________________

## Installation

```sh
pip install doppler-dsp        # → the `wavegen` command + the doppler.wfmgen API
```

The `wavegen` console script and the `doppler.wfmgen` Python module install with
the wheel. The **`wfmgen`** composer binary is POSIX-only (it links the vendored
ZMQ) and is built from source:

```sh
git clone https://github.com/doppler-dsp/doppler && cd doppler
cmake -B build -DBUILD_PYTHON=ON && cmake --build build --target wfmgen_cli wavegen
# binaries: build/native/src/wfmcompose/wfmgen   and   build/wavegen
```

______________________________________________________________________

## Waveform types

`--type` selects the waveform; every type shares the same parameter set.

| `--type` | What it is                                                 | Key parameters                      |
| -------- | ---------------------------------------------------------- | ----------------------------------- |
| `tone`   | a complex sinusoid at `--freq`                             | `--freq`                            |
| `noise`  | complex AWGN (unit power)                                  | `--snr` (ignored — it *is* noise)   |
| `pn`     | a maximum-length sequence (±1 chips), `--sps` samples/chip | `--pn_length`, `--pn_poly`, `--sps` |
| `bpsk`   | BPSK symbols (PN-sourced data), `--sps` samples/symbol     | `--sps`, `--snr`                    |
| `qpsk`   | Gray-coded QPSK symbols (PN-sourced data)                  | `--sps`, `--snr`                    |

The data bits for `bpsk`/`qpsk` come from a deterministic PN sequence (seeded by
`--seed`), so output is reproducible and receiver-correlatable.

______________________________________________________________________

## Parameter reference

### Engine (shared by `wavegen` and `wfmgen`)

| Flag          | Type                      | Default  | Meaning                                                       |
| ------------- | ------------------------- | -------- | ------------------------------------------------------------- |
| `--type`      | `tone noise pn bpsk qpsk` | `tone`   | waveform                                                      |
| `--fs`        | float (Hz)                | `1e6`    | sample rate                                                   |
| `--freq`      | float (Hz)                | `0`      | frequency offset from baseband (mixed by the LO)              |
| `--snr`       | float (dB)                | `100`    | SNR; metric chosen by `--snr_mode` (≈clean at 100)            |
| `--snr_mode`  | `auto fs ebno esno`       | `auto`   | how `--snr` is interpreted (see below)                        |
| `--seed`      | uint32                    | `1`      | PRNG / LFSR seed (deterministic)                              |
| `--sps`       | int                       | `8`      | samples per symbol (`*psk`) / per chip (`pn`)                 |
| `--pn_length` | int (2..64)               | `7`      | LFSR register length → period `2ⁿ−1`                          |
| `--pn_poly`   | uint64                    | `0`      | LFSR polynomial; `0` ⇒ auto-pick the MLS polynomial           |
| `--lfsr`      | `galois fibonacci`        | `galois` | LFSR realization (same polynomial/period, different sequence) |
| `--count`     | int                       | `1024`   | number of complex samples to generate                         |

### Output

| Flag              | Values                                   | Default | Meaning                                   |
| ----------------- | ---------------------------------------- | ------- | ----------------------------------------- |
| `--sample_type`   | `cf32 cf64 ci32 ci16 ci8`                | `cf32`  | wire type; integers are full-scale ±1.0   |
| `--file_type`     | `raw csv` *(+ `blue sigmf` in `wfmgen`)* | `raw`   | container (see [Containers](#containers)) |
| `--endian`        | `le be`                                  | `le`    | byte order (raw/BLUE only; csv is text)   |
| `--output` / `-o` | path *(or `zmq://…` in `wfmgen`)*        | stdout  | sink                                      |
| `--record`        | path                                     | —       | write a JSON record of the resolved run   |

### `wfmgen`-only (the composer)

| Flag                    | Meaning                                                                                       |
| ----------------------- | --------------------------------------------------------------------------------------------- |
| `--from-file SPEC.json` | run a multi-segment spec (see [Multi-segment](#multi-segment-specs))                          |
| `--fc HZ`               | capture center frequency, written into BLUE/SigMF metadata                                    |
| `--off N`               | trailing off-time (zeros) after the segment                                                   |
| `--repeat`              | loop the whole sequence                                                                       |
| `--continuous`          | never stop (implies repeat) — for streaming                                                   |
| `--detached`            | BLUE only: write `<out>.hdr` (HCB) + `<out>.det` (data)                                       |
| `--realtime`            | pace the output to `fs`, mimicking a sample clock (see [Real-time pacing](#real-time-pacing)) |
| `--realtime-resync`     | like `--realtime`, but re-anchor to "now" on each underrun                                    |

______________________________________________________________________

## Amplitude & full-scale

The amplitude invariant is **unit average power**: every waveform is normalised
so its mean power is `1.0`. That — *not* a constant envelope — is what the rest
of the system is built on. It is the reference the SNR math uses (signal power
≡ 1, so the noise σ falls straight out of the target SNR; see
[SNR & noise](#snr-noise)), and the level you control is the SNR, not a signal
gain. The I/Q full-scale is **±1.0** per axis (→ the largest integer code).

Today's waveforms all *happen* to be **constant-envelope**, so for them the peak
equals the average and they sit exactly at ±1.0 — but that is a property of the
current set, **not** a design assumption:

| `--type`      | Sample values                         | Envelope           | Avg. power |
| ------------- | ------------------------------------- | ------------------ | ---------- |
| `tone`        | `exp(j·2πft)`                         | constant, mag 1    | `1.0`      |
| `bpsk` / `pn` | `±1` (real axis)                      | constant, mag 1    | `1.0`      |
| `qpsk`        | `(±1/√2, ±1/√2)`                      | constant, mag 1    | `1.0`      |
| `noise`       | complex Gaussian, `σ = 1/√2` per axis | Gaussian, PAPR > 0 | `1.0`      |

**Don't rely on `|z| = 1`.** A pulse-shaped (RRC), QAM, or OFDM waveform has a
**peak-to-average power ratio (PAPR) above 0 dB**: at unit *average* power its
*peaks* run well past ±1.0. `noise`, and any signal-plus-noise sum, already do.

### Scaling to the wire, and headroom

`cf32` / `cf64` carry samples verbatim and **never clip** — peaks past ±1.0 are
preserved. The integer types map **±1.0 → ±max-code** by **saturating each axis
to ±1.0, then truncating toward zero** (a plain cast, not round-to-nearest):

| `--sample_type` | Map                   | Full-scale code  |
| --------------- | --------------------- | ---------------- |
| `ci32`          | `clip(v, ±1)·(2³¹−1)` | `±2 147 483 647` |
| `ci16`          | `clip(v, ±1)·32767`   | `±32 767`        |
| `ci8`           | `clip(v, ±1)·127`     | `±127`           |

So clipping is governed by **PAPR**, not by something being "signal" vs "noise":

- A **constant-envelope, clean** signal (today's tone/PSK/PN at `--snr 100`)
    fills the integer range exactly, with no clipping.
- **Any PAPR > 0 dB content clips** at the rails — added noise (at `--snr 0`,
    noise power = signal power, ~⅓ of integer I/Q components already saturate)
    and any future pulse-shaped / QAM / OFDM mode. Such a signal needs
    **headroom**: its average power set *below* full-scale so the peaks fit. The
    generator does **not** yet expose a peak-backoff / target-amplitude control —
    when high-PAPR waveforms land it will need one. Until then, carry
    envelope-varying signals as a **float** type (`cf32` / `cf64`), which never
    clips, or accept the saturation.

[`Reader`](#reading-a-capture-back) inverts the same map, so a float round-trip
is exact and an integer round-trip is exact only where it neither clipped nor
truncated.

```python
>>> import numpy as np
>>> from doppler.wfmgen import Synth
>>> # the invariant is unit *average* power (here a clean, constant-envelope QPSK)
>>> x = Synth(type="qpsk", sps=1, snr=100.0).steps(4096)
>>> bool(np.allclose(np.mean(np.abs(x) ** 2), 1.0))
True
>>> # add noise (or, later, pulse-shaping / QAM) and peaks exceed full-scale:
>>> y = Synth(type="qpsk", sps=1, snr=0.0).steps(100000)
>>> float(np.mean(np.abs(y.real) > 1.0)) > 0.1   # many samples clip in ci*
True
```

______________________________________________________________________

## SNR & noise

`--snr` is applied as AWGN; `--snr_mode` chooses the reference:

| Mode   | `--snr` means                                               | Use for              |
| ------ | ----------------------------------------------------------- | -------------------- |
| `fs`   | SNR over the full sample rate (in-band power / noise power) | tones, wideband      |
| `esno` | **Es/No** — energy per *symbol* over noise PSD              | modulated (`*psk`)   |
| `ebno` | **Eb/No** — energy per *bit* over noise PSD                 | link-budget work     |
| `auto` | `fs` for `tone`/`noise`/`pn`, `esno` for `bpsk`/`qpsk`      | the sensible default |

**`--snr 100` (the default) is *clean*** — `snr ≥ 100 dB` generates **no AWGN at
all**, so a clean waveform pays no noise cost. Lower `--snr` to add noise; the
signal stays at unit average power ([Amplitude & full-scale](#amplitude-full-scale)),
so the per-axis noise σ is `σ = sqrt(1 / (2·10^(snr_fs/10)))`, where Es/No and Eb/No
are first converted to an over-`fs` SNR using `10·log10(sps)` (and, for Eb/No,
the bits/symbol: 1 for BPSK/PN, 2 for QPSK). (`--type noise` always generates
AWGN.)
Likewise **`--freq 0` skips the LO** — the carrier is a constant 1 — so a clean
baseband waveform is pure signal generation.

!!! example "Same QPSK at three references"

    ```sh
    wavegen --type qpsk --snr 10 --snr_mode esno     # 10 dB Es/No (the auto default)
    wavegen --type qpsk --snr 7  --snr_mode ebno     # 7 dB Eb/No  (= 10 dB Es/No)
    wavegen --type qpsk --snr 1  --snr_mode fs        # 1 dB over fs (per-sample)
    ```

______________________________________________________________________

## PN sequences & MLS

`--type pn` emits a maximum-length sequence; `--pn_length n` sets the LFSR
register length (**2 to 64**, period `2ⁿ−1`). The register, polynomial, and
`--pn_poly` are full 64-bit. Leave **`--pn_poly 0`** and the engine selects a
primitive polynomial that yields a true MLS for that length (a built-in table of
verified primitive polynomials for every length 2..64) — verified by
period, balance, and the thumbtack autocorrelation. Supply `--pn_poly` only to
force a specific tap set.

```sh
wavegen --type pn --pn_length 7   --sps 1 --count 127   # one full period (2⁷−1)
wavegen --type pn --pn_length 11  --sps 4               # length-11 MLS, 4× oversampled
wavegen --type pn --pn_length 7   --lfsr fibonacci      # Fibonacci realization
```

`--lfsr` selects the LFSR realization: **`galois`** (default, internal XOR
feedback) or **`fibonacci`** (external XOR of the tapped bits). Both use the same
primitive polynomial and have the same period `2ⁿ−1`; they differ only in the
chip sequence/phase. The Fibonacci taps are derived from the same polynomial, so
`--pn_poly 0` still auto-selects the MLS for either mode.

______________________________________________________________________

## Containers

`--sample_type` (the *datatype*) is orthogonal to `--file_type` (the *container*)
and `--endian` (byte order).

| `--file_type` | Output                                        | Notes                                                      |
| ------------- | --------------------------------------------- | ---------------------------------------------------------- |
| `raw`         | interleaved I/Q in the chosen `--sample_type` | the SDR default; honors `--endian`                         |
| `csv`         | one `I,Q` line per sample                     | `%0.9f` cf32, `%0.17g` cf64, `%d` integer; text, no endian |
| `blue`        | **X-Midas / REDHAWK BLUE type-1000**          | *(`wfmgen` only)* self-describing 512-byte header          |
| `sigmf`       | `<base>.sigmf-data` + `<base>.sigmf-meta`     | *(`wfmgen` only)* one annotation per segment               |

**BLUE type-1000** writes a complete 512-byte X-Midas/REDHAWK Header Control
Block so one file is fully self-describing: `data_rep`←`--endian`, `format`
(`CB`/`CI`/`CL`/`CF`/`CD`)←`--sample_type`, and `xdelta = 1/fs`. Add
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
wfmgen --type qpsk --count 200000 --sample_type ci16 --endian be \
       --file_type blue -o capture.blue

# a SigMF pair (capture.sigmf-data + capture.sigmf-meta)
wfmgen --from-file scenario.json --sample_type ci16 --file_type sigmf -o capture
```

______________________________________________________________________

## Sinks

| `--output`           | Result                                                                 |
| -------------------- | ---------------------------------------------------------------------- |
| *(omitted)*          | binary stream to **stdout** (pipe it)                                  |
| `file.iq`            | write to a file                                                        |
| `zmq://tcp://*:5555` | *(`wfmgen` only)* publish to a **ZMQ PUB** endpoint (SIGS wire format) |

```sh
wavegen --type tone --count 1000000 | other-tool          # pipe via stdout
wfmgen  --type tone --continuous --output zmq://tcp://*:5555   # stream forever to ZMQ
```

A `dp_sub_*` subscriber (e.g. `examples/c/spectrum_analyzer`) reads the ZMQ
stream.

______________________________________________________________________

## Real-time pacing

By default `wfmgen` emits as fast as the CPU allows — `fs` is only metadata
(the BLUE `xdelta`, the ZMQ header). Add **`--realtime`** to throttle the output
to `fs`, so blocks leave on an `epoch + n/fs` schedule — mimicking a hardware
sample clock feeding the sink. This is what you want when a downstream consumer
expects samples to arrive at the real rate (a live spectrum display, an SDR
playback emulation):

```sh
# Stream QPSK to a live receiver at the true 1 MS/s, not as fast as possible
wfmgen --type qpsk --fs 1e6 --sps 8 --continuous --realtime \
       --output zmq://tcp://*:5555
```

The schedule is **drift-free**: each deadline is recomputed from the cumulative
sample count against a fixed epoch, so sleep jitter never accumulates — the
long-run rate is exactly `fs`. Pacing does **not** alter the samples; a file
written with and without `--realtime` is byte-identical.

If the producer can't keep up (a block takes longer than its `N/fs` period —
an *underrun*), `wfmgen` keeps the absolute timeline and prints a summary to
stderr at exit (`wfmgen: 3 underrun(s) — worst 1.2 ms behind real time`). Use
**`--realtime-resync`** instead to re-anchor the clock to "now" on each
underrun, staying near real time going forward at the cost of an inserted gap.

!!! note "Software pacing is average-rate, not sample-accurate"

    On a non-realtime OS you get a drift-free *average* rate with bounded
    per-block jitter, never true sample-clock fidelity. Keep blocks large
    enough that the period `N/fs` comfortably exceeds scheduler jitter, and let
    the consumer's buffer absorb the rest. The same engine is in Python as
    [`SampleClock`](../api/python-wfmgen.md#compose-multi-segment-composition-writers-and-a-zmq-sink).

______________________________________________________________________

## Multi-segment specs

`wfmgen --from-file SPEC.json` sequences segments — each a waveform plus an
optional trailing off-time — and can repeat or run forever. The schema is
**`wfmgen-1`**:

```json
{
  "version": "wfmgen-1",
  "repeat": false,
  "continuous": false,
  "segments": [
    { "type": "tone", "fs": 1e6, "freq": 1e5, "snr": 100.0,
      "num_samples": 10000, "off_samples": 5000 },
    { "type": "qpsk", "fs": 1e6, "snr": 9.0, "snr_mode": "esno",
      "sps": 8, "num_samples": 40000, "off_samples": 0 }
  ]
}
```

`type` and `snr_mode` are strings; every other field is numeric and **falls back
to the engine default** if omitted. `num_samples` is the on-time;
`off_samples` is a trailing gap of zeros. `repeat` loops the sequence;
`continuous` never finishes (for streaming).

```sh
wfmgen --from-file scenario.json -o scenario.cf32
```

______________________________________________________________________

## Reproducible runs (`--record`)

`--record run.json` writes the **fully-resolved** spec — every value *after*
defaulting (the auto-selected MLS polynomial, the resolved SNR mode, …). Feed it
straight back with `--from-file` and you get a byte-identical stream:

```sh
wfmgen --type bpsk --count 50000 --sps 4 --record run.json -o a.iq
wfmgen --from-file run.json -o b.iq      # a.iq and b.iq are identical
```

Use it to document a capture next to its data, or to pin an exact scenario in a
test.

______________________________________________________________________

## The three faces of `wavegen`

`wavegen` is generated by `just-makeit` in three faces that accept the same flags
and produce **byte-identical** output:

```sh
wavegen --type qpsk --count 4096 -o out.iq            # 1. installed C-backed console script
python -m doppler.wfmgen.cli --type qpsk --count 4096 # 1b. same, as a module
python wavegen.py --type qpsk --count 4096            # 2. PEP 723 script (uv run wavegen.py)
./build/wavegen --type qpsk --count 4096              # 3. standalone C binary
```

______________________________________________________________________

## Python API

The engine is also a Python class — ideal for notebooks and pipelines.

```python
import numpy as np
from doppler.wfmgen import Synth

# Every flag is a keyword argument; the same defaults apply.
synth = Synth(type="qpsk", fs=1e6, snr=12.0, snr_mode="esno", sps=8, seed=1)

block = np.asarray(synth.steps(4096))   # → complex64 ndarray
one   = synth.step()                     # → a single complex64 sample
synth.reset()                            # restart the sequence (keeps config)
```

Also exported from `doppler.wfmgen`:

| Symbol                                                             | Use                                                 |
| ------------------------------------------------------------------ | --------------------------------------------------- |
| `Synth(type=…, …)`                                                 | the waveform engine (above)                         |
| `PN(poly, seed, length)`                                           | a raw LFSR / PN sequence object                     |
| `bpsk_map(bits)` / `qpsk_map(syms)`                                | map bits/symbol-indices → cf32 constellation points |
| `wfm_awgn_amplitude(snr_db, signal_power)`                         | AWGN amplitude for a target SNR over fs             |
| `wfm_ebno_to_snr_db(ebno_db, bits_per_symbol, samples_per_symbol)` | Eb/No → over-fs SNR                                 |

```python
# A matched-filter QPSK constellation (the receiver view):
sym = np.asarray(Synth(type="qpsk", snr=20, snr_mode="esno", sps=8).steps(8*600))
pts = sym.reshape(-1, 8).mean(axis=1)    # boxcar matched filter per symbol
```

### Reading a capture back

The `raw` container is **interleaved** I/Q in the chosen `--sample_type`, so a
naive `np.fromfile` gets the layout (and, for integers, the scale) wrong.
`read_iq` does the right thing — a zero-copy complex view for the float types, a
SIMD rescale to ±1.0 for the integer types — or pass `raw=True` for the raw
`(N, 2)` on-disk view:

```python
from doppler.wfmgen.readback import read_iq

iq = read_iq("capture.iq", sample_type="ci16")   # → complex64, ±1.0
iq = read_iq("capture.iq", sample_type="cf32")   # → complex64, zero-copy
```

`generate → read_iq` is bit-faithful. See
[Type System → Reading interleaved I/Q](../types.md#reading-interleaved-iq-in-python).

______________________________________________________________________

## Recipes

```sh
# A clean tone at +100 kHz, 1 Msample, 16-bit I/Q to a file
wavegen --type tone --freq 1e5 --count 1000000 --sample_type ci16 -o tone.ci16

# Noisy BPSK at 6 dB Eb/No, as CSV for quick inspection
wavegen --type bpsk --snr 6 --snr_mode ebno --count 2000 --file_type csv -o bpsk.csv

# A scenario: tone burst, gap, then QPSK — recorded for reproducibility
wfmgen --from-file scenario.json --record run.json --file_type blue -o scene.blue

# Stream continuous QPSK to ZMQ for a live receiver
wfmgen --type qpsk --snr 10 --continuous --output zmq://tcp://*:5555
```

______________________________________________________________________

## See also

- [Gallery: wfmgen — one engine, every waveform](../gallery/wfmgen.md) — the
    spectra/constellations behind each type, with the demo script.
- [Python: Source (NCO / LO / AWGN)](../api/python-nco.md) — the building-block
    primitives the engine composes.
