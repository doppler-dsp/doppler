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

---

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

---

## Waveform types

`--type` selects the waveform; every type shares the same parameter set.

| `--type` | What it is | Key parameters |
|---|---|---|
| `tone`  | a complex sinusoid at `--freq` | `--freq` |
| `noise` | complex AWGN (unit power) | `--snr` (ignored — it *is* noise) |
| `pn`    | a maximum-length sequence (±1 chips), `--sps` samples/chip | `--pn_length`, `--pn_poly`, `--sps` |
| `bpsk`  | BPSK symbols (PN-sourced data), `--sps` samples/symbol | `--sps`, `--snr` |
| `qpsk`  | Gray-coded QPSK symbols (PN-sourced data) | `--sps`, `--snr` |

The data bits for `bpsk`/`qpsk` come from a deterministic PN sequence (seeded by
`--seed`), so output is reproducible and receiver-correlatable.

---

## Parameter reference

### Engine (shared by `wavegen` and `wfmgen`)

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `--type` | `tone\|noise\|pn\|bpsk\|qpsk` | `tone` | waveform |
| `--fs` | float (Hz) | `1e6` | sample rate |
| `--freq` | float (Hz) | `0` | frequency offset from baseband (mixed by the LO) |
| `--snr` | float (dB) | `100` | SNR; metric chosen by `--snr_mode` (≈clean at 100) |
| `--snr_mode` | `auto\|fs\|ebno\|esno` | `auto` | how `--snr` is interpreted (see below) |
| `--seed` | uint32 | `1` | PRNG / LFSR seed (deterministic) |
| `--sps` | int | `8` | samples per symbol (`*psk`) / per chip (`pn`) |
| `--pn_length` | int | `7` | LFSR register length → period `2ⁿ−1` |
| `--pn_poly` | uint32 | `0` | LFSR polynomial; `0` ⇒ auto-pick the MLS polynomial |
| `--count` | int | `1024` | number of complex samples to generate |

### Output

