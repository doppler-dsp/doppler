<!-- domain: util -->

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
- [`dsss-use-cases.md`](../dev/contributing/dsss-use-cases.md) states a real design
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
are a packaging/build concern — see [`repository-map.md`](../dev/contributing/repository-map.md)
— not a naming one, though closer alignment between the two is a nice side
effect where it's cheap).

| #   | Layer                                | What it does                                                    | Current members                                                                                                           |
| --- | ------------------------------------ | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| 1   | **Sources**                          | produce samples from nothing                                    | `LO`, `NCO`, `PN`, `AWGN`, the `wfm_compose` family                                                                       |
| 2   | **Filtering & rate conversion**      | reshape a stream's spectrum/rate                                | `FIR`, `CIC`, `Resampler`, `RateConverter`, `HalfbandDecimator`, `HalfbandDecimatorQ15`, `Farrow`, `DDC`, `MovingAverage` |
| 3   | **Detection & acquisition**          | find presence/timing/frequency *once*, no persistent feedback   | `Corr`, `Corr2D`, `CorrDetector`, `CorrDetector2D`, `Acquisition`, `PolynomialPhaseEstimator`                             |
| 4   | **Tracking & synchronization loops** | continuously refine an estimate via feedback, sample-by-sample  | `LoopFilter`, `Costas`, `Dll`, `CarrierMpsk`, `CarrierNda`, `SymbolSync`, `AGC`                                           |
| 5   | **Composite receivers**              | combine layers 2+3+4 into one receiver, in exactly two framings | continuous: `dsss.Despreader`, `track.MpskReceiver`; burst: `dsss.BurstDespreader`, `BurstDemod`                          |
| 6   | **Measurement & analysis**           | characterize signal quality                                     | `PSD`, `ToneMeasure`, `NPRMeasure`, `IMDMeasure`, `Specan`                                                                |
| 7   | **Quantization & fixed-point**       | model/convert numeric representations                           | `ADC`, the `cvt` family, Q15/UQ15                                                                                         |
| 8   | **Support**                          | averaging, plumbing                                             | `AccF32`/`AccCf64`/`AccQ15`/`AccQ8`/`AccTrace`, `Buffer`, `DelayCf64`                                                     |

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

