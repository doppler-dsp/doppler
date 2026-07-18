# DsssReceiver Finishing Plan Checklist

Tracks progress against `SPEC.md`'s "Target implementations" and its
derived requirements. Updated 2026-07-18 after the Acquisition/
BurstAcquisition split (uncommitted, branch `feat/dsss-receiver-object`)
and the SPEC-operating-point validation pass that followed it.

## Target implementations (SPEC.md)

- [x] **Complete C DsssReceiver** in `libdoppler.{a,so}` —
  `native/src/dsss_receiver/dsss_receiver_core.c`. Composes
  `Acquisition` (continuous) -> `Dll` -> `RateConverter` ->
  `MpskReceiver`. `native/tests/test_dsss_receiver_core.c` covers
  construction, search->track handoff, decode, state round trip
  (both searching and tracking), envelope rejection.
- [x] **Complete Python DsssReceiver** — `doppler.dsss.DsssReceiver`,
  jm-generated binding over the same C core. Covered end-to-end by
  `src/doppler/dsss/tests/test_dsss_receiver.py`,
  `dsss_receiver_demo.py`, `dsss_receiver_stress.py` (randomized
  CN0/Doppler/spc/power sweep).
- [ ] **Stateless composable blocks deployable as k8s microservices** —
  partially real, partially aspirational:
  - [x] **Acquisition** — `doppler.dsss.Acquisition` (continuous) /
    `BurstAcquisition`, both serializable (`get_state`/`set_state`/
    `state_bytes`), both stateless-resumable (pod-handoff proven by
    the bespoke round-trip tests in `test_acq_continuous.py`/
    `test_burst_acq.py`).
  - [x] **DDC** — `doppler.ddc`, already serializable, predates this
    story.
  - [ ] **Despreader** — no standalone serializable "Despreader"
    microservice block exists. `DsssReceiver` composes Acquisition
    straight into `Dll` internally (not a separate deployable hop);
    the k8s-block decomposition SPEC.md envisions (Acquisition and
    Despreader as *separate* services with `DetectionEvent` as the
    wire format between them) is not built. `dsss_acq_handoff_from_result()`
    (the C struct/function that would build a wire-ready
    `DetectionEvent` from an `acq_result_t`) does not exist yet —
    flagged in `SPEC.md`'s own "Output data structure" section,
    still true. Only the Python prototype
    (`prototypes/async_despreader/acq_handoff.py`) does this
    conversion, and only in-process.
  - [x] **RateConverter** — already serializable, predates this story.
  - [x] **MpskReceiver** — already serializable, predates this story.

## Acquisition/BurstAcquisition split (this session's core work)

- [x] C engine rewrite: `acq_create_burst`/`acq_create_continuous`,
  `coherent_bins`/`window_bins` internal split, unified public
  `doppler_bins`, `max_noncoh` removed in favor of the internal
  `ACQ_N_NONCOH_SAFETY_CEILING` (256) safety valve.
