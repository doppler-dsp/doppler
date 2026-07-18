# DsssReceiver Specifications

- Level: Any
- Nominal frequency: 2.5 GHz
- Frequency uncertainty: +/- 50 kHz
- Frequency rate of change: < 5 kHz/s
- Waveform: Continuous DSSS BPSK
- Waveform exemplary use-case:
    - Code: CCSDS Command link Gold Code 1023 chips repeating
    - Chip rate: 3.069 Mcps
    - Modulation: Asynchronous Rectangular BPSK @ 2700 bps
- Es/N0 >= 3 dB

## Target implementations

- Complete C DsssReceiver available in libdoppler.{a,so} to compile into C/C++ applications
- Complete Python DsssReceiver to include in Python applications
- Set of stateless (serialized state passing) composable blocks deployable as k8s microservices
  - DDC
  - Acquisition
  - Despreader
  - RateConverter (may be absorbed by despreader or MpskReceiver)
  - MpskReceiver

## Derived: tracking loop bandwidths (all loops: code DLL, Costas/CarrierMpsk carrier, FLL-assist)

- Design target: loop SNR `rho >= 20 dB` at the Es/N0 floor (3 dB), using
  the standard PLL loop-SNR relation `rho(dB) = Es/N0(dB) - 10*log10(2*bn)`
  (`bn` = the loop's own noise bandwidth, normalised to its update rate --
  `doppler.track.LoopFilter`'s own `bn` convention, so the update rate
  cancels out of the relation entirely; it depends only on Es/N0 and `bn`).
- Solving at the floor: `bn <= 10^((EsN0_dB - rho_dB)/10) / 2`
  `= 10^((3 - 20)/10) / 2 = 10^-1.7 / 2 ~= 0.00998`
- **Default rule: `bn <= 0.01` for every tracking loop**, sized against
  the worst-case Es/N0 floor, not a comfortable/typical operating point.
  (Applies to the code loop directly; the code loop's own per-epoch SNR
  is Es/N0 scaled by `1/epochs_per_symbol` -- at this waveform's
  `epochs_per_symbol = (chip_rate/sf)/data_rate = 3000/2700 = 10/9 ~= 1.11`,
  within ~0.5 dB of Es/N0 itself, so the same `bn<=0.01` bound applies
  without a separate derivation.)

## Acquisition

### User-facing API

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

| Parameter | Type | Default | Description |
|---|---|---|---|
| `code` | `NDArray[uint8]` | *(required)* | Binary (0/1) code, segment, or preamble chips to search for; sets `sf = len(code)`. |
| `spc` | `int` | `4` | Samples per chip (>= 1). |
| `chip_rate` | `float` | `1e6` | Chip rate in Hz (> 0). |
| `symbol_rate` | `float` | `1000.0` | Continuous data-symbol rate in Hz (> 0). |
| `cn0_dbhz` | `float` | `50.0` | Carrier-to-noise density in dB-Hz (> 0) -- the sensitivity used to size the search. |
| `doppler_uncertainty` | `float` | `0.0` | One-sided Doppler search half-range in Hz; `0` = full native span (one `doppler_bin`). Tiles into `doppler_bins` windows whenever it exceeds one native span. |
| `pfa` | `float` | `1e-3` | Target system (max-of-N) false-alarm probability, in `(0,1)`. |
| `pd` | `float` | `0.9` | Target detection probability, in `(0,1)`. |
| `noise_mode` | `Literal["mean","median","min","max"]` | `"mean"` | CFAR reference-cell aggregation mode. |

#### `BurstAcquisition`

`doppler_bins` here is the `coherent_bins` mechanism, auto-sized in
`[1, reps]` for coherent gain (today's existing `Acquisition`
behavior) -- assumes an unmodulated (or preamble) acquisition window,
so there's no data-bit-straddle loss to price and no `symbol_rate` to
supply.