| axis            | earns a name?           | why                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| --------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **framing**     | **yes**                 | continuous and burst are different *objects* — different acquisition, different latency contract. It is already `mpsk.md` §0's own division, and §4.1's landed precedent                                                                                                                                                                                                                                                                                                                                                                                                              |
| **modulation**  | no                      | `m ∈ {2,4,8}` is one constructor argument over one implementation. Naming it forces three classes per framing over a single core, or makes the name a lie at `m = 4`                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **input dtype** | **no, since jm 0.62.0** | `MpskReceiverR` WAS a separate type because `steps()` takes `f32` rather than `cf32` — a *method-signature* difference, which `mpsk.md` §1.2 made the type/flavor test. [just-makeit#1012](https://github.com/just-buildit/just-makeit/issues/1012) removed the constraint that forced it: a view method restating a parent's NAME may now declare its own signature when it binds its own C symbol via `fn`. So the dtype is a **flavor** (a view) too, and the collapse landed it — `mpsk.md` §12. The test itself is unchanged; what changed is that jm can now express the answer |

So `BpskStream` / `BpskBurst` — naming the modulation, dropping the role — is
rejected on two counts. It splits an axis that is a working parameter, and
`Stream` collides with **layer 1**, whose members produce samples from
nothing: doppler ships a BPSK *source* (`wfm.bits(modulation="bpsk")`), so
`BpskStream` reads as a generator rather than a receiver.

**Spelling: mark both framings explicitly.**

| framing    | name                |
| ---------- | ------------------- |
| continuous | `MpskReceiver`      |
| burst      | `BurstMpskReceiver` |

**This now FOLLOWS §4.1's unmarked-continuous, and it did not always.** The
divergence was real while it lasted, and the reason it ended is worth keeping:
`MpskReceiver` used to be a general, two-mode-capable object, so the plain
name was taken and the continuous framing needed a marked one —
`ContinuousMpskReceiver`, a view that pinned `acq_to_track = 0`. Deleting the
handover (#877) removed the second mode, so the general object and the
continuous one became the same object and the marked name became a duplicate
of the plain one. The naming question dissolved rather than being decided:
there is one framing to name, so it takes the unmarked name.

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

## 6. The package tree: modules follow the hierarchy

§6.4, the consolidation into one `Correlator`, is decided, and #2752
tracks its four steps. The tree around it (§6.1 to §6.3) is a proposal, and
#2753 holds the questions that decide it. §2 calls module boundaries
a packaging concern. This section reverses that: the import path should
*be* the task, so a user who knows the task finds the class. Today 30 flat
modules group by accident; the docs site inherits it, and every section
re-groups by hand (Gallery by 10 ad-hoc domains, API by module, Guides flat,
Design flat). Modules are cheap to add and classes cheap to move, so a class
under the wrong task moves; the docs never re-file it.

jm already supports sub-packages: a module declared
`[module."synchronization.acquisition"]` lands in
`src/doppler/synchronization/acquisition/`, and `[module.X] package` places
a module's classes in any package (`_config.module_package_resolved`).

### 6.1 The tree, in C

The headers are the layout of record. Every directory below is an
**area**, optional **sub-areas**, then the **object** directory jm already
owns (its `<obj>_core.h`). Today's 145 entries under `native/inc/doppler/`
mix object directories, per-module directories (`acquire/acquire_core.h`),
internal cores (`resamp`, `hbdecim`, `rs`, `conv`), three CamelCase
directories and 20 loose headers. First stab:

```text
native/inc/doppler/
├── doppler.h, version.h.in        the umbrella header and version stay at the root
├── core/                          shared, no DSP of their own
│     dp_complex.h dp_simd.h dp_state.h dp_state_pyhelp.h dp_thread.h
│     dp_parallel.h dp_once.h dp_format.h dp_hash64.h dp_isotime.h
│     jm_perf.h jm_simd.h clib_common.h pyex_common.h
├── sources/
│   ├── oscillators/               nco/ lo/
│   └── waveforms/                 wfm/ wfm_synth/ wfm_compose/ pn/ gold/ frame/
├── impairment/                    adc/ awgn/ doppler_channel/
├── filtering/                     fir/ boxcar/ interp_table/ delay/
├── rateconversion/                resampler/ rate_converter/ farrow/ cic/
│                                  halfband_decimator/ hbdecim_q15/
│                                  (internal cores resamp/ and hbdecim/ nest
│                                  under the object that owns them)
├── downconversion/                ddc/ ddcr/
├── correlation/                   correlator/ (the one primitive: §6.4)
├── spectral/
│   ├── transforms/                fft/ fft2d/
│   ├── estimation/                psd/ spectrogram/ specan/ acc_trace/
│   └── measurement/               tonemeas/ nprmeas/ imdmeas/
├── synchronization/
│   ├── acquisition/               burst_capture/ syncword/
│   ├── feedforward/               ppe/ carrier_estimator/ (today carrier_acq/)
│   └── feedback/                  loop_filter/ costas/ dll/ symsync/
│                                  carrier_mpsk/ carrier_nda/ ratesync/ lockdet/
│                                  agc/ despreader/ burst_despreader/
├── receiver/
│   ├── narrowband/                mpsk_receiver/
│   └── spreadspectrum/
│       ├── burst/                 burst_demod/ dsss_burst_receiver/
│       └── continuous/            dsss_receiver/ async_dsss_receiver/
│                                  async_dsss_pool/
├── modulation/                    mpsk/ (M-PSK map and demap: hard, soft, differential)
├── coding/
│   ├── fec/                       conv_enc/ viterbi/ rs_codec/ (cores conv/, rs/ nest)
│   ├── crc/                       dp_crc16.h (error detection, the frame trailer)
│   ├── interleaving/              interleaver/ (dp_interleave.h)
│   └── ccsds/                     ccsds/ ccsds_tm/
├── estimation/                    ber_meter/ frame_meter/ snr/
│                                  (the detection statistics)
├── io/
│   ├── capture/                   wfm_reader/ wfm_writer/
│   ├── stream/                    stream/ dp_interrupt_guard/ (dp_interrupt*.h)
│   ├── timing/                    timing/
│   ├── buffer/                    f32_buffer/ f64_buffer/ i16_buffer/
│   └── telemetry/                 dp_tlm/ dp_tlm_capture/ dp_event_log/
└── numeric/
    ├── averaging/                 acc_f32/ acc_cf64/ acc_q15/ acc_q8/, ema_step,
    │                              ema_alpha_decim
    ├── conversion/                the 14 cvt objects: f32_to_i16/ … uq15_to_f32/
    ├── fixedpoint/                arith/ (q15_mac.h), saturate, square_clip
    └── functions/                 the math library (`math` is taken):
                                   sinc, bessel_i0, next_pow_two,
                                   gauss_hermite, simpson_weights,
                                   midpoint_nodes, mean_sinc, complement_power
                                   (util/ dissolves: see §6.3)
```

`pocketfft/` is vendored third-party code, so it leaves `inc/doppler/` for
`native/vendor/`.

**The mirrors.** Every other tree repeats the path exactly:
`native/{src,tests,benchmarks,examples,validation}/<area>/<sub-area>/<obj>/`.
jm generates `src`, `tests` and `benchmarks` there under
`layout = "mirrored"` (just-makeit #2423). doppler moves `examples` and
`validation` by hand and gates them.

**Python is the same path without the object.** jm nested modules make
`doppler.synchronization.acquisition`, holding `BurstCapture` and
`SyncFinder`; `doppler.synchronization.feedforward`, holding
`PolynomialPhaseEstimator` and `CarrierEstimator`; and `doppler.correlation`, holding
`Correlator`. A per-module header such as
`acquire/acquire_core.h` (the module's functions) moves into its package
directory: `synchronization/acquisition/acquisition_core.h`.

**Names.** Areas and sub-areas are the task words run together in lower case,
as in your examples (`spreadspectrum`, `rateconversion`). Object directories
keep their current names, except that the three CamelCase ones become
snake_case (`halfband_decimator/`, `rate_converter/`, `resampler/`).
`acq/`, `burst_acq/`, `corr/`, `corr2d/`, `detector/` and `detector2d/`
become one `correlator/` (§6.4).

### 6.2 Rules

- **Receivers use synchronizers; they are not synchronizers.**
    `doppler.receiver` composes `doppler.synchronization` (acquisition, then
    the feedback loops), so imports point one way only: receiver →
    synchronization → spectral / frontend, never back. Today `track` holds
    the loops and three receivers, and `dsss` holds the despreaders and the
    receivers built on them; both split along that line. A layering check
    can hold this, in Python imports and C `#include`s alike.
- **A handful of primitives, many use cases.** When an object computes what
    a primitive already computes, it composes the primitive and keeps only
    what is its own. The use case lives in the consumer and in the guide,
    never in a new class with a private copy of the search. `Correlator`
    (§6.4) is the first application: seven classes become one, and its
    consumers, from `BurstCapture` to `PolynomialPhaseEstimator`, compose it.
- **The package is the task.** Guides, gallery pages, examples and the API
    group by the tree, generated from it; the docs declare no second
    grouping.
- **A class whose task does not match its package moves.** It is never
    re-filed in the docs to look right.

### 6.3 What moves

Every class not already in a package of that name moves. The ones that
change task, not only path:

| today                                                                                                                  | lands in                                                  | why                                                                                                                                                              |
| ---------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `spectral.Corr`, `Corr2D`, `DelayDoppler`, `CorrDetector`, `CorrDetector2D`; `acquire.Acquisition`, `BurstAcquisition` | `correlation.Correlator`                                  | one primitive with knobs (§6.4)                                                                                                                                  |
| `track.MpskReceiver`, `MpskReceiverR`, `BpskReceiver`                                                                  | `receiver.narrowband`                                     | receivers use the loops; they are not loops                                                                                                                      |
| `dsss` receivers and `BurstDemod`                                                                                      | `receiver.spreadspectrum`                                 | the same split, by framing: burst or continuous                                                                                                                  |
| `dsss.BurstDespreader`                                                                                                 | `synchronization.feedback`                                | a Costas carrier loop and a DLL code loop, pulled in on the preamble                                                                                             |
| `dsss.Despreader`                                                                                                      | `synchronization.feedback`                                | a Costas carrier loop and a DLL code loop: it tracks, it does not receive                                                                                        |
| `acquire.CarrierAcquisition`                                                                                           | `synchronization.feedforward`, renamed `CarrierEstimator` | a one-shot residual-carrier refinement after the search (PSD, then a correlation against a template): an estimator, not acquisition; it composes `Correlator`    |
| `util` (the module)                                                                                                    | dissolves, one function at a time                         | "util" names no task; each function goes to the task that uses it (rows below)                                                                                   |
| `util.gauss_hermite`, `simpson_weights`, `midpoint_nodes`, `mean_sinc`, `complement_power`, `sinc`, `next_pow_two`     | `numeric.functions`                                       | the math library, named for what it holds since `math` would shadow the standard module; callers are the threshold models, filtering, acquisition and FFT sizing |
| `util.saturate`, `square_clip`                                                                                         | `numeric.fixedpoint`                                      | clipping to a range, beside the Q15 arithmetic; callers are cvt, agc, wfm                                                                                        |
| `util.ema_step`, `ema_alpha_decim`                                                                                     | `numeric.averaging`                                       | an EMA is a moving average with a forgetting factor; callers are agc, acc_trace, the async receiver                                                              |
| `util.fromto`                                                                                                          | `doppler.examples`                                        | a script's output path, not a library function                                                                                                                   |
| `dsss.PolynomialPhaseEstimator`                                                                                        | `synchronization.feedforward`                             | a feedforward estimator, used by the despreaders                                                                                                                 |
| `detection.LockDet`                                                                                                    | `synchronization.feedback`                                | a loop's lock indicator                                                                                                                                          |
| `agc.AGC`                                                                                                              | `synchronization.feedback`                                | the receiver estimates and tracks level, as it does carrier and timing                                                                                           |
| `detection.SyncFinder`                                                                                                 | `synchronization.acquisition`                             | finds a frame marker once                                                                                                                                        |
| `measure.ToneMeasure`, `NPRMeasure`, `IMDMeasure`                                                                      | `spectral.measurement`                                    | spectral measurements, built on the FFT and estimates                                                                                                            |
| `analyzer.Specan`                                                                                                      | `spectral.estimation`                                     | a spectrum analyzer; the `doppler-specan` CLI stays                                                                                                              |
| `accumulator.AccTrace`                                                                                                 | `spectral.estimation`                                     | the trace fold PSD and Spectrogram average with                                                                                                                  |
| `cvt.ADC`                                                                                                              | `impairment`                                              | a quantizing model of the hardware, not a format conversion                                                                                                      |
| `source.AWGN`                                                                                                          | `impairment`                                              | noise is an impairment added to a signal, not a source                                                                                                           |
| `snr`, `detection` statistics                                                                                          | `estimation`                                              | each estimates a quantity from data: an error rate, an SNR, a detection statistic                                                                                |
| `interp`, `delay`                                                                                                      | `filtering`                                               | one-class modules; both are filters                                                                                                                              |
| `interrupt`                                                                                                            | `io.stream`                                               | stopping a run is runtime                                                                                                                                        |
| `wfm_reader`, `wfm_writer`                                                                                             | `io.capture`                                              | reading and writing a capture is I/O, not a source                                                                                                               |
| `compose`                                                                                                              | retired                                                   | it repeats `wfm`'s Plan exports                                                                                                                                  |

### 6.4 Consolidation: one primitive, `Correlator`

**Seven public classes are one primitive with more or fewer knobs.** Today
`Corr`, `CorrDetector`, `Corr2D`, `DelayDoppler`, `CorrDetector2D`,
`Acquisition` and `BurstAcquisition` all correlate input against a
reference, over lag or over a 2-D grid, and some then threshold it. The 1-D
pair is the same shape one dimension down. Measured from the code on 2026-10-31:

- **`BurstAcquisition`'s C core is a 92-line forwarder** onto
    `Acquisition`'s engine: `burst_acq_core.c` only forwards, and the engine
    builds it with its own burst constructor, `dp_acq_create_burst`. Its
    Python surface is its own, though: `pd_burst`, `psl_db`, `reps`,
    `rate_hypotheses` and the `preamble` are burst-only, and `Acquisition`
    has about 20 members it lacks (`surface*`, `threads`, `set_telemetry`,
    `sf`, `spc`, …).
- **`DelayDoppler`** is `Corr2D` built in delay-Doppler mode: a slow-time FFT
    down each delay column, then the code correlation.
- **`CorrDetector2D`** is `Corr2D` plus streaming framing plus a CFAR threshold.
- **`Acquisition`** (3,253 lines) is streaming framing, **its own slow-time
    FFT tiles** (`acq_tile_block`, on the `tile_slow` plans in `acq_core.c`),
    a plain `Corr2D` and a CFAR. Its map is a second implementation of
    `DelayDoppler`'s column stage (`corr2d_col_tile` in `corr2d_core.c`),
    which `CLAUDE.md` forbids.
- **The CFAR is already shared.** `Acquisition` reuses `CorrDetector2D`'s ring
    and noise estimator, header-only.
- **Nothing outside the family uses `Corr2D`.** Its only C users are
    `Acquisition` and `CorrDetector2D`. The real consumers, `BurstCapture`,
    `DsssReceiver` and `AsyncDsssReceiver`, all go through `Acquisition`.

**The one object and its knobs.** One top-level C object, `correlation/correlator/`
(Python `doppler.correlation.Correlator`), holds one map implementation and
one CFAR. It is named for what it does for the caller: that it computes
spectrally underneath is an implementation detail, so it is not filed under
`spectral/`.

| knob        | values                                                            | replaces                                                            |
| ----------- | ----------------------------------------------------------------- | ------------------------------------------------------------------- |
| reference   | 1-D (lag), a code row (delay x Doppler), or a full 2-D template   | `Corr`, `CorrDetector` vs `DelayDoppler`, `Acquisition` vs `Corr2D` |
| output      | maps (every map, batch reductions, hypothesis bank) or detections | `Corr2D`, `DelayDoppler` vs `CorrDetector2D`, `Acquisition`         |
| input       | whole frames (`execute`) or a stream (`push`)                     | `Corr2D`, `DelayDoppler` vs `CorrDetector2D`, `Acquisition`         |
| framing     | continuous or burst (a stream's two front doors)                  | `Acquisition` vs `BurstAcquisition`                                 |
| hypotheses  | the carrier grid `L`, Doppler rates                               | `DelayDoppler`'s bank, `Acquisition`'s rate search                  |
| integration | dwell: coherent, non-coherent                                     | all seven                                                           |
| threshold   | k x noise, or set from Pfa / Pd                                   | `CorrDetector2D` vs `Acquisition`                                   |
| threads     | `nthreads`                                                        | all seven                                                           |

**The path there.** Each step is its own PR, held to the oracle it names, and
#2752 tracks them. Two rules govern every step (owner, 2026-11-02): when a
step lands, no private copy of what it consolidates survives on main, and
the consolidated search is at least as fast as the fastest path it replaces,
on every pinned benchmark row.

1. Folded into step 4 (owner, 2026-11-02). The forwarder holds no
    algorithm of its own: one-line calls into `dp_acq_*`, plus
    `burst_acq_wrap`, which passes a NULL engine through and snapshots the
    engine's `underpowered`. So it is not the second implementation this
    consolidation removes, and step 4 retires it with both classes.
1. `Acquisition` composes the delay-Doppler map on both of its paths, the
    narrow slow-time path and the wideband tile path, and both private
    copies are deleted. What made them fast moves into the one primitive,
    together with the per-carrier code-rate ramp the tiles need; #2732's
    shared slow-time transform is measured as part of the design. This is
    #2683 phase 4 (closes #2670), and the rates run on the bank's column
    stage. It cannot be bit-exact: acq transforms slow time before the row
    correlation, and the primitive after it. Oracle:
    - an equivalence test against today's surface and hits, written and
        passing before the switch. Its tolerance is derived from the data
        path, not fitted to a run, and stated once at the map's own
        normalisation: Corr2D's, whose row inverse carries `1/cb` and whose
        slow-time transform is unnormalised, so a row correlation is at
        most `‖x_i‖₂ · ‖r‖₂` and a cell at most `√D · ‖x‖₂ · ‖r‖₂`. Per cell,
        `|Δ| ≤ c · ε_f32 · (log2 cb + log2 lb + log2 dI) · √D · ‖x‖₂ · ‖r‖₂`.
        `ε_f32 = 2^-24`. The three lengths are the map's three transforms,
        the same on both paths: `cb` (`code_bins`) is the forward per period
        (Corr2D's `row_fwd`, acq's `wide_fwd`), `lb` (`lag_bins`) is the lag
        inverse (`row_inv`, `tile_inv`), and `dI = D · interp` is the
        slow-time transform (`slow_fft`, `tile_slow`). The shape helpers'
        `n`-length plans are off the map's path. x is the block's
        `D · cb` input samples, x_i its period i, r the one-period replica,
        and `c` a constant fixed from the FFT error analysis before any run.
        acq's wideband surface, which skips the `1/cb`, is divided by `cb`
        before the comparison. The sabotage is absolute, like the bound: it
        adds `10 × bound` to one cell of the primitive's map, and separately
        drops its `1/cb`; each must go red;
    - every acq and Corr2D C test, and every validator verdict unchanged:
        `native/validation/acq_*.c` (`acq_block_coherent`, `acq_emitter_floor`,
        `acq_peak_list`, `acq_rate_pd`, `acq_surface`, `acq_template_pd`)
        and `src/doppler/acquire/tests/validation/{acq,burst_acq,burst_capture}/validate.py`;
    - the speed gate, run with the two-commit A/B tool #2759 adds, a
        prerequisite: today's `make bench-interleaved` compares portable
        with native builds of ONE commit. Every pinned row equal or faster,
        per-row min-of-K, a tie judged within each side's own spread. The
        rows are `acq::op5M_*` and `acq::op2M_*` (eleven, e.g.
        `acq::op5M_U50k_D150_t1_nc1`), `async_dsss_pool::push[native,…]`
        and `push[full,…]` (five), and `corr2d::delay_doppler_*` and
        `corr2d::bank_*`.
1. `Corr2D`, `DelayDoppler` and `CorrDetector2D` become modes of the one
    object, and so do `Corr` and `CorrDetector` (the 1-D reference): one
    `correlator/`, and the other directories are retired. Oracle: each
    retired object's C tests, run against the mode that replaces it.
1. The Python surface settles. `Correlator` is the one class, and the
    knobs are its arguments, burst or continuous framing included. **No
    `Acquisition` or `BurstAcquisition` class survives** (owner,
    2026-10-31). The objects that compose one today (`BurstCapture`,
    `DsssReceiver`, `AsyncDsssReceiver`) compose `Correlator`, and
    the forwarder step 1 folded in here goes: `native/inc/doppler/burst_acq/`,
    `native/src/burst_acq/`, `native/src/acquire/acquire_ext_burst_acq.c`,
    `objects/burst_acq.toml`, and burst_acq's test and bench directories. The name
    lives on in prose: `Correlator`'s docstring and guide say it is "also
    called acquisition when it is part of a detector or receiver", so a
    search for the word lands on it. Oracle: the composing objects' tests
    and validators, and every retired class's examples ported, not
    dropped.

**Its consumers compose it; none keeps a private copy of the search.**

| consumer                                        | uses `Correlator` as                                                                                                                                          | keeps of its own                                                         |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| `BurstCapture`                                  | streaming code-row search, burst framing, detections                                                                                                          | period resolution, refine, retain, emit                                  |
| `DsssReceiver`, `AsyncDsssReceiver`             | streaming code-row search, continuous framing, detections                                                                                                     | the hand-off to the tracking loops                                       |
| `CarrierEstimator` (today `CarrierAcquisition`) | 1-D correlation of an averaged PSD against a template shape, with a Pfa/Pd threshold                                                                          | the PSD and the sub-bin carrier estimate                                 |
| `PolynomialPhaseEstimator`                      | a one-sample reference (nx = 1) over ny = N samples, with one dechirp `exp(-j pi r m^2)` per rate as the bank's `rates` weights: the rate x frequency surface | the parabolic refine in both axes, and the modulation-stripping guidance |

`PolynomialPhaseEstimator` folds only after two checks, which gate that step:
its estimates match today's within tolerance, with `Correlator` handing back
the per-rate surface rather than only its power maximum; and the nx = 1
shape is no slower than ppe's plain FFT per rate, measured with
#2759's two-commit A/B tool, because ppe is a one-shot estimator and overhead
dominates it.

This is a redesign of a certified object, so it follows
[Adding an algorithm](../dev/contributing/adding-algorithms.md) from
phase 1. Its own design page carries the knob table and the migration, and
the validation reports are re-certified on the one object.

### 6.5 Three more families, by the same rule

Surveyed on 2026-10-31, the same way as §6.4: each object's header, what
its state composes, and its calls into other objects. The sharpest claims
were then checked directly in the code. Each family comes down to a few
primitives. The findings are evidence; each fix is a proposal, not a
decision, until an issue takes it up.

**Rate conversion: resamp, halfband, cic, then RateConverter and DDC
composing them.**

| finding                                          | evidence                                                                                    | fix                                                                                                                               |
| ------------------------------------------------ | ------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| `Resampler` is `resamp`                          | a typedef (`Resampler_core.h:33`) plus 16 one-line forwards                                 | one object: the public name over the one core                                                                                     |
| `HalfbandDecimator` is `hbdecim`                 | a typedef (`HalfbandDecimator_core.h:31`) plus 9 forwards                                   | the same                                                                                                                          |
| two copies of the 19-tap halfband                | `RateConverter_core.c:56` and `ddcr_core.c:28`, the same values                             | one table in `hbdecim`; a C halfband designer, since today only Python designs it                                                 |
| `Ddcr` re-implements `DDC`'s half                | its LO and RateConverter execute, push and `narrow_pulse` copy `ddc_core.c:188-219`         | `Ddcr` = the real-to-complex halfband + a composed `DDC`. Its input is real, so it stays a separate type: composition, not a view |
| `hbdecim_r2c` clones `hbdecim`'s kernel          | the same ring and halfband skeleton (`hbdecim_core.c:17-75` vs `hbdecim_r2c_core.c:52-100`) | one halfband core with input = complex, real (fs/4 implied), or Q15 int16. Q15 keeps its AVX2 kernel                              |
| two Bessel-I0s and window loops                  | `resamp_core.c:16,62` vs `spectral/kaiser_window.c:8`                                       | one `bessel_i0` in `numeric.functions`; `resamp` calls the spectral Kaiser window                                                 |
| `ratesync` reaches into `RateConverter`'s stages | `ratesync_core.c:95-101` casts `stage_ptrs`                                                 | a `RateConverter` accessor                                                                                                        |

Not duplication: CIC, polyphase and halfband are different math, and
`RateConverter` rightly composes all three. Its matched-pulse bank is a
different prototype fed through `dp_resamp_create_custom`, which is the
right shape. Farrow (symsync's 4-tap interpolator), `interp_table`, `delay`
and `fir` are distinct.

**Feedback loops: loop_filter, lockdet and nco/lo are already the shared
engine; the carrier loops collapse to one.**

| finding                                                 | evidence                                                                                                                                                                                                                                    | fix                                                                                                                 |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `CarrierMpsk` is `Costas` with an M-PSK slicer          | the structs match field for field; Costas' discriminator is `mpsk_slice` at M = 2; `CarrierMpsk` has no consumers and lacks lockdet and telemetry                                                                                           | one decision-directed carrier loop, knob `m`                                                                        |
| `BurstDespreader` privately re-implements five things   | a `cexpf` oscillator (`burst_despreader_core.c:361`), the early/prompt/late correlator, the DLL discriminator (`:400`), the Costas discriminator (`:414`, identical to `costas_update`) and the lock metric; it composes only `loop_filter` | it composes `Costas` and `Dll`; it keeps its preamble switch, the atan2 preamble discriminator and burst statistics |
| `MpskReceiver`'s carrier loop re-assembles `CarrierNda` | `mpsk_rx_loops.h:558-595` calls `carrier_nda_disc`, the same lock EMA, `lockdet` and `loop_filter`                                                                                                                                          | `CarrierNda` gains knobs for an external symbol strobe and an external NCO port, and the receiver composes it       |
| `symsync` has a private phase conversion                | `(uint32_t)(4294967296.0 / s)` at `symsync_core.c:140`, the defect class `CLAUDE.md` records for nco/lo/dll                                                                                                                                 | call `nco_norm_freq_to_inc`                                                                                         |

The proposal is then one carrier loop overall: `CarrierLoop(discriminator = decision-directed | non-data-aided | coherent, m, bn, zeta, bn_fll, update = symbol | sample)`.
That would also settle §4.6's naming axis. Not duplication: `SymbolSync` (Farrow
on an NCO) vs `RateSync` (the polyphase cascade) already share their timing
detectors, and `AGC` is a first-order log-domain loop of a different kind.
`Dll` and `Despreader` already compose correctly. `DsssReceiver` builds
Costas and Dll itself rather than composing `Despreader`, which may be
legitimate (it needs segmented updates); that needs its own look.

**Sample formats: two primitives, quantize and dequantize.**

| finding                                                   | evidence                                                                                                                   | fix                                                                              |
| --------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| six float-to-int converters are one skeleton, hand-copied | scale, clip latch, clamp, round, pack, copied with their comments (`f32_to_i16_core.h:134` and five more); no shared macro | one `quantize`: width, signed or offset-binary, container, scale, clip reporting |
| `ADC` is already the superset of that math                | `bits` 1..64, full scale 2^(bits-1), TPDF dither, the same clip latch                                                      | the quantize primitive *is* ADC with a narrow output type; dither is a knob      |
| seven int-to-float converters are one line each           | `(float)UNPACK(x) * iscale`                                                                                                | one stateless `dequantize` function: input type, offset, scale                   |
| the library already treats them as functions              | `wfm_sink.c:79`, `wfm_reader_core.c:156` and `pocketfft.c:352` build throwaway state structs per sample                    | functions, not objects                                                           |
| two pairs nobody uses                                     | `F32ToI16U32/U64` and their inverses: examples and benchmarks only                                                         | fold or drop                                                                     |

Typed outputs still need per-type entry points (int8, int16, int32,
uint16...), generated from the one template rather than hand-copied.
`U8ToF32` (RTL-SDR cu8) may stay as a named alias, being offset 128 or
127.5 with scale 1/127.5.

**Four candidates do not wait for the consolidation:** `symsync`'s private
phase conversion, the duplicate halfband tables, the duplicate Bessel-I0,
and `BurstDespreader`'s discriminator against `costas_update`'s. If they
hold, they are defects whatever the tree decides. #2755 verifies each,
then fixes it in its own small PR.

### 6.6 Namespacing: `dp_` everywhere a C caller can collide

**Functions are clean; the headers are not.** Every exported symbol in
`build/libdoppler.a` and `libdoppler_stream.a` starts with `dp_` (1,772
symbols, 0 without it). But a C caller shares more than the linker's
namespace: types, struct tags, macros and enum constants are in the
preprocessor's and compiler's, and those collide too. Counted on
2026-10-31 over the public headers in `native/inc/doppler/` (excluding
`pocketfft/`) with grep:

| kind                              | without `dp_` / `DP_` | the largest families                                                           |
| --------------------------------- | --------------------- | ------------------------------------------------------------------------------ |
| typedef names                     | 103                   | `wfm_*` 43, `ccsds_*` 8, `acq_*` 6, `det_*` 4                                  |
| macros (excluding include guards) | 297                   | `<OBJ>_STATE_MAGIC`/`_VERSION` 132, `WFM_*` 28, `ASYNC_DSSS_*` 26, `MPSK_*` 12 |
| enum constants                    | 150                   | `WFM_*` 103, `RC_*` 6, `ASYNC_*` 5                                             |

Some are generic enough to collide with a caller's own code today:
`N_TYPES`, `N_STYPES`, `N_STAGE_KINDS`, `RC_MAX_STAGES`, and struct tags
such as `struct wfm_compose_state`.

**Where they come from.** None are generated: jm names the types it
generates from the object's C prefix (`dp_fir_state_t`), and calls an
author's `FIR_STATE_MAGIC` the author's own. The 132 serialization macros
follow a documented convention: `CLAUDE.md` and
[State Serialization](state-serialization.md) both prescribe
`<OBJ>_STATE_MAGIC`. The proposal is to fix the convention where it is
defined, as `DP_<OBJ>_STATE_MAGIC`, and to rename the rest.

**Not defects:**

- `dp_complex.h`'s `complex`, `I`, `creal`, `conj` and the like restore
    C99's own names on the Windows UCRT path, under `_WIN32`.
- `WIN32_LEAN_AND_MEAN`, `PY_SSIZE_T_CLEAN`, `M_PI`, `MAP_ANONYMOUS` and
    `SYS_memfd_create` are platform switches and fallbacks. They are listed
    by name, with a reason, rather than renamed.

**The proposed gate.** A header lint would fail on any public typedef,
struct or enum tag, enum constant or macro not starting with `dp_`/`DP_`,
or the include-guard pattern, with today's names on a shrink-only list that
the renames drain. Shipping the renames in the same breaking release as the
moves in §6.3 would let callers change their includes and names once.
Proposal; #2753 decides it with the tree.

## 7. See also

- [Repository Map](../dev/contributing/repository-map.md) — the vertical file/generation
    architecture this doc's hierarchy sits orthogonal to.
- [DSSS Primary Use Cases](../dev/contributing/dsss-use-cases.md) — the UC1/UC2 framing
    §4.1's rename is derived from.
- [Architecture](../architecture.md) — the 4-layer vertical stack.
