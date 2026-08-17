# API taxonomy: the DSP building-block hierarchy and its naming axis

**Status:** hierarchy + naming axis still live design reference; all 5
concrete §4 rename proposals have landed
**Scope:** distill doppler's implicit organizing vision for the DSP building
blocks in `objects/*.toml` into an explicit hierarchy, so that naming a new
class — or renaming an existing one — follows from *where it sits*, not from
whatever felt right at the time. All five concrete rename proposals in §4
(the one that motivated this doc,
[`Channel` → `Despreader`](#41-trackchannel-dsssdespreader-continuous-dsssdespreader-dsssburstdespreader),
plus four more surfaced by applying the same hierarchy) have landed — see
each subsection's `Landed:` link. §4.7 is a decision rather than a rename: it
files the composite receivers under layer 5 and picks **framing** as their
axis, which required no rename because the names already agreed. §4.6 and §5
remain open: flagged overlaps and lower-confidence ideas not yet actioned.

______________________________________________________________________

## 1. Why this doc exists

Renaming `track.Channel` (a GPS-flavored name — its own docstring said
"GPS-style tracking channel," and its `nav_period` constructor param is
GPS/GNSS jargon for "code periods per data bit") surfaced a real question: what
*should* it be called instead? Answering that for one class exposed that
doppler has never written down the horizontal structure its API is organized
around — only the vertical one.

[`architecture.md`](../architecture.md) states the vertical stack clearly:
DSP library → transport → pipeline CLI → apps. That tells you nothing about
how the ~50 classes *inside* the DSP library relate to each other, which is
exactly the context naming needs. That horizontal structure turns out to
already exist, just scattered across three places that mostly agree:

- The [gallery nav](../../mkdocs.yml) groups examples into informal categories
    (Sources & Waveforms, Filters & Resampling, Detection & Acquisition,
    Synchronization Loops, Constellations & Receivers, Measurement,
    Quantization & Fixed-Point, Gain Control).
- [`dsss-use-cases.md`](../dev/dsss-use-cases.md) states a real design
    principle explicitly for one slice of the library: DSSS receivers come in
    exactly two flavors — **UC1**, "GPS-like always on" (continuous), and
    **UC2**, "Burst Transmission" (latency-bound) — built from the same
    acquisition primitives but composed differently downstream. It even says
    the burst path "hands off... to the shipped `Despreader`" — so
    `dsss.Despreader` was already conceived, in the design's own language, as
    the *burst* payload tracker.
- The [naming survey](#5-lower-confidence-not-actioned) run while researching
    this doc, which checked all ~50 classes against the `Channel`/`nav_period`
    pattern.

This doc distills those three into one explicit hierarchy, states the naming
axis each layer should follow, and proposes renames where a class doesn't fit
its layer's axis.

______________________________________________________________________

## 2. The hierarchy

Every class is a DSP building block at one of eight conceptual layers,
independent of which Python module it currently lives in (module boundaries
are a packaging/build concern — see [`repository-map.md`](../dev/repository-map.md)
— not a naming one, though closer alignment between the two is a nice side
effect where it's cheap).

| #   | Layer                                | What it does                                                    | Current members                                                                                                           |
| --- | ------------------------------------ | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| 1   | **Sources**                          | produce samples from nothing                                    | `LO`, `NCO`, `PN`, `AWGN`, the `wfm_compose` family                                                                       |
| 2   | **Filtering & rate conversion**      | reshape a stream's spectrum/rate                                | `FIR`, `CIC`, `Resampler`, `RateConverter`, `HalfbandDecimator`, `HalfbandDecimatorQ15`, `Farrow`, `DDC`, `MovingAverage` |
| 3   | **Detection & acquisition**          | find presence/timing/frequency *once*, no persistent feedback   | `Corr`, `Corr2D`, `CorrDetector`, `CorrDetector2D`, `Acquisition`, `PolynomialPhaseEstimator`                             |
| 4   | **Tracking & synchronization loops** | continuously refine an estimate via feedback, sample-by-sample  | `LoopFilter`, `Costas`, `Dll`, `CarrierMpsk`, `CarrierNda`, `SymbolSync`                                                  |
| 5   | **Composite receivers**              | combine layers 2+3+4 into one receiver, in exactly two framings | continuous: `dsss.Despreader`, `track.ContinuousMpskReceiver`; burst: `dsss.BurstDespreader`, `BurstDemod`                |
| 6   | **Measurement & analysis**           | characterize signal quality                                     | `PSD`, `ToneMeasure`, `NPRMeasure`, `IMDMeasure`, `Specan`                                                                |
| 7   | **Quantization & fixed-point**       | model/convert numeric representations                           | `ADC`, the `cvt` family, Q15/UQ15                                                                                         |
| 8   | **Support**                          | gain control, accumulation, plumbing                            | `AGC`, `AccF32`/`AccCf64`/`AccQ15`/`AccQ8`/`AccTrace`, `Buffer`, `DelayCf64`                                              |

## 3. The naming axis per layer

Most layers already hold one naming axis consistently — the trouble is
concentrated in layers 3–5, which never had one written down:

| Layer            | Axis                                                                      | Holds today?           |
| ---------------- | ------------------------------------------------------------------------- | ---------------------- |
| 1 Sources        | mechanism (`NCO`, `PN`)                                                   | yes                    |
| 2 Filtering      | mechanism (`FIR`, `CIC`, `Corr`-adjacent)                                 | yes, since §4.2 landed |
| 3 Detection      | mechanism (`CorrDetector`, `CorrDetector2D`)                              | yes, since §4.4 landed |
| 4 Tracking loops | *should be* one of {mechanism, target-signal-type} — currently mixes both | **no** — see §4.6      |
| 5 Composites     | framing (continuous/burst) + role (despread/demod/receive)                | yes — §4.1, §4.7       |
| 6 Measurement    | what it measures (`ToneMeasure`)                                          | yes                    |
| 7 Quantization   | representation (`Q15`, `UQ15`)                                            | yes                    |
| 8 Support        | role                                                                      | yes                    |

## 4. Rename proposals

### 4.1 `track.Channel` → `dsss.Despreader` (continuous), `dsss.Despreader` → `dsss.BurstDespreader`

**Landed:** [doppler-dsp/doppler#357](https://github.com/doppler-dsp/doppler/issues/357).

The rename that started this doc. Per §1, the design doc's own language
already treats the burst path's target as "the shipped `Despreader`" — so the
existing `dsss.Despreader` keeps the base name and picks up the `Burst` prefix
that `BurstDemod` already establishes, and `track.Channel` — a Costas+DLL
continuous tracker with no burst-specific features (no preamble-aided
acquisition) — becomes the plain, continuous-flavored `Despreader`, moved into
`dsss` to sit next to its burst sibling.

| From                           | To                | Module                 |
| ------------------------------ | ----------------- | ---------------------- |
| `track.Channel`                | `Despreader`      | `track` → `dsss`       |
| `dsss.Despreader`              | `BurstDespreader` | `dsss` (unchanged)     |
| `Channel`'s `nav_period` param | `periods_per_bit` | (moves with the class) |

`nav_period` is GPS/GNSS jargon for "code periods per data bit" — the same
smell as the class name it lived on. `periods_per_bit` says exactly that,
generically.

**Not yet decided:** whether `BurstDemod` (frame-sync + CRC on top of
despreading) should also move to sit next to both, or stays separate as a
higher-scope object built *on* `BurstDespreader`. Flagging, not proposing.

### 4.2 `dsss.PolyPhaseEstimator` (ppe) → `PolynomialPhaseEstimator`

**Landed:** [doppler-dsp/doppler#358](https://github.com/doppler-dsp/doppler/issues/358).

Does polynomial-phase (frequency + chirp-rate) estimation via a 2-D
matched-filter/dechirp search — nothing to do with polyphase filter-bank
structures. This is worse than an external-domain leak: it collides with the
library's *own* vocabulary. "Polyphase" is used consistently elsewhere
(`Resampler`, `RateConverter`, `HalfbandDecimator`, `DDC`, `NCO`, `Farrow`) to
mean the classic decimation/interpolation branch structure — layer 2's axis.
Compressing "polynomial-phase" into the visually/verbally identical
"PolyPhase" breaks that axis for anyone skimming the API.

| From                 | To                         |
| -------------------- | -------------------------- |
| `PolyPhaseEstimator` | `PolynomialPhaseEstimator` |

Picked `PolynomialPhaseEstimator` over the shorter `ChirpEstimator`: the
header's own brief already calls it "a polynomial-phase estimator," and it
degenerates to pure-Doppler (no chirp at all) when `max_rate = 0` — "Chirp"
would misleadingly imply a specific waveform the object doesn't require.

### 4.3 `filter.HBDecimQ15` → `resample.HalfbandDecimatorQ15`

**Landed:** [doppler-dsp/doppler#359](https://github.com/doppler-dsp/doppler/issues/359).

Same halfband 2:1 decimator algorithm as `resample.HalfbandDecimator`,
differing only by dtype (Q15 fixed-point vs. CF32) — but one name is
abbreviated, the other spelled out, and they live in different modules with no
naming cue that they're siblings.

| From                | To                     | Module                |
| ------------------- | ---------------------- | --------------------- |
| `filter.HBDecimQ15` | `HalfbandDecimatorQ15` | `filter` → `resample` |

### 4.4 `spectral.Detector` / `Detector2D` → `CorrDetector` / `CorrDetector2D`

**Landed:** [doppler-dsp/doppler#360](https://github.com/doppler-dsp/doppler/issues/360).

Too generic for what it actually is — an FFT-correlation + CFAR-threshold
detector, built directly on `Corr`/`Corr2D` but with no naming relationship to
either. "Detector" alone could mean an edge detector, onset detector, envelope
detector, preamble detector, etc., in different DSP subfields.

| From         | To               |
| ------------ | ---------------- |
| `Detector`   | `CorrDetector`   |
| `Detector2D` | `CorrDetector2D` |

### 4.5 `track.MpskReceiver`'s `auto_handover` param → `acq_to_track`

Cellular-network jargon ("handover" = transferring a call between base
stations) describing what's purely an internal loop-mode transition: swap from
the NDA acquisition-mode carrier loop to a lower-jitter decision-directed
tracking-mode loop once locked. Same flavor of leak as `nav_period`.

| From            | To             |
| --------------- | -------------- |
| `auto_handover` | `acq_to_track` |

**Landed:** [doppler-dsp/doppler#361](https://github.com/doppler-dsp/doppler/issues/361).

### 4.6 Flagged, no rename proposed yet

- **`track.Costas` vs. `track.CarrierMpsk`** — `CarrierMpsk`'s own docstring
    states "at m=2 this is exactly the BPSK Costas loop." Same overlap pattern
    as `Channel`/`Despreader`, but retiring or aliasing `Costas` is a bigger
    design call than a naming fix — flagging for the same kind of reorg
    conversation, not proposing a rename here.
- **Layer 4's mixed axis** — `Costas`/`Dll`/`LoopFilter` name by mechanism;
    `CarrierMpsk`/`CarrierNda` name by target-signal-type. Worth picking one
    axis deliberately once the `Costas`/`CarrierMpsk` overlap above is
    resolved, since the axis choice and the overlap are the same underlying
    question.

### 4.7 Composite receivers are layer 5, and **framing** is the axis

**Decided; no rename required.** The M-PSK receivers were filed under layer 4,
and that was the misfiling behind the question. `MpskReceiver` is a matched DDC
plus two loops — it *combines* layers 2, 3 and 4, which is layer 5's own
definition. Only `dsss` had composites when this doc was written, so layer 5
was described as "DSSS composite receivers" and phrased around PN; the shape
was never DSSS-specific. Layer 5 is now **Composite receivers**, and the M-PSK
family sits in it.

That matters because layer 5's axis is the one that **holds** (§4.1), while
layer 4's is the one that does not (§4.6). Moving the composites out removes
one of layer 4's two confusions for free.

**Three axes are available, and exactly one earns a class name.**

| axis            | earns a name?           | why                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| --------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **framing**     | **yes**                 | continuous and burst are different *objects* — different acquisition, different latency contract. It is already `mpsk.md` §0's own division, and §4.1's landed precedent                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **modulation**  | no                      | `m ∈ {2,4,8}` is one constructor argument over one implementation. Naming it forces three classes per framing over a single core, or makes the name a lie at `m = 4`                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **input dtype** | **no, since jm 0.62.0** | `MpskReceiverR` WAS a separate type because `steps()` takes `f32` rather than `cf32` — a *method-signature* difference, which `mpsk.md` §1.2 made the type/flavor test. [just-makeit#1012](https://github.com/just-buildit/just-makeit/issues/1012) removed the constraint that forced it: a view method restating a parent's NAME may now declare its own signature when it binds its own C symbol via `fn`. So the dtype is a **flavor** (a view) too, and the collapse landed it — `mpsk-refactor.md`. The test itself is unchanged; what changed is that jm can now express the answer |

So `BpskStream` / `BpskBurst` — naming the modulation, dropping the role — is
rejected on two counts. It splits an axis that is a working parameter, and
`Stream` collides with **layer 1**, whose members produce samples from
nothing: doppler ships a BPSK *source* (`wfm.bits(modulation="bpsk")`), so
`BpskStream` reads as a generator rather than a receiver.

**Spelling: mark both framings explicitly.**

| framing    | name                     |
| ---------- | ------------------------ |
| continuous | `ContinuousMpskReceiver` |
| burst      | `BurstMpskReceiver`      |
| (general)  | `MpskReceiver`           |

This deliberately **diverges from §4.1's unmarked-continuous**, and the reason
is local rather than a change of mind: in the DSSS pair the plain name was
free, so `Despreader`/`BurstDespreader` could carry the distinction with one
prefix. Here the plain name is taken — `MpskReceiver` is the general,
two-mode-capable object, and the continuous flavor is a *view* that pins its
gating (`acq_to_track = 0`, `nda_tap = mf_in`). Reusing the plain name for the
continuous flavor would mean renaming or retiring the general object, which is
a breaking change bought for symmetry alone. Marking both is longer and asks
the reader to know nothing.

`BurstMpskReceiver` does not exist yet. It is named here so that when it does,
the name is a consequence of a written axis rather than a fresh argument.

## 5. Lower-confidence / not actioned

Found during the survey, judged borderline or low-priority — noted for
completeness, not proposed for action:

- `Acquisition`'s `cn0_dbhz` param — GNSS term, but also standard general RF
    link-budget vocabulary; borderline rather than a clear leak.
- `dwell` across the `Corr`/`Corr2D`/`CorrDetector`/`CorrDetector2D` family —
    reads as radar/EW jargon outside that subfield, but used consistently
    across the whole family, so low priority.
- `CarrierNda` — "Nda" (non-data-aided) is synchronization-theory
    terminology, less immediately recognizable than sibling `CarrierMpsk`'s
    modulation-based name — this is the layer-4 axis inconsistency in §4.6,
    not a standalone issue.
- `BurstDemod` — legitimately a bigger-scope object (dechirp + despread +
    frame-sync + CRC) than a despreader. Not itself a naming defect; worth
    double-checking it doesn't get confused with `BurstDespreader`, now that
    §4.1 has landed.

Everything else checked (`FFT`/`FFT2D`, `FIR`, `CIC`, `AWGN`, `DDC`, the `cvt`
and `Acc*` families, the `Resampler`/`RateConverter`/`HalfbandDecimator`
family, `Specan`, `SymbolSync`, the measurement suite, `DelayCf64`,
`MovingAverage`) held up against its layer's axis with no changes suggested.

**Out of scope for this survey** (not `class_name`-declared objects in
`objects/*.toml`): the `wfm_compose` classes (`Synth`, `Segment`, `Timeline`,
`Composer`, `Plan`) — worth a follow-up pass if this taxonomy should extend
there too. `Ddcr` was also out of scope as a handle module; it is now a
declared object (with a `MatchedDdcr` flavor) and sits on the same axis as
`DDC`.

## 6. See also

- [Repository Map](../dev/repository-map.md) — the vertical file/generation
    architecture this doc's hierarchy sits orthogonal to.
- [DSSS Primary Use Cases](../dev/dsss-use-cases.md) — the UC1/UC2 framing
    §4.1's rename is derived from.
- [Architecture](../architecture.md) — the 4-layer vertical stack.