| Flag | Values | Default | Meaning |
|---|---|---|---|
| `--sample_type` | `cf32 cf64 ci32 ci16 ci8` | `cf32` | wire type; integers are full-scale ±1.0 |
| `--file_type` | `raw csv` *(+ `blue sigmf` in `wfmgen`)* | `raw` | container (see [Containers](#containers)) |
| `--endian` | `le be` | `le` | byte order (raw/BLUE only; csv is text) |
| `--output` / `-o` | path *(or `zmq://…` in `wfmgen`)* | stdout | sink |
| `--record` | path | — | write a JSON record of the resolved run |

### `wfmgen`-only (the composer)

| Flag | Meaning |
|---|---|
| `--from-file SPEC.json` | run a multi-segment spec (see [Multi-segment](#multi-segment-specs)) |
| `--fc HZ` | capture center frequency, written into BLUE/SigMF metadata |
| `--off N` | trailing off-time (zeros) after the segment |
| `--repeat` | loop the whole sequence |
| `--continuous` | never stop (implies repeat) — for streaming |

---

## SNR & noise

`--snr` is applied as AWGN; `--snr_mode` chooses the reference:

| Mode | `--snr` means | Use for |
|---|---|---|
| `fs`   | SNR over the full sample rate (in-band power / noise power) | tones, wideband |
| `esno` | **Es/No** — energy per *symbol* over noise PSD | modulated (`*psk`) |
| `ebno` | **Eb/No** — energy per *bit* over noise PSD | link-budget work |
| `auto` | `fs` for `tone`/`noise`/`pn`, `esno` for `bpsk`/`qpsk` | the sensible default |

The default `--snr 100` is effectively clean — lower it to add noise. The noise
amplitude is derived as `amp = sqrt(1 / (2·10^(snr_fs/10)))`, where Es/No and
Eb/No are first converted to an over-`fs` SNR using `10·log10(sps)` (and, for
Eb/No, the bits/symbol: 1 for BPSK/PN, 2 for QPSK).

!!! example "Same QPSK at three references"
    ```sh
    wavegen --type qpsk --snr 10 --snr_mode esno     # 10 dB Es/No (the auto default)
    wavegen --type qpsk --snr 7  --snr_mode ebno     # 7 dB Eb/No  (= 10 dB Es/No)
    wavegen --type qpsk --snr 1  --snr_mode fs        # 1 dB over fs (per-sample)
    ```

---

## PN sequences & MLS

`--type pn` emits a maximum-length sequence; `--pn_length n` sets the LFSR
register length (period `2ⁿ−1`). Leave **`--pn_poly 0`** and the engine selects
a primitive polynomial that yields a true MLS for that length — verified by
period, balance, and the thumbtack autocorrelation. Supply `--pn_poly` only to
force a specific tap set.

```sh
wavegen --type pn --pn_length 7   --sps 1 --count 127   # one full period (2⁷−1)
wavegen --type pn --pn_length 11  --sps 4               # length-11 MLS, 4× oversampled
```

---

## Containers

`--sample_type` (the *datatype*) is orthogonal to `--file_type` (the *container*)
and `--endian` (byte order).

| `--file_type` | Output | Notes |
|---|---|---|
| `raw`   | interleaved I/Q in the chosen `--sample_type` | the SDR default; honors `--endian` |
| `csv`   | one `I,Q` line per sample | `%0.9f` cf32, `%0.17g` cf64, `%d` integer; text, no endian |
| `blue`  | **X-Midas / REDHAWK BLUE type-1000** | *(`wfmgen` only)* self-describing 512-byte header |
| `sigmf` | `<base>.sigmf-data` + `<base>.sigmf-meta` | *(`wfmgen` only)* one annotation per segment |

**BLUE type-1000** packs everything into a 512-byte header so one file is fully
self-describing: `data_rep`←`--endian`, `format` (`CB`/`CI`/`CL`/`CF`/`CD`)←
`--sample_type`, and `xdelta = 1/fs`.

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

---

## Sinks

| `--output` | Result |
|---|---|
| *(omitted)* | binary stream to **stdout** (pipe it) |
| `file.iq` | write to a file |
| `zmq://tcp://*:5555` | *(`wfmgen` only)* publish to a **ZMQ PUB** endpoint (SIGS wire format) |

```sh
wavegen --type tone --count 1000000 | other-tool          # pipe via stdout
wfmgen  --type tone --continuous --output zmq://tcp://*:5555   # stream forever to ZMQ
```

A `dp_sub_*` subscriber (e.g. `examples/c/spectrum_analyzer`) reads the ZMQ
stream.

---

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

---

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

---

## The three faces of `wavegen`

`wavegen` is generated by `just-makeit` in three faces that accept the same flags
and produce **byte-identical** output:

```sh
wavegen --type qpsk --count 4096 -o out.iq            # 1. installed C-backed console script
python -m doppler.wfmgen.cli --type qpsk --count 4096 # 1b. same, as a module
python wavegen.py --type qpsk --count 4096            # 2. PEP 723 script (uv run wavegen.py)
./build/wavegen --type qpsk --count 4096              # 3. standalone C binary
```

---

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

| Symbol | Use |
|---|---|
| `Synth(type=…, …)` | the waveform engine (above) |
| `PN(poly, seed, length)` | a raw LFSR / PN sequence object |
| `bpsk_map(bits)` / `qpsk_map(syms)` | map bits/symbol-indices → cf32 constellation points |
| `wfm_awgn_amplitude(snr_db, signal_power)` | AWGN amplitude for a target SNR over fs |
| `wfm_ebno_to_snr_db(ebno_db, bits_per_symbol, samples_per_symbol)` | Eb/No → over-fs SNR |

```python
# A matched-filter QPSK constellation (the receiver view):
sym = np.asarray(Synth(type="qpsk", snr=20, snr_mode="esno", sps=8).steps(8*600))
pts = sym.reshape(-1, 8).mean(axis=1)    # boxcar matched filter per symbol
```

---

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

---

## See also

- [Gallery: wfmgen — one engine, every waveform](../gallery/wfmgen.md) — the
  spectra/constellations behind each type, with the demo script.
- [Python: Source (NCO / LO / AWGN)](../api/python-nco.md) — the building-block
  primitives the engine composes.