| Parameter | Type | Default | Description |
|---|---|---|---|
| `code` | `NDArray[uint8]` | *(required)* | Binary (0/1) code, segment, or preamble chips to search for; sets `sf = len(code)`. |
| `reps` | `int` | `1` | `doppler_bins` ceiling -- the coherent-depth axis (>= 1). |
| `spc` | `int` | `4` | Samples per chip (>= 1). |
| `chip_rate` | `float` | `1e6` | Chip rate in Hz (> 0). |
| `cn0_dbhz` | `float` | `50.0` | Carrier-to-noise density in dB-Hz (> 0) -- the sensitivity used to size the search. |
| `doppler_uncertainty` | `float` | `0.0` | One-sided Doppler search half-range in Hz; `0` = full native span (one `doppler_bin`). Tiles into `doppler_bins` windows whenever it exceeds one native span. |
| `pfa` | `float` | `1e-3` | Target system (max-of-N) false-alarm probability, in `(0,1)`. |
| `pd` | `float` | `0.9` | Target detection probability, in `(0,1)`. |
| `noise_mode` | `Literal["mean","median","min","max"]` | `"mean"` | CFAR reference-cell aggregation mode. |

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

### Output data structure: `DetectionEvent` (the acquisition handoff)

`DetectionEvent` is the DATA -- the acquisition handoff is the ACTION
(the process of converting a raw `push()` hit into this record and
handing it to the next block/service); the two aren't the same thing,
naming them separately on purpose.

Per the target-implementations goal above ("stateless composable
blocks deployable as k8s microservices"), `Acquisition`/
`BurstAcquisition`'s detection output has to be consumable by another
process/service, not just another Python object in the same
interpreter -- so it can't be the raw grid-relative indices
(`doppler_bin`, `code_phase`) alone, since those are meaningless
without also shipping the emitting object's own config (`spc`,
`doppler_res_hz`, ...) alongside. Every field below is already
converted to a physical unit, so the record is self-contained: a flat,
pointer-free POD, safe to serialize (JSON, protobuf, whatever the
transport is) across a process boundary. This is Phase 0 of the
coupled-tracker roadmap (`~/.claude/plans/jiggly-munching-newell.md`)
made concrete -- matches `dsss_acq_handoff_from_result()`'s planned
C struct, and is exactly what `prototypes/async_despreader/
acq_handoff.py`'s `DetectionEvent`/`handoff_from_hit()` already
prototyped and validated in Python this session (code-phase
conversion exact, full search -> handoff -> refine -> track chain
locks).

One `DetectionEvent` record is emitted per detection event (i.e. once
per `push()` hit, on both classes -- same shape, since both share the
underlying engine):

