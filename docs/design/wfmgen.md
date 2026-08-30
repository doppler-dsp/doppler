# wfmgen — the waveform generator

!!! note "Status: shipped, and uncertified"

    Every part of this is built and gated. None of it is **certified**: no
    `wfm` object appears in the [Validation Log](../dev/contributing/validation-log.md),
    so there is measurement but no stated envelope. That is the gap this page
    exists to make visible, and [Unknowns](#unknowns) is the list.

A fast, full-featured waveform generator — modulations, impairments and
output streams — driven identically from a CLI, a Python API and a JSON
scene file. Six design pages already own a slice of it and until now none
owned the tool. This page is the spine: what wfmgen is for, what it promises,
what it composes, and — the part that earns it a place — **what we do not yet
know about it**.

It contains no mechanics. How to *use* wfmgen is the
[Waveform Generator guide](../guide/wfmgen/index.md); how each slice works
is the page that owns it, linked below.

______________________________________________________________________

## Why

**wfmgen is a waveform generator for people who need waveforms.** Fast,
full-featured and easy to drive, across three axes:

- **Modulations** — tone, noise, PN/MLS, BPSK, QPSK, chirp, arbitrary bit
    patterns, arbitrary symbol streams, and DSSS in both burst and continuous
    asynchronous form; with CCSDS framing on top of any of them (Reed-Solomon,
    convolutional, randomiser, ASM, block interleaving, CRC).
- **Impairments** — AWGN under four SNR conventions, per-source level with
    headroom and observable clipping, and clock Doppler with offset, rate and
    two channel lifetimes. Declared per *source*, so two emitters in one scene
    can be on different geometries.
- **Output streams** — raw, CSV, BLUE type-1000 and SigMF; ten sample types
    in either endianness; to a file, to stdout, or paced to wall clock over
    NATS.

Easy to use is a design goal, not a nicety: the same scene is a handful of
CLI flags, a Python object, or a JSON file, and all three drive one engine to
byte-identical output. Fast is a goal too — `Plan` exists so a Monte-Carlo
sweep re-weights a cached render instead of re-synthesising it, and
`Plan.prepare()` fans its per-source builds across cores.

**Secondarily — and only secondarily — it is our SSOT for stimulus.** Every
receiver in this library is measured against a signal somebody had to
generate, and when that signal came from a hand-rolled numpy cell per test,
all three predictable things happened: the stimulus drifted between tests, a
receiver was scored against a waveform nobody else could reproduce, and a bug
in the stimulus read as a bug in the receiver. wfmgen ended that by being one
implementation, declared rather than coded.

Both readings point the same way on certification. A tool users depend on
owes them a stated envelope; a tool we calibrate against owes it to every
result derived through it. It currently has neither.

## Use cases

Users first — the internal rows are real, but they are not why it exists.

| who                          | with what                                     | what they do with the answer                                           |
| ---------------------------- | --------------------------------------------- | ---------------------------------------------------------------------- |
| Someone who needs a waveform | `wfmgen --type qpsk --snr 12 -o capture.cf32` | feeds a receiver, a lab instrument, or another tool                    |
| A field/interop test         | `wfmgen … -o capture.sigmf`                   | hands a real file to another tool, with a sidecar saying what is in it |
| A live consumer              | `wfmgen --realtime -o nats://…`               | paces a stream to wall clock for a running pipeline                    |
| A bug report                 | `--record scene.json`                         | replays someone else's exact waveform byte-for-byte                    |
| A receiver test              | `Composer([...]).compose()` in-process        | scores demod/BER against truth it also gets from the scene             |
| A Monte-Carlo sweep          | `Plan.prepare()` then `.at(snr)`              | re-weights a cached render instead of re-synthesising                  |
| A validation report          | a scene declared once, swept                  | measures a limit that goes in a certified envelope                     |

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

1. **Easy to use, identically from three faces.** The same scene is CLI
    flags, a Python object, or JSON, and all three drive one engine to
    byte-identical output. *Gated:* `make drift-check` and
    `gen_wfmgen_flag_matrix.py` keep the faces from diverging;
    "easy" itself is a review judgement.

1. **Fast enough to be the inner loop.** A sweep should not pay to
    re-synthesise what it already rendered: `Plan` caches a scene's clean
    per-source signal and re-weights it, and `Plan.prepare()` fans those
    builds across cores. *NOT gated:* benchmarks exist for every wfm
    component, but **no stated throughput** — see [Unknowns](#unknowns).

1. **A scene round-trips.** `--record` → `--from-file` reproduces the
    waveform byte-for-byte, including the resolved `headroom` and
    `seed_advance` that a naive re-render would lose.
    *Gated:* `test_cli_record_replays.py`, `test_schema.py`.

1. **A ranged field records its span, not one draw.** Each drawn value is a
    hash of `(seed, repeat index, segment, source, field)` — no RNG state —
    so a recorded sweep replays as the same *sequence* of draws.
    *Gated:* `test_cli_ranged.py`; the draw's **statistics** are not, see
    [Unknowns](#unknowns).

1. **What is rendered and what is reported cannot disagree.** The composer
    and `wfm_compose_draws()` resolve through the same helper, so the SigMF
    sidecar carries the value each instance actually flew.
    *Gated:* `test_compose.py`. This was not always true — a ranged scene
    once annotated every instance with its `lo`, measured 1224 Hz and 6.0 dB
    out ([#1086](https://github.com/doppler-dsp/doppler/issues/1086)).

1. **One waveform engine, one pull path.** A one-segment scene is
    byte-identical to driving its `synth` directly, and the streaming
    composer and the `Plan` cache pull through the same function, so a
    holdover cannot exist on one side and not the other.
    *Gated:* `test_wfm_compose.c`.

1. **Adding a knob cannot fork a face.** A field is declared once and
    reaches JSON, Python and the CLI from that declaration; enum names have
    one C home. *Gated:* `make drift-check`, `check_wfm_enum_tables.py`,
    `check_wfmgen_flag_docs.py`, `gen_wfmgen_flag_matrix.py`.

1. **0 dBFS is unit average power**, and clipping is observable rather than
    silent. *Gated:* `TestQuantization`; the PAPR budget behind it is not
    measured, see [Unknowns](#unknowns).

## What it composes, and what it does not re-implement

wfmgen owns **sequencing, levels, framing and I/O**. Every signal primitive
it uses is somebody else's object, called rather than copied — the rule that
`nco`/`lo`/`dll` broke once by growing three private copies of one
conversion.

```text
wfmgen (CLI, 67 flags)
  └── wfm_compose  ── composer: segments, instances, gaps, the noise floor
        ├── wfm_synth ── one source: tone/noise/pn/psk/chirp/bits/symbols/dsss
        │     └── lo · nco · awgn · fir · resamp · dsss_spread · rrc/rc
        ├── doppler_channel ── clock Doppler on a source (gh-942)
        ├── wfm_frame ── the frame as a description
        │     └── ccsds_tm · conv · rs · gold · crc16
        └── wfm_writer ── raw / csv / BLUE / SigMF
  └── wfm_plan     ── the sweep cache (a handle over the same render path)
  └── wfm_sink     ── file or NATS, paced or not
  └── wfm_reader   ── reading a capture back
```

Seventeen components reach `wfm_compose` as declared `depends_on` entries.
The composer's job is that none of them is re-implemented inside it.

## Shape and state

- **`wfm_source_t` is the unit of declaration** — a waveform type plus its
    level, SNR, framing, and now its clock Doppler. Per *source*, not per
    segment, because two transmitters in one `sum` are on different
    geometries.
- **A segment is `delay | on | off`**, played `repeats` times; ranged fields
    re-draw per instance and the AWGN is fresh, while the signal stays fixed.
- **The composer is a pull engine.** The caller asks for `k` samples and gets
    `k`; everything about how the internals chunk is invisible, which is the
    property that a resampling impairment threatens and a holdover preserves.
- **Gaps are not silence.** They carry the segment's noise floor by default,
    so an impairment keeps running through them — otherwise a per-second rate
    quietly means "per second of transmission".

## What is already measured

`src/doppler/wfm/tests/test_dsp_correctness.py` is 509 lines and is the
honest answer to "is the instrument right": tone unit magnitude, exact-bin
placement and LUT SFDR; noise unit power, I/Q independence, Gaussianity and a
flat PSD; PN period, balance and two-level autocorrelation; the BPSK/QPSK
constellations; chirp instantaneous frequency and phase continuity; RRC
Nyquist no-ISI and polyphase-equals-dense; DSSS spread/despread; the SNR
formulae; integer round-trip within 1 LSB; and determinism, including
`step`/`steps` parity.

This is a strong base and it is **not** a certification: it pins claims
one at a time, in Python, without enumerating the header's promises or
stating a bound a caller may rely on. See
[Object Validation](../dev/contributing/validation.md) for the difference,
and the order that difference imposes.

## Unknowns

Written down so a sweep is designed to *find out* rather than to confirm.
Each is a phase-7 measurement and then a phase-8 limit.

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
1. **No stated throughput, for a tool whose first goal is speed.** Nothing
    says how fast wfmgen generates: samples/s per waveform type, how that
    scales with `sum` sources and with framing, what `Plan` actually buys
    over re-composing, or the maximum `fs` a `--realtime` stream sustains
    before it slips, per sample type and sink. Benchmarks exist for every wfm
    component, so the instruments are there and the claim is not — which is
    the gap that matters most to a user choosing this over writing their
    own.
1. **Multi-source noise-floor resolution.** How accurately the shared floor
    is placed when sources sit at widely different levels.
1. **The composer cannot be checkpointed.** It carries `epoch`, `instance`,
    segment position, its per-source synths and any persisting Doppler
    channels across `execute()` calls, yet declares no state triplet — and it
    is absent from both the serializable set and
    `scripts/.serializable-stateless`, so `check_serializable.py` never
    reaches it. A long paced run therefore cannot be resumed. This is a
    phase-6 gap, not a phase-8 one.

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
    `sum`, headroom: the RFC behind goal 6
- [Capture Files](capture-files.md) · [Ending a Capture](end-of-capture.md) —
    the `Reader`/`Writer` subsystem and its termination contract
- [Streaming](streaming.md) · [Ending a Wait](io-termination.md) — the
    transport half
- [A Frame as a Description](frame-description.md) — the framing model
- [One Home for the Waveform Enum Tables](waveform-enum-ssot.md) — goal 5's
    enum half
- [State Serialization](state-serialization.md) — what unknown 8 owes
- [Adding an algorithm](../dev/contributing/adding-algorithms.md) — the
    lifecycle this page is phase 1 of
