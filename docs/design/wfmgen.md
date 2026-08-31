# wfmgen — the waveform generator

!!! note "Status: shipped, and uncertified"

    Every part of this is built, and most of it is gated. None of it is
    **certified** — no `wfm` object appears in the
    [Validation Log](../dev/contributing/validation-log.md) — so there is
    plenty of measurement but no published statement of the bounds a caller
    may rely on and that a change may not silently break — what
    [Object Validation](../dev/contributing/validation.md) calls a certified
    envelope.
    That is the gap this page exists to make visible, and
    [Unknowns](#unknowns) is the list.

A fast, full-featured waveform generator — modulations, impairments and
output streams — reached through four APIs over one engine: C at the core,
with the CLI, the Python API and the JSON scene file wrapping it, all
rendering byte-identically.

Six design pages already own a slice of it and until now none owned the tool.
This page is the spine: what wfmgen is for, what it promises, what it
composes, and — the part that earns it a place — **what we do not yet know
about it**.

It contains no mechanics. How to *use* wfmgen is the
[Waveform Generator guide](../guide/wfmgen/index.md); how each slice works
is the page that owns it, linked below.

______________________________________________________________________

## Waveforms: Defining Terms

A scene is built as a ladder, each rung wrapping the one above it:

```text
  tone() · bpsk() · qpsk() · pn() · noise() · chirp() · bits()
                        │  factories, each returning a Synth
                        ▼
   Synth  ── one source ──────────────── .steps(n) ──►  samples
     │
     │  Segment(...) or Segment.sum(synth, ...)
     ▼
  Segment ── sources summed over a fixed duration
     │
     │  .add(segment, ...)
     ▼
  Timeline ── segments in sequence
     │
     │  Composer(timeline | segment | [segment, ...])
     ▼
  Composer ── the whole scene ────────── .compose() ──►  samples
```

Either end renders: a `Synth` on its own for a single source, a `Composer`
for the whole scene. Five terms name the rungs, and the rest of this page
assumes them. The last two columns are where each one lives, so the
vocabulary and the API are defined together rather than a page apart.

| term             | is                                                                                                                                    | in C                                    | in Python                                      |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------- | ---------------------------------------------- |
| **source**       | one emitter — a waveform type plus its frequency, level, SNR, framing and impairments                                                 | `wfm_source_t`                          | `Synth`                                        |
| **segment**      | one or more sources summed together over a fixed duration, at one sample rate, with optional leading delay and trailing gap           | `wfm_segment_t`                         | `Segment`                                      |
| **scene**        | the concatenation of all segments plus the global options (repeat, continuous, seed advance, headroom) — the full waveform definition | a `wfm_segment_t[]` plus those flags    | a `Timeline` (or list) plus `Composer`'s flags |
| **instance**     | one particular playing of a segment                                                                                                   | the composer's `instance` counter       | the `repeats` argument                         |
| **ranged field** | a field given `[lo, hi]` instead of one number, and re-picked at random for every instance                                            | a `_hi` companion + a `WFM_RANGE_*` bit | a `(lo, hi)` tuple                             |

______________________________________________________________________

## Why

**Waveform Generation** Fast, full-featured and easy to drive.

- **Modulations** — tone, noise, PN/MLS, BPSK, QPSK, chirp, arbitrary bit
    patterns, arbitrary symbol streams, framing and DSSS in both burst and
    continuous or asynchronous form.
- **Impairments** — AWGN under four SNR conventions, per-source level with
    headroom and observable clipping, and clock Doppler with offset, rate and
    two channel lifetimes. Declared per *source*, so two emitters summed into
    one segment can be on different geometries.
- **Output streams** — raw, CSV, BLUE type-1000 and SigMF; ten sample types
    in either endianness; to a file, to stdout, or to NATS. Wall-clock pacing
    is a separate switch, orthogonal to all of them: a paced file and an
    unpaced NATS firehose are both ordinary runs.

Easy to use is a design goal, not a nicety: the same scene is a C struct, a
handful of CLI flags, a Python object, or a JSON file — four APIs onto one
engine, rendering byte-identically. Fast is a goal too — `Plan` exists so a
Monte-Carlo sweep re-weights a cached render instead of re-synthesising it,
and `Plan.prepare()` fans its per-source builds across cores.

**Secondarily — and only secondarily — it is our single source of truth
(SSOT) for stimulus.** Every receiver in this library is measured against a
signal somebody had to generate, and when that signal came from a
hand-rolled numpy cell per test, three things followed: the stimulus drifted
between tests, a receiver was scored against a waveform nobody else could
reproduce, and a bug in the stimulus read as a bug in the receiver. wfmgen ended that by being one
implementation, declared rather than coded.

Both readings point the same way on certification. A tool users depend on
owes them a stated envelope; a tool we calibrate against owes it to every
result derived through it. It currently has neither.

## Use cases

Users first — the internal rows are real, but they are not why it exists.

| who | with what | what they do with the answer |
| \--- | --- | --- | --- |
| Someone who needs a waveform | `wfmgen --type qpsk --snr 12 -o capture.cf32` | feeds a receiver, a lab instrument, or another tool |
| A C application | `wfm_compose_create()` -> `_execute()` -> `_destroy()` | pulls IQ straight into its own buffer, no Python in the process |
| A field/interop test | `wfmgen … -o capture.sigmf` | hands a real file to another tool, with metadata saying what is in it |
| A live consumer | `wfmgen --realtime -o nats://…` | wall-clock pacing, here to NATS but available to any sink |
| A bug report | `--record scene.json` | replays someone else's exact waveform byte-for-byte |
| A receiver test | `Composer([...]).compose()` in-process | scores demod/BER against truth it also gets from the scene |
| A Monte-Carlo sweep | `Plan.prepare()` then `.at(snr)` | re-weights a cached render instead of re-synthesising |
| A validation report | a scene declared once, swept | measures a limit that goes in a certified envelope |

The `--record` row shapes the design most: **a scene is a value**, not a
script. Anything a run can be told must be expressible in the JSON a
`--record` writes, or the run is not reproducible.

## Design goals

Each is a promise a caller may lean on, and each names the gate that keeps it
true. Where a promise is *not* gated, that is said — and the first three, the
ones the tool exists for, are the least gated of all.

1. **Full-featured across the three axes.** A user should not have to leave
    wfmgen for a modulation, an impairment or a container the domain
    routinely needs. *Gated:* only per feature — `check_wfmgen_flag_docs.py`
    proves every one of the 67 flags is documented, but nothing states the
    intended coverage, so a missing capability is a gap nobody's gate can
    see.

1. **Easy to use, identically from four APIs.** This is a C-first library,
    so the **C API is the primary one** — `wfm_compose_create` / `_execute` /
    `_destroy` and its siblings — and the CLI, the Python objects and the
    JSON scene are wrappers over it, never reimplementations. The same scene
    expressed through any of the four renders byte-identically. *Gated:*
    `make drift-check` and `gen_wfmgen_flag_matrix.py` keep the four from
    diverging, `make test-examples-c` runs the C example, and the doc fences
    compile against the real library; "easy" itself is a review judgement.

1. **Fast enough to be the inner loop.** A sweep should not pay to
    re-synthesise what it already rendered: `Plan` caches a scene's clean
    per-source signal and re-weights it, and `Plan.prepare()` fans those
    builds across cores. *Half gated:* `make bench-coverage-check` is in the
    gate set and proves every tested component **has** a benchmark that runs,
    but nothing gates the **numbers** — `make bench-compare` is in no CI job
    and the perf-regression workflow was removed as untrustworthy
    ([#543](https://github.com/doppler-dsp/doppler/issues/543)). So a
    regression is recorded, not caught. See [Unknowns](#unknowns).

1. **A scene round-trips.** `--record` → `--from-file` reproduces the
    waveform byte-for-byte, including the resolved `headroom` and
    `seed_advance` that a naive re-render would lose.
    *Gated:* `test_cli_record_replays.py`, `test_schema.py`.

1. **A ranged field records its span, not one draw.** Each drawn value is a
    hash of `(seed, repeat index, segment, source, field)` — no RNG state —
    so a recorded sweep replays as the same *sequence* of draws.
    *Gated:* `test_cli_ranged.py`; the draw's **statistics** are not, see
    [Unknowns](#unknowns).

1. **Every draw is readable back — without it there is no ground truth.**
    A ranged field is only usable if a consumer can learn what it actually
    drew: score a receiver against a scene whose frequency was re-picked per
    instance and you are scoring against a number you do not have. So the
    composer and `wfm_compose_draws()` resolve through the *same* helper, and
    the drawn value is reported per source per instance rather than inferred.
    Reachable from every API that should have it: `wfm_compose_draws()` in
    C, `draws(scene)` in Python, and the SigMF metadata beside a capture —
    all three reading the same rows, so they cannot disagree. `--record` is
    the deliberate exception: it stores the **span**, because a spec answers
    "what does this permit" and a draw answers "what did this run do".
    *Gated:* `test_compose.py`, including a test that the Python rows equal
    the metadata for the same scene. It was not always so — a ranged scene
    once annotated every instance with its `lo`, measured 1224 Hz and 6.0 dB
    out ([#1086](https://github.com/doppler-dsp/doppler/issues/1086)).

1. **One waveform engine, one pull path.** A one-segment scene is
    byte-identical to rendering that one source on its own, and the
    streaming
    composer and the `Plan` cache pull through the same function, so a
    holdover cannot exist on one side and not the other.
    *Gated:* `test_wfm_compose.c`.

1. **Adding a knob cannot fork an API.** The C struct field is the
    declaration, and JSON, Python and the CLI each reach it from there
    rather than restating it; enum names have one C home. *Gated:* `make drift-check`, `check_wfm_enum_tables.py`,
    `check_wfmgen_flag_docs.py`, `gen_wfmgen_flag_matrix.py`.

1. **0 dBFS (decibels relative to full scale) is unit average power**, and
    clipping is observable rather than
    silent. *Gated:* `TestQuantization`; the peak-to-average power ratio
    (PAPR) budget behind it is not measured, see [Unknowns](#unknowns).

## What it composes, and what it does not re-implement

wfmgen owns **sequencing, levels, framing and I/O**. Every signal primitive
it uses is somebody else's object, called rather than copied — the rule that
`nco`/`lo`/`dll` broke once by growing three private copies of one
conversion.

```text
WRAPPERS  wfmgen CLI (67 flags) · doppler.wfm (Python) · JSON scene file
          └── none holds logic; each fills in the C structs below

ENGINE    wfm_compose ── composer: segments, instances, gaps, the noise floor
            ├── wfm_synth ── renders one source:
            │     │          tone/noise/pn/psk/chirp/bits/symbols/dsss
            │     └── lo · nco · awgn · fir · resamp · dsss_spread · rrc/rc
            ├── doppler_channel ── clock Doppler on a source (gh-942)
            ├── wfm_frame ── the frame as a description
            │     └── ccsds_tm · conv · rs · gold · crc16
            └── wfm_writer ── raw / csv / BLUE / SigMF
          wfm_plan   ── the sweep cache (a handle over the same render path)
          wfm_sink   ── where samples go: file, stdout or NATS
          wfm_reader ── reading a capture back
```

Seventeen components reach `wfm_compose` as declared `depends_on` entries.
The composer's job is that none of them is re-implemented inside it.

## Shape and state

- **Repeats are trials, not copies.** Each instance re-picks every ranged
    field and draws fresh AWGN, while the signal itself — codes, payload, PN
    phase — stays fixed. That is what makes `repeats` a Monte-Carlo control
    rather than a duplicator.
- **Impairments belong to a source, not a segment.** Level, SNR and clock
    Doppler are each declared on the source that carries them, because two
    transmitters summed into one segment are on different geometries and a
    segment-wide figure could not describe both.
- **The composer is a pull engine.** The caller asks for `k` samples and
    gets `k`, whatever block sizes the internals happen to use. A resampling
    impairment threatens that — clock Doppler consumes roughly
    `k * (1 + d)` inputs per `k` outputs — so the renderer keeps whatever it
    produced but has not yet handed out, and serves the next call from that
    before generating more.
- **Gaps are not silence.** They carry the segment's noise floor by default,
    so an impairment keeps running through them — otherwise a per-second rate
    quietly means "per second of transmission".

## What is already measured

Two layers, and the C one comes first. `native/tests/test_wfm_*.c` pin the
engine's own contracts — the composer's segment sequencing and block-size
invariance, the frame description, the reader/writer round trips — and run
under ctest, ASan, TSan and UBSan on every push.

Over them, `src/doppler/wfm/tests/test_dsp_correctness.py` is 509 lines and
is the honest answer to "is the instrument right": tone unit magnitude, exact-bin
placement and LUT SFDR; noise unit power, I/Q independence, Gaussianity and a
flat PSD; PN period, balance and two-level autocorrelation; the BPSK/QPSK
constellations; chirp instantaneous frequency and phase continuity; RRC
Nyquist no-ISI and polyphase-equals-dense; DSSS spread/despread; the SNR
formulae; integer round-trip within 1 LSB; and determinism, including
`step`/`steps` parity.

That is a strong base, and it is **not** a certification. Both layers pin
claims one at a time, as they were thought of; neither enumerates what the
headers promise, nor states a bound a caller may rely on. Closing that is
[Object Validation](../dev/contributing/validation.md)'s job, in the order it
insists on: the header's claims first, then the C tests that pin them, then
the report.

## Unknowns

Written down so a sweep is designed to *find out* rather than to confirm a
decision already made. Each is first a measurement (the *explore* phase of
[Adding an algorithm](../dev/contributing/adding-algorithms.md)) and then a
stated limit (its *certify* phase).

1. **The envelope itself.** No `wfm` object is certified, so a user is
    handed a generator with no statement of what it guarantees — the largest
    gap, and the one the others are found by.
1. **Realised Es/N0 in `esno`, `ebno` and `auto`.** Only `fs` mode has a
    realised-SNR test. The requested-versus-measured error, and its
    dependence on `sps` and pulse shape, is unmeasured — and
    `wfm_awgn_amplitude` computes in float32, so it is not bit-exact against
    the closed form.
1. **The ranged draw's statistics.** Reproducibility is pinned; *uniformity*
    over `[lo, hi]` and *independence* across `(field, instance, source)` are
    not. A hash that correlates two fields would produce sweeps that silently
    do not cover their space.
1. **PAPR versus headroom.** `--headroom` exists because shaped waveforms
    have PAPR > 0 dB, but no measured PAPR-per-waveform or clipping-fraction
    curve says how much headroom a given scene needs.
1. **Doppler accuracy end to end.** ppm requested versus time-base scale
    measured at the composer's output, and the same for `doppler_rate` — the
    channel is certified nowhere and the composition is new
    ([#942](https://github.com/doppler-dsp/doppler/issues/942)).
1. **Measured speed, but no promised speed.** The numbers exist and are
    published — [Benchmarks](../benchmarks.md) carries `wfm_synth` at tens of
    GSa/s for a 64k block, per-waveform costs, `wfm_compose::execute`, and
    the reader/writer paths, with snapshots per release. What is missing is a
    **claim**: an envelope, with its conditions, that a user may rely on and
    that a regression would violate. Nor does any of it answer the questions
    a scene raises rather than a component — how the rate scales with `sum`
    source count and with framing/coding, what `Plan` actually buys over
    re-composing, and the maximum `fs` a `--realtime` stream sustains before
    it slips, per sample type and sink.
1. **Multi-source noise-floor resolution.** How accurately the shared floor
    is placed when sources sit at widely different levels.
1. **The composer cannot be checkpointed.** Across `execute()` calls it
    carries its whole position in the scene — which segment, which instance,
    and which pass of a repeating scene — plus the renderer it built for
    each source and any persisting Doppler channels. Yet it declares no **state triplet**
    (`state_bytes` / `get_state` / `set_state`, which every stateful object
    here owes so it can be checkpointed and resumed), and it is in neither
    the serializable set nor `scripts/.serializable-stateless` — so
    `check_serializable.py` never reaches it and nothing says so. A long
    paced run therefore cannot be resumed. This is a missing capability
    rather than a missing measurement: owed by the lifecycle's *instrument*
    phase, before certification rather than by it.

## Non-goals

- **Reading captures** is [Capture I/O](../guide/wfm-io/index.md); wfmgen
    writes, `Reader` reads, and they meet at the file format.
- **Being a channel simulator.** Impairments are declared per source and each
    is a named model; there is no propagation environment, no multipath, no
    interference scheduler.
- **Being a receiver.** A scene carries its own truth so a receiver can be
    scored, but wfmgen never demodulates.

## Related pages

- [Waveform Amplitude & Composition](wfmgen-composition.md) — level, power,
    `sum`, headroom: the RFC behind the "0 dBFS is unit average power" goal
- [Capture Files](capture-files.md) · [Ending a Capture](end-of-capture.md) —
    the `Reader`/`Writer` subsystem and its termination contract
- [Streaming](streaming.md) · [Ending a Wait](io-termination.md) — the
    transport half
- [A Frame as a Description](frame-description.md) — the framing model
- [One Home for the Waveform Enum Tables](waveform-enum-ssot.md) — the enum
    half of "adding a knob cannot fork an API"
- [State Serialization](state-serialization.md) — the interface the
    composer's missing state triplet owes
- [Adding an algorithm](../dev/contributing/adding-algorithms.md) — the
    lifecycle this page is phase 1 of