- [x] `BurstAcquisition` — new, genuinely separate jm object
  (`objects/burst_acq.toml`), thin C forwarder
  (`native/src/burst_acq/burst_acq_core.c`) onto the SAME
  `acq_state_t`/`acq_core.c` engine `Acquisition` uses (the
  "composing object" pattern, no jm feature existed for "two Python
  classes, one core" — filed upstream as
  [just-makeit#504](https://github.com/just-buildit/just-makeit/issues/504)).
- [x] `DsssReceiver` migrated to `acq_create_continuous`; lost its own
  `reps`/`max_noncoh`/`doppler_resolution` params (dead once the
  engine it embeds is always continuous).
- [x] Every Python/C caller migrated (~30 files: tests, examples,
  benchmarks, `orchestrator.py`, `track/tests/`) — each one actually
  run, not just import-checked.
- [x] New regression coverage: `test_acq_continuous.py` (continuous
  class's own config/ceiling/underpowered/state-roundtrip coverage,
  previously nonexistent), `test_burst_acq.py` (composing object's
  own construction/reset/state-roundtrip + the two cross-class tests:
  same-config divergence, cross-class state-blob rejection).
- [x] Full verification: `ctest` 86/86, `pytest src/doppler/` 2427
  passed (15 pre-existing/environment failures confirmed unrelated
  via before/after `git stash` comparison — missing CLI executables
  on `PATH`, not this work), `jm status --check` clean.
- [x] Fixed a real, unrelated C build break surfaced by the full
  rebuild: `native/tests/test_dsss_receiver_core.c` and
  `native/benchmarks/bench_dsss_receiver_core.c` still called the OLD
  16-arg `dsss_receiver_create()` (never updated when the constructor
  lost `reps`/`max_noncoh`/`doppler_resolution`). Fixed, plus bumped
  `test_dsss_receiver_core.c`'s sizing `cn0_dbhz` 45.0->70.0 (see
  inline comment there): at 45.0 the new continuous-only engine rides
  the 256-look non-coherent ceiling (genuinely under-powered per the
  new, correct model) and the resulting handoff seed was imprecise
  enough for this test's unusually short 7-chip code to leave a
  persistent ~0.25-0.3 BER floor — not a settling transient (confirmed
  by direct per-segment measurement), and not a bug: non-coherent
  accumulation legitimately doesn't sharpen per-look phase precision
  the way the old (structurally unsafe, per task #67) joint coherent
  search did. 70.0 keeps sizing comfortably off the ceiling, matching
  this test's own stated scope (wiring, not re-proving Acquisition's
  own precision-vs-sizing trade-offs).
- [x] Docs pass: `docs/guide/dsss-acquisition.md`,
  `docs/design/dsss-acquisition.md`, `docs/dev/dsss-use-cases.md`,
  `docs/gallery/dsss-acq-async-data.md` all updated to the split API;
  every touched code fence re-verified to actually run (this repo's
  docs are tested in CI, not just prose).
- [x] `prototypes/async_despreader/acq_handoff.py` rewritten to
  validate the FULL real `SPEC.md` operating point end-to-end (see
  below), not a scaled-down placeholder.

## SPEC.md operating-point validation (this session)

Prior to this session, no test exercised `SPEC.md`'s actual numbers
together: full ±50 kHz Doppler uncertainty (the real 34-bin wideband
grid), the Es/N0 = 3 dB worst-case floor (`cn0_dbhz = 37.31` exactly),
the real CCSDS Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps waveform,
and `bn<=0.01` on every tracking loop. `bench_acq_core.c` used the real
numbers but only as a latency benchmark (detection-only, no
despread/decode). Every decode-level test used easier/different
numbers and stayed in single-bin mode (Doppler uncertainty never
exceeded ~1.3 kHz).

- [x] `acq_handoff.py` rewritten to construct at the real operating
  point and run the full search -> handoff -> refine -> track chain.
- [x] Acquisition/handoff proven EXACT at the real point: `doppler_bins
  =34`, `n_noncoh=112` (matches `bench_acq_core.c`'s own pd=0.9
  measurement precisely), 0.00 chip / 0.0 Hz handoff error.
- [x] Two real, previously-invisible handoff bugs found and fixed
  (both specific to the Python `CoupledAsyncDespreader` prototype, not
  `DsssReceiver`'s own C handoff, which already got this right):
  1. `freq_refine.refine_seed()` never threaded `init_chip` through
     (always defaulted its collection tracker to code phase 0) —
     invisible before because every earlier script in this folder
     always collected at code phase 0. Fixed: added `init_chip=0.0`
     param, default preserves every existing caller byte-for-byte.
  2. `Acquisition.code_phase`'s convention and
     `CoupledAsyncDespreader.init_chip`'s convention are MIRROR
     IMAGES (`sf - chip_phase`, not `chip_phase` directly) — confirmed
     via a controlled A/B test. `d.code_rate` staying pinned near 1.0
     regardless of whether the phase was right or catastrophically
     wrong is why this stayed hidden (see
     `reference_code_rate_not_a_lock_indicator` memory) — it only
     reflects the loop filter's rate integrator, not absolute lock.
  3. (Red herring, caught before it became a false conclusion: an
     early isolation script of mine had its own sign bug and briefly
     looked like a THIRD bug — see the memory checkpoint for the full
     trail.)
- [x] After both fixes: full chain LOCKS cleanly at the real operating
  point (refine residual +9.3 Hz, final tracking error 0.06 Hz).
- [x] **Doppler-rate (<500 Hz/s, corrected -- see below) tracking at the
  real 3 dB floor** — investigated via `doppler_rate_floor_test.py`
  (extends `doppler_rate_test.py`'s exact static-batch/FLL-assist/
  integrated methodology to `Es/N0={3,5,10}dB x rate={0,500,1000}Hz/s`
  -- the corrected worst case plus a 2x margin point, `bn=bn_car=0.01`
  per `SPEC.md`'s derived ceiling, 8 seeds/point).
  **ORIGINAL finding (superseded — kept for the historical record of
  why the rework below happened): FLL-assist is UNSAFE at the literal
  3 dB floor, confirmed at the ACTUAL corrected rate, not just an
  inflated one.** At Es/N0=3dB, rate=500 Hz/s (SPEC's real worst case):
  FLL=on is worse than FLL=off in 7/8 seeds (mean err 3018 Hz vs.
  FLL=off's 676 Hz -- over 4x worse); rate=0: 8/8 worse; rate=1000 (2x
  margin): 6/8 worse. Clean transition at 5 dB, 10 dB behaves as
  originally validated. Also notable: Costas-ALONE (FLL=off) settles
  to a persistent ~675 Hz error at rate=500 regardless of Es/N0 --
  past its own ~117 Hz/s lock-loss cliff (see below) -- so Costas-alone
  isn't a safe fallback either. This was the ad-hoc periodic-FFT
  mechanism's own result; it directly extends `SPEC.md`'s "still open"
  per-epoch bit-straddle line, but the mechanism itself turned out to
  be the wrong thing to fix (see "The real fix" section below).

  **UPDATED result, same 8-seed/point sweep, after the rework to the
  real `bn_fll` cross-product mechanism (`bn_fll_car=0.03`,
  `test_costas_core.c`'s own validated calibration point) — now a
  categorically different, and clean, result:**

  | Es/N0 | rate (Hz/s) | FLL=off mean err (Hz) | FLL=on mean err (Hz) | FLL=on worse than off |
  |---|---|---|---|---|
  | 3 dB  | 0    |    4.2 |   57.1 | 8/8 (rate=0: no ramp to correct — pure added noise floor, see below) |
  | 3 dB  | 500  |  668.3 |   62.9 | 0/8 |
  | 3 dB  | 1000 | 1351.3 |   51.8 | 0/8 |
  | 5 dB  | 0    |    3.0 |   52.6 | 6/8 |
  | 5 dB  | 500  |  674.6 |   51.5 | 0/8 |
  | 5 dB  | 1000 | 1346.6 |   21.6 | 0/8 |
  | 10 dB | 0    |    1.4 |   10.9 | 8/8 |
  | 10 dB | 500  |  668.2 |   18.7 | 0/8 |
  | 10 dB | 1000 | 1351.1 |   13.6 | 0/8 |

  **At SPEC's literal worst case (3 dB / 500 Hz/s): mean error 668 Hz
  (FLL=off) -> 63 Hz (FLL=on), a ~10x reduction; 0/8 seeds worse.** No
  gross wrong-peak errors anywhere in the sweep (max error across every
  nonzero-rate cell stays under 200 Hz). The only place FLL=on costs
  anything is `rate=0` (no ramp present) — a real, understood, minor
  trade-off: an always-on cross-product discriminator adds a small
  noise floor (tens of Hz) even with nothing to pull in, since it's
  still computing and folding in a nonzero (if small) `freq_err` every
  epoch from noise alone. Full raw sweep output:
  `/home/matt/.claude/jobs/6e54682b/tmp/floor_test_reworked_out.txt`.
  **This closes the follow-up** — the mechanism itself (not a gate
  bolted onto the old one) was the fix; see below.

## Still open / explicit follow-ups (not started)

- [x] ~~FLL-assist safety gate near the Es/N0 floor~~ — **SUPERSEDED,
  see "The real fix" below.** The prototype's own ad-hoc FLL-assist
  (periodic squaring+FFT batch re-estimation) does need a safety gate
  to be trustworthy near the floor, and Costas-alone genuinely cannot
  track SPEC's corrected +/-500 Hz/s requirement on its own (confirmed:
  a hard lock-loss cliff at ~117 Hz/s, ~4x below the requirement,
  traced down to the discriminator level -- accumulated phase error
  from the uncorrected ramp outruns the Costas phase-discriminator's
  inherent +/-90 degree unambiguous range within tens of symbols once
  the rate exceeds the loop's own correction bandwidth; confirmed
  independent of the `aid_code` carrier->code coupling via a byte-
  identical A/B). But building a safety gate for the prototype's own
  invented mechanism turned out to be solving the wrong problem --
  see below.

## The real fix: use the shipped `costas_core.h` `bn_fll`, not a hand-rolled FLL

Found while asking "where does this Costas discriminator actually come
from" (this session): `prototypes/async_despreader/despreader_coupled.py`
hand-rolls its own Python copy of just the PHASE-discriminator formula
(`e_car = (im_p if re_p>=0 else -im_p)/a_p`, matching `costas_core.h`'s
`costas_update`'s form) but never calls the real, shipped `Costas`
object at all -- and its own "FLL-assist" (periodic squaring+FFT batch
re-estimation over 300-epoch blocks) is a from-scratch invention,
**duplicating a mechanism that already exists, is already shipped, and
is already tested**: `native/inc/costas/costas_core.h` /
`native/src/costas/costas_core.c` has a built-in `bn_fll` parameter --
a decision-directed CROSS-PRODUCT frequency discriminator (continuous,
per-symbol, standard GNSS/DSSS technique), exposed to Python via
`doppler.track.Costas(bn_fll=...)`, with its own passing test
(`test_costas_core.c`, test 7: "FLL assist widens pull-in" --
`bn=0.01, bn_fll=0.03` acquires a large static residual a bare PLL
can't, at zero noise).

That existing test never combined bn_fll with BOTH a Doppler RAMP and
low Es/N0 together -- exactly the untested combination this session's
whole investigation was chasing. Tested directly (new, this session):
real `Costas(bn=0.01, bn_fll=0.03, tsamps=2046)` against the identical
rate=500 Hz/s ramp, Es/N0=3dB and 10dB. **Result: the real bn_fll
mechanism is safe and effective at BOTH points** -- final tracking
error -24.9 Hz at 3dB, -10.6 Hz at 10dB (vs. the prototype's own
FLL-assist: -7800 Hz at 3dB). No gross failures, no runaway, smooth
ramp-following throughout at both SNR points. This is because a
cross-product discriminator's failure mode is structurally different
from squaring+FFT batch re-estimation: it doesn't depend on collecting
enough samples in a short, noisy block to average down before
committing to an estimate -- it's continuous and per-symbol, feeding
the SAME loop filter that's already handling the phase-tracking noise
averaging.

**Conclusion: the safety-gate follow-up above is superseded.** The
prototype doesn't need a custom gate bolted onto its own invented FLL
mechanism -- it needs to stop reimplementing Costas and use the real,
shipped, already-validated one instead (per this project's own "never
reimplement existing logic" principle). This is now the actual
follow-up:

- [x] **Rework `despreader_coupled.py`'s carrier tracking to use the
  real `costas_core.h` mechanism.** `Costas.steps()` couldn't be
  called directly -- it's a self-contained block API (raw samples in,
  one symbol-rate prompt out) that does its own internal wipeoff+I&D,
  discarding the intermediate per-sample derotated stream this
  module's code-tracking correlator needs (and per-sample, PRE-despread
  correction is load-bearing here, not optional -- a downstream-only
  fix lets the signal walk out of the correlator's bandwidth over
  minutes of operation). So the port is at the mechanism level, not an
  object swap: `costas_update`'s exact control flow (cross-product
  `freq_err` folded into `car_lf.integ` ahead of `step(e_car)`, PLUS
  the proportional phase-kick `nco.phase += nco_norm_to_inc(kp*e/2pi)`
  this module was missing entirely) now drives the SAME real `LO`/
  `LoopFilter` C objects already in use -- reusing the canonical
  primitive's actual numerics, only the epoch-level wiring (which
  prompt feeds which discriminator) is new, since that wiring is
  exactly what `Costas.steps()`'s fixed shape can't expose. New
  constructor param `bn_fll_car` (mirrors `Costas`'s own `bn_fll`,
  `k_fll=4*bn_fll_car`) replaces `fll_block_epochs`/`fll_n_fft`/
  `fll_zero_pad` and the whole periodic-FFT mechanism outright -- one
  tracked carrier deviation now, not a coarse/fine split.
  Smoke-verified (`Es/N0=3dB, rate=500Hz/s`: err 664 Hz FLL-off -> 69 Hz
  FLL-on; `10dB`: 659 Hz -> 18 Hz) before re-running the full 8-seed
  sweep -- see updated numbers below, replacing the old mechanism's
  entries. Callers migrated: `doppler_rate_test.py`,
  `doppler_rate_floor_test.py` (both `run_integrated*` now take
  `bn_fll_car=` instead of `fll_block_epochs=`, `BN_FLL_CAR=0.03` --
  `test_costas_core.c`'s own validated calibration point), and
  `acq_handoff.py`'s docstring (its tracking construction never passed
  `fll_block_epochs=`, so its behavior is unaffected -- FLL stays
  disabled there by default, same as before).
- [ ] The plots backing the ORIGINAL finding (kept for the historical
  record of why this rework happened): `/home/matt/.claude/jobs/
  6e54682b/tmp/loop_stress_3_10db.png` (prototype's OLD FLL-assist,
  runaway at 3dB) and `real_costas_fll_3_10db.png` (real bn_fll,
  stable at both, standalone `Costas` object) -- not yet copied into
  the repo/docs; worth adding to `docs/design/` if this finding gets
  written up formally.

## Corrected Doppler-rate figure (this session, later)

`SPEC.md`'s "Frequency rate of change: < 5 kHz/s" was a typo -- 10x too
high; the intent was 500 Hz/s. Verified three independent ways, all
landing in the same "few hundred Hz/s" range and nowhere near 5 kHz/s:
(1) standard LEO nadir-pass orbital-mechanics derivation
(`f_dot_max = (f_c/c)*(v^2/h)`) at SPEC's own 2.5 GHz and a
representative ~800 km altitude: 579 Hz/s; (2) pass-duration averaging
(a 10-minute pass over SPEC's own ±50 kHz swing): ~167 Hz/s average,
~250-333 Hz/s peak; (3) the same averaging over the design doc's
±100 kHz worked-case swing: ~333 Hz/s average, ~500-667 Hz/s peak.
Corrected in `SPEC.md` (line 6) and every place that cited the old
figure: `docs/design/dsss-acquisition.md` §8 (its own Δrdot/R worked
table recomputed -- the corrected, 10x-smaller rate span means even
50 ms coherent integration stays "dynamics free," and the worst-case
tile count drops from ~10,000 to ~1,200), `despreader_coupled.py`,
`doppler_rate_test.py`, `doppler_rate_floor_test.py` (its own
`RATE_LIST_HZ_PER_S` changed from `[0, 1000, 5000]` to `[0, 500,
1000]` -- the corrected worst case plus a 2x stress margin, re-run;
see the FLL-assist finding above for the updated numbers).
- [ ] **`dsss_acq_handoff_from_result()`** (C struct/function building
  a wire-ready `DetectionEvent` from an `acq_result_t`) — the
  microservice-decomposition target's missing piece; only the Python
  prototype does this conversion today, in-process only.
- [ ] **`DsssBurstReceiver`** (composing `BurstAcquisition`+
  `BurstDemod` the way `DsssReceiver` composes `Acquisition`+`Dll`) —
  explicitly confirmed OUT OF SCOPE for this PR by the user; a real,
  separate design exercise (`BurstDemod`'s seeded `set_prior()` API
  needs its own auto-drive design). Filed as task #80.
- [ ] **Standalone "Despreader" k8s microservice block** — see above;
  `DsssReceiver` composes Acquisition->Dll in-process, not as two
  separately-deployable hops with a serialized `DetectionEvent` wire
  format between them.
- [ ] Nothing has been committed yet. Once this checklist's shipped
  items are reviewed, this needs one (likely large) commit on
  `feat/dsss-receiver-object` — never commit to main directly.

## Explicitly out of scope, don't revisit

- `DsssBurstReceiver` (see above, task #80).
- Redesigning `CoupledAsyncDespreader`'s FLL-assist mechanism itself
  beyond flagging the gate it needs — that's real, separate DSP design
  work, not a quick fix.