| Field | Type | Description |
|---|---|---|
| `timestamp_ns` | `uint64_t` | UNIX time (ns) this detection's samples occurred, per the codebase's existing `dp_sample_clock_t` convention (`native/inc/timing/timing_core.h`): `epoch_real_ns + samples_consumed/fs`, NOT a fresh syscall timestamp at emit time -- reproducible, and already how `dp_header_t`/SigMF-metadata timestamps are derived elsewhere in this project. |
| `samples_consumed` | `uint64_t` | The raw sample offset (since this engine's own stream start) this detection's epoch ended at -- the `n` that `timestamp_ns` above was derived from. Kept alongside `timestamp_ns`, not instead of it: replay-safe (no wall-clock dependency) and lets a consumer re-derive/cross-check the time against its own clock anchor. |
| `chip_phase` | `float` | Code phase in CHIPS (not raw samples) -- the code-tracking seed for the next stage. |
| `doppler_hz_est` | `float` | Coarse Doppler estimate in Hz, already folded/signed/scaled from the raw `doppler_bin` index. |
| `doppler_res_hz` | `float` | Width of that estimate -- the remaining uncertainty (±`doppler_res_hz`/2) a downstream refine/tracking stage still has to close. |
| `cn0_dbhz_est` | `float` | Estimated carrier-to-noise density (dB-Hz) -- informs downstream loop-bandwidth/dwell sizing (e.g. `fll_block_epochs`, still an open question in the plan). |
| `peak_mag` | `float` | Raw CFAR peak magnitude -- diagnostic/observability passthrough, not needed for tracking math. |
| `noise_est` | `float` | Raw CFAR noise-floor estimate -- diagnostic passthrough. |
| `test_stat` | `float` | Raw CFAR gating statistic -- diagnostic passthrough. |

**A real gap this surfaced, not yet fixed**: `acq_result_t` (today's
shipped struct) carries neither field above -- `acq_core.c` computes
`samples_consumed` right before appending each result (`acq_push()`,
around the `dp_f32_consume`/`result[ndet++]` site) and never stamps it
onto the result at all, so it's silently unavailable per-hit even
though the engine already knows it at exactly the right moment.
Getting `timestamp_ns` onto `DetectionEvent` needs two additions,
neither of them a new timestamp SCHEME (reuse `dp_sample_clock_t`'s
existing `epoch_real_ns + n/fs` formula, don't invent a fourth one
alongside `dp_header_t.timestamp_ns` and `dp_tlm_rec_t.n`):

1. `acq_result_t` gains a `samples_consumed` field, stamped at the
   same point `acq_core.c` already computes it per-hit.
2. The handoff ACTION (Python's `handoff_from_hit()` today, the
   planned `dsss_acq_handoff_from_result()` in C) takes a
   `dp_sample_clock_t` as an extra input and resolves `timestamp_ns =
   c->epoch_real_ns + samples_consumed/c->fs*1e9` -- note this is
   `dp_sample_clock_t`'s underlying FORMULA, not a literal
   `dp_sample_clock_stamp()` call, since that function stamps the
   clock's OWN current position, not an arbitrary historical `n` (a
   `push()` call can emit a hit for a `samples_consumed` behind the
   clock's live position if multiple epochs were processed in one
   call) -- worth a tiny `dp_sample_clock_stamp_at(c, n)` helper
   alongside `dp_sample_clock_stamp()` rather than duplicating the
   formula at every call site.
3. `Acquisition`/`BurstAcquisition` themselves stay wall-clock/epoch
   -agnostic (pure sample-domain engines, no I/O) -- the
   `dp_sample_clock_t` anchor comes from whatever upstream source
   (DDC, a real front end) is actually feeding them samples, and gets
   threaded through by the composing layer (`DsssReceiver`, or the k8s
   block wrapper), not owned by `Acquisition` itself.

This gap isn't specific to `Acquisition` -- `dp_tlm_rec_t`'s own doc
comment already flags the general pattern ("if never stamped it stays
0"), meaning this is likely unwired in other streaming objects too,
not just here.

**Settles the plan's open question: no `carrier_freq` parameter on
either class.** The plan flagged "does `carrier_freq` need to become a
new `dsss_receiver_create()`/`acq_create()` parameter" -- answer: no.
`Acquisition` has no other reason to know the RF carrier frequency (it
operates in baseband/chip-rate Doppler Hz throughout); the
carrier-aiding scale (`doppler_hz_est * chip_rate/carrier_freq`) is
computed by whichever component actually knows `carrier_freq` --
`prototypes/async_despreader/despreader_coupled.py`'s tracker already
does exactly this with its own `carrier_freq_hz` parameter, not
something `Acquisition` hands it pre-scaled. Keeps `Acquisition`
carrier-frequency-agnostic and reusable for a baseband-only caller
that has no carrier concept at all.

### Notes

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
  alias-window searches (each its own 2D correlation, `code_bins =
  sf*spc = 2046` at `spc=2`) -- "34 x 2046" -- run either:
  1. **Sequentially** (34x acquisition latency -- almost certainly
     fails the "FAST" requirement), or
  2. **In parallel** (34x compute/hardware, full latency preserved), or
  3. **Behind a DDC channelizer bank** (`doppler.ddc`/`RateConverter`,
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
  - **Discrepancy found and resolved in the process**: the `n_noncoh=
    96/128/192` operating points quoted earlier in this doc came from a
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
