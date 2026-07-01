# Scenes — multi-segment specs

A **scene** is more than one waveform: sources mixed at the same time, segments
sequenced in time, repeats, and reproducible randomness. This is the
[Segment / Timeline / Composer](concepts.md) rungs of the ladder in practice.
Both the CLI (`--from-file SPEC.json`) and the Python `Composer` drive the same
C engine, so their output is **byte-identical** for the same parameters.

## Composer parameter reference

| Flag                    | Meaning                                                                                  |
| ----------------------- | ---------------------------------------------------------------------------------------- |
| `--from-file SPEC.json` | run a multi-segment spec                                                                 |
| `json-template [FILE]`  | subcommand: dump an editable example spec (to `FILE`, else stdout)                       |
| `--level DB`            | source level in dBFS (≤0); scales the segment by `10^(DB/20)` (SNR-invariant; default 0) |
| `--headroom DB`         | back the output off to `−DB` dBFS so peaks fit (SNR-invariant; default 0)                |
| `--clip-report`         | print the clipped fraction + peak; `--clip-error` exits non-zero on a clip               |
| `--fc HZ`               | capture center frequency, written into BLUE/SigMF metadata                               |
| `--off N`               | trailing off-time (zeros) after the segment                                              |
| `--repeat`              | loop the whole sequence                                                                  |
| `--continuous`          | never stop (implies repeat) — for streaming                                              |
| `--seed-advance A`      | `none` (default) / `noise` / `all`: how the seed advances per repeat                     |
| `--detached`            | BLUE only: write `<out>.hdr` (HCB) + `<out>.det` (data)                                  |
| `--realtime`            | pace the output to `fs` (see [Streaming](streaming.md))                                  |
| `--realtime-resync`     | like `--realtime`, but re-anchor to "now" on each underrun                               |

______________________________________________________________________

## Sequencing segments in time

`--from-file SPEC.json` sequences segments — each a waveform plus an optional
trailing off-time — and can repeat or run forever.

=== "CLI (JSON spec)"

    ```json title="scenario.json"
    {
      "version": 1,
      "segments": [
        { "type": "tone", "fs": 1e6, "freq": 1e5, "snr": 100.0,
          "num_samples": 10000, "off_samples": 5000 },
        { "type": "qpsk", "fs": 1e6, "snr": 9.0, "snr_mode": "esno",
          "sps": 8, "num_samples": 40000 }
      ]
    }
    ```

    ```sh
    wfmgen --from-file scenario.json -o scenario.cf32
    ```

=== "Python API"

    ```python
    from doppler.wfm import Composer, tone, qpsk

    scene = (
        Composer(fs=1e6)
        .add(tone(freq=1e5, snr=100.0, num_samples=10000, off_samples=5000))
        .add(qpsk(snr=9.0, snr_mode="esno", sps=8, num_samples=40000))
    )
    scene.write("scenario.cf32")
    ```

`type` and `snr_mode` are strings in JSON; every other field is numeric and
**falls back to the engine default** if omitted. `num_samples` is the on-time;
`off_samples` is a trailing gap of zeros. `repeat` loops the sequence;
`continuous` never finishes (for streaming).

Rather than write the JSON schema from memory, dump a ready-to-edit example with
**`wfmgen json-template`** and edit it down:

```sh
wfmgen json-template > scenario.json   # or: wfmgen json-template scenario.json
wfmgen --from-file scenario.json -o scenario.cf32
```

The template is a representative spec — an inline tone, an RRC-shaped
QPSK-from-bits burst with a trailing gap, and a two-source `sum` mix — that is
**valid by construction**: it round-trips through `--from-file` unchanged, so it
doubles as a working starting point, not just documentation.

______________________________________________________________________

## Mixing sources (`sum`) and sequencing them (`add`)

A segment can hold **several sources mixed at the same time** — a signal of
interest plus interferers plus a noise floor — instead of just one. The two
composition verbs are orthogonal:

- **`.sum()` mixes** sources over the *same* span (one receiver → one sample
    rate, one shared noise floor). SNR lives on a source; the floor is resolved
    **once, in C**, so the Python, JSON, and CLI faces are byte-identical.
- **`.add()` sequences** segments in *time*, back-to-back — the timeline above,
    built fluently.

```python
from doppler.wfm import Composer, Segment, qpsk, tone

# A scene: a −12 dB QPSK SoI at +50 kHz over a CW interferer, at 15 dB Es/No.
scene = Segment.sum(
    qpsk(snr=15, snr_mode="esno", level=-12),  # the anchor sets the floor
    tone(freq=5e4),                             # an interferer (level 0 dBFS)
    num_samples=65536,
)

# Sequence a clean preamble tone, then the scene:
timeline = Segment("tone", freq=1e5, num_samples=2000, off_samples=500).add(scene)
iq = Composer(timeline).compose()
```

