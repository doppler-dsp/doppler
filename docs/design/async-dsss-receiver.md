# AsyncDsssReceiver — the continuous DSSS receiver, from spec to object

*One page for continuous asynchronous DSSS, consolidated 2026-09-02 from
five: the receiver specification (`async-dsss-spec.md`, §1–§2), the
asynchronous despreader (`async-symbol-despreader.md`, §3) and its original
working design (`async-despreader-working-design.md`, §3.6), the continuous
use case that had grown inside [`burst-bank.md`](burst-bank.md) as its §11
(§5), and the searcher design written for it (`acq-multi-peak.md`, §6–§10,
§12–§13). §4 is the receiver as built; §11 is what it must gain for the
multi-emitter, always-searching use case. What the spec said about bursts
and about fleet service boundaries is not this receiver's concern and was
cut (git has it); `burst-bank.md` is bursts only.*

______________________________________________________________________

## 1. The specification

The waveform and the receiver requirements, as given:

- Level: Any
- Nominal frequency: 2.5 GHz
- Frequency uncertainty: +/- 50 kHz
- Frequency rate of change: < 500 Hz/s <sup>[(1)](#note-1)</sup>
- Waveform: Continuous DSSS BPSK
- Waveform exemplary use-case:
    - Code: CCSDS Command link Gold Code 1023 chips repeating
    - Chip rate: 3.069 Mcps
    - Modulation: Asynchronous Rectangular BPSK @ 2700 bps
- Es/N0 >= 5 dB <sup>[(2)](#note-2)</sup>

### 1.1 Target implementations

- Complete C receiver in `libdoppler.{a,so}`, to compile into C/C++
    applications <sup>[(3)](#note-3)</sup>.
- Complete Python receiver, the same object through the binding.

What the application wants from it (maintainer, 2026-09-02): **continuous**
reception — tracking loops that run for the life of a pass, not bounded
bursts — and **parallelism on one server, not a cluster**. The server has
many cores, and the design should use as many processes and threads as
the work needs: the searcher on its own, each assigned receiver on its
own, all fed from one stream on one machine (§5, §11). What is ruled out
is the fleet — pods, a scheduler, state hopping between nodes. The
receiver's `get_state`/`set_state` remain for a checkpoint and restart
mid-pass and for handing a receiver between processes on the same box,
not for scaling across machines. The fleet and per-burst service shapes
the original spec also described belong to the burst chain and are not
this page's concern.

### 1.2 Footnotes

<a id="note-1"></a>**(1)** Was mistyped "5 kHz/s" -- an order of
magnitude too high; matches the standard LEO worst-case nadir-pass
derivation `f_dot_max = (f_c/c)*(v^2/h)` at this spec's own 2.5 GHz and
a representative ~800 km altitude: ~579 Hz/s, i.e. this bound with a
small margin.

<a id="note-2"></a>**(2)** Raised from an earlier 3 dB floor: this
receiver's own characterization
(a full-characterization sweep, task #99) found a hard, SNR-only
pull-in cliff between 4 dB and 5 dB --
3 dB and 4 dB never lock (BER ~0.47-0.48, near-chance) while 5 dB locks
cleanly (BER ~0.01-0.02, matching theory), confirmed independent of
loop bandwidth (`bn_car` swept 0.005-0.02) and Doppler rate (swept
0-500 Hz/s) alike, so this is not a tunable margin, it's a real floor.
**Caveat, not yet resolved (task #99 remains open)**: this floor was
characterized against the real C `DsssReceiver`'s current pipeline --
Acquisition's coarse handoff feeding directly into the pre-despread
Costas/FLL loop, with no refinement stage in between. "Acquisition hit
quality" was flagged as the leading remaining candidate for the cliff
at the time, and this folder's own Python prototype work (see
`FINISHING_PLAN.md`'s `CarrierAcquisition` section) has since built and
validated exactly the missing piece -- a one-shot PSDMF refinement
stage between Acquisition's handoff and tracking -- that this floor was
set before having available. This 5 dB number may be revisitable
downward once that stage exists in C and task #99 is re-run against
it; treat it as the current best-known floor, not a settled intrinsic
limit, until that's actually tried.

<a id="note-3"></a>**(3)** Should internally compose Fine Carrier
Frequency Refinement (`CarrierAcquisition`) as a stage between
Acquisition's coarse handoff and Dll/Costas tracking -- motivated
directly by task #99's still-open 4-5dB pull-in cliff (see the Es/N0
footnote above). **Built:** that stage is §4's *refining* state, the C
port of the Python prototype that validated it.

### 1.3 Derived: tracking loop bandwidths (all loops: code DLL, Costas/CarrierMpsk carrier, FLL-assist)

- Design target: loop SNR `rho >= 20 dB` at the Es/N0 floor (5 dB), using
    the standard PLL loop-SNR relation `rho(dB) = Es/N0(dB) - 10*log10(2*bn)`
    (`bn` = the loop's own noise bandwidth, normalised to its update rate --
    `doppler.track.LoopFilter`'s own `bn` convention, so the update rate
    cancels out of the relation entirely; it depends only on Es/N0 and `bn`).
- Solving at the floor: `bn <= 10^((EsN0_dB - rho_dB)/10) / 2`
    `= 10^((5 - 20)/10) / 2 = 10^-1.5 / 2 ~= 0.01581`
- **Default rule: `bn <= 0.01` for every tracking loop** (kept as the
    already-shipped, already-validated value -- it sits comfortably
    inside the revised 5 dB floor's own `~0.01581` bound, so no
    implementation change is forced by the floor's move from 3 to 5 dB;
    the tighter 3 dB-derived `~0.00998` bound above is superseded). Sized
    against the worst-case Es/N0 floor, not a comfortable/typical
    operating point. (Applies to the code loop directly; the code loop's
    own per-epoch SNR is Es/N0 scaled by `1/epochs_per_symbol` -- at this
    waveform's `epochs_per_symbol = (chip_rate/sf)/data_rate = 3000/2700 = 10/9 ~= 1.11`, within ~0.5 dB of Es/N0 itself, so the same
    `bn<=0.01` bound applies without a separate derivation.)
- **Empirically, `bn` turned out not to be the deciding factor for the
    pull-in cliff itself** -- sweeping `bn_car` from 0.005 to 0.02 (both
    sides of the shipped 0.01) left the 4-5 dB cliff completely
    unchanged; the loop-SNR derivation above sizes STEADY-STATE tracking
    jitter once locked, not the separate (and still not fully explained)
    pull-in/lock-acquisition behavior below the floor. See task #99.

### 1.4 A second operating point

The C++ application's continuous waveform is the same shape at different
numbers — a 1023-chip Gold code at **2 to 5 Mcps**, a DDC from **13 MSa/s** to
twice the chip rate, `D = 1`, ±50 kHz to start and likely ±5 kHz after
Doppler pre-compensation, up to ten emitters on one code at once — and a
throughput floor of 30 MSa/s, comfortably. Those numbers, and what they do to
the search and the receiver pool, are worked in §6.1 and §6.4; §11 is the
receiver's side of them.

______________________________________________________________________

## 2. Acquisition

### 2.1 User-facing API

**Split at the user level into two classes, `Acquisition` (continuous)
and `BurstAcquisition`, sharing ONE C engine underneath.** Per direct
user redirect: rather than one class with a `mode` param and
per-parameter "ignored in this mode" caveats, each class exposes only
the parameters that are actually meaningful for it -- no dead
knobs, no mode-dependent documentation. Underneath, both are thin
front doors onto the SAME `acq_state_t` / `acq_core.c` (state struct,
`_auto_config`, `push()`, serialization all shared, single
implementation) -- two public constructor entry points calling one
internal builder with their own mode fixed, the same "secondary
constructor" idiom this project already uses elsewhere (e.g.
`dll_core.h`'s reconfigure/secondary-constructor pattern). Not yet
implemented -- reflects the `doppler_bins` naming settled this
session, not what's currently shipped in `acq_core.h`.

**Terminology: no "sub_bins".** Rolling the shared epoch FFT by `k`
bins produces another Doppler hypothesis exactly as much as a
slow-time FFT row does -- they're both just `doppler_bins`, full stop,
so there's only ever ONE public name, on both classes. Internally the
shared C struct keeps two distinctly-named fields for the two
*mechanisms* that produce them (each class only ever has one active):
`coherent_bins` (slow-time FFT depth, produced by coherent multi-epoch
integration -- `BurstAcquisition`'s axis) and `window_bins`
(roll-tiled frequency windows, each one a single-epoch coherent FFT
rolled to a different hypothesis -- `Acquisition`'s axis). Named for
mechanism, not regime, on purpose: "coherent"/"noncoherent" would
describe which class uses which, but the roll-tiled axis isn't
actually computed non-coherently (that word already means something
else here -- `n_noncoh`, a completely orthogonal axis: repeated
dwells accumulated for SNR at a FIXED hypothesis set, composing with
EITHER bin mechanism). Both `coherent_bins`/`window_bins` stay
implementation detail; the public property is `doppler_bins` on both
classes.

#### `Acquisition` (continuous)

`doppler_bins` here is the `window_bins` mechanism (roll-tiled): no
coherent multi-epoch combining is ever attempted, closing the aliasing
footgun (task #67: coherent combining is not a graceful-loss
trade-off under continuous async data, it's a structural mislock).
Sensitivity margin comes entirely from auto-selected `n_noncoh`.

| Parameter             | Type                                   | Default      | Description                                                                                                                                                   |
| --------------------- | -------------------------------------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `code`                | `NDArray[uint8]`                       | *(required)* | Binary (0/1) code, segment, or preamble chips to search for; sets `sf = len(code)`.                                                                           |
| `spc`                 | `int`                                  | `4`          | Samples per chip (>= 1).                                                                                                                                      |
| `chip_rate`           | `float`                                | `1e6`        | Chip rate in Hz (> 0).                                                                                                                                        |
| `symbol_rate`         | `float`                                | `1000.0`     | Continuous data-symbol rate in Hz (> 0).                                                                                                                      |
| `cn0_dbhz`            | `float`                                | `50.0`       | Carrier-to-noise density in dB-Hz (> 0) -- the sensitivity used to size the search.                                                                           |
| `doppler_uncertainty` | `float`                                | `0.0`        | One-sided Doppler search half-range in Hz; `0` = full native span (one `doppler_bin`). Tiles into `doppler_bins` windows whenever it exceeds one native span. |
| `pfa`                 | `float`                                | `1e-3`       | Target system (max-of-N) false-alarm probability, in `(0,1)`.                                                                                                 |
| `pd`                  | `float`                                | `0.9`        | Target detection probability, in `(0,1)`.                                                                                                                     |
| `noise_mode`          | `Literal["mean","median","min","max"]` | `"mean"`     | CFAR reference-cell aggregation mode.                                                                                                                         |

#### `BurstAcquisition`

The burst front door over the same engine — `doppler_bins` there is the
`coherent_bins` mechanism, auto-sized in `[1, reps]` for coherent gain over
an unmodulated preamble. It is not this receiver's concern; its parameters
and the burst chain are in
[`dsss-burst-receiver.md`](dsss-burst-receiver.md).

**Removed from today's shipped API**: `doppler_resolution`,
`doppler_rate`. Both existed to size a coherent-depth (`coherent_bins`)
ceiling/floor safely under continuous data modulation -- but task #67
already found that premise doesn't hold for this waveform: coherent
combining under continuous async data isn't a tunable trade-off, it's
a structural mislock (aliasing), regardless of how carefully
`doppler_resolution`/`doppler_rate` size it. `Acquisition` (continuous)
uses the `window_bins` mechanism unconditionally -- there is no
coherent-depth axis for either parameter to tune, so both become dead
weight and are dropped rather than kept as no-ops -- and since they
only ever applied to the continuous case, they simply don't exist on
that class at all now (no "ignored" caveat needed).

**Also removed: `max_noncoh`.** Per direct user redirect -- `n_noncoh`
should be auto-selected to meet `pd` at `pfa`, same as `doppler_bins`
already is, not capped by a separate caller-tuned knob (whose default
of `1` was also an `Acquisition` (continuous) footgun in its own
right: with only the `window_bins` mechanism active there, `n_noncoh`
is the ONLY sensitivity lever, so a cap defaulting to "don't use it"
would silently underpower it). `n_noncoh` becomes a purely derived
output (already exposed as a read-only property) on both classes.

This does NOT remove the need for an internal ceiling, though -- just
moves it out of the user-facing API. The semi-analytical `pd_predicted`
model itself is only reliable up to a point: this exact geometry's own
sweep (see below) found it "turns non-monotonic and unreliable past
`n_noncoh~256`" -- a MODELING breakdown, not a physical sensitivity
limit (more non-coherent looks always help in reality). Without a
caller-supplied cap, the auto-sizer's ascend loop still needs to stop
before wandering into that unreliable region and falsely reporting
`pd_predicted >= pd` -- that stopping bound should be an internal,
documented safety valve (or the ascend loop should detect it's
entering the model's known-unreliable regime and set `underpowered`
instead of trusting the number), not a parameter the caller has to
discover and tune correctly.

### 2.2 Output data structure: `DetectionEvent` (the acquisition handoff)

`DetectionEvent` is the DATA -- the acquisition handoff is the ACTION
(the process of converting a raw `push()` hit into this record and
handing it to the next block/service); the two aren't the same thing,
naming them separately on purpose.

The detection output has to be consumable by another thread or process
— the orchestrator of §5 and §11, a C++ application — not just another
Python object in the same interpreter, so it can't be the raw
grid-relative indices
(`doppler_bin`, `code_phase`) alone, since those are meaningless
without also shipping the emitting object's own config (`spc`,
`doppler_res_hz`, ...) alongside. Every field below is already
converted to a physical unit, so the record is self-contained: a flat,
pointer-free POD, safe to serialize (JSON, protobuf, whatever the
transport is) across a process boundary. This is Phase 0 of the
coupled-tracker roadmap (`~/.claude/plans/jiggly-munching-newell.md`)
made concrete -- matches `dsss_acq_handoff_from_result()`'s planned
C struct, and is exactly what the acquisition hand-off prototype's
`DetectionEvent`/`handoff_from_hit()` already
prototyped and validated in Python this session (code-phase
conversion exact, full search -> handoff -> refine -> track chain
locks).

One `DetectionEvent` record is emitted per detection event (i.e. once
per `push()` hit, on both classes -- same shape, since both share the
underlying engine):

| Field              | Type       | Description                                                                                                                                                                                                                                                                                                                                         |
| ------------------ | ---------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `timestamp_ns`     | `uint64_t` | UNIX time (ns) this detection's samples occurred, per the codebase's existing `dp_sample_clock_t` convention (`native/inc/timing/timing_core.h`): `epoch_real_ns + samples_consumed/fs`, NOT a fresh syscall timestamp at emit time -- reproducible, and already how `dp_header_t`/SigMF-metadata timestamps are derived elsewhere in this project. |
| `samples_consumed` | `uint64_t` | The raw sample offset (since this engine's own stream start) this detection's epoch ended at -- the `n` that `timestamp_ns` above was derived from. Kept alongside `timestamp_ns`, not instead of it: replay-safe (no wall-clock dependency) and lets a consumer re-derive/cross-check the time against its own clock anchor.                       |
| `chip_phase`       | `float`    | Code phase in CHIPS (not raw samples) -- the code-tracking seed for the next stage.                                                                                                                                                                                                                                                                 |
| `doppler_hz_est`   | `float`    | Coarse Doppler estimate in Hz, already folded/signed/scaled from the raw `doppler_bin` index.                                                                                                                                                                                                                                                       |
| `doppler_res_hz`   | `float`    | Width of that estimate -- the remaining uncertainty (±`doppler_res_hz`/2) a downstream refine/tracking stage still has to close.                                                                                                                                                                                                                    |
| `cn0_dbhz_est`     | `float`    | Estimated carrier-to-noise density (dB-Hz) -- informs downstream loop-bandwidth/dwell sizing (e.g. `fll_block_epochs`, still an open question in the plan).                                                                                                                                                                                         |
| `peak_mag`         | `float`    | Raw CFAR peak magnitude -- diagnostic/observability passthrough, not needed for tracking math.                                                                                                                                                                                                                                                      |
| `noise_est`        | `float`    | Raw CFAR noise-floor estimate -- diagnostic passthrough.                                                                                                                                                                                                                                                                                            |
| `test_stat`        | `float`    | Raw CFAR gating statistic -- diagnostic passthrough.                                                                                                                                                                                                                                                                                                |

**Shipped** (`cb1765cc`, task #71's timestamp-mechanism follow-on):
`acq_result_t` gained the `samples_consumed` field this section called
for, `dp_sample_clock_t` gained the `stamp_at(c, n)`/`track(...)`
pair, and the stream layer gained a one-shot `timestamp_ns` override so
a hop-to-hop send no longer clobbers the true origin timestamp with a
fresh syscall read. The acquisition hand-off prototype's
`handoff_from_hit()` already takes the `dp_sample_clock_t` analogue
(`doppler.wfm.SampleClock`) as its optional `clock` param and resolves
`timestamp_ns = clock.stamp_at(samples_consumed)` -- verified exact
end to end (`python acq_handoff.py`, `LOCKED`, timestamp checked
against the formula directly). What's still open is only the C side of
the handoff ACTION itself: `dsss_acq_handoff_from_result()` (Phase 0 of
the coupled-tracker roadmap) doesn't exist as a C struct/function yet
-- today's validated conversion lives only in Python. `Acquisition`/
`BurstAcquisition` themselves stay wall-clock/epoch-agnostic (pure
sample-domain engines, no I/O) -- the `dp_sample_clock_t` anchor comes
from whatever upstream source (DDC, a real front end) is actually
feeding them samples, and gets threaded through by the composing layer
(`DsssReceiver`, or the orchestrator), not owned by `Acquisition`
itself.

This gap wasn't specific to `Acquisition` -- `dp_tlm_rec_t`'s own doc
comment already flagged the general pattern ("if never stamped it
stays 0"), so the same unwired-timestamp check is worth running over
other streaming objects too, not just this one.

**Settles the plan's open question: no `carrier_freq` parameter on
either class.** The plan flagged "does `carrier_freq` need to become a
new `dsss_receiver_create()`/`acq_create()` parameter" -- answer: no.
`Acquisition` has no other reason to know the RF carrier frequency (it
operates in baseband/chip-rate Doppler Hz throughout); the
carrier-aiding scale (`doppler_hz_est * chip_rate/carrier_freq`) is
computed by whichever component actually knows `carrier_freq` --
the coupled-despreader prototype's tracker already
does exactly this with its own `carrier_freq_hz` parameter, not
something `Acquisition` hands it pre-scaled. Keeps `Acquisition`
carrier-frequency-agnostic and reusable for a baseband-only caller
that has no carrier concept at all.

### 2.3 Notes — the wideband search, and how it was settled

- FFT bin spacing per code epoch (native unambiguous Doppler span,
    ANY coherent depth D -- a D-point slow-time FFT sampled at the
    epoch rate has a fixed +/-(epoch_rate/2) Nyquist range regardless
    of D; more bins only subdivide that SAME fixed range more finely,
    never widen it): `chip_rate / sf = 3.069 Mcps / 1023 = 3.000 kHz`
    exactly (half-span `chip_rate/(2*sf) = 1.500 kHz`).
- **The big one**: required uncertainty is +/-50 kHz = 100 kHz total,
    ~33.3x the 3 kHz native span -> **34 non-overlapping native windows
    needed to cover it** (`ceil(100/3) = 34`). `Acquisition`'s own
    `doppler_uncertainty` parameter cannot help here -- it only NARROWS
    the search within one native span (`doppler_uncertainty <= span` is
    an enforced precondition), it can't widen coverage beyond it.
    Covering the full uncertainty therefore needs 34 independent
    alias-window searches (each its own 2D correlation, `code_bins = sf*spc = 2046` at `spc=2`) -- "34 x 2046" -- run either:
    1. **Sequentially** (34x acquisition latency -- almost certainly
        fails the "FAST" requirement), or
    1. **In parallel** (34x compute/hardware, full latency preserved), or
    1. **Behind a DDC channelizer bank** (`doppler.ddc`/`RateConverter`,
        already in the codebase -- reuse, don't reimplement): split the
        +/-50 kHz input into a SMALLER number of wider sub-channels
        first, so each channel's own residual uncertainty fits a cheaper
        per-channel search, trading DDC channelizer cost against fewer
        parallel/sequential full correlation engines.
- **CORRECTED architecture, per direct user redirect ("slow-time
    doesn't work for this case"): pure code-phase search, `D=1`, no
    coherent Doppler-axis combining at all.** The first baseline number
    below used `Acquisition`'s `symbol_rate`-aware auto-config, which
    picked `doppler_bins=31` -- exactly the coherent multi-epoch
    combining this whole story already proved unsafe under continuous
    async data (data-modulation aliasing across the Doppler-bin axis,
    `docs/design/dsss-acquisition.md`'s "ceiling (b) fails hard").
    **`D=1` sidesteps this entirely** -- with only one epoch, there is no
    multi-epoch axis for the data's own spectrum to alias across.
    Detection SNR margin instead comes from **non-coherent accumulation**
    (`n_noncoh`, magnitude-squared summing) across independent epochs --
    provably immune to data-modulation sign flips (already established
    earlier in this story), unlike coherent combining.
- Real measured `pd_predicted` sweep at `D=1`, this waveform's exact
    `cn0_dbhz=37.31`: crosses the 0.9 target around **`n_noncoh=96`**
    (0.917), comfortable margin at **`n_noncoh=128`** (0.965) or
    **`n_noncoh=192`** (0.994). (`pd_predicted` turns non-monotonic and
    unreliable past `n_noncoh~256` in this exact config -- a modeling
    edge case at very large `nc`, not a real detection cliff; stay
    stay well under it -- `n_noncoh<=192` is comfortably clear of it.)
- **Per-frequency-bin dwell time, measured throughput (33.2 MSa/s),
    `code_bins=2046` per epoch**: `n_noncoh=96` -> **5.9 ms**;
    `n_noncoh=128` -> **7.9 ms**; `n_noncoh=192` -> **11.8 ms** -- ALL
    faster than the (now-superseded) D=31 slow-time baseline's 15.3 ms,
    AND with none of its aliasing risk.
- **Architecture: 34 of these `D=1` pure-code-phase searches, one per
    3 kHz-spaced candidate frequency bin spanning +/-50 kHz, run in
    PARALLEL for "fast as possible"** -- total acquisition latency ≈ ONE
    bin's dwell time (**~6-12 ms** depending on the `n_noncoh` margin
    chosen), not 34x it. Sequential would cost 34x (`~200-400 ms`) and is
    ruled out by the "fast as possible" directive. Each of the 34 bins
    needs its own frequency-shifted (down-converted) copy of the input
    feeding an independent code-phase correlator -- realizing that bank
    of 34 parallel down-conversions efficiently (a DDC/mixer bank,
    `doppler.ddc`/`RateConverter`, reuse not reimplement) is the
    remaining open engineering question, not a way to reduce the count
    of 34 -- the count is fixed by `+/-50kHz / 3kHz` regardless.
- **Resolved: how to realize the 34-bin frequency grid per epoch**
    (`bench_freq_bank.py`, real `doppler.spectral.FFT`, real wall-clock
    timing, not just operation-count theory). Two candidates: (A) one
    forward FFT of the received epoch, then roll its spectrum by k bins
    per hypothesis (exact -- the 3 kHz hypothesis spacing IS this
    N=2046-sample epoch's own FFT bin spacing) against one fixed
    precomputed replica spectrum, 1 fwd + 34 inverse FFTs; vs. (B) a
    tuned mixer bank, 34 independent down-conversions each needing its
    own forward FFT, 34 fwd + 34 inverse FFTs. Both cross-checked
    bit-exact-cell correct first (identical injected true k/code-phase
    recovered by both). **(A) wins empirically, consistently, across
    repeated runs: ~0.6-0.8 ms/epoch vs. (B)'s ~0.9-1.0 ms/epoch, a
    1.2x-1.55x speedup** -- directionally confirms the op-count theory
    (35 vs. 68 FFT-equivalents, ~1.94x) but the *measured* margin is
    smaller, because (A) pays an extra O(N) `np.roll` memory copy per
    hypothesis that the theory didn't count, and Python-level per-call
    overhead partially masks the underlying FFT-count gap. **Settled
    architecture: (A), roll the replica/received spectrum, not a tuned
    mixer bank** -- a hybrid (C) was considered but has no analytical
    basis here (code-phase correlation needs the full N=2046-sample
    resolution regardless of how the frequency search is realized, so
    there's no reduced-rate sub-problem for a DDC front-end to help
    with).
- **Resolved: full C end-to-end benchmark of the wideband search**
    (`native/benchmarks/bench_acq_core.c`, task #71). The roll-FFT
    architecture is now wired directly into `dsss.Acquisition`'s real C
    core (`acq_core.c`'s wideband mode, task #72) -- the "34 bins run in
    parallel from one shared epoch FFT" requirement is satisfied
    internally by that one object's per-epoch loop (no separate 34-way
    thread fan-out needed, since all 34 hypotheses share one forward FFT
    per epoch by construction). Benchmarked one real, timed `acq_push()`
    call per non-coherent dwell (`n_noncoh` consecutive epochs, one call)
    at this exact waveform (`sf=1023`, `spc=2`, `chip_rate=3.069 Mcps`,
    `cn0_dbhz=37.31`, `doppler_uncertainty=+/-50kHz` -> `n_freq_bins=34`
    automatically), with a real injected burst + AWGN, confirming correct
    detection (right frequency window + code phase) at every point.
    **Measured**: `n_noncoh=74` (pd_predicted 0.984) -> 32.7 ms;
    `n_noncoh=101` (pd 0.999) -> 44.1 ms; `n_noncoh=123` (pd ~1.0) ->
    54.5 ms -- consistently **~0.44 ms/epoch**, i.e. faster than
    `bench_freq_bank.py`'s own Python/numpy roll-FFT prototype
    (0.6-0.8 ms/epoch), as expected for real C over numpy dispatch
    overhead.
    - **Discrepancy found and resolved in the process**: the `n_noncoh= 96/128/192` operating points quoted earlier in this doc came from a
        standalone Python sizing sketch written *before* this wideband mode
        existed in C, and don't match the real, now-implemented 34-bin
        Bonferroni-corrected auto-sizer -- the real model is considerably
        more optimistic at this cn0 (`pd_predicted` reaches ~0.999 by
        `n_noncoh~101`, not ~0.917 at 96 / ~0.994 at 192 as estimated
        there). The benchmark sweeps by **pd target** (0.9/0.99/0.999,
        letting the real auto-sizer pick `n_noncoh` honestly) rather than
        forcing the old sketch's exact nc values, since the real C model is
        now the authoritative source, not the earlier estimate. The
        `n_noncoh=96/128/192` numbers above are left as historical context
        for how this story arrived here, not as the operating spec.
    - **Still open**: confirm `n_noncoh` choice against the occasional
        epoch that straddles a data-bit transition (a graceful per-epoch
        SNR loss for SOME of the `nc` epochs, not a structural mislock,
        since non-coherent summing doesn't alias -- but not yet separately
        quantified here).

______________________________________________________________________

## 3. The asynchronous despreader

**Status:** draft / validated architecture
**Scope:** the receive-side despreader when the **data-symbol rate is on the
order of the code-epoch rate but asynchronous** to it. This is theory, the
failure mechanism, and a validated robust architecture that composes existing
`doppler.track` primitives. The reproducible study is
`src/doppler/examples/async_despreader_study.py`
(`python -m doppler.examples.async_despreader_study`).

______________________________________________________________________

### 3.1 The two-clock problem

A DSSS receiver despreads by integrating early/prompt/late correlations over one
**code epoch** (`TE = sf·sps` samples) — an integrate-and-dump locked to the
*code* clock. The data symbols are a separate stream; the despread prompt per
epoch carries the data.

That works when the symbol clock is locked to the code clock at an integer ratio
(GPS C/A: 20 code epochs per data bit, bit edges on epoch edges). It **breaks**
when the symbol clock is *independent*:

```
T_sym = TE · (1 + delta)        # symbol period, samples
                                # delta = symbol-vs-code rate offset
phi_sym                         # independent symbol phase
```

with `T_sym ≈ TE` (symbol ≈ one epoch). This is the hard regime: ~one symbol per
epoch, a transition roughly every epoch, and — crucially — `delta ≠ 0` makes the
symbol boundary **slide continuously** through the epoch at the beat rate
`delta / TE`.

______________________________________________________________________

### 3.2 Why per-epoch despreading fails

The coherent prompt over an epoch whose data flips at fraction `f ∈ [0,1]`:

```
P(f) = A·[ f·d1 + (1−f)·d2 ]  =  A·d1·(2f−1)        (d2 = −d1)
```

- `f → 0, 1` (flip at an epoch edge): `|P| = A` (full despread).
- `f → 0.5` (flip mid-epoch): **`|P| = 0`** — total coherent cancellation.

Because `delta ≠ 0`, `f` sweeps through every value, so ~half of all epochs
straddle a transition and their prompts collapse. The consequences:

1. **Data**: per-epoch decisions floor — the BER plateaus regardless of `Es/N0`
    (the straddle epochs carry no usable energy). Measured floor ≈ 1e-1 even when
    the bound is < 1e-5.
1. **Code**: the early/late discriminator `(|E|−|L|)/(|E|+|L|)` collapses to
    `0/0` on straddle epochs → the DLL is starved → the code loop wanders.

**Root cause:** at one prompt per epoch the symbol clock is **unobservable** (a
single sample per symbol cannot drive a timing loop), and the integration window
is forced to straddle transitions.

#### Diagnostic fingerprint

The straddle modulation is periodic at the symbol↔epoch beat. The spectrum of
the prompt-magnitude stream `|P[n]|` shows a **tone at `|delta|` cycles/epoch**
(centre panel of the figure). This is the signature to look for when a DSSS link
shows unexplained despread fades — it identifies this failure class directly.

______________________________________________________________________

### 3.3 Robust architecture

![Async despreader study](../assets/async_despreader_study.png)

The fix gives the symbol clock its own observability and its own matched filter,
and makes code tracking insensitive to data sign — composing primitives that
already exist.

#### 3.3.1 Data path — partial correlations + symbol matched filter + SymbolSync

1. **Partial correlations.** Split each code epoch into `K` sub-epoch partial
    prompt correlations (each `TE/K` samples, known code phase). This yields `K`
    despread samples per epoch ≈ `K` samples per symbol — the symbol clock is now
    **observable**.
1. **Symbol matched filter.** A length-`K` **boxcar** over the partial stream.
    This is a *sliding, symbol-aligned* coherent re-integration of the partials —
    the full-symbol despread the epoch-locked window could not form. It is
    essential: without it, the rectangular symbol pulse is sampled at one point
    and only ~1/`K` of the symbol energy is captured (the BER floors at ~2e-2).
1. **SymbolSync.** [`track.SymbolSync`](../api/python-track.md) (Gardner TED +
    Farrow interpolator) recovers the independent symbol clock (`delta`, `phi`)
    from the matched-filtered stream and decimates at the symbol-aligned peak.

**Result (left panel):** the BER follows the BPSK matched-filter bound within
~1–2 dB. A **genie** reference (coherent symbol-aligned despread with *known*
timing) hits the bound exactly — the loss was only window misalignment, never
SNR. The broken per-epoch path floors.

| Es/N0  | bound  | genie (known timing) | partial+MF+SymbolSync | broken epoch |
| ------ | ------ | -------------------- | --------------------- | ------------ |
| 6 dB   | 2.4e-3 | 2.5e-3               | 4.5e-3                | ~7e-2        |
| 8 dB   | 1.9e-4 | 1.5e-4               | 5.8e-4                | ~6e-2        |
| 9.6 dB | 9.7e-6 | 0                    | 0                     | ~5e-2        |

#### 3.3.2 Code path — non-coherent partial combining

The DLL keeps tracking through data flips by combining the partial correlations
**non-coherently**: `|E| = Σ_k |E_k|`, `|L| = Σ_k |L_k|`. A data flip changes a
partial's *sign*, not its *magnitude*, so only the one straddling segment
degrades (~`1/K`). This roughly **halves the discriminator variance** versus the
coherent-epoch form (right panel) — keeping the (already validated, smooth
sub-chip) code loop locked. It needs no symbol timing, so it works from cold
start; the bootstrap order stays sequential: DLL (non-coherent) → SymbolSync →
data.

#### 3.3.3 Choosing K

`K` trades observability and straddle-robustness against the non-coherent
squaring/Rician bias (which erodes the discriminator gain as `K` grows). The
study shows **`K = 4` as the sweet spot** for `T_sym ≈ TE` (best discriminator
SNR; `K = 8` loses more gain than variance). `K` must divide `TE`.

______________________________________________________________________

### 3.4 Scope: the despreader removes the code and outputs samples

The despreader's one job is to **remove the PN code and output samples**. The
asynchronous symbol clock is merely *why* it despreads in `K` partial
correlations (§3.3) — it is not a reason to recover symbols here. **Carrier
recovery and symbol extraction are downstream problems**, handled by separate
objects fed from the despreader's output:

```
              ┌──────────────── the despreader ───────────────┐
acq seed →    Dll(segments=K):  E/P/L correlate · partial dump · non-coherent
   (code phase)                 (|E|−|L|) code loop
              └───────────────── partial stream out ──────────┘
                         │  K oversampled async BPSK samples/symbol
                         │  (PN removed; residual carrier + data still on them)
                         ▼
   downstream:  Costas (carrier recovery)  →  SymbolSync (symbol timing) → bits
```

This is **`track.Dll(..., segments=K)`** — no new object. `segments=1` is the
classic coherent full-epoch DLL; `segments=K>1` is the streaming async
despreader. It composes downstream with `Costas` and `SymbolSync`, which already
exist (the data path of §3 is exactly that composition).

#### Why the carrier belongs downstream

The DLL's `|E|−|L|` discriminator is **non-coherent**, so code tracking is
**carrier-blind** — it locks with a residual carrier still on the samples. And
because each output is a *partial* (a `TE/K`-sample integrate-and-dump, not a
full epoch), a residual carrier barely dents it. For a ½-Doppler-bin residual
after acquisition the I&D loss is `sinc(Δφ/2)` with `Δφ = π/segments`:

| segments | window | Δφ at ½-bin residual | despread loss |
| -------- | ------ | -------------------- | ------------- |
| 1        | `TE`   | `π`                  | **−3.9 dB**   |
| 4        | `TE/4` | `π/4`                | **−0.2 dB**   |

So short partials make the despread carrier-tolerant: the small residual just
rides out on the output (a ring in the constellation; see the gallery demo), and
a downstream `Costas` loop removes it at full symbol SNR. Putting a carrier loop
*inside* the despreader would only matter for long coherent integration — which
partials deliberately avoid.

#### The same scope rule applies to the DSSS-MPSK composition

`Dll(segments=K) -> MpskReceiver` (`docs/gallery/dsss-receiver.md`) is
the other downstream composition, and the same rule bites the same way: the
despreader's partial-correlation output rate is whatever `K*chip_rate/SF`
comes out to — a sub-multiple of the chip rate, not chosen with
`MpskReceiver`'s `sps` in mind. An early version of that gallery page
violated its own §3.4 by picking `K` specifically so
`round(K*T_sym/T_epoch)` landed on an integer, coupling `Dll`'s own
tracking parameter to `MpskReceiver`'s sample-rate requirement. That made a
perfectly good `Dll` tuning look downstream-broken. The fix is
`doppler.resample.RateConverter` between the two — an explicit, arbitrary-
ratio resample stage, the same category of fix as `Costas`/`SymbolSync`
being separate objects from `Dll` here. Choose `segments` for the
despreader's own tracking quality; choose the demodulator's `sps` for its
own reasons; bridge the two with a resampler, never by coupling the
parameters directly.

### 3.5 Code-lock detection (always on)

A tracking channel must always answer one question: *am I locked?* The DLL
carries an **always-on** lock detector that reuses **acquisition's** non-coherent
test statistic, so acquire and track agree on what "detected" means.

**Statistic.** Each emitted look (a partial in `segments` mode, the full-epoch
prompt when `segments=1`) contributes its prompt power `|P_k|²`. The detector
sums `N = n_looks` consecutive looks and forms

```
R = sqrt( 2 · Σ_{k=1}^{N} |P_k|²  /  E|O|² )
```

which under H0 (noise only) has `P(R > η) = marcum_q(N, 0, η)` — exactly the
acquisition tail. So a caller sizes the threshold `η = det_threshold_noncoherent(pfa, N)` and the depth `N = det_n_noncoh(snr, …)` to
meet a target `(Pfa, Pd)`; `configure_lock(pfa, n_looks)` does the conversion
(default `pfa=1e-3`, `N=20`).

**The noise reference `E|O|²`.** Instead of a separate noise channel, the loop
correlates each look a second time at a **random off-peak code phase** — a whole
chip offset re-drawn every epoch and kept clear of the prompt/early/late lobe by
`noise_guard` chips. For a low-sidelobe code (Gold, long PN) that offset
correlation is signal-free, so `|O_k|²` is a sample of the per-look noise power.
Cycling the offset and averaging recovers the same noise estimate a bank of
fixed off-peak taps would, with O(1) state.

**Why an EMA, and why it must be long.** The reference is an EMA of `|O_k|²`
(`E|O|² += α(|O_k|² − E|O|²)`), which is adaptive (tracks a drifting noise floor)
and O(1) — matching the `Costas` lock-metric pattern. The subtlety, found by
Monte-Carlo: the *detection* integrates a fixed `N` looks (that sets the χ²(2N)
threshold), but the *noise estimate* must average **many more** cells than `N`,
or its own variance inflates Pfa. One offset cell per look (`L=N`) drives Pfa
~400× high; `1/α = max(1024, 32·N)` (`L_eff ≫ N`) holds Pfa at target with
`Pd ≈ 0.98`. So the integration depth and the noise-averaging length are
**decoupled**: `N` is the test, `1/α` is the reference. The reference uses a
**cumulative-mean bootstrap** — it is the running average until `1/α` looks have
accrued, then relaxes to the fixed-α EMA — so the noise floor is unbiased from
the first look instead of seed-dominated for the ~`1/α`-look warm-up (otherwise
Pfa runs ~10× high until the EMA settles, ~hundreds of epochs in). Verified
end-to-end: empirical Pfa ≈ `9e-4` against the `1e-3` target right from the
start of a noise stream.

**Readouts.** `Dll.locked` (bool, latched each `N`-look decision), `Dll.lock_stat`
(the last `R`), `Dll.noise_est` (`E|O|²`). The detector runs inside the normal
`steps()` — no separate method, no opt-in. The threshold conversion (the one
`detection`-module call) lives in the binding so `dll_core` links only `-lm`.

### 3.6 The look-back window — the original working design

*The note the C `Dll`'s dwell-integral look-back was built from
(`native/inc/dll/dll_core.h` cites it as its reference); kept verbatim, in
NumPy, as the algorithm's own statement.*

> **Important:** This assumes at most one data symbol transition per code epoch

```mermaid
flowchart LR
subgraph TED

end
LUT["LOCAL CODE \n INTERPOLATED LUT"]
TED --> LF
RX["RX CODE"] --> TED
LF["LOOP FILTER"] --> SCALE["SCALE BY \n EPOCHS / SAMPLE"]
SCALE --> SH["SAMPLE\nAND\nHOLD"]
SH --> NCO["U32 NCO\n MAX = SAMPLES / EPOCH"]
NCO --$$i + \mu$$--> LUT
LUT --E / P / L--> TED
```

- TED generates one error per epoch using the signal power formed by correlating
    the rx signal with local code replicas E, P, and L over a window _which maximizes power_
    of the prompt correlation and forms the error:

    ```text
    code_phase_error = 0.5 * (early_power - late_power) / signal_plus_noise_power
    ```

- This requires storing a buffer of the last received samples to "look back" in the case
    where a transition occurs in the current sample buffer so a transition free epoch may be
    obtained

- This is scaled down and repeated driving the NCO at 2x chip rate

- Local code is 2 samples per chip and linear interpolation is used to compute fractional samples

- LUT outputs early, prompt, and late codes offset by 1/2 chip (1 sample)

```text

# Init
code_size = 1023
samples_per_chip = 2
max_error = 0.5 # dB async correlation loss
phases = code_size * samples_per_chip
phase_resolution = 1 - 10 ** (-max_error / 10)
phase_step = int(np.ceil(phases * phase_resolution))
factors = [i for i in range(1, phases + 1) if phases % i == 0]
phase_step = factors[np.abs(np.array(factors) - phase_step).argmin()]
windows, window_size = int(phases / phase_step), phase_step
last_backard_sums = np.zeros(windows, np.complex128)
last_early_sums = np.zeros_like(last_backward_sums)
last_late_sums = np.zeros_like(last_backward_sums)

def find_max_power(x, windows, step_size, last_backward_sums):
    """Find max correlation over different output phase offsets."""

    # First compute the partial sums of the current correlation
    partial_sums = x.reshape(windows, step_size).sum(axis=1)

    # Now sum up the portions of the windows this epoch contributes
    sums = partial_sums.cumsum()
    backward_sums = partial_sums[::-1].cumsum()

    # Use the last epochs backward looking sums and the current
    # epochs forward looking sums to comput the overlapping correlation
    # at each phase across the two epochs and keep the maximum
    correlations = np.zeros(sums.size)
    correlations[-1] = np.abs(sums[-1] / (code_size * samples_per_chip))
    correlations[:-1] = (
        np.abs(sums[:-1] + last_backward_sums[::-1][1:])
        / (code_size * samples_per_chip)
    )
    max_window = correlations.argmax()
    max_abs = correlations[max_window]
    max_power = max_abs ** 2

    # Use partial sums as integrate and dump downsampled output
    integrate_and_dump = partial_sums / (step_size * max_abs)

    # Compute window index. This is the offset from the end of the last
    # correlation window that is the start of the max power correlation
    # window.
    window_index = (windows - 1 - max_window) * step_size

    return (
        max_power,
        max_window,
        backward_sums,
        integrate_and_dump,
        window_index
    )

def get_window(x_window, x, last_x, index):

    if index:
        x_window[:index] = last_x[-index:]
        x_window[index:] = x[:-index]
    else
        x_window = x[:]

    return x_window

# In your loop

while signal_buffer,more_data:

    # NCO + interpolated LUT
    early, prompt, late = pn_gen.steps(
        pn_control
    )

    b = signal_buffer.get()
    x = b * prompt
    power, window, last_backward_sums, integrate_and_dump,window_index = find_max_power(
        x, windows, window_size, last_backward_sums
    )
    signal_plus_noise_power = power

    b_win = get_window(b_window, b, last_b, window_index)
    last_b = b[:]
    early_win = get_window(early_window, early, last_early, window_index)
    last_early = early[:]
    late_win = get_window(late_window, late, last_late, window_index)
    last_late = late[:]

    early_power = np.mean(b_win * early_win) ** 2
    late_power = np.mean(b_win * late_win) ** 2
    code_phase_error = 0.5 * (early_power - late_power) / signal_plus_noise_power
    loop_filter.step(code_phase_error)
    pn_control = np.full(loop_filter.out / (code_size * samples_per_chip))

```

______________________________________________________________________

## 4. The receiver as built

`AsyncDsssReceiver` (`native/inc/async_dsss_receiver/async_dsss_receiver_core.h`)
is the composed continuous receiver, one C object, the production port of the
validated Python `search → refine → track` prototypes. It has three states,
read back through `get_refining()`/`get_tracking()`:

- **searching** — samples feed an embedded continuous `Acquisition` (§2,
    window-tiled over `doppler_uncertainty`, `D = 1`). A hit becomes a hand-off
    through `acq_build_handoff()`, which seeds the refine stage; the unconsumed
    tail of the same call is handed straight to it.
- **refining** — a frozen-carrier derotation at the coarse estimate feeds a
    collection `Dll` whose look-back segments oversample each epoch, then a
    `RateConverter` to `CarrierAcquisition`'s own rate, then
    `CarrierAcquisition` itself. When it reports ready or gives up, the live
    tracking chain is built **fresh** from the *original* hand-off chip phase
    and the refined (or, on give-up, unrefined) Doppler.
- **tracking** — the refined carrier is unfrozen into a live pre-despread
    Costas loop (`costas_update()` once per code period, driven by a
    non-data-aided squaring discriminator over the period's coherent partials)
    → `Dll` (§3, `segments = K`) → `RateConverter` → `MpskReceiver`. Two lock
    detectors run: the `Dll`'s own CFAR-based **code lock** (`get_code_locked()`,
    §3.5) and a hysteretic **symbol lock** on the emitted symbols
    (`get_locked()`, the `cos(2φ)` statistic over a 30-symbol dwell, declared
    after 30 consecutive symbols at or above 0.5 and dropped after 15 below 0.3).

`DsssReceiver` is the same object without the refining stage — a hit's coarse
Doppler goes straight to tracking — and §1.2's note (2) is why the refine
exists: the 4–5 dB pull-in cliff the coarse-only hand-off left. `reset()` on
either returns to searching: a receiver that has locked cannot be reset back
onto the same signal, only back to the hunt. Both are serializable
(`state_bytes`/`get_state`/`set_state`), every child included.

### 4.1 Status

- **Shipped — the despreader.** `Dll(..., segments=K)` (the §3.3 code+symbol path;
    `segments=1` = the classic coherent DLL). Validated **carrier-present**: code
    lock holds with a residual carrier on the samples, and the partial output is
    losslessly recoverable by a downstream carrier wipe + symbol despread
    (`test_dll.py::test_segments_carrier_present_*`). The streaming binding returns
    an independent array per call (block-size invariant).
- **Shipped — the inline symbol-loop primitive.** `symsync_step()` (the
    per-sample SymbolSync composition API); `symsync_steps()` is it in a loop.
- **Shipped — the always-on code-lock detector** (§3.5). `Dll.locked` /
    `lock_stat` / `noise_est`, tuned by `configure_lock(pfa, n_looks)`; reuses
    acquisition's non-coherent statistic with a random off-peak EMA noise
    reference. Validated signal-vs-noise in `test_dll.py` / `test_dll_core.c`.
- **Downstream, already available:** `Costas` (carrier recovery) and
    `SymbolSync` (Gardner + Farrow symbol timing). A receiver is the pipeline
    `Dll(segments) → Costas → SymbolSync`; the §3.3 study and the
    `async_despread_demo` gallery example show the composition.
- **End-to-end validated with a real acquisition front end.**
    `Dll(segments=K) → MpskReceiver` (`MpskReceiver` already fuses matched
    filter + NDA carrier acquisition + Gardner/Farrow timing + acq↔track
    handover into one object — its own docstring names this exact
    composition) is now proven at real physical parameters — a continuous
    1023-chip code at 3 Mchips/s, async 2100 sym/s BPSK data, with a genuine
    `Acquisition` search in front (see the
    [DsssReceiver](../gallery/dsss-receiver.md) gallery page,
    `src/doppler/examples/dsss_receiver_demo.py`).
    One correction to this section's own guidance surfaced doing so: `K=4`
    (§3.3.3's sweet spot) is tuned for the DLL's own code-discriminator
    variance, not for feeding a downstream matched filter — each partial is
    `K`-times weaker than a full coherent epoch, so a downstream receiver
    needs a much larger `K` (34, in the validated example) to reconstruct
    real coherent gain before its own carrier/timing loops can converge.
    The acquisition hand-off also needs two non-obvious unit conversions
    (`Dll`'s `init_chip` is phase-*inverted* relative to `Acquisition`'s
    `code_phase`; `MpskReceiver`'s `init_norm_freq` is cycles per its own
    *partial-rate* input, not per raw ADC sample) — see the example's
    docstring for the exact formulas.

#### Possible refinements

- **Symbol MF length.** A downstream length-`K` boxcar matched filter follows the
    BPSK bound within ~1–2 dB; matching it to the *tracked* symbol period closes
    the gap.
- **Closed-loop code-jitter asset.** Drive the non-coherent partial code loop
    under async data + code Doppler; confirm lock retention and the low-SNR
    threshold (`bn≈1e-5` held to 4 dB Es/N0; `bn≈0.002` lost lock at 6 dB).

______________________________________________________________________

## 5. The continuous case — the C++ application's waveform

*Written 2026-09-02 from the maintainer's description, in `burst-bank.md`
until that page was cut back to bursts; the numbers are derived from §1's
waveform and the measurements in `burst-bank.md` §10.4, and the questions at
the end are open or answered in the sections that follow.*

The C++ application does not receive bursts. It receives **continuous**
DSSS with asynchronous data — the CCSDS command-link shape
[`async-dsss-receiver.md`](async-dsss-receiver.md) already specifies (a 1023-chip
Gold code, 3.069 Mcps, ±50 kHz) — and the stream carries a **data-free
period of one code period just before each frame sequence**. Several
emitters are in the air at once on the **same** Gold code, and what tells
them apart is Doppler: each emitter's frequency difference *is* its
Doppler. There is **one frequency channel**: every emitter is in the same
band on the same code, and what distinguishes them is **code phase, power
and Doppler**. *(Maintainer, 2026-09-02: "one Gold code period; each
emitter frequency difference is Doppler"; "ONE frequency channel, emitters
are distinguished by code phase, power, and Doppler.")*

### 5.1 What the data-free window changes

Everything the burst family assumes about a preamble holds for that window
and for nothing else in the stream:

- **There is no coherent gain to buy.** The data-free window is one code
    period, so `reps = 1` and the coherent depth is one epoch — exactly the
    continuous `Acquisition` engine's search (`D = 1`, sensitivity from
    non-coherent looks, `dsss-acquisition.md`'s warning). The window buys
    one clean epoch without a data transition inside it, which the
    continuous engine already prices as a straddle loss and survives. The
    bank's reason to exist in this use case is therefore **not** gain —
    §5.3 says what it is.
- **The hand-off is to a tracking receiver, not to a frame demodulator.**
    A burst ends; a continuous signal is tracked from the seed onward
    (`carrier_acq → Dll + Costas`, the monolithic C receiver). So the
    channel's product is the `DetectionEvent` the async spec defines —
    Doppler, code epoch, C/N0 — and the window copy `BurstCapture` makes is
    not needed for the signal's sake. What may still be needed is the
    capture's **refine**: the frame begins where the data-free window ends,
    so *which* code period the window ended on is the frame epoch, and
    acquisition alone cannot say (§3.1 of the receiver design). Whether
    the tracking receiver's own frame sync makes that redundant is
    question 3 below.
- **The channel repeats.** A burst is acquired once; a continuous signal
    is re-acquired at every data-free window, and between windows it drifts
    (< 500 Hz/s in the spec). The claim rule across windows is then
    "same signal, next frame", not "same preamble".
- **Emitters come and go, at their own frequencies, and the bank is
    always on the air.** An emitter rises into the band at some Doppler,
    is acquired at its next data-free window, is handed to a tracker, keeps
    transmitting while others rise and set around it, and eventually
    leaves. The bank never stops searching: a channel that has handed one
    emitter off must go on watching its band for the next, and an emitter
    that drops out must be noticed and re-acquired when it returns. That is
    a **lifecycle** — searching → acquired → tracked → lost → searching —
    the burst family has no state for; a `BurstCapture` is done when the
    window is out. It is also a **duration** requirement: the process runs
    for hours or days, so nothing in the bank may grow with time
    (`samples_fed` is 64-bit; the per-push scratch reaches its high-water
    mark and stays; the rings are fixed) and a checkpoint is for a restart
    mid-pass, taken while everything is live.

### 5.2 The numbers, from the spec and `burst-bank.md` §10.4

- Native span `3.069e6 / (2·1023)` = **1.5 kHz**; channel spacing 3.0 kHz;
    covering ±50 kHz takes `2·ceil(50/3)+1` = **35 channels** — one bank,
    since there is one code.
- At `spc = 2` the source is 6.14 MSa/s; at `burst-bank.md` §10.4's 48 ns/sample a channel
    is **0.29× real time**, so the bank is **~10× real time** — eight cores
    at the measured 5.8× pool speedup do not keep up. Two things follow:
    the C++ application's own threads (`burst-bank.md` §10.1, the primary path) are not
    optional, and the per-channel cost is the number to attack first — 48
    ns/sample was measured for `DDC → BurstCapture`, and a channel that
    hands off a `DetectionEvent` rather than a window needs neither the
    capture's ring nor its refine.
- The continuous engine's own `window_bins` tiling covers ±50 kHz in
    **one** engine at the same `D = 1` — the same tiling this bank does
    with DDCs, at the same sensitivity. What the single engine cannot do is
    §5.3's first item, and that, not gain, is what the `K`-fold cost buys.

### 5.3 The async tools, and what the bank adds to them

The continuous chain exists and is the thing to compose, not to rebuild.
`AsyncDsssReceiver` is one object with a three-state machine — **searching**
(the continuous `Acquisition`, window-tiled over the uncertainty),
**refining** (`acq_build_handoff` → a frozen-carrier `Dll` →
`CarrierAcquisition`), **tracking** (Costas → `Dll` → `RateConverter` →
`MpskReceiver`) — and it is the validated C port of the search → refine →
track prototypes. `DsssReceiver` is the same without the refining stage.
Both cover the whole ±50 kHz in one engine at `D = 1`.

So the C++ application's channel is not `DDC → BurstCapture`. Against what
already exists, the bank adds exactly three things, and each is a design
decision rather than a given:

- **Resolution on the (Doppler × code phase) surface.** Every emitter
    is a peak on the same 2-D surface a channel already computes, at its
    own Doppler bin and code phase, with its own power. A Doppler bank
    partitions one axis of that surface: emitters more than a span apart
    land in different channels and are found independently, with
    independent CFAR references. But emitters *within* a span — the normal
    case, since there is one band and only Doppler separates them — share a
    surface, and a detector that takes the **maximum** of it reports one
    of them per dwell, the strongest, and masks the rest. So the channel's
    detector must report **every** peak above threshold in a dwell, each
    with an exclusion zone around it (a bin in Doppler, a chip in code
    phase) so one emitter is not reported as several — a multi-peak report
    the engine does not make today. Then **power**: a 1023-chip Gold code's
    cross-correlation floor is about −24 dB, so an emitter that much weaker
    than the strongest in the same surface sits under the strongest one's
    sidelobes and is found only by cancelling the strong one first
    (successive interference cancellation) — and two emitters at the same
    Doppler *and* code phase within a chip are one peak, distinguishable by
    nothing. Question 7 is therefore answered: emitters do share a span,
    and the bank's channel count buys parallel surfaces and independent
    references but not resolution; the resolution is the detector's, per
    surface, and it is the piece to design.
- **Many emitters, one band.** One `AsyncDsssReceiver` tracks one signal;
    its state machine has no "lost" state and no second emitter. The bank
    is what holds the pool: which emitters are up, which channel each is
    in, which tracker it went to, and when it stopped being heard. That is
    §5.3's question 5, and the tools do not answer it today.
- **The frame epoch.** The refining stage recovers carrier, not which code
    period the frame started on; if the application needs that from the
    bank, it is the capture's refine, transplanted.

Everything else — the DDC, the tiling rule, the tracker, the hand-off
record — is already there.

**Two rules from the maintainer (2026-09-02) fix the channel's shape:**

- **It always has to be searching.** A channel never stops acquiring: the
    emitter it just handed off keeps transmitting in its band while a
    second one rises beside it, and the first one's loss has to be noticed
    by something that is still looking. That rules out
    `AsyncDsssReceiver` as the channel — its state machine *replaces* the
    search with refining and then tracking, feeding every sample to the
    tracker. In the bank, search and track are **concurrent** per channel:
    the search engine runs on every block, and each hand-off spawns a
    consumer that is fed the same samples beside it. Two things follow. A
    channel that keeps searching re-detects the emitter it handed off at
    every data-free window, so something must recognise "that one is
    already handed off" — a suppression keyed by emitter (its Doppler and
    code phase), the analogue of the capture's `suppress_until` keyed by
    time — and that is the bank's, which settles the *minimum* of question
    5\. And the per-channel cost in §5.2 is the search alone; each tracked
    emitter adds a tracker's cost on top, on the application's threads.
- **The hand-off logic is selectable.** What a detection becomes is a
    policy, not a property of the channel: hand a `DetectionEvent` to a
    tracker (this use case), capture a window for a frame demodulator (the
    burst use case), or report and do nothing (surveillance). The channel
    owns the search and the event; the policy owns what happens next and
    is chosen per bank, possibly per channel. This answers question 1 —
    the channel is `DDC → search`, and `BurstCapture`'s ring and refine are
    one *policy's* apparatus, attached only when that policy is selected.

### 5.4 Questions this raises (open)

1. ~~**Hand-off target.**~~ **Answered:** selectable — a policy on the
    detection (track / capture a window / report), not a property of the
    channel. The channel is `DDC → search`, always searching.
1. ~~**One Gold code per signal.**~~ **Answered:** one Gold code, shared;
    emitters differ by Doppler. One bank; the multi-signal case is *within*
    it, across channels.
1. **The frame epoch.** Does the tracking chain's own frame sync recover
    where the frame starts, or does the channel owe the refined code epoch
    (the capture's refine, at `1023·spc`-sample periods)?
1. ~~**The data-free window's length.**~~ **Answered:** one code period,
    so `reps = 1` and no coherent gain. Still open: the frame cadence — how
    often an emitter can be (re)acquired and how far it drifts (< 500 Hz/s)
    in between.
1. **Who owns the lifecycle.** *Partly answered:* because the channel
    always searches, the bank must at least remember what it handed off
    (an emitter keyed by Doppler and code phase) or it re-hands-off the
    same emitter every window. The receiver's half — how "gone" is
    decided and what it releases — is designed in
    §10. Still open: whether the
    bank also owns the tracker pool and the assigned table, or reports
    "still there / gone" to an application that owns them.
1. ~~**How many emitters at once**, and how long an emitter is typically
    in view.~~ **Answered** (maintainer, 2026-09-02): at least one always
    on, up to 10 at once, each on for 5 to 15 minutes on average. The pool
    and the soak follow in §6.1,
    §5 and §6.
1. ~~**Can two emitters sit within one span of each other?**~~
    **Answered: yes** — one frequency channel, one code; emitters are
    separated by code phase, power and Doppler on one surface. A channel
    therefore needs a multi-peak report per dwell with exclusion zones, and
    the engine has none. Open in its place: the **power spread** between
    emitters that are up at once — inside the Gold code's ~−24 dB
    cross-correlation floor a multi-peak report suffices; beyond it the
    weak ones need the strong ones cancelled first, which is a different
    object.

______________________________________________________________________

## 6. The searcher — every emitter on one surface

*The searcher's design, written 2026-09-02 as its own page and folded in
here the same day. Nothing in §6–§10 and §12 is implemented, and nothing
has been measured; §12 is the work that would measure it. Follow
[adding an algorithm](../dev/contributing/adding-algorithms.md).*

### 6.1 What is settled, and what the page is for

The C++ application's waveform fixes the frame this page works in, and none
of it is re-derived here (§5):

- **One Gold code, one frequency channel.** Every emitter is on the same
    1023-chip code in the same band; what tells them apart is Doppler, code
    phase and power — three coordinates on **one** (Doppler × code phase)
    surface, the surface a channel already computes.
- **`reps = 1`.** The data-free window is one code period, so the search is
    the continuous engine's: coherent depth one, sensitivity from
    non-coherent looks, no coherent gain to buy.
- **The channel always searches.** It never hands its samples over to a
    tracker and stops; search and track are concurrent.
- **The hand-off is a policy** — track, capture a window, or report — chosen
    per bank, and not a property of the channel.
- **The population** (maintainer, 2026-09-02): **at least one emitter is
    always on**, there may be **up to 10 at once**, and each is on for **5
    to 15 minutes** on average. So the surface never has fewer than one
    peak, has up to ten, and an emitter rises or sets about once a minute
    at the full population — every data-free window of every emitter is a
    re-acquisition opportunity, and a rise between two of them is the
    normal event the searcher exists for. This answers `burst-bank.md`
    §11.4's question 6: the receiver pool is sized at ten plus release
    headroom (§10), and the soak's population is known (§12 step 7).
- **The rate** (maintainer, 2026-09-02): all of it — the front end, the
    searcher, every receiver, and the cancellation if it is built — must
    run **comfortably at 30 MSa/s or more**, and running at exactly 30
    MSa/s counts as slow. That is the machinery's floor; the waveform's
    own operating point is below it (13 MSa/s in, next table), and the
    page prices every option at both (§6.4), not as a benchmark to run
    at the end.

The numbers the page is worked at (maintainer, 2026-09-02) — these
supersede §5.2's, which were the async spec's waveform:

| quantity                       | value                                                                                                                                         | from                                                                   |
| ------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| chip rate                      | **2 to 5 Mcps** — design to the worst case, which is per quantity: 5 Mcps for anything priced per sample, 2 Mcps for anything priced per tile | given                                                                  |
| code                           | 1023 chips → one epoch is **204.6 µs** at 5 Mcps, **511.5 µs** at 2                                                                           | given                                                                  |
| coherent depth                 | **`D = 1`** — one epoch, no slow-time FFT                                                                                                     | given                                                                  |
| DDC input                      | **13 MSa/s**                                                                                                                                  | given — chosen to force the arbitrary-ratio path (§6.4)                |
| DDC output                     | **2× chip rate**: 10 MSa/s at 5 Mcps, 4 at 2 (`spc = 2`)                                                                                      | given; the ratios 1.3 and 3.25 both lack an integer factor             |
| samples per epoch              | 2046, at every rate                                                                                                                           | `1023 · spc`                                                           |
| Doppler tile                   | `1/T_epoch` = **4.89 kHz** at 5 Mcps, **1.96 kHz** at 2; a tile spans ± half that                                                             | at `D = 1` the Doppler axis is the `window_bins` tile index            |
| uncertainty                    | **±50 kHz to start**; Doppler pre-compensation will likely bring it to **±5 kHz**                                                             | given — design at the full width, and record what the narrow one saves |
| tiles over ±50 kHz             | **23** at 5 Mcps, **53** at 2                                                                                                                 | `2·ceil(U/tile)+1`; the searcher's worst case is the low rate          |
| tiles over ±5 kHz              | 3 at 5 Mcps, 7 at 2                                                                                                                           | same rule, after pre-compensation                                      |
| budget, one core, operating    | **77 ns per input sample**; per output sample **100 ns** at 5 Mcps, 250 at 2                                                                  | `1/13e6`, `1/10e6`, `1/4e6`                                            |
| budget, one core, at the floor | **33 ns per input sample**; 43 per output at 5 Mcps                                                                                           | `1/30e6`, same ratio                                                   |

The maintainer's description of the running system (2026-09-02) adds the
lifecycle the policy serves, and it is the shape everything below is fitted
to:

> The acquisition part continuously looks for signals, and async receivers
> track them as they are found, until they are gone. A receiver does not
> stop tracking once it has been assigned.

So there are two kinds of thing on the air side of the bank. A **searcher**
per channel (`DDC → search`), which runs on every block for the whole life
of the process. And a pool of **async receivers**, one per emitter, each
spawned by the track policy from one detection, fed the same samples as the
searcher, and living from that hand-off until *its own* loss decision — the
searcher never stops one, never re-seeds one, and never assigns a second
receiver to an emitter that already has one. The searcher's product is
therefore not "the strongest signal present"; it is **every emitter present
that is not yet assigned**, per dwell.

The receiver is the object that exists. `AsyncDsssReceiver`
(`native/inc/async_dsss_receiver/async_dsss_receiver_core.h`) is the
validated `search → refine → track` chain in one C object: its searching
stage feeds an embedded `Acquisition`, a hit is turned into a hand-off by
`acq_build_handoff()`, and that hand-off seeds the refine stage. What the
lifecycle needs from it is two things and no new receiver
(maintainer, 2026-09-02): **an acquisition input** — the searcher's
detection arrives from outside as the hand-off — and **an internal
acquisition bypass** for that mode, so the object starts in refining from
the given seed and its own `Acquisition` never runs. That is a difference
in constructor, not in method, so it is the `ddc`/`MatchedDDC` shape: a
second `create` over the same state, a view in the manifest, the chain
past the seed shared verbatim. The receiver already carries a symbol lock
detector (`lockdet`, hysteretic, on the emitted symbols), which is where
"until they are gone" is decided — what it lacks is the transition that
decision drives (§10).

### 6.2 What one maximum per dwell loses

Today's detector reports one cell. `det_result2d_t`
(`native/inc/detector2d/detector2d_core.h`) is one `(row, col, peak_mag,   noise_est, test_stat)`, and the acquisition engine's `acq_compute_stat`
(`native/src/acq/acq_core.c`) takes the same two maxima
[`dsss-acquisition.md`](dsss-acquisition.md) §9.1 describes — the
interpolated one to gate, the native one to report — and stops. The argmax
itself is a private loop in each of the two objects; only the CFAR
reference under it, `det_noise_estimate`, is shared through
`det_private.h`. There is no exclusion zone and no second peak, in either.

With `K` emitters up, the surface has `K` peaks, and a maximum reports the
strongest. The rest are not below threshold; they are simply not looked
at. In the burst use case that costs little — bursts are short and rarely
overlap in one channel. In the continuous case the strongest emitter is up
for hours, and every dwell for those hours reports it and nothing else, so
a second emitter rising beside it is **never** acquired while the first is
on the air. Nor does hand-off help: the assigned receiver goes on tracking
the first emitter, the searcher goes on re-detecting it at every data-free
window (the suppression-by-emitter §5.3 asks the bank
for), and after the suppression drops that re-detection the dwell has
reported nothing at all. **The single maximum is the gap, and it is the
searcher's, not the bank's** — the bank's channel count partitions Doppler
into spans, but emitters within one span share a surface, and that is the
normal case here.

### 6.3 What the power spread decides

Two emitters at different Dopplers or code phases are two peaks on the
surface, and a detector that reports every peak above threshold finds
both — provided the second *is* a peak above threshold. A strong emitter
does not only put one peak on the surface: a 1023-chip Gold code's
cross-correlation with itself at every other lag is not zero, and the
maintainer's figure for that floor is **about −24 dB** below the peak
(§5.3, not re-derived here). That floor
lies across the whole surface — every Doppler bin, every code phase — so an
emitter weaker than the strongest by more than the floor plus the
detection margin is under the strongest one's sidelobes: it is not a peak,
and no peak detector reports it.

Two things follow, and they are why the mechanism forks on the spread:

- **The CFAR reference is right to rise.** `det_noise_estimate` measures
    the surface's floor, and with a strong emitter present that floor *is*
    the strong emitter's sidelobes. The threshold moves up with it, which
    is what CFAR means — the weak emitter is genuinely below the floor of
    the surface as it stands.
- **Only removing the strong emitter lowers that floor.** A peak list
    cannot; that needs cancellation, and cancellation needs a replica of
    the strong emitter — which is a different object with a different
    information source (§7.2).

So the decision is the emitters' **power spread**, §5.4's
question 7, and it is open. Inside the floor a peak list suffices; beyond
it the weak emitters need the strong ones cancelled first. This page
covers both branches (§9), so that whichever way the number falls the page
already says what to build.

Two cautions about the number itself, for the work in §12. The −24 dB is
the three-valued bound for a full-period, zero-Doppler cross-correlation;
at a Doppler offset the correlation is partial-period and the bound does
not apply as stated. And it is a *maximum* over lags — the RMS floor of a
1023-chip code is nearer `1/√1023`, about −30 dB — so which of the two the
detector experiences is a measurement (§12 step 1), not a lookup.

### 6.4 The throughput floor

At the operating point one core has **100 ns per DDC-output sample** for
everything after the front end, and **77 ns per input sample** for the
front end itself; at the 30 MSa/s floor those are 43 and 33 ns.
"Comfortably" means a margin under that, and this page takes **half** as
the working target — the whole population inside 50 ns per output sample
per core at the operating point, 21 at the floor, across the cores the
application gives it — with the margin a number the benchmark reports,
not one it assumes. Equality with the budget is a failure by the
requirement's own words.

The decimation is only 1.3× at the top of the rate range, and that is
the fact that shapes the cost: **nothing runs at a fraction of the input
rate.** At 5 Mcps the searcher and every receiver run at 10 MSa/s,
three-quarters of what the front end sees, so the population's cost is
`(searcher + 12 receivers + 10 replicas)` per output sample, not that
divided by anything. The rate range splits the worst case in two. Every
receiver and every replica is priced per output sample, so their worst
case is **5 Mcps**. The searcher is priced per tile per output sample,
and tiles go up as the rate comes down — 23 at 5 Mcps, 53 at 2 — so its
tile-samples per second are nearly the same at both ends (230 M against
212 M over ±50 kHz) and its worst case is **the low rate, by a small
margin, at the full uncertainty**. Doppler pre-compensation to ±5 kHz
takes the searcher to 3 or 7 tiles, an eightfold cut in its cost and none
in anyone else's; the page designs at ±50 kHz and step 8 records both.

The one measured number is an order of magnitude, not a price.
`burst-bank.md` §10.4 put a `DDC → BurstCapture` channel at **47–51 ns
per source sample** — but on the 3.069 Mcps waveform, with its own
decimation, and with the front end's share of it unmeasured, so it does
not transfer to this rate as a figure. Taken as it stands it is 62% of
one core at 13 MSa/s and 1.4× real time at 30, for one channel and no
receiver; which is enough to say the chain is priced near the budget
before the population is on it. Three things follow for the shapes:

- **One front-end DDC, shared, on its slowest path — on purpose.** There
    is one frequency channel, so the only stage at the input rate is one
    conversion, 13 to 10 MSa/s. That ratio was chosen for the budget, not
    the radio (maintainer, 2026-09-02): the `DDC`'s `RateConverter`
    builds the cheapest cascade the ratio allows — CIC, halfband, then a
    polyphase resampler — and 1.3 has no integer factor, so no CIC or
    halfband stage exists and the whole conversion runs through the
    **polyphase arbitrary resampler**, the most expensive sample the front
    end can produce. The budget is therefore priced with the slow path
    baked in; a deployment whose rate happens to give an integer factor
    can only be cheaper, and a bench that ran at a convenient ratio would
    have measured the wrong front end. The receivers take chip-rate input
    already (`AsyncDsssReceiver` ingests at `chip_rate · spc`), so they
    share this one front end rather than each owning one.
- **The searcher is one window-tiled engine, not a DDC bank.** A bank of
    23 to 53 `DDC → search` channels at anything like 48 ns each is 14
    to 33× real time at the operating point on one core and fits on no
    node; the
    continuous engine's own `window_bins` tiling covers the uncertainty
    in one engine at the same `D = 1` sensitivity (`burst-bank.md`
    §11.2), and with the peak list inside it (§8 (a)) it lacks nothing
    the bank had for this use case. That is a change to what §11.2
    assumed, and the throughput floor is what forces it.
- **The receivers are the population's cost, and they parallelize; the
    cancellation does not.** Twelve receivers at 10 MSa/s on the
    application's threads scale across cores; the replicas on the strong
    branch are subtracted on the searcher's path, serially, ten of them
    per block — so (iii)'s coupling has a per-sample price on one
    thread, and it is the searcher's.

What is not known is every per-stage number at this rate: the front-end
DDC per input sample; the searcher per output sample with the list at
both ends of the rate range; one receiver per output sample; one replica
per output sample.
§12 step 8 measures them, and the bench that does it must count what it
acquired and tracked beside the rate — a throughput that was reached by
missing an emitter is not a throughput.

______________________________________________________________________

## 7. The two mechanisms

### 7.1 The peak list with exclusion zones

The list is the maximum, iterated:

```text
repeat up to max_peaks times
  take the maximum of the surface
  if it is below eta · noise_est: stop
  report it (at its native row where the surface is interpolated)
  exclude ±1 Doppler bin × ±1 chip around it
```

At `D = 1` the surface is the native one: the engine interpolates only
the slow-time axis, and there is none, so the interpolated-vs-native
split of `dsss-acquisition.md` §9.1 collapses and the gate and the report
read the same cells. The Doppler axis is the `window_bins` tile index,
one row per tile, `1/T_epoch` apart — 4.89 kHz at 5 Mcps, 1.96 at 2.

**Why one bin and one chip.** They are the widths of one emitter's main
lobe: an epoch's frequency response is the `sinc` of a one-epoch
rectangle, whose first nulls fall one tile (`1/T_epoch`) either side, and
the code's autocorrelation triangle reaches zero one chip either side of
its apex. Inside that zone the surface belongs to the emitter just reported —
its own shoulders would otherwise be the next "peak" — and outside it a
second emitter has its own maximum. The zone is therefore also the
detector's **resolution**: two emitters within one bin *and* one chip of
each other are one peak, distinguishable by nothing on this surface
(§5.3), and that is a property of the code and the dwell,
not of the detector. In surface units the zone is `±interp` rows (one
row at `D = 1`) and `±spc` columns — two, here — circular in code phase;
on the native report it is `±1`
and `±spc`.

**The threshold does not change.** `eta` is sized from `N = searched_bins · code_bins` cells (`dsss-acquisition.md` §9.1); it counts the noise's
chances over the *surface*, and a second reported peak is another draw
from the same cells against the same gate, so the per-dwell false-alarm
event — *any* reported peak is false — is bounded by the same union.
Exclusion zones remove a few cells from the count, in the safe direction
and negligibly. What does change is the floor under a strong emitter
(§6.3): the reference rises, so does `eta·noise_est`, and false peaks in
the strong emitter's sidelobes are what §12 step 4 measures.

**Fixed size.** `max_peaks` is configuration, the result is an array of
that many `(doppler_bin, code_phase, peak_mag, test_stat)` entries plus a
count, ordered by `test_stat`; nothing allocates per dwell and nothing
grows with time — the duration rule of §5.1. Today's
single-peak result is the same array at `max_peaks = 1`. The population
sizes it: on the branch where the searcher sees every emitter (§9) the
list must hold all ten plus the false peaks the gate admits, so
`max_peaks` is of order 16; on the branch where assigned emitters are
cancelled it holds only what rose since the last window, a few.

### 7.2 Cancellation

Cancellation subtracts a replica of a strong emitter so the surface
underneath it can be searched. The replica needs the emitter's code phase,
Doppler, amplitude and **carrier phase** — and, for any epoch that is not
that emitter's own data-free window, its **data**. That last item decides
the shape, because emitters' frames are not aligned: while emitter A is in
its data-free window, emitter B is carrying data, and B's contribution to
A's dwell is a data-modulated, straddle-lossed correlation whose sign flips
at a place the searcher does not know.

Where the replica's information comes from is therefore the design axis:

- **From the peak** (acquisition-side). The detection gives code phase and
    Doppler to within a cell; amplitude and phase must be estimated from
    the complex peak; the data is unknown. Exact only in the strong
    emitter's own data-free epoch — which is not, in general, the epoch
    being searched.
- **From the assigned receiver** (decision-directed). The receiver already
    tracking the strong emitter knows its chips, its carrier, its
    amplitude, and its decided bits, block by block, and refines all of
    them continuously. Its replica is exact to the tracker's own error,
    data included.

And where the subtraction happens is the second axis: on the **surface**
(subtract the emitter's known response, the code's autocorrelation across
lag times a `sinc` across Doppler, scaled by the complex peak — the radio
astronomer's CLEAN) or on the **samples** (regenerate the chip stream,
subtract, correlate again).

______________________________________________________________________

## 8. The shapes — where each piece lives

The peak list has one place it belongs and two it could be put:

|                                                 | mechanism                                                                                                                                                                                | fits                                                                                                                                                   | cost                                                                                                                                                   |
| ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **(a) one primitive under both detectors**      | a peak-list function beside `det_noise_estimate` in `det_private.h`: `(mag, ny, nx, gate, excl_rows, excl_cols, out[], max_peaks) → count`; both callers use it at `max_peaks = 1` today | one argmax instead of the two private copies; `CorrDetector2D` gains the list for free; the interpolated/native split stays where it is, in the caller | both result structs become an array plus a count, and every consumer of `acq_result_t` sees `n_peaks`                                                  |
| **(b) inside `acq_compute_stat` only**          | the engine's loop iterates with exclusion; `detector2d` stays single-peak                                                                                                                | the engine alone changes                                                                                                                               | a third private copy of the pick, and the two detectors' behaviours diverge on the same surface                                                        |
| **(c) a second pass over the surface, outside** | the bank asks the engine for its surface and picks peaks itself                                                                                                                          | no engine change                                                                                                                                       | the surface is the engine's scratch, not a product — exporting it is a copy of `ny·nx·interp` floats per dwell, and the gate's `eta` leaves the engine |

(a) is the repository's rule applied — fix it where the primitive is
defined, once — and the only one under which the burst detector and the
acquisition engine keep agreeing.

Cancellation is a separate object, and its shape follows its information
source:

|                                                             | mechanism                                                                                                                                                                                                                                       | fits                                                                                                                                                                                                                      | cost                                                                                                                                                                                                                                                 |
| ----------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **(i) surface CLEAN, from the peak**                        | subtract `A·acf(τ − τ_i)·sinc(f − f_i)` from the *complex* surface for each strong peak, then re-pick                                                                                                                                           | no second correlation; stays inside the engine                                                                                                                                                                            | needs the complex surface where the engine keeps `\|·\|`; the response is exact only in the strong emitter's own data-free epoch, and a data transition inside the dwell leaves a residual the model does not have                                   |
| **(ii) sample SIC, from the peak**                          | regenerate the strong emitter from its detection, subtract from the epoch, correlate again                                                                                                                                                      | one object, no dependency on the tracker pool                                                                                                                                                                             | one extra correlation per cancelled emitter per dwell; the same unknown-data residual as (i); amplitude and phase from a single cell's estimate                                                                                                      |
| **(iii) sample cancellation fed by the assigned receivers** | `DDC → cancel(assigned) → search`: each assigned receiver publishes its replica for the block (or the estimates that make one: code phase, Doppler, amplitude, phase, decided chips); the searcher subtracts every replica before it correlates | the only replica that is right through data; makes the searcher see **exactly what is not assigned**, which retires the suppress-by-emitter table (§5.3) — an assigned emitter is not re-detected because it is not there | couples the searcher to the receiver pool on the push path; a receiver that has lost lock publishes a wrong replica, so the subtraction must be lock-gated; one replica per assigned emitter per block; and the *receivers* still see the raw stream |

(iii) is the shape the lifecycle already asks for. The receivers own the
emitters and keep tracking them regardless of what the searcher does; the
searcher wants to see only what they do not own; and only they know the
data — `AsyncDsssReceiver`'s track stage holds exactly the replica's
ingredients per block: the live carrier loop's phase and frequency, the
`Dll`'s code phase, the despreader's amplitude, and the decided symbols.
It is also the option that makes the two branches of §9 one mechanism at
two settings. Its cost is a real coupling — whoever holds the receiver
pool must also stand on the searcher's push path — which is why
§5.4's question 5 (who owns the lifecycle) becomes
load-bearing the moment the strong branch is chosen, and not before.

A refinement (iii) opens but this page does not take: a receiver can be
fed the stream with every *other* assigned emitter cancelled, which lowers
its own floor as well. That is the receivers' concern, on their own path,
and it changes nothing about the searcher.

______________________________________________________________________

## 9. The two branches

Both branches share the peak list (a) and the assigned-emitter table the
bank keeps in any case: which emitters have a receiver, at what Doppler
and code phase **now** (the receiver's estimate, since an emitter drifts at
up to 500 Hz/s between windows, not the detection's). The branches differ
in what the searcher is allowed to see.

**Spread inside the floor — the list is enough.** The searcher sees every
emitter, assigned or not, and reports every peak above `eta`. The bank
drops any peak within one exclusion zone of an assigned emitter's current
estimate and hands the rest to the policy. An assigned receiver is never
touched by a re-detection of its own emitter. What has to hold: no
unassigned emitter above the floor is missed while a stronger one is up
(§12 steps 2–3), and the re-detection of an assigned emitter never becomes
a second receiver (§12 step 7).

**Spread beyond the floor — cancel, then list.** The searcher's input has
every lock-gated assigned replica subtracted (iii), and then runs the same
list. The assigned table does the same job as before, now only as a guard
against the residual: a cancelled emitter that is imperfectly cancelled
leaves a peak at its own coordinates, and the zone around the receiver's
estimate is what keeps that residual from becoming a detection. What has
to hold: the residual after cancellation sits below the unassigned
emitters the application needs to find (§12 step 5).

The branch is chosen by one number — the application's operating spread
against the knee §12 step 3 measures — and the second branch strictly
contains the first, so building the list first is right either way.

______________________________________________________________________

## 10. The release — the lock detector decides "gone"

"Until they are gone" is a decision the receiver makes about itself, and
the pieces of it exist. `AsyncDsssReceiver` carries two de-chattered lock
flags, each a `lockdet` — level hysteresis between a declare and a drop
threshold, time hysteresis of consecutive looks either way, a NaN look
counted as a miss (`native/inc/lockdet/lockdet_core.h`):

- **Code lock**, `get_code_locked()`: the live `Dll`'s own CFAR-based,
    verify-counted detector — "am I despreading". This is the fundamental
    DSSS lock: an emitter that leaves takes its code with it, and the
    correlation at the tracked code phase and Doppler falls to the floor.
- **Symbol lock**, `get_locked()`: the BPSK statistic `cos(2φ)` over the
    emitted symbols, SNR-weighted over a 30-symbol dwell, declared after
    30 consecutive symbols at or above 0.5 and dropped after 15 consecutive
    below 0.3 (`ASYNC_DSSS_RX_LOCK_*`). This is the health of the *carrier*
    leg: a cycle slip or a deep fade drops it while the code is still
    being despread.

What is missing is the transition. Today a receiver whose flags fall keeps
running its loops on noise, and the only exit is `reset()`, which returns
to *searching* — a state the hand-off mode of §6.1 does not have.

**The rule.** An emitter is gone when **code lock drops and stays
dropped** for a confirm interval; symbol lock alone is a degrade, not a
release. Code lock is the right flag because it is the one that measures
presence rather than quality: a receiver can lose the carrier and re-lock
it on the same emitter (the loops are designed to pull in), but it cannot
re-lock a code that is no longer on the air. Symbol lock stays on the
record as health — a long stretch of code-locked but symbol-unlocked
tracking is a receiver reporting that it is holding an emitter it cannot
decode, which the application may want to know and this page does not
decide.

**The transition.** Hand-off mode adds a fourth state, **lost**, beside
searching / refining / tracking, and the receiver enters it on the rule
above. In it the loops stop updating, the replica (§8 (iii)) is no longer
published — the lock gate that (iii) already needs is the same flag, so
publication stops at the *drop*, before the confirm interval has run —
and the receiver reports lost to whoever holds the pool. The holder then
**releases the assignment**: the emitter leaves the assigned table, so the
searcher may report those coordinates again, and the receiver is reset to
the hand-off mode's idle — *waiting for a seed*, not searching — for the
pool to reuse. Nothing else moves: the searcher was never told to stop
looking there and the other receivers are untouched. The one
re-assignment the lifecycle permits is this one: an emitter released while
in fact still present is re-detected at its next data-free window and
seeded into a fresh receiver, which is a recovery, not a hand-back.

**What the interval costs, and what it buys.** Against on-times of 5 to
15 minutes, release latency is nothing: the symbol detector's drop is 15
symbols — milliseconds at any data rate in the thousands of symbols per
second — and any confirm interval under a second is well under 1% of the
shortest on-time. The number that matters
is the other one, the **false release**. A receiver that releases an
emitter still on the air loses that emitter's data until the next
data-free window plus a refine (the cadence of §5.4
question 4), and on the cancellation branch its replica leaves the
searcher's input for the same interval, so the floor rises under every
weaker emitter for a frame. The confirm interval is therefore sized from
a false-release budget — far rarer than once per on-time, per receiver —
in exactly the vocabulary `lockdet` documents: at the per-look miss
probability the tracked C/N0 gives, `n_down` consecutive misses set the
false-drop rate, and `det_verify_count()` sizes `n_down` against the
budget. Both the miss probability and the resulting interval are
measurements (§12 step 6).

**The pool.** Ten emitters at once plus the receivers still inside a
confirm interval on emitters that have just left: at one departure a
minute and a confirm interval of a second, the headroom is one. A pool of
about twelve hand-off-mode receivers, each a tracker chain on the
application's threads beside the searcher's own cost (`burst-bank.md`
§11.2), is the whole population.

**The read-back.** Whoever holds the pool needs to know, for each
receiver, which signal it is tracking, for how long, and in what
condition (maintainer, 2026-09-02). The facts have two owners, and the
split falls out of who produced each one:

- **The orchestrator owns the assignment.** It handed the seed to the
    receiver, so it holds the `DetectionEvent` verbatim — `timestamp_ns`,
    `samples_consumed`, `chip_phase`, `doppler_hz_est`, `cn0_dbhz_est` —
    beside the receiver it went to. It fed every sample since, so it holds
    the sample count at assignment and the count now; duration is their
    difference over the rate, the repository's `dp_sample_clock_t`
    arithmetic, replay-safe. And it recorded the state changes it was
    told about — refining to tracking, tracking to lost — with the sample
    count at each. Nothing here needs the receiver to remember its own
    history, which keeps the receiver thin: it tracks; the orchestrator
    keeps the books. This is the assigned table of §9 with three more
    columns, and it is what question 5's holder holds.
- **The receiver owns its condition.** Only it knows where the emitter
    is now — the live carrier loop's Doppler, the `Dll`'s code phase, the
    C/N0 the despreader sees (the drift since the seed is that against
    the orchestrator's row) — and its health: the state it is in, both
    lock flags, the symbol-lock metric against its declare threshold, the
    residual carrier errors the header already exposes, and, in lost, the
    samples since the code flag dropped. Today that is a scatter of
    getters — `get_locked`, `get_code_locked`, `get_lock_metric`,
    `get_car_nco_freq`, and the rest — each a separate call, so a reader
    that wants one consistent picture across a `push` on another thread
    cannot get one. The shape that fits is **one status record, returned
    by value** — the `measure` objects' `single` record (`ToneMetrics`), a
    jm-generated structseq over a C struct — read on demand and never
    pushed.

The orchestrator's *now* columns are refreshed from the receiver's record
at whatever cadence it reads, and the exclusion zone of §9 is keyed on
those, not on the seed. So one read per receiver per window is the
minimum, and the table is the join of the two owners' facts.

The record is a read of live state, distinct from `get_state()`: the
bytes triplet is for resuming the receiver elsewhere, the record is for
describing it here, and the two must not be confused — a record that
tried to be both would be a serialized blob a human cannot read. On the
cancellation branch the replica output is a third thing again, per block
and on the push path, and rides neither.

______________________________________________________________________

## 11. What the multi-emitter use case needs from the tracking receiver

§6–§10 fix the lifecycle: a **searcher**
per channel that never stops, and one `AsyncDsssReceiver` per emitter,
**assigned once** from a detection and tracking until *its own* loss decision
— never stopped, never re-seeded, never doubled up by the searcher. Up to ten
emitters at once, each on the air for 5 to 15 minutes, on one Gold code, on a
stream the whole pool must consume at 30 MSa/s comfortably. The receiver of §4
is the right object for that (maintainer, 2026-09-02) and needs five things,
none of which is a new receiver. Each is a Phase-1 design here and an
implementation item in
[adding an algorithm](../dev/contributing/adding-algorithms.md)'s order; the
measurements that size them are §12.

### 11.1 The hand-off mode: an acquisition input, and an internal bypass

Today the only way in is the receiver's own search. In the pool the search is
the searcher's, so the receiver needs to **take a detection from outside** —
the `DetectionEvent` of §2.2, exactly as its own `acq_build_handoff()` would
have produced it — and to **skip its own acquisition entirely** in that mode:
no embedded `Acquisition` is built (a 23-to-53-tile engine per receiver, twelve
times over, is memory and work nothing uses), the searching branch of `push()`
is unreachable, and the object starts in *refining* from the given seed.

That is a difference in **constructor**, not in method, so it is the
`ddc`/`MatchedDDC` shape: a second `create` over the same state, declared as a
`[[async_dsss_receiver.views]]` entry in the manifest, the chain past the seed
shared verbatim. Two consequences follow:

- **`seed(event)` is a method on the base type**, not the view's alone. The
    base receiver's own hit already takes this path internally — a hit is a
    seed the object made for itself — so exposing it is honest on both
    flavors, and a view shares methods verbatim in any case. On a receiver that
    is not idle it **refuses**: "assigned once" is enforced by the object, not
    by the orchestrator's discipline.
- **`reset()` in hand-off mode returns to idle — waiting for a seed — not to
    searching**, because there is no search to return to. Samples pushed in
    idle are consumed and discarded, so the feeding loop has no special case,
    and the pool reuses the object without reallocating.

### 11.2 The lost state, and the release

"Until they are gone" is the receiver's decision, and §4's two lock flags are
the pieces of it; what is missing is the transition. Today a receiver whose
flags fall keeps running its loops on noise, and the only exit is `reset()`.

The rule, argued in §10: an emitter is gone when **code
lock drops and stays dropped** for a confirm interval. Code lock measures
presence — an emitter that leaves takes its code with it — where symbol lock
measures the carrier leg's health, which a cycle slip or a fade can take down
while the code is still despread and which the loops are built to recover.
Symbol lock alone is therefore a **degrade**, reported and not acted on.

Hand-off mode adds a fourth state, **lost**, beside searching / refining /
tracking. On the rule above the receiver enters it: the loops stop updating,
the replica of §11.4 stops being published — at the *drop*, before the confirm
interval has run, on the same flag — and `get_lost()` reports it. The holder
of the pool then releases the assignment and calls `reset()`, which in this
mode goes to idle. The confirm interval is `n_down` consecutive misses in the
`lockdet` vocabulary, sized by `det_verify_count()` from a **false-release
budget**, because against 5-to-15-minute on-times release latency costs nothing
and a false release costs a frame of that emitter's data plus, on the
cancellation branch, a frame of raised floor under every weaker emitter. Both
the per-look miss probability and the interval it gives are measurements
(§12 step 6) — and whether the code flag really rides
through a 20 dB fade for a second is what that step decides, so the rule is
written as the expectation the measurement confirms or corrects.

### 11.3 The status record

The holder of the pool needs to ask each receiver what it is doing. The facts
split by who owns them (§10): the **orchestrator** made the
assignment and fed the samples, so it holds the seed event verbatim, the sample
counts at assignment and at each state change, and the duration they give by
the `dp_sample_clock_t` arithmetic — nothing the receiver has to remember. The
**receiver** owns only what it alone knows, and today that is a scatter of
getters (§4's `get_*` family, one call each), which a reader on another thread
cannot assemble into one consistent picture across a `push()`.

So the receiver gains **one status record, returned by value** — the `measure`
objects' `single = true` record (`ToneMetrics` is the model), a jm-generated
structseq over a C struct, read on demand and never pushed — carrying: the
state (idle / searching / refining / tracking / lost); where the emitter is
**now** — the live carrier loop's Doppler, the `Dll`'s chip phase and code
rate, the despreader's C/N0 estimate; both lock flags, the symbol-lock metric
and its threshold, the two residual carrier errors; and the samples since the
state was entered and, in lost, since the code flag dropped. The existing
getters stay as the same fields' other face. The orchestrator refreshes its
*now* columns from this record at whatever cadence it reads — once per
data-free window is the minimum, because the searcher's exclusion zone is keyed
on the receiver's current estimate, not the seed. The record is a read of live
state and is not `get_state()`: the bytes triplet resumes the receiver
elsewhere, the record describes it here.

### 11.4 The replica output

On the strong branch of §9 — emitters more than the Gold
code's ~−24 dB cross-correlation floor apart in power — the searcher cancels
every assigned emitter from its input before it correlates, and the only
replica that is right through data modulation is the assigned receiver's: it
holds the live carrier's phase and frequency, the `Dll`'s code phase, the
despreader's amplitude and the decided symbols, block by block. So the receiver
gains a **replica output**: after a `push()`, the reconstructed chip stream of
the samples just consumed — code at the tracked phase and rate, carrier at the
tracked phase and frequency, amplitude from the prompt, data from the
decisions — into a caller buffer, for the searcher to subtract.

Three things about it are design, not detail:

- **It is lock-gated on code lock.** A receiver that is not code-locked
    publishes nothing, so a wrong replica is never subtracted; that is the
    same flag §11.2 releases on, read at the drop.
- **It lags by the decision latency.** The data on a block's chips is known
    only once the matched filter and the symbol timing have decided the
    symbols under it, some symbols after the block was pushed. The replica for
    block `k` is therefore complete only later, and the searcher's input is a
    **delayed** copy of the raw stream — a ring of the raw samples sized by
    that latency, which the holder owns. The receivers themselves always see
    the raw stream, live.
- **It is per output sample, on the searcher's thread.** Ten replicas
    subtracted serially per block is the cancellation's price, priced in
    §6.4, and the reason the pool's holder stands on the
    searcher's push path on this branch and not otherwise.

The replica is not needed on the weak-spread branch, and this item is built
only if §12 step 3's knee says so.

### 11.5 The cost

Twelve receivers at twice the chip rate — 10 MSa/s at the top of the range —
on the application's threads, beside one searcher and one front-end DDC; the
budget is 100 ns per output sample per core at the operating point and 43 at
the 30 MSa/s floor, half of that as the working margin (§6.4). What that asks of the receiver: nothing allocates per `push()` or per
state change (the pool runs for hours), the replica writes into a caller
buffer, the status record is by value, and one receiver's cost per output
sample is a number the bench of §12 step 8 reports beside
the count of emitters it kept.

______________________________________________________________________

## 12. The work that answers it

1. **Measure the floor one emitter puts on the surface.** One emitter,
    no noise, design C/N0; tabulate the surface's maximum and RMS relative
    to the peak over `(Δf, Δτ)` — at zero Doppler across every lag, and at
    Doppler offsets of 0.5, 1, 2, 4 bins — with and without a data
    transition inside the epoch. Beside it, `noise_est` with and without
    the emitter present: how far the CFAR reference rises. Expected: the
    three-valued −24 dB at zero Doppler, something between that and −30 dB
    elsewhere. This is the number the branch decision uses, and it is the
    engine's, so it belongs in `acq`'s characterization.
1. **Separability of two equal emitters.** Two emitters at the design
    C/N0 separated by `Δf ∈ {0.5, 1, 2, 4}` bins and
    `Δτ ∈ {0.5, 1, 2, 4}` chips, 200 trials per cell: Pd of *both* under
    the list, and the coordinates each is reported at. Expected: both
    found outside the exclusion zone, one found inside it, and no cell
    where the second is reported off its own coordinates by more than a
    cell. This pins the zone's edges as the resolution.
1. **The power-spread knee, list only.** Strong emitter fixed at the
    design C/N0, weak stepped from 0 to −40 dB below it in 3 dB, 200
    trials each, at a `Δf`/`Δτ` well outside the zone: Pd(weak). Expected
    a knee at the floor plus the detection margin. **The knee is the
    decision** — an operating spread inside it means branch one and no
    cancellation object.
1. **Pfa under the list.** Pure noise, `max_peaks ∈ {1, 4, 8}`, the same
    frame count `dsss-acquisition.md` §9.1 used: realized per-dwell Pfa
    against configured. Expected unchanged, since `N` counts cells, not
    peaks. Then with one strong emitter present: the rate of false peaks
    in its sidelobes — if it is not the configured rate, the reference is
    not tracking the raised floor and that is a CFAR finding, not a
    list finding.
1. **Cancellation depth, (iii).** An assigned receiver locked on the
    strong emitter; measure the residual after subtraction, relative to
    the strong peak, against C/N0 and against the receiver's steady-state
    phase and timing error; then re-run step 3 with cancellation on. The
    residual is the new floor and the distance it moves the knee is what
    the object buys. Run it once with (ii) as the control: the gap
    between the two is the price of not knowing the data.
1. **The release.** A hand-off-mode receiver locked on one emitter at the
    design C/N0; the emitter is switched off mid-track, faded 10 and 20 dB
    for a second, and given one carrier cycle slip, 100 trials each:
    the time from the event to code-lock drop and to symbol-lock drop, and
    whether the code flag survives the fade and the slip. Then, with the
    emitter left on for the length of an on-time, the per-look miss
    probability of each flag — the number `det_verify_count()` turns into
    the confirm interval for a false-release budget. Expected: the code
    flag rides through the slip, both flags drop within tens of
    milliseconds of switch-off, and the deep fade is the case that
    decides the interval.
1. **The lifecycle soak.** The population of §6.1 — one emitter always
    on, up to ten, on-times drawn around 5 to 15 minutes — at random
    Dopplers within one span and a spread on each side of the knee: each
    is acquired once, assigned once, tracked by the same receiver until
    it leaves, released by the rule of §10, and re-acquired on return; no
    receiver is ever assigned twice to a live emitter, no emitter above
    the floor is missed while others are up, and the pool never exceeds
    twelve. An hour sees about sixty arrivals at the full population,
    enough to count misses and false releases; the hours-long form with
    the memory and scratch checks is §5.1's duration
    requirement and runs once the bank exists.
1. **The budget, per stage.** Its own bench target, on one core,
    minimum of runs, at the operating point's numbers (§6.1): the
    front-end DDC in ns per input sample at 13 MSa/s — confirmed to be on
    the polyphase arbitrary path, not a cascade the bench's ratio let it
    shortcut (§6.4); then, in ns per
    output sample, the window-tiled searcher with `max_peaks = 16` at
    both ends of the rate range — 23 tiles at 5 Mcps and 53 at 2 over
    ±50 kHz, then 3 and 7 over the ±5 kHz pre-compensation leaves — one
    hand-off-mode receiver tracking at 5 Mcps, and one
    replica subtraction. Then the whole population — the front end, the
    searcher, ten receivers, and on the strong branch ten replicas — on
    the core count the application gives, reported as the fraction of
    real time **beside the count of emitters acquired and tracked** in
    the same run, twice: at the operating point and at the 30 MSa/s
    floor (the same chain fed 2.3× faster). Target: under 0.5 at both.
    At 1.0 the requirement is missed by its own words, and the stage that
    owns the excess is the next thing to attack — §6.4's channel number
    says today's chain is already priced near it.
1. **Decide by the spread.** The application's operating spread
    (§5.4 question 7) against step 3's knee: inside,
    branch one ships and (iii) is not built; beyond, (iii) is built and
    step 5's residual is the number its characterization pins.

Steps 1–4 are Python over the shipped engine plus the peak-list primitive,
and are the same harness the burst characterization already runs. Steps
5–6 need the hand-off-mode `AsyncDsssReceiver` (§6.1) with the lost state
of §10 and, for step 5, a replica output it does not have today. Steps 7–8
need the orchestrator holding the population.

______________________________________________________________________

## 13. What this page does not settle

Of §5.4's open questions, this page answers 6 (the
population, §6.1) and designs the receiver half of 5 (the release, §10).
Still open, and none of them blocking the list: the frame epoch (3), the
frame cadence (4) — which is what a false release costs, so it prices §10's
budget — and the other half of 5, who holds the pool and the assigned
table. That holder becomes load-bearing only on the strong branch, because
(iii) puts it on the searcher's push path. What the receiver still lacks
is implementation, not design: the hand-off-mode constructor (§6.1), the
lost state and the idle it resets to, the status record (§10), and, for
the strong branch, a replica output.

______________________________________________________________________

## 14. See also

- [`burst-bank.md`](burst-bank.md) — the burst bank this page's continuous
    case was split from; §9–§10 there for the fold and the parallelism
    measurements §5 and §6 lean on.
- [`coarse-channel.md`](coarse-channel.md) — the channel as an object, which
    is what carries the searcher.
- [`dsss-acquisition.md`](dsss-acquisition.md) — the acquisition engine
    under §2 and §6: the tiling, the CFAR vocabulary (`dsss-acquisition.md`
    §9.1, which §7 uses), the roadmap.
- [`dsss-burst-receiver.md`](dsss-burst-receiver.md) — the burst chain, which
    shares §2.2's `DetectionEvent`.
- [AsyncDsssReceiver: the SPEC waveform](../gallery/async-dsss-receiver-spec.md)
    and [Streaming Async Despreader](../gallery/async-despread.md) — the
    gallery demonstrations of §4 and §3.
