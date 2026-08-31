# Scenes — composing in time

A scene is segments in sequence: what plays, for how long, what follows,
and what varies per repeat. This page covers the spec format, then the
two things you do with a finished scene — sweep it cheaply with `Plan`,
or stream it in real time.

What each waveform *is* belongs to [Waveforms](waveforms.md); the object
model is on the [guide index](index.md#the-ladder-the-whole-mental-model).

______________________________________________________________________

A **scene** is more than one waveform: sources mixed at the same time, segments
sequenced in time, repeats, and reproducible randomness. This is the
[Segment / Timeline / Composer](index.md#the-ladder-the-whole-mental-model) rungs of the ladder in practice.
Both the CLI (`--from-file SPEC.json`) and the Python `Composer` drive the same
C engine, so their output is **byte-identical** for the same parameters.

## Composer parameter reference

| Flag                    | Meaning                                                                                   |
| ----------------------- | ----------------------------------------------------------------------------------------- |
| `--from-file SPEC.json` | run a multi-segment spec                                                                  |
| `json-template [FILE]`  | subcommand: dump an editable example spec (to `FILE`, else stdout)                        |
| `--level DB`            | source level in dBFS (≤0); scales the segment by `10^(DB/20)` (SNR-invariant; default 0)  |
| `--headroom DB`         | back the output off to `−DB` dBFS so peaks fit (SNR-invariant; default 0)                 |
| `--clip-report`         | print the clipped fraction + peak; `--clip-error` exits non-zero on a clip                |
| `--fc HZ`               | capture center frequency, written into BLUE/SigMF metadata                                |
| `--off N`               | trailing gap after the segment (carries the noise floor; see below)                       |
| `--delay N`             | leading gap before the segment — arrival delay / jitter                                   |
| `--gap-noise M`         | `auto` (default: gaps carry the segment's noise floor) / `off` (hard zeros)               |
| `--repeat`              | loop the whole sequence                                                                   |
| `--continuous`          | never stop (implies repeat) — for streaming                                               |
| `--seed-advance A`      | `none` (default) / `noise` / `all`: how the seed advances per repeat                      |
| `--detached`            | BLUE only: write `<out>.hdr` (HCB) + `<out>.det` (data). Refuses `--realtime` — see below |
| `--realtime`            | pace the output to `fs` (see [Streaming](scenes.md#streaming-real-time-pacing))           |
| `--realtime-resync`     | like `--realtime`, but re-anchor to "now" on each underrun                                |

______________________________________________________________________

!!! note "`--detached` is a file format, not a process model"

    It selects BLUE's *detached header* — the HCB in `<out>.hdr`, the samples
    in `<out>.det` — and nothing about how `wfmgen` runs. So it is refused,
    rather than quietly dropped, alongside anything that cannot honour it:
    `--realtime`/`--realtime-resync`, a `nats://` destination, or a
    `--file-type` other than `blue`.

    Pacing in particular has no consumer to serve: the `.hdr` carries the
    final sample count, so it is written only once the run ends, and a
    detached run must be finite anyway. To pace, write a single file
    (`--file-type blue` without `--detached`) or stream to a broker.

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
    from doppler.wfm import Segment, Timeline, Composer

    # The same two segments as the JSON spec above: a tone, then a QPSK burst.
    timeline = Timeline([
        Segment("tone", fs=1e6, freq=1e5, snr=100.0,
                num_samples=10000, off_samples=5000),
        Segment("qpsk", fs=1e6, snr=9.0, snr_mode="esno",
                sps=8, num_samples=40000),
    ])
    iq = Composer(timeline).compose()   # complex64 — byte-identical to the CLI
    ```

`type` and `snr_mode` are strings in JSON; every other field is numeric and
**falls back to the engine default** if omitted. A segment's shape is
`delay | on | off`: `delay_samples` is a leading gap (arrival delay),
`num_samples` the on-time, `off_samples` a trailing gap — use `off` for
inter-burst *spacing* and `delay` for arrival *jitter*. Gaps are not
silence by default: a noisy segment's AWGN keeps running through its
delay and trailing gap (the channel's noise floor — gh-409), while a clean
scene's gaps stay exact zeros; `gap_noise: "off"` forces hard zeros.
`repeat` loops the sequence; `continuous` never finishes (for streaming).

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

The schema itself is published alongside these pages:
[`wfmgen.schema.json`](../../schema/wfmgen.schema.json) (JSON Schema
2020-12). It is the normative description of every key used below — a
template shows one valid spec, the schema says what the whole space of them
is, which is what an editor or a validator needs.

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

<!-- docs-snippet: no-exec=unbounded --continuous --realtime stream; burst.json is illustrative -->

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
  "freq":        [11200, 12800],   // carrier offset re-drawn every burst
  "snr":         [8, 14],          // a fresh SNR each burst
  "num_samples": 8192,
  "off_samples": [4000, 5600] }    // jittered trailing gap → code phase walks
```

On the CLI the same fields take `LO:HI` in place of a scalar — `--freq 11200:12800`, `--off 4000:5600`, `--snr 8:14`, `--level -12:-3` — and a bare
number is still just that number.

A ranged `freq` moves the carrier and nothing else. For a source whose
**clock** is moving — symbol and chip rates scaling with the carrier, as a
real pass does — see [Clock Doppler](waveforms.md#clock-doppler-a-source-that-is-moving); `doppler`/`doppler_rate`
are ranged fields on this same list.

The draw is **reproducible without RNG state**: each value is a hash of
`(seed, repeat index, segment index, source index, field)`, so `--record` stores
the *range* and `--from-file` replays the identical sequence of draws
byte-for-byte. Ranges compose with the advancing seed and with `chirp`'s
`freq`/`f-end` (a sweep whose endpoints jitter per burst).

```sh
# Endless bursts, each at a random carrier offset and a jittered gap:
wfmgen --type bpsk --fs 1e6 --sps 8 --pn-length 7 \
       --freq 11200:12800 --count 8192 --off 4000:5600 \
       --continuous --realtime -o stream.cf32
```

______________________________________________________________________

## Burst trains (`repeats`)

A segment can play itself `repeats` times back-to-back — each **instance** is
`delay | on | off` (arrival delay, burst, trailing gap) — before the
timeline advances. That turns
"N bursts, randomly placed with a minimum gap" into **one declaration**
instead of N copy-pasted segments:

```jsonc
{ "type": "dsss", "fs": 4e6, "sps": 4,
  "snr": 10.0, "snr_mode": "esno",
  "acq_code": "…", "acq_reps": 5, "data_code": "…",
  "payload": "…",
  "off_samples": [15000, 40000],   // jittered gap, min 15k — per instance
  "repeats": 5 }                    // → a 5-burst train
```

Instance semantics are exactly what a burst train wants:

- **Ranged fields re-draw per instance** — with `off_samples: [lo, hi]` every
    gap is a fresh draw (`lo` = the guaranteed minimum gap), and a ranged
    `delay_samples` jitters each burst's arrival. The draw key extends the
    ranged hash with the instance index, so instance 0 renders
    **byte-identically** to a repeats-less segment and old scenes are
    unchanged.
- **The AWGN is always fresh per instance** — two instances never share a
    noise realization, regardless of `--seed-advance`.
- **The signal is fixed** — codes, payload, PN phase, and every non-ranged
    parameter repeat exactly. (To vary the signal too, that is what
    `--seed-advance all` on a looped timeline is for.)

`repeats` is per-segment instancing; `--repeat` loops the **whole timeline**
(advancing the epoch seed). They compose: a two-segment spec with
`"repeats": 5` on the first plays 5 bursts, then the second segment, then —
under `--repeat` — the whole thing again with the next epoch's draws.

On the CLI the single-segment face is `--repeats N`; in Python it is
`Segment(..., repeats=5)`. See [DSSS bursts](waveforms.md#dsss-bursts) for the worked
burst-train walkthrough.

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

______________________________________________________________________

## Prepare once, sweep many — `Plan`

Evaluating a system — a detector, a demodulator, a synchroniser — means feeding
it the **same scene at many operating points**: a detection or BER curve is a
sweep over SNR; a robustness check nudges a gain or a phase; a Monte-Carlo run
repeats one scene under fresh noise. Re-composing from scratch at every point is
wasteful, because a composed scene is already a **linear form**,

$$
\text{out} \;=\; \sum_k \text{gain}_k \cdot \text{signal}_k \;+\; \text{noise},
$$

and the expensive DSP — spreading, root-raised-cosine pulse shaping, the local
oscillator — lives entirely in the **signal** terms. Those do not change when you
sweep a level, a phase, the SNR, or the noise seed. Only cheap coefficients do.

`prepare(scene)` renders and caches each source **once**, returning a `Plan`.
Every subsequent render is a cheap re-weighted sum of the cache — and **bit-for-bit
identical** to a full compose. It is not a fifth rung on the
[object-model ladder](index.md#the-ladder-the-whole-mental-model); it is a *cache over a finished scene*, for
when you need that scene many times.

### Preparing a scene

Build a [scene](scenes.md) exactly as you would for `compose()`, then prepare it.
The baseline `render()` (no overrides) reproduces `Composer(scene).compose()`
exactly:

```python
import numpy as np
from doppler.wfm import Composer, Segment, prepare, qpsk

scene = Composer(Segment.sum(
    qpsk(snr=8.0, seed=7, sps=8, pn_length=9),        # the wanted user (anchor)
    qpsk(seed=101, sps=8, pn_length=9, level=-6.0),   # a co-channel interferer
    fs=1e6, num_samples=4096,
))

plan = prepare(scene)                                 # render + cache ONCE
assert np.array_equal(plan.render(), scene.compose())  # baseline is bit-exact
len(plan), plan.n_sources                             # samples, signal sources
```

`len(plan)` is the sample count; `plan.n_sources` counts the **signal** sources
(the resolved noise floor is separate). `prepare(scene)` is shorthand for
`Plan(scene)` — either works, and a `Plan` is a context manager if you want to
free its cache promptly.

### The overridable axes

`render()` takes five optional overrides. Omit them all for the baseline; pass
any subset to vary that axis. The three per-source axes are lists in scene order,
length `n_sources`:

| Override | Type          | Meaning                                                     |
| -------- | ------------- | ----------------------------------------------------------- |
| `gains`  | `list[float]` | absolute source levels in dBFS (`0` = unit power)           |
| `phases` | `list[float]` | per-source phase rotation in radians (`0` = identity)       |
| `enable` | `list[bool]`  | `False` drops a source — an exact `gain = 0` term           |
| `snr`    | `float`       | global SNR (dB) — moves **only** the noise floor            |
| `seed`   | `int`         | the noise realization (defaults to the scene's anchor seed) |

```python
# one wanted user, interferer pulled down 6 dB and rotated 90°, floor at 3 dB
x = plan.render(gains=[0.0, -12.0], phases=[0.0, np.pi / 2], snr=3.0)
x.shape, x.dtype
```

Every override composes: `gains` and `phases` and `enable` and `snr` and `seed`
can all be set in one call. Anything you leave out keeps its resolved value from
the scene.

### Which method to reach for

`render()` is the general form. Three convenience methods wrap it for the common
campaigns — reach for whichever names your intent:

- **`at(snr, seed=None)`** — the scalar fast path (no JSON round-trip), the hot
    loop of a sweep. `seed` defaults to the anchor seed, which reproduces a full
    compose at that SNR.
- **`sweep(snrs, seed=None)`** — yields `(snr, samples)` across an SNR list at a
    **held** noise seed, so only the floor moves. The natural stimulus for a
    Pd/BER-vs-SNR curve.
- **`monte_carlo(snr, n, seed0=0)`** — yields `n` independent noise realizations
    at a **fixed** SNR; the signal is identical across draws, only the noise
    differs.

```python
# a held-seed SNR curve — same noise realization, only the floor moves
curve = {snr: x for snr, x in plan.sweep([-3.0, 0.0, 3.0, 6.0, 9.0])}

# 16 independent noise draws at 6 dB — identical signal, different noise
draws = list(plan.monte_carlo(6.0, 16, seed0=1000))
assert len({d.tobytes() for d in draws}) == 16       # every realization differs
```

### Recipe — a detection / BER curve

Sweep the channel SNR and measure a per-point statistic. Here a light
matched-filter peak-SNR against a clean copy of the wanted user (itself produced
by disabling the interferer — see the next recipe). A real campaign averages each
point over Monte-Carlo draws:

```python
# a clean, interference-free copy of the wanted user = the matched filter
template = plan.render(enable=[True, False])[: len(plan) // 2]
template = template / (np.linalg.norm(template) + 1e-30)

def peak_snr(x):
    c = np.abs(np.correlate(x, template, mode="valid"))
    pk = int(c.argmax())
    off = np.delete(c, slice(max(0, pk - 4), pk + 5))
    return 10 * np.log10(c[pk] ** 2 / (np.mean(off ** 2) + 1e-30))

snrs = np.arange(-6.0, 13.0, 3.0)
# each point: mean peak-SNR over 8 independent noise draws
detect = [np.mean([peak_snr(plan.at(s, 2000 + j)) for j in range(8)])
          for s in snrs]
len(detect) == len(snrs)
```

The measured curve climbs with channel SNR and then flattens as the
multiple-access interference floor takes over — and the cache reproduces the
precise noise power the resolver placed at every point.

### Recipe — isolate or recombine sources

`enable` drops a source as an exact `gain = 0` term, so you can pull any subset
out of a scene without rebuilding it — a clean reference, an interference-only
capture, a jammer-free template:

```python
wanted_only = plan.render(enable=[True, False])       # signal + noise, no MAI
interferer_only = plan.render(enable=[False, True])   # co-channel only + noise
```

### Recipe — a gain-imbalance or phase sweep

Because `gains` and `phases` are cheap post-multiplies on the cache, a
sensitivity sweep over a relative level or phase is nearly free:

```python
# sweep the interferer's level from 0 down to -18 dB (wanted user fixed at 0)
gain_sweep = [plan.render(gains=[0.0, g]) for g in range(0, -19, -3)]

# sweep its carrier phase across a full turn
phase_sweep = [plan.render(phases=[0.0, ph])
               for ph in np.linspace(0, 2 * np.pi, 8, endpoint=False)]
len(gain_sweep), len(phase_sweep)
```

### Why it is fast (and exact)

The cost of `prepare()` is one full render of every source. After that, each
`render()`/`at()` is a handful of scaled vector adds over the cache — no LFSR, no
convolution, no transcendentals. So a campaign of `P` points costs roughly
"one compose + `P` cheap sums" instead of "`P` full composes", and the speedup
grows with the number of sources and the sample count, since that is exactly the
signal work the cache elides.

Exactness is guaranteed by construction: the composer's accumulate is
`Σₖ gainₖ·synthₖ` in source order with `gainₖ = 10^(levelₖ/20)`, and `synthₖ`
depends on everything *except* level — so a re-weighted sum of the cached renders
is bitwise identical to a full compose at those gains. `render()` with no
overrides equals `compose()` to the last bit, which is the standing test
contract (checked in both harnesses). Phase is a defined render-time rotation
(`φ = 0` is the identity, skipped entirely), exact by construction rather than
reproduced.

### Scope and limits

`prepare()` needs a scene whose length is **fixed** and whose per-source
weights are **fixed**, because that is what makes a re-render a re-weighted
sum instead of a re-synthesis. Two things break that, and they are the two it
refuses:

| refused                                                       | why                                                                                                                                                |
| ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| a **ranged signal** field — `snr=(4, 8)`, `freq=(0.01, 0.05)` | the value the cache was rendered at is the one thing a sweep is supposed to vary afterwards, so a per-repeat redraw of it has nothing to re-weight |
| **`continuous=True`**                                         | the length is open-ended, and the cache is the rendered samples                                                                                    |

Everything else the scene can say is fine — including three things this page
used to claim were rejected. Multi-segment scenes, `repeats`, and ranged
**timing** (`off_samples`, `delay_samples`, the per-instance jitter a burst
train wants) all prepare, which is what
[DSSS bursts](waveforms.md#sweeping-a-burst-train-with-plan) relies on. So
does a lone **bundled** noisy source, whose private RNG is fused into the
signal.

```pycon
>>> from doppler.wfm import Composer, Segment, prepare
>>> plan = prepare(Composer([Segment(type="qpsk", snr=6.0, num_samples=512,
...                                  off_samples=(100, 300), repeats=2)]))
>>> len(plan) > 0                     # ranged TIMING + repeats: prepared
True
>>> try:                              # a ranged SIGNAL field: refused
...     prepare(Composer([Segment(type="qpsk", snr=(4.0, 8.0),
...                               num_samples=512)]))
... except ValueError:
...     print("rejected")
rejected
```

A prepared `Plan` **can** be serialized — `plan.save()` → `bytes`,
`plan.dump(path)` → a file, restored with `PlanFromBlob` / `PlanFromFile` — and
the restored cache reproduces the stimulus bit-for-bit. But reach for it only to
checkpoint or resume a *live* Plan, **not** to move one between processes: the
blob is the whole rendered cache — roughly `n_sources × len(plan) × 8` bytes —
so it grows with the scene, and for anything large, restoring a multi-gigabyte
blob is **slower than re-rendering it from scratch**.

For a hand-off, persist the scene's compact **spec JSON** (`Composer.to_json()`)
and re-`prepare()` on the far side — the re-render is almost always cheaper than
shipping and deserializing the cache, and the spec is kilobytes, not gigabytes.
The rule of thumb: transport the *recipe*, not the *rendered signal*.

Frequency (Doppler) and delay (multipath) are planned follow-ups on the same
frame — additive axes, not a rewrite.

______________________________________________________________________

## Streaming — real-time pacing

By default `wfmgen` emits as fast as the CPU allows — `fs` is only metadata (the
BLUE `xdelta`, the NATS header). Add **`--realtime`** to throttle the output to
`fs`, so blocks leave on an `epoch + n/fs` schedule — mimicking a hardware
sample clock feeding the sink. This is what you want when a downstream consumer
expects samples to arrive at the real rate (a live spectrum display, an SDR
playback emulation):

```sh
# Stream QPSK to a live receiver at the true 1 MS/s, not as fast as possible.
# Requires a nats-server reachable at the endpoint.
wfmgen --type qpsk --fs 1e6 --sps 8 --continuous --realtime \
       --output nats://127.0.0.1:4222/iq
```

The schedule is **drift-free**: each deadline is recomputed from the cumulative
sample count against a fixed epoch, so sleep jitter never accumulates — the
long-run rate is exactly `fs`. Pacing does **not** alter the samples; a file
written with and without `--realtime` is byte-identical.

If the producer can't keep up (a block takes longer than its `N/fs` period — an
*underrun*), `wfmgen` keeps the absolute timeline and prints a summary to stderr
at exit (`wfmgen: 3 underrun(s) — worst 1.2 ms behind real time`). Use
**`--realtime-resync`** instead to re-anchor the clock to "now" on each underrun,
staying near real time going forward at the cost of an inserted gap.

!!! note "Software pacing is average-rate, not sample-accurate"

    On a non-realtime OS you get a drift-free *average* rate with bounded
    per-block jitter, never true sample-clock fidelity. Keep blocks large enough
    that the period `N/fs` comfortably exceeds scheduler jitter, and let the
    consumer's buffer absorb the rest.

______________________________________________________________________

### The same clock in Python — `SampleClock`

The same C core is exposed as `SampleClock`, which paces and timestamps a stream
against an ideal `fs`-Hz clock — throttle a producer to real time and tag blocks
with their ideal timestamp:

<!-- docs-snippet: skip=illustrative real-time pacing loop over reader IQ -->

```python
from doppler.wfm import Composer, SampleClock, StreamSink

# Requires a nats-server reachable at the endpoint.
comp = Composer(type="qpsk", sps=8, continuous=True)
clk = SampleClock(fs=1e6)
with StreamSink("nats://127.0.0.1:4222/iq") as sink:
    while True:
        blk = comp.execute(4096)
        ts = clk.stamp()              # ideal ns timestamp of this block
        sink.send(blk, fs=1e6, fc=0.0)
        clk.pace(len(blk))            # sleep to epoch + n/fs (GIL released)
```

The schedule is drift-free (deadlines come from the cumulative sample count, not
summed sleeps); underruns are counted in `clk.underruns` / `clk.max_lateness`,
and `SampleClock(fs, resync=True)` re-anchors to "now" on each underrun.
`SampleClock` and `StreamSink` are POSIX-only. See the
[Python API](python.md) for the full class surface.

______________________________________________________________________

## See also

- [Waveforms](waveforms.md) — what goes into a segment.
- [Python API](python.md) — the same scenes from Python.
- [Gallery: scene composition](../../gallery/wfm-composition.md).
