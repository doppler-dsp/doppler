# Waveform Generator — `wfmgen`

doppler ships a C-first **waveform generator**: one declarative synth engine
(every algorithm in C, exactly once) that goes well past single waveforms —

- **Scenes** — multi-segment specs, in JSON or Python, that mix sources,
    sequence them in time, and can **randomize** a parameter (frequency, SNR,
    gap length) per repeat instead of holding it fixed.
- **Self-describing containers & streaming** — raw / CSV / **BLUE type-1000** /
    **SigMF**, plus real-time-paced streaming to **NATS**.
- **`Plan`** — a bit-exact sweep cache: prepare a scene once, then re-render it
    at any SNR/gain/phase/seed as a cheap re-weighted sum instead of
    re-synthesizing the DSP — the engine behind fast Monte Carlo and BER/Pd
    curves.

All of it is **exposed two ways**, both driving the same C engine to
byte-identical output:

- **`wfmgen`** — the one command-line tool. (A one-segment run is the simple
    single-waveform case.)
- **`doppler.wfm`** — the same engine as a Python API, one import path:
    `from doppler.wfm import …`.

![wfmgen engine](../../assets/wfmgen_demo.png)

Reach for `--from-file` (or the Python `Composer`) when you need multiple
segments, mixing, BLUE/SigMF, or a NATS stream — otherwise a handful of flags
generate a single waveform.

!!! tip "The 30-second version"

    ```sh
    wfmgen --type qpsk --snr 12 --count 100000 -o capture.cf32   # 100k QPSK samples @ 12 dB Es/No
    wfmgen --type tone --freq 0.1 --count 4096                   # a 0.1·Fs tone → stdout (cf32)
    wfmgen --type pn --pn-length 9 --file-type csv -o pn.csv      # length-9 MLS as text
    ```

______________________________________________________________________

## Where to go next

| Page                             | What it covers                                                                 |
| -------------------------------- | ------------------------------------------------------------------------------ |
| [Concepts](concepts.md)          | The object model — **Synth · Segment · Timeline · Composer**. Read this first. |
| [Waveforms](waveforms.md)        | The eight `--type`s, PN/MLS codes, RRC pulse shaping.                          |
| [Levels & SNR](levels.md)        | Unit-average-power, full-scale, clipping, headroom, the SNR model.             |
| [Output & containers](output.md) | Sample types, raw/CSV/BLUE/SigMF, byte order, sinks.                           |
| [Scenes](scenes.md)              | Multi-segment specs, `sum`/`add`, seeds, ranged values, `--record`.            |
| [Streaming](streaming.md)        | Real-time pacing and streaming to NATS.                                        |
| [Python API](python.md)          | The `Synth` class, the composer builders, reading captures back.               |
| [Prepare Once, Sweep Many (Plan)](plan.md) | Bit-exact sweep cache — fast Monte Carlo / BER curves, no re-synthesis. |
| [Recipes](recipes.md)            | Copy-paste one-liners and how `wfmgen` is packaged.                            |

______________________________________________________________________

## Installation

```sh
pip install doppler-dsp        # → the `wfmgen` command + the doppler.wfm API
```

The wheel ships the self-contained `wfmgen` binary as package data and a
`wfmgen` console-script — a thin shim that `exec`s it — alongside the
`doppler.wfm` Python module. To build from source instead:

```sh
git clone https://github.com/doppler-dsp/doppler && cd doppler
cmake -B build -DBUILD_PYTHON=ON && cmake --build build --target wfmgen_cli
# binary: build/native/src/wfmcompose/wfmgen
```

______________________________________________________________________

## See also

- [Gallery: wfmgen — one engine, every waveform](../../gallery/wfmgen.md) — the
    spectra/constellations behind each type, with the demo script.
- [Python: Source (NCO / LO / AWGN)](../../api/python-nco.md) — the building-block
    primitives the engine composes.