**Rules of the floor** (resolved per segment): an explicit `noise(level=N)`
source fixes it at `N` dBFS; otherwise the first source carrying `snr` is the
anchor and the floor is `level(anchor) − SNR_fs(anchor)`. Other sources place
themselves with `level` (a plain dBFS offset); giving a non-anchor *both* `snr`
and `level` is a spec error. A single-source segment keeps its bundled AWGN
untouched, so it is byte-identical to the pre-composition path.

In the JSON schema, a mixed segment replaces the inline source fields with a
**`sum`** array (each entry is a source; `fs`/`num_samples`/`off_samples` stay on
the segment):

```json
{ "fs": 1e6, "num_samples": 65536, "off_samples": 0,
  "sum": [
    { "type": "qpsk", "snr": 15.0, "snr_mode": "esno", "sps": 8, "level": -12.0 },
    { "type": "tone", "freq": 5e4 }
  ] }
```

______________________________________________________________________

## Seed control on repeat

A repeated stream should be a *stream*, not the same bytes over and over. By
default repeats are **byte-identical** (the seed is fixed). The
**`--seed-advance`** knob (spec field `seed_advance`, honoured by `--from-file`
and `Composer.from_json`) chooses how much of the seed advances on each loop:

| `--seed-advance` | Per repeat                                     | Use it for                                     |
| ---------------- | ---------------------------------------------- | ---------------------------------------------- |
| `none` (default) | byte-identical                                 | exact reproduction / regression                |
| `noise`          | only the **AWGN** seed; signal bit-identical   | BER / detection curves over one fixed waveform |
| `all`            | the **whole** seed → code, data, **and** noise | a fully stochastic, whole-ensemble stream      |

The level is **ordered and cumulative** — `noise` keeps the signal, `all` lets
everything change. For `pn`/`bpsk`/`qpsk` the code and data come from the *same*
PN LFSR (one `seed`), so they advance together under `all`. Under `noise`, a
**fixed preamble or sync code re-acquires every burst** while the channel noise
changes — ideal for a soak test, a live receiver feed, or a rotating-file
recorder. The first loop is always the unmodified seed for every mode, so a
finite single-pass run is unaffected and `--record` stays byte-reproducible.

```sh
# A PN preamble + payload, repeating forever, fresh noise each burst:
wfmgen --from-file burst.json --continuous --realtime --seed-advance noise \
       -o stream.cf32
```

______________________________________________________________________

## Ranged values (`lo:hi`)

The advancing seed re-rolls the *noise* (and PN data), but the **parameters** —
frequency, SNR, level, on/off lengths — stay put. To vary a parameter too, give
it a **range**: a numeric field accepts either a scalar (used as-is) or a
`[lo, hi]` pair drawn **uniformly** on each segment repeat.

```jsonc
{ "type": "bpsk", "fs": 1e6, "sps": 8, "pn_length": 7,
  "freq":        [11200, 12800],   // Doppler offset re-drawn every burst
  "snr":         [8, 14],          // a fresh SNR each burst
  "num_samples": 8192,
  "off_samples": [4000, 5600] }    // jittered trailing gap → code phase walks
```

On the CLI the same fields take `LO:HI` in place of a scalar — `--freq 11200:12800`, `--off 4000:5600`, `--snr 8:14`, `--level -12:-3` — and a bare
number is still just that number.

The draw is **reproducible without RNG state**: each value is a hash of
`(seed, repeat index, segment index, source index, field)`, so `--record` stores
the *range* and `--from-file` replays the identical sequence of draws
byte-for-byte. Ranges compose with the advancing seed and with `chirp`'s
`freq`/`f-end` (a sweep whose endpoints jitter per burst).

```sh
# Endless bursts, each at a random Doppler offset and a jittered gap:
wfmgen --type bpsk --fs 1e6 --sps 8 --pn-length 7 \
       --freq 11200:12800 --count 8192 --off 4000:5600 \
       --continuous --realtime -o stream.cf32
```

______________________________________________________________________

## Reproducible runs (`--record`)

`--record run.json` writes the **fully-resolved** spec — every value *after*
defaulting (the auto-selected MLS polynomial, the resolved SNR mode, a summed
segment's cleaned anchor + explicit noise floor) **and** the `--headroom`. Feed
it straight back with `--from-file` and you get a byte-identical stream:

```sh
wfmgen --type bpsk --count 50000 --sps 4 --headroom 6 --record run.json -o a.iq
wfmgen --from-file run.json -o b.iq      # a.iq and b.iq are identical
```

The recorded `--headroom` is reapplied on replay; an explicit `--headroom` on the
`--from-file` run overrides it. Use `--record` to document a capture next to its
data, or to pin an exact scenario in a test. The resolved spec also round-trips
through JSON in Python — `Composer.from_json(c.to_json())` reproduces the stream.
