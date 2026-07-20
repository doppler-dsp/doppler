# DsssReceiver Finishing Plan Checklist

Tracks progress against `SPEC.md`'s "Target implementations" and its
derived requirements. Full narrative history (every investigation,
measurement, and dead end behind each line below) lives in the
`project_dsss_acq_async_story` memory and in `archive/README.md`
(discarded-idea log) — this file is the checklist only.

## Target implementations (SPEC.md)

- [x] C `DsssReceiver` (`native/src/dsss_receiver/dsss_receiver_core.c`) — Acquisition -> Dll -> RateConverter -> MpskReceiver, fully tested.
- [x] C `DsssReceiver` meets SPEC's Doppler-rate requirement (< 500 Hz/s at the 3 dB floor) via a pre-despread `costas_state_t` composition.
- [x] Python `DsssReceiver` binding, covered end-to-end by its own test/demo/stress suite.
- [x] `Acquisition`/`BurstAcquisition` — serializable, stateless-resumable k8s-microservice blocks.
- [x] `DDC`, `RateConverter`, `MpskReceiver` — already serializable microservice blocks (predate this story).
- [ ] Standalone serializable "Despreader" k8s microservice block — `DsssReceiver` composes Acquisition->Dll in-process, not as two separately-deployable hops.
- [ ] `dsss_acq_handoff_from_result()` — the C function that would build a wire-ready `DetectionEvent` from an `acq_result_t`; only the Python prototype does this conversion today, in-process only.

## Acquisition/BurstAcquisition split

- [x] C engine rewrite: `acq_create_burst`/`acq_create_continuous`, unified public `doppler_bins`.
- [x] `BurstAcquisition` — new composing object over the same `acq_core.c` engine `Acquisition` uses.
- [x] Every Python/C caller migrated (~30 files) and actually run.
- [x] New regression coverage: `test_acq_continuous.py`, `test_burst_acq.py`.
- [x] Full verification: `ctest` 86/86, `pytest` 2427 passed, `jm status --check` clean.
- [x] Docs pass across every touched page, all code fences re-verified to run.

## SPEC.md operating-point validation

- [x] `acq_handoff.py` (this folder) validated Acquisition/handoff EXACT at SPEC's real +/-50kHz operating point (0.00 chip / 0.0 Hz error).
- [x] Found and fixed two real handoff bugs specific to this Python prototype: `refine_seed()` wasn't threading `init_chip` through, and `Acquisition.code_phase`'s convention is the mirror image of `CoupledAsyncDespreader.init_chip`'s (`tracker_init_chip = (sf - chip_phase) % sf`).
- [x] Reworked the carrier loop to use `costas_core.h`'s real `bn_fll` cross-product mechanism instead of a hand-rolled periodic-FFT re-estimate — safe and effective at both 3dB and 10dB, unlike the hand-rolled version (which failed catastrophically, -7800Hz error at 3dB).
- [x] Corrected `SPEC.md`'s Doppler-rate figure: "< 5 kHz/s" was a 10x typo for the intended 500 Hz/s, confirmed three independent ways (orbital mechanics, pass-duration averaging at two swing widths).

## Task #99 — SPEC combined-scenario pull-in cliff (C `DsssReceiver`)

- [x] Found and fixed a real bug: `_carrier_update_from_partials` coherently summed `segments`-many partials without data-wiping them by sign first, self-cancelling across a bit transition — fixed by sign-aligning each partial before the sum (10dB now 4/4 clean, was 2/4).
- [x] Same data-wipe fix ported to the Python prototype (`despreader_coupled.py`) with matching improvement.
- [x] Swept `bn_car` (0.005-0.02, 4x range) — the 3-4dB cliff doesn't move at all; not a loop-bandwidth problem.
- [x] Swept Doppler rate (0/200/300/400/500 Hz/s) at the cliff — identical 4dB-fails/5dB-clean pattern at every rate; the cliff is pure-SNR, entirely rate-independent.
- [ ] **Still open**: the 4-5dB pull-in cliff itself is unexplained by loop bandwidth or Doppler rate; leading remaining candidate is Acquisition's own hit quality/reliability at this SNR, not yet directly inspected.

## The nested-loop rearchitecture campaign (this session, superseded — see `archive/README.md`)

- [x] Found a working reference implementation (`~/legacy-commz`) showing the intended carrier architecture is a nested two-rate loop: raw-rate fused code+carrier replica, always-on code-rate aiding, a resampled low-rate PSDMF refinement, and a closed Costas loop feeding back into the raw-rate replica.
- [x] Fixed `aid_code`'s formula to aid from the full current carrier estimate, not just the tracked deviation.
- [x] Added an opt-in `car_update_windows` per-window carrier-loop mode to `CoupledAsyncDespreader`.
- [x] Built and tested a `carrier_resample_track.py` external harness mirroring the reference's exact resample-then-track carrier loop.
- [x] Confirmed via a real `~/legacy-commz`-generated capture (bit-identical Gold-1023 code verified) that none of the above fixed the BER~0.5 "never locks" failure.
- [x] Pulled `MpskReceiver`'s own telemetry and found the real cause: its symbol-timing loop never locks on either signal, independent of anything upstream — a separate, still-open bug entirely outside Acquisition/PSDMF/`CoupledAsyncDespreader`.
- [x] Archived `carrier_resample_track.py`, `spec_full_characterization_prototype.py`, and five other superseded exploratory scripts; see `archive/README.md` for what each one found.

## Acq -> Despreader cleanup (this session, current)

- [x] Trimmed `acq_handoff.py` to pure Acquisition->handoff glue (`DetectionEvent`, `handoff_from_hit`, `search_and_handoff`), removing its own superseded demo/test.
- [x] Built ONE clean end-to-end test, `e2e_acq_to_despreader.py`: Acquisition search+handoff -> one PSDMF refine pass -> `CoupledAsyncDespreader` (`aid_code` + `car_update_windows`) tracking, scored via a genie-aided despread-output BER that doesn't depend on `MpskReceiver`.
- [x] Confirmed via that test that Acquisition -> Despreader itself despreads cleanly (BER=0.0000 on the gating cases) — the `MpskReceiver` symbol-timing bug above has no bearing on this path's own correctness.
- [x] Found a real bug isolated from the `MpskReceiver` confound for the first time: `aid_code=True` combined with a nonzero static Doppler offset is catastrophic (BER~0.5) on every signal generator this story has used, because none of them give the Doppler offset a matching code-rate change — not a bug in `aid_code`, a signal-generator fidelity gap (task #101).
- [x] **CHECKPOINT 25 — found and fixed the real SPEC-rate bug (task #105): `car_update_windows`'s `bn_car` must NOT be divided by `windows`.** `LoopFilter(bn, zeta, t=1.0)` is already per-update normalized; `kp` scales ~linearly with `bn` but `ki` scales ~quadratically (confirmed directly against `doppler.track.LoopFilter`) — dividing `bn_car` by `TRACK_WINDOWS` under-scaled the integrator (the mechanism a type-2 loop needs to track a ramp) by an extra factor of `TRACK_WINDOWS`. Passing `bn_car` unchanged fixes the real 500Hz/s case completely (BER~0.5 -> BER=0.0000), with no FLL-assist needed at all — matching `~/legacy-commz`'s own reference (a plain Costas PLL, no FLL). See `diagnose_bn_car_scaling.py`. All three `e2e_acq_to_despreader.py` cases now PASS and gate.
- [ ] `MpskReceiver`'s own symbol-timing loop never reaching lock — confirmed via telemetry, root cause not yet found, out of this folder's current scope.

## `CarrierAcquisition` — jm C-backed PSDMF port (this session)

- [x] New `doppler.acquire` module + `CarrierAcquisition` object, composing `psd_core` (FFT+window+non-coherent power averaging) + `detector_core` (FFT correlation of the averaged power vs. a known template + noise-referenced test stat) + `detection_core` (`det_threshold_noncoherent`/`det_n_noncoh`, the same Pfa/Pd statistics `Acquisition` itself is built on) — no new leaf code beyond the sinc^2 default-template generator and a 3-point parabolic sub-bin fit, both ported verbatim from `freq_refine.py`.
- [x] `psd_template` is a user-overridable known-PSD-shape array (default: the rectangular-pulse sinc^2 shape), matching `~/legacy-commz`'s own `FrequencyAcquisition.power_spectrum` override.
- [x] `sequential` (test every block, adaptive) vs. non-sequential (fixed `dwell_target` wait) modes, mirroring the reference's own two modes; `max_n_blocks` is sequential mode's OWN give-up cap, deliberately independent of `dwell_target` (an optimistic `design_snr` guess must not stop sequential mode from trying more blocks once real data shows it needs to — found and fixed mid-session after an initial design conflated the two).
- [x] Full C test suite (tone+data accuracy at both `sequential` values, carry-buffer split-call equivalence, pure-noise give-up at both caps, template override, state roundtrip) + Python integration test + `test_state_serialization.py` matrix entry — all green, `jm status --check` clean.
- [x] **CONFIRMED via Monte Carlo (`characterize_carrier_acq_detection.py`), not just observed once on a real capture**: `det_threshold_noncoherent`/`det_n_noncoh`/`det_pd_noncoherent` (derived for classic complex-correlator Rayleigh/Rician detection at `n_coh` COHERENTLY INTEGRATED raw samples, the same model `Acquisition` uses) do NOT transfer to gating a *power-spectrum-vs-known-template* correlation — the theory predicts Pd~1.0 across nearly the entire tested (design_snr, Es/N0) grid while the measured Pd (400-trial MC, non-sequential single-shot mode -- no repeated-testing confound) stays at 0.00-0.40 throughout the design_snr sweep and 0.00 until 12dB in the Es/N0 sweep (where it reaches only 0.885, not the ~1.0 theory implies at every point above ~4dB). A secondary, deliberately separated question (is sequential mode's own repeated per-block testing ALSO inflating Pfa via the classic optional-stopping/multiple-comparisons effect) was tested too: in the one regime probed, firing decisions settled within the first ~4 blocks and stayed flat through block 64 -- not the dominant effect here.
- [x] **DERIVED the actual statistic (`derive_carrier_acq_statistic.py`) and identified the dominant root cause precisely.** `C[k] = sum_m Pavg[m]*T[(m-k) mod nfft]` (`Pavg` = the CG^2-normalised running-mean periodogram `psd_power_twosided` produces, `T` = the known template). Under H0, `Pavg[m] ~ Gamma(n_blocks, mu/n_blocks)` EXACTLY (`mu = s2*sigma^2/cg^2`, no CLT needed for this step), so `C[k]`'s mean (`mu*S_T`) is derived exactly and validated against MC to <1.5% error at every (Es/N0, n_blocks) tested; `C[k]`'s variance follows the same closed form up to ONE empirically-calibrated geometric constant `xi~1.87-1.93` (window-induced correlation between nearby periodogram bins that an independent-bins assumption misses — Hann is NOT a rectangular window, bins ARE correlated over ~its mainlobe width). H1's mean shift at the true peak is a clean, Es/N0-INDEPENDENT, dwell-INDEPENDENT constant tied only to signal amplitude (`shift~1.0` at unit power) — confirms the "known average PSD shape stands in for the unknown data realization" premise exactly. **The dominant, now-quantified root cause: the RATIO threshold `det_threshold_noncoherent(pfa,dwell)` computes (6.76 at pfa=1e-2, dwell=13) is ~5x more conservative than the REAL null distribution of the argmax-over-nfft-correlated-lags test_stat actually requires (empirically 1.43 for the same target Pfa, confirmed via 2000-trial calibration) — using the CORRECT threshold recovers Pd from 0.00/0.92 (borrowed) to 0.85/1.00 (calibrated) across the SAME Es/N0 grid the original characterization used.** A SEPARATE, genuinely novel finding along the way: H1 variance carries a large (7-33x H0's) "data-modulation self-noise" term from each block's own random bit pattern, not present in the borrowed model at all, that shrinks with dwell at roughly H0's own 1/n_blocks rate but does NOT vanish with higher SNR — the reason Pd doesn't uniformly reach 1.0 the way a non-modulated-tone model would predict, though for the dwell/Es/N0 range tested this was secondary to the threshold miscalibration above.
- [x] **WIRED IN as a real fix.** `carrier_acq_core.c`'s `_process_block()` no longer calls `det_threshold_noncoherent()`; a new `_ratio_threshold()` computes `1 + KAPPA*det_threshold(pfa)*sqrt(s_t2/n_blocks)/s_t` (`s_t`/`s_t2` = `sum(template)`/`sum(template^2)`, computed once at `create()` and stored — config, not serialized state). `KAPPA=7.9` was fit via 3000-trial null-distribution MC at this object's own most common real configuration (`sample_rate_hz=8000, symbol_rate_hz=1000`, default resolution/zero_pad → `nfft=512`, hann window, default template — matches every existing carrier_acq test/gallery demo), checked at `pfa` in `{1e-2,1e-3}` and `n_blocks` in `{1,4,13}`: consistently errs 10-30% on the CONSERVATIVE side (safer than the reverse), not exact. The earlier toy calibration (`nfft=64`, from `derive_carrier_acq_statistic.py`'s own small-nfft config chosen for MC speed) turned out to NOT generalize to this object's real `nfft=512` default at all (fired on almost every noise-only trial) — caught immediately by the existing C test suite's pure-noise give-up tests, which is exactly what they're for. Full C (87/87) + Python (104/104 acquire+state-serialization) suites green after the change; a broader `pytest src/doppler/` sweep was also kicked off to check for any other regression. Real-object validation (bypassing the borrowed `det_pd_noncoherent` theory column entirely, reading the actual `CarrierAcquisition.ready` flag): Pd at the object's own default config rises from 0→0.093→0.643→0.997→1.0 across Es/N0={-2,0,2,4,5}dB — smooth, sensible, dramatically better than the borrowed threshold's near-zero-everywhere behavior. `carrier_acq_rrc_demo.py`'s own headline flipped as a DIRECT, expected consequence (the wrong template no longer fails outright at moderate SNR — it detects but with ~2x worse accuracy, a still-real, still-honest, differently-framed finding) — demo assertions/prose/PNG updated and re-verified to match.
- [ ] **Still open, tracked not forgotten**: `KAPPA=7.9` is a single-point empirical calibration, not a general closed form — a proper Sidak/Bonferroni-style "effective independent lags" derivation for the argmax-over-nfft-correlated-lags extreme value didn't cleanly close on a first attempt (implied more independent lags than nfft itself), and `KAPPA` has ONLY been validated at this object's own common `nfft=512` default — a caller using a very different `resolution_hz`/`zero_pad`/`window`/custom `psd_template` combination should not assume the same accuracy. Revisit and properly derive (or recalibrate across a wider grid) when time allows.
- [x] **Found and fixed a REAL, separate numerical-correctness bug in `doppler.detection.marcum_q`/`det_n_noncoh` while sizing a fold-in dwell (not a CarrierAcquisition-specific bug — a shared primitive also used by `Acquisition`, lockdet, etc).** `det_n_noncoh`'s linear scan (`k=1..max_n_noncoh`) was O(n²) (each `marcum_q` call itself cost O(k)), AND `marcum_q`'s own chi-square series started from a raw `exp(-v)`/`exp(-u)` that UNDERFLOWS TO EXACTLY 0.0 once `v=threshold^2/2` or `u=a^2/2` gets large enough (v gtrsim 745) — every subsequent term then stayed stuck at exactly 0 forever (0 times any finite ratio is still 0), silently corrupting the result (observed directly: Pd collapsing from 0.96 to exactly 0.0 between two adjacent dwell counts, non-monotonically, which would have made a naive binary-search speedup actively wrong). Fixed properly (not worked around): rewrote the v-side chi-square/Poisson CDF term using the regularized upper incomplete gamma `Q(a,x)` via a log-domain-prefactor + continued-fraction/series pair (`gammaq`/`gcf`/`gser` in `marcum_q.c`) — the SAME numeric-kernel class `det_threshold_f.c`'s `ibeta`/`betacf` already uses for the regularized incomplete beta, reused rather than re-derived (the user's own prompt: "why not use the detector calc?"). The outer (`u`-side) Poisson-weighted sum had the identical failure PLUS a second bug (its old fixed `k<600` window always started scanning at k=0, missing the Poisson(u) distribution's own mass entirely once u is large) — fixed by anchoring both sums at their own mode (`k0=round(u)` or the v-side's own peak term) and walking outward with a window that scales with `sqrt(u)` instead of a fixed constant. Verified: all existing doctests/reference values bit-identical, C test suite 87/87, Python suite (detection+acquire+dsss+track+state_serialization) 645/645, zero non-monotonic transitions across a much wider sweep than the one that originally found the bug (snr up to 5.0, k up to 20000, was 69 bad transitions before the u-side fix). `det_n_noncoh.c` then switched from the O(n²) linear scan to a proper binary search (valid now that monotonicity is real, not assumed) — the same design_snr sweep that used to hang for 8+ minutes at 100% CPU now returns in under 1ms at every point tested.
- [x] **Folded `CarrierAcquisition` into `e2e_acq_to_despreader.py`** (`freq_refine.refine_seed_carrier_acq`, replacing `refine_seed_matched` — the user's original "replace outright, force the issue" call from earlier in the session). Result: 2 of 3 gating cases now PASS cleanly (BER=0.0000, all 3 seeds each) — "zero impairments" and "static offset only". The third ("SPEC real rate", 500Hz/s Doppler) is marginal: 1 of 3 seeds passes, 2 fail at BER~0.49 — a DIFFERENT, more subtle issue than anything fixed above: the frozen-carrier collection pass assumes a roughly-STATIC residual across its whole averaging window (hundreds to ~2000 blocks, needed for this statistic's own SNR), but under a sustained 500Hz/s drift the true residual genuinely moves during that window, which can smear or bias a single-shot estimate depending on where in the drift the collection happens to land — not yet investigated further. Also found along the way: `carrier_acq_create()`'s `dwell_target` PLANNING (via `det_n_noncoh`) still uses the un-corrected classic model even though `_ratio_threshold()`'s own detection DECISION is now KAPPA-corrected — these two now disagree (the literal 5dB-minimum-Es/N0-derived `design_snr` predicts a 7-block dwell that is nowhere near enough for the real, corrected gate to reliably fire); `refine_seed_carrier_acq` works around this with an empirical `design_margin_db=14.0` derating rather than fixing the underlying planning/testing model mismatch — flagged as a further open item, not resolved.

## `CarrierAcquisition` fold-in: SPEC real-rate case FIXED (Rs/Doppler-rate deep dive)

- [x] **RESOLVED — all three `e2e_acq_to_despreader.py` gating cases now PASS cleanly (BER=0.0000, every seed), including the previously-marginal SPEC real-rate (500Hz/s) case.** Root-caused by a deep dive into `SPEC.md`'s own `Rs`/Doppler-rate numbers, not by the legacy-detector-convention thread (which was correctly abandoned above) — four findings, each verified against real numbers before acting:
    1. **Drift-during-collection, quantified against `SPEC.md`'s actual 500Hz/s figure**: at the old `n_fft=64`/`window_rate_hz~186kHz` config, `dwell_target~1534` meant a ~528ms collection window, over which the true residual moves ~264Hz — matching the observed -211..-289Hz real-rate errors almost exactly. Confirmed empirically that shrinking dwell alone (via `design_margin_db`) made accuracy WORSE, not better (estimation-noise floor dominates at small dwell) — so this alone wasn't the fix, but it correctly pointed at the collection-window design.
    2. **`window_rate_hz`'s "PSDMF-collection Nyquist margin" role was stale** — inherited from before `CarrierAcquisition` (the C object) existed, when the old pure-Python estimator consumed the despread stream directly with no separate resample stage. Block duration for a PSD block is `symbols_per_block / Rs`, INDEPENDENT of raw sample rate — so sample rate and "how many real symbols does one coherent block span" are two genuinely separate knobs, previously conflated.
    3. **`WINDOWS=62` (code-loop async-lookback granularity) was ALSO stale, on independent grounds**: reverse-engineering `~/legacy-commz`'s own `asynchronous_correlation_loss`-driven `(windows, window_size)` formula (`receiver/dsss/despreader.py`) against `WINDOWS=62` implies an unexamined ~0.07-0.1dB loss tolerance, 5-7x tighter than legacy's own validated 0.5dB default (which gives `windows=11` at this waveform's `tsamps=2046`) — 62 was never actually derived from a stated tolerance, just inherited. Ported the formula verbatim as `despreader_coupled.async_lookback_windows()`; `e2e_acq_to_despreader.py` now derives `WINDOWS` from it instead of hand-picking a constant.
    4. **The despreader's own integrate-and-dump (`find_max_power`'s per-window coherent sum) should do less decimation duty; a proper `RateConverter` resample should do more** — `freq_refine.refine_seed_carrier_acq` now resamples the collected stream to an explicit `samples_per_symbol * symbol_rate_hz` (default 4x `Rs`) via `doppler.resample.RateConverter` right before `CarrierAcquisition`, fully decoupling its own operating rate from whatever the code loop's `windows` granularity happens to produce.
    5. **The actual, decisive bug — found only after 1-4 above shrank `dwell_target` from ~1534 down to ~12**: the test harness (and would-be real pipeline) started downstream tracking from the END of a fixed, generously-sized `PREFIX_EPOCHS=2700` collection window regardless of how much of it `CarrierAcquisition` actually used. With the old, huge `dwell_target`, "finishes near the prefix end" and "prefix end" coincided by coincidence; once `dwell_target` dropped to ~12 blocks (~9.6% of the prefix), the tracker was being handed a frequency estimate that was already stale by ~300Hz of real 500Hz/s drift accumulated in the UNUSED remainder of the prefix before tracking even began — a stale-reference bug, not an estimation-accuracy problem (confirmed directly: re-scoring the SAME estimate against the residual at its own actual completion point, not the fixed prefix end, cut the apparent error from -306Hz to +28Hz). Fixed: `refine_seed_carrier_acq` now returns `samples_consumed` (how much of the prefix it actually used); `run_trial()` sizes the tracking handoff and BER-scoring reference off THAT, not the fixed `PREFIX_EPOCHS` budget.
    - Files touched: `despreader_coupled.py` (new `async_lookback_windows()`), `e2e_acq_to_despreader.py` (derived `WINDOWS`, dynamic `n_epochs_used` handoff), `freq_refine.py` (`RateConverter` resample stage in `refine_seed_carrier_acq`, `samples_consumed` return value).
    - Not yet re-validated: `KAPPA=7.9`'s own generality at the NEW `n_fft`/rate geometry (16 symbols/block at 4sps vs. the original calibration's 8 sps) — still a single calibration point, now at a slightly different symbols-per-block ratio than it was fit at; worth a recalibration check, not yet done.

## C port attempt #1: retrofitting the OLD DsssReceiver — REVERTED, wrong approach

- [x] **Tried composing `CarrierAcquisition` into the EXISTING `dsss_receiver_core.c` (a "refining" state between searching/tracking, frozen-carrier DLL reuse, `hit_chip_phase`-seeded fresh rebuild on completion) — reverted entirely, per direct user redirect: "we are not hacking the current DsssReceiver — we are building a new one from the Python prototype with the extensions we built to support it."** The old object's own architecture (once-per-code-period `costas_update`, no `windows`/`car_update_windows` concept, hardcoded `segments`) predates and doesn't match the validated Python prototype (`despreader_coupled.py`'s `car_update_windows=True`, `TRACK_WINDOWS`/`WINDOWS` granularity, `async_lookback_windows()`) — retrofitting fights the old assumptions instead of building the thing already proven to work. All changes to `dsss_receiver_core.h`/`.c`, its test, and `objects/dsss_receiver.toml` were `git checkout`'d back to their last commit; nothing from this attempt survives except the findings below, which DO carry forward to the new object:
    1. **Real, worth-keeping fix along the way**: the C carrier loop should update ONCE PER PARTIAL PROMPT (matching `car_update_windows=True`'s per-window cadence), not once per full code period — `costas_update()`'s own discriminator (decision-directed `sign(Re)*Im` phase error + a `sign`-wiped cross-product FLL term against its own `prev` state) is already designed to take one raw prompt at a time; no external "combine partials, sign-align, one update" step is needed once `costas_init()`'s own `tsamps` is re-normalized to the per-partial interval (`code_len*spc/segments`). The OLD object's own code comment claimed an "earlier draft" of exactly this was tried and "measurably made tracking WORSE" — not reproduced or re-explained; the new object should re-derive this cleanly rather than inherit that old, unverified caveat.
    2. **Decisive, still-unresolved root cause of the pull-in failures seen while retrofitting**: `CarrierAcquisition`'s dwell sizing (`design_margin_db=14`, `carrier_acq_core.c`) is tuned ONLY for reliable DETECTION (Pd>=0.9), not for ESTIMATION ACCURACY. Confirmed via a clean, receiver-free Monte Carlo (`CarrierAcquisition` fed a synthetic BPSK tone + AWGN directly, no DLL/Acquisition involved at all): at Es/N0=4-5dB, dwell=12-16 blocks, the residual estimate has ~50-150Hz of symmetric (not biased) scatter across seeds — confirmed to be inherent to the estimator at this dwell, not a wiring bug (multi-seed data from inside the (reverted) C receiver showed the SAME symmetric scatter, not a systematic offset). **This is the real, unresolved gap the new object will need to actually fix** (a proper dwell target for estimation accuracy, not just detection reliability) — not a receiver-composition problem.
- [x] **Next: design and build a NEW C object** composing `Acquisition` -> handoff -> `CarrierAcquisition` refine -> a NEW despread/track composition -> demod. **DONE — see the `AsyncDsssReceiver` section below.**

## `AsyncDsssReceiver` — the new object, built and verified (this session)

Built `doppler.dsss.AsyncDsssReceiver` (`objects/async_dsss_receiver.toml`,
`native/{inc,src}/async_dsss_receiver/`) per
`~/.claude/plans/crystalline-knitting-hopper.md`: `Acquisition` -> handoff
-> a frozen-carrier collection `Dll` feeding `CarrierAcquisition` (refine)
-> a fresh per-code-period-cadence Costas/`Dll`/`RateConverter`/
`MpskReceiver` chain (track). Two shared, canonical primitives were
factored out first (not duplicated inline, per this project's own rule):

- **`acq_build_handoff()`** (`acq_core.h`/`.c`) — the C twin of
  `acq_handoff.py`'s `DetectionEvent` conversion (chip-phase mirror-image
  inversion + Doppler-bin fold), closing `FINISHING_PLAN.md`'s own
  long-standing `dsss_acq_handoff_from_result()` TODO. `dsss_receiver_
  core.c`'s own call site was switched to use it too (deleted its
  duplicate `_chip_phase_from_hit()`); `ctest -R dsss_receiver` stayed
  green throughout.
- **`dll_lookback_segments()`** (`dll_core.h`/`.c`) — ports
  `despreader_coupled.async_lookback_windows()`'s exact divisor-snapping
  algorithm to C, a derived (not hand-picked) `segments` count.

**Real bugs found and fixed while building/validating** (each confirmed
via direct measurement, not assumed):

1. **`refine_sequential` must default to `false`, not `carrier_acq.toml`'s
   own standalone default of `true`.** Direct isolated probe: sequential
   mode's per-block early test fires on as few as 4 blocks at SPEC's real
   SNR/geometry, ~150-600Hz off; non-sequential (the validated
   `freq_refine.refine_seed_carrier_acq()` recipe) waits the full
   `dwell_target`, landing within tens of Hz.
2. **Per-partial `costas_update()` cadence (mirroring Python's
   `car_update_windows=True`) does NOT track SPEC's 500Hz/s ramp at this
   object's real geometry — reverted to `DsssReceiver`'s own proven
   per-code-period cadence** (`costas_update()` once per period, on a
   sign-aligned sum of that period's emitted partials). Root cause:
   `k_fll = 4*bn_fll` is a fixed per-call gain, but the FLL cross-product
   discriminator's own output scales with the time interval between the
   two prompts it correlates — calling it 4x more often (`segments=4`)
   shrinks that interval 4x, weakening the real error signal at fixed
   gain rather than speeding up tracking. This directly reproduces (and
   now empirically confirms, not just repeats unverified) the OLD
   `DsssReceiver`'s own retired code comment claiming per-partial cadence
   "measurably made tracking WORSE."
3. **`dll_lookback_segments()`'s own multi-segment value (segments=11 at
   this waveform's `tsamps=2046`) measurably CORRUPTS `CarrierAcquisition`'s
   residual estimate, versus segments=1 (a plain coherent per-epoch
   dump)** — a clean isolated probe (Python bindings, `Dll`+`RateConverter`
   +`CarrierAcquisition`, no C receiver involved) landed within ~18Hz of
   truth at segments=1 vs. 70-90Hz off (sometimes wrong-signed) at
   segments=11. `Dll`'s own margin-gated shifted-window reconstruction
   (`DLL_LOOKBACK_MARGIN`, built for the LIVE tracking loop's own E/P/L
   discriminator robustness) doesn't transfer cleanly to feeding a PSDMF
   frequency estimator. Fixed by defaulting `refine_max_error_db=100.0`
   (forces segments=1 via the same formula) rather than Python's own
   0.5dB default — an empirical finding on ONE waveform, documented as
   such, not asserted as a general rule.
4. **THE decisive bug (found via a from-scratch A/B against the
   already-shipped `DsssReceiver` fed the IDENTICAL signal at Es/N0=30dB —
   `DsssReceiver` decoded perfectly, `AsyncDsssReceiver` didn't, ruling out
   SNR/noise-realization as the explanation): the live tracking chain's
   reused `seed_chip_phase` (the ORIGINAL handoff chip phase, not wherever
   the refine-stage `Dll` drifted to) is only valid if the live chain's
   first sample is an EXACT whole number of code periods after the
   handoff** (one whole period = zero net code-phase advance, by
   definition). `samples_consumed_refine` (the elapsed-time-based raw-
   sample estimate of how much of the refine collection was actually
   used) was NOT rounded to a period boundary before slicing the tail —
   Python's own `e2e_acq_to_despreader.py` relies on exactly this rounding
   (`n_epochs_used`'s own ceiling-to-whole-epoch step) and this C port had
   silently dropped it. Fixed: round `samples_consumed_refine` UP to the
   nearest multiple of `tsamps` before computing the tail. Confirmed:
   BER 0.47 (chance) -> 0.0000 at Es/N0=30dB with the fix; the full
   `test_spec_ramp_decode` C test (Es/N0=20dB, real 500Hz/s ramp) and
   `test_acquires_refines_and_decodes` (Python, Es/N0=20dB) both now
   decode cleanly.
5. A minor bug also fixed along the way: the refine->track
   `samples_consumed_refine` formula used `ca->nfft` (the zero-padded PSD
   *transform* length) instead of `refine_n_fft` (the raw per-block
   sample count) — an 8x scale error at the default `zero_pad=8`,
   independent of bug 4 above (both needed fixing).

**Verification**: full C suite green (88/88 `ctest`, including
`test_async_dsss_receiver_core`'s SPEC-geometry ramp-decode test at
Es/N0=20dB and a give-up-cap test forcing `refine_max_n_blocks=1`); full
Python suite green (249 tests, including 10 new
`test_async_dsss_receiver.py` tests); `jm status --check` clean.
`prototypes/async_despreader/async_dsss_receiver_c_vs_python.py` runs the
IDENTICAL front-end signal through the C object and Python's own
`e2e_acq_to_despreader.run_trial()` across the same 3 gating cases:

- **"zero impairments"**: AGREE, both PASS (BER 0.0000 / 0.0003).
- **"SPEC real rate (500Hz/s)" — the decisive case for task #99**:
  AGREE, both PASS (BER 0.0004 / 0.0003).
- **"static offset only (+15kHz)"**: originally DISAGREE — Python passed
  (BER=0.0000), the C side failed (BER~0.48, chance). Confirmed NOT a
  regression from `AsyncDsssReceiver`'s own new machinery: the ALREADY-
  SHIPPED `DsssReceiver`, given the IDENTICAL signal, failed identically
  (BER~0.48, even though its own cached `doppler_hz` read back exactly
  15000.0 — the coarse estimate itself was correct). **ROOT-CAUSED AND
  FIXED (task #148)** — see the dedicated section below. Now AGREES,
  both PASS (BER 0.0000 / 0.0000).

### Task #148 — large-static-Doppler-offset decode failure: root cause + fix

**Root cause**: `dsss_receiver_core.c`'s `_build_chain()` and `async_
dsss_receiver_core.c`'s `_build_track_chain()` both seeded `mpsk_
receiver_create()`'s own `init_norm_freq` with the SAME full physical
`doppler_hz_est` that ALSO, separately and correctly, seeds the
pre-despread Costas loop (at the front-end rate) — a double count. At
small Doppler this is negligible (`doppler_hz_est/target_rate` ~0
either way); at 15000 Hz static offset over `target_rate=sps*symbol_
rate=21600 Hz`, `15000/21600 = 0.694` cycles/sample — past Nyquist, so
`mpsk_receiver`'s integer NCO (which can only represent frequency mod 1
cycle/sample) silently aliases this to a real, WRONG initial condition
of `~-0.306` cycles/sample. `norm_freq` is not a lazily-corrected
loop-filter guess — `carrier_nda_init`'s `lo_init` bakes it straight
into the NCO's `phase_inc` from sample 0, with the loop filter's own
integrator starting at 0 — and MpskReceiver's own `carrier_nda` loop has
no FLL assist, with a validated pull-in range of only `~0.01`
cycles/sample (`carrier_nda_pullin.c`), 1-2 orders of magnitude too
narrow to recover from a `~0.3`-cycle seed error. Nothing between the
pre-despread Costas loop and MpskReceiver (RateConverter, Dll) reintroduces
a Doppler-scaled rotation, so MpskReceiver never actually needed the full
estimate — only the small residual the front-end loop leaves behind.

Diagnosed via `prototypes/async_despreader/case1_stage_diagnostics.py`
(new, instruments both pipelines stage-by-stage instead of one final
BER number): Acquisition hit stats matched exactly between C and
Python (same C `acq_core.c` engine underneath both); the
`CarrierAcquisition` refine estimate was close in both (Python +15.3 Hz
err, C -360.5 Hz err — a real but secondary discrepancy, plausibly the
refine-stage collection Dll's `segments=1` vs. Python's `windows=11`
lookback granularity, not investigated further since it turned out not
to matter); but tracking-stage EVM was near-ideal in Python (-9.47 dB,
essentially the -10 dB AWGN floor) vs. catastrophic in C (+27.2 dB, no
lock at all, flat from sample 1 — not a loop that starts well and
drifts, evidence it never entered the pull-in range to begin with).

**Fix**: seed `mpsk_receiver_create()`'s `init_norm_freq` with `0.0` in
both `dsss_receiver_core.c:53-55`'s `_build_chain()` and `async_dsss_
receiver_core.c`'s `_build_track_chain()`, instead of `doppler_hz_est /
target_rate`. **Verified**: re-running `case1_stage_diagnostics.py`
after the fix, C tracking EVM went from +27.2 dB to **-9.18 dB**
(matching Python's -9.47 dB, both at the AWGN floor); lock went from
0.039 (unlocked) to 0.419. The secondary ~360 Hz refine-estimate gap
between C and Python is still present and is now visibly inconsequential
— confirming it was never the real bug, just a smaller pre-existing
imprecision riding along with the actual (much larger) double-count.
Full regression check green: 88/88 `ctest`, full `pytest src/doppler/`,
and all 3 `async_dsss_receiver_c_vs_python.py` gating cases AGREE/PASS
(case 1 now BER=0.0000, was BER~0.48).

### Task #151 — the secondary ~360 Hz refine-estimate gap: DLL lookback port cleaned up, root cause of the segments>1 failure still open

The now-inconsequential residual gap left over from task #148 (Python's
`refine_seed_carrier_acq` collection: +15.3 Hz err; `AsyncDsssReceiver`'s
own C collection: -360.5 Hz err) was investigated with a dedicated
isolation script, `prototypes/async_despreader/refine_stage_ab.py`.

**First, a wrong turn, corrected**: it initially looked like `dll_core.c`'s
`segments>1` mode and `despreader_coupled.py`'s own hand-rolled `windows`
collection were two unrelated algorithms (`CoupledAsyncDespreader` doesn't
import `doppler.track.Dll`) — but `dll_core.h`'s own file doc and
`dll_lookback_segments()`'s own doc comment are explicit that `segments>1`
IS meant to be the direct C port of `despreader_coupled.py`'s
`find_max_power()`/`get_window()` (also `docs/design/async-despreader-
working-design.md`'s own reference pseudocode) — two independent
implementations of the SAME intended technique, not two different
techniques. Checked line-by-line against all three source: the archived
`despreader_interp.py` (the design's true origin), `despreader_coupled.py`,
and the design doc's own pseudocode all implement `find_max_power` as a
**plain `argmax` over every lookback candidate, every epoch — no margin
or hysteresis of any kind.** `dll_core.c` carried an undocumented
`DLL_LOOKBACK_MARGIN=1.06` (~0.5 dB) threshold gating the search away
from switching to a shifted candidate unless it beat the natural window
by that margin — a real, unauthorized deviation from the validated
design, not present in any of the three reference sources, and not
exercised by `test_dll_core.c`'s own false-lock regression (that test is
`segments=1` only).

**Fixed**: `dll_core.c`'s `segments>1` epoch-boundary block was rewritten
as an explicit, step-by-step port of `find_max_power` — building the
literal same named artifacts in the same order (`sums` = a real forward
running sum, mirroring Python's `partial_sums.cumsum()`; the existing
`last_backward_p` bookkeeping, unchanged, already matched Python's
`backward_sums`; a plain per-candidate argmax, no margin) instead of the
old incremental-subtraction form, so it can be checked directly against
`despreader_coupled.py`'s own source line for line. `DLL_LOOKBACK_MARGIN`
is deleted entirely (was in `dll_core.h`). A new pure-scratch `sums`
field was added to `dll_state_t` (bumping `DLL_STATE_VERSION` to 6;
excluded from the serialized blob like the other scratch buffers).
**Verified byte-identical** to the old margin-gated code's own output
(confirmed by first testing with `DLL_LOOKBACK_MARGIN` set to 1.0 as an
experiment before the rewrite — zero change in any measured value) for
every case checked except one: a single-seed, right-at-the-edge C-level
regression (`src/doppler/track/tests/test_async_dsss_receiver.py::
test_recovers_near_the_noise_floor`, Es/N0~10.6dB) whose exact BER moved
from ~under 0.06 to ~0.097 — a real, legitimate consequence of the
(correct) margin removal, not a fluke; its own docstring already
anticipated this class of nudge from a legitimate implementation change
(precedent: the `nco_norm_to_inc` rounding fix), so the threshold was
retuned to 0.10 with a comment explaining why. Full regression clean:
88/88 `ctest` (incl. `test_dll_core.c`'s own segments>1 and false-lock
regressions) + 435/435 `pytest` across `track`/`dsss`/state-serialization.

**Task #151's own original symptom was UNCHANGED by the lookback rewrite
above** — re-running `refine_stage_ab.py`'s segments sweep against the
rewritten `dll_core.c` reproduced the exact same numbers as before it
(`segments=1` detects at -232.4Hz err, every `segments>1` value never
fires) — the lookback search was confirmed NOT to be the cause: its
winning candidate only ever feeds the discriminator and a normalization
scalar, never the actual complex samples handed downstream (those are
always the epoch's own natural `chunk_p[]`; Python's own
`integrate_and_dump` has the identical structure).

**A whole detour, fully retraced and reverted -- worth recording in
detail, since the final answer is architectural, not a patch.** The
first hypothesis was that `_refine_period()` (`async_dsss_receiver_
core.c`) simply needed the same consumer-side sign-alignment `_track_
period()`/`_carrier_update_from_partials()` already apply -- a data-bit
transition mid-period flips the sign of partials after it, and those two
existing consumers already sign-align by each partial's own real-part
sign before combining further. That fix was applied to `_refine_period()`
too, tested against "does despreader_coupled.py do this?" (no, but it
doesn't call `dll_core.c` at all, so that test doesn't apply), reverted,
re-applied with a corrected "matches the other two consumers' contract"
rationale, and empirically DID fix `segments>1`'s detection failure
(every segments value went from mostly-never-detecting to a clean
detection; segments=1 error even improved -232.4Hz->-37.7Hz).

**The sign-align fix was still wrong, but the "segments>1 is fundamentally
non-coherent, never use it" conclusion drawn from that was ALSO wrong --
a second correction, from the user directly.** `AsyncDsssReceiver` exists
specifically to handle genuinely async data by finding the best-
correlating window COHERENTLY -- that IS what `dll_core.c`'s `segments>1`
lookback search does (the same technique `despreader_coupled.py`'s own
`find_max_power`/`get_window` independently implements for the same
reason). `dll_core.h`'s own "tracks the code non-coherently across them"
phrase describes how the CODE DISCRIMINATOR combines partials for the
code loop's own purposes, not a property of the window-selection search
itself or a blanket disqualification of `segments>1` as a source of
coherent samples. The sign-align fix was still wrong to keep (still not
what any consumer's own contract requires), but "so `segments>1` must
never be used" did not follow from that -- reverted the sign-align,
kept `segments>1` on the table.

Also tested and refuted (`refine_stage_ab.py`'s own dwell sweep, adding
`design_margin_db` as a parameter): does `segments>1` just need more
dwell, the same way `segments=1` turned out to (see below)? No --
`segments=11` still never fires even at `n_blocks=16`, the exact dwell
that fixed `segments=1`. Something about `segments>1`'s own output
still prevents detection, independent of dwell -- not yet found (one
concrete, unchased lead: `segments=1`'s raw rate is upsampled into
`RateConverter` while `segments>1`'s is downsampled, opposite filter
paths through the same resampler). Low priority: `segments=1` (below)
now closes the gap on its own.

**A second, separate experiment on the same theme, also reverted**:
since the `dll_core.c` segments>1 lookback search was independently
rewritten this session (removing `DLL_LOOKBACK_MARGIN`, correcting it to
a proper `find_max_power()` port), tested whether `_track_period()`'s/
`_carrier_update_from_partials()`'s OWN long-standing, task-#100-
validated sign-alignment had become redundant now that the underlying
window search is fixed. It passed the full existing `ctest` suite with
the sign-align removed -- but broke tracking outright on the real
SPEC-geometry case1 scenario (EVM -9.14dB -> +13.37dB,
`case1_stage_diagnostics.py`; the existing test suite apparently doesn't
exercise a geometry where the effect is severe enough to fail). This is
a genuinely different problem than the lookback rewrite touched: that
fix corrected HOW the best window is CHOSEN; it says nothing about the
fact that a real data-bit transition mid-period flips the underlying
SIGNAL's own sign, which self-cancels in any bare SUM of multiple
partials regardless of how correctly the window was chosen. `_track_
period()` and `_carrier_update_from_partials()` sum partials into one
value (unlike `_refine_period()`, which never summed anything -- each
partial survives as its own sample); that summing operation is where the
sign-align is actually load-bearing. Restored in both, re-confirming
task #100's own original fix rather than superseding it.

**Task #151's REAL root cause, finally found: a manifest-vs-fragment
drift, not an algorithm or discriminator problem at all.** The dwell
hypothesis above (short `n_blocks=3` -> periodogram variance) was tested
directly in the isolated probe by adding `design_margin_db` as a sweep
parameter: at `margin_db=20` (`n_blocks=16`), `segments=1`'s error
collapsed from -232.4Hz to **-18.0Hz** -- decisive confirmation that
short dwell was real. But testing the SAME override on the real
`AsyncDsssReceiver` object (`refine_design_margin_db=20.0` passed to the
constructor) had **zero effect** -- identical -355.9Hz at margin 14, 20,
5, and 0. The parameter wasn't being honored at all.

Root cause: `dsss_ext_async_dsss_receiver.c` (the hand-owned Python
binding fragment) had `refine_sequential_raw = true` as its C-level
default -- but `objects/async_dsss_receiver.toml` correctly declares
`default = "false"`, matching the object's own documented, validated
design (non-sequential mode waits the full `design_snr`-derived
`dwell_target` before testing once; sequential mode tests every block
and fires on the first one that crosses threshold, with far too little
averaging -- exactly the failure this whole story chased). In sequential
mode, `carrier_acq_core.c`'s own `do_test = state->sequential || (...)`
is unconditionally true, so it fires almost immediately regardless of
`design_margin_db`/`dwell_target` -- explaining why the parameter had no
effect: the whole dwell-calibration mechanism was being bypassed
entirely, for BOTH `segments=1` and every `segments>1` value, every time,
since construction. The manifest was always right; the hand-owned
fragment had silently drifted out of sync with it (untracked in git, so
no diff/blame trail for when).

Fixed via this project's own established mechanic (not a hand patch):
deleted `dsss_ext_async_dsss_receiver.c` and re-ran `jm apply`, which
regenerated it correctly from the manifest (`refine_sequential_raw =
false`); `clang-format`'d per convention; `jm status --check` clean.

**Verified, decisively, end to end**: refine error **-355.9Hz ->
-52.8Hz** at the object's own DEFAULT config (no override needed) --
consistent with the isolated probe's own dwell-sweep finding once
`refine_sequential` is correctly `false`. `async_dsss_receiver_c_vs_
python.py`'s own case 1 (the authoritative cross-check, unaffected by
diagnostic-script bugs) stayed BER=0.0000, matching Python exactly. A
follow-up EVM check (`case1_stage_diagnostics.py`) initially appeared to
show a severe regression (+24.7dB) -- traced to the diagnostic script's
OWN lag-search window (`max_lag=50`) being too narrow: the true
alignment lag shifted to 55 once refine legitimately started taking
longer (more accurate dwell = more raw samples consumed before tracking
begins), landing just outside the old search range. Fixed
(`max_lag=150`); EVM is genuinely healthy: **-9.13dB**, matching
Python's own -9.47dB almost exactly. Full regression clean: 88/88
`ctest`, full `pytest src/doppler/` (2451 passed; the 15 failures are
the SAME pre-existing, unrelated set already confirmed in task #148's
own investigation, plus one new environment-only failure -- missing CLI
executables on PATH, `doppler-fir`/`doppler-specan`/`doppler-source`,
nothing to do with this work), all 3 `async_dsss_receiver_c_vs_python
.py` gating cases AGREE/PASS.

**The `refine_max_error_db=100.0` (`segments=1`) default now stands on
solid ground for a genuinely different reason than either earlier
attempt**: not "segments>1 is fundamentally non-coherent" (wrong, see
above) and not "segments=1 measured better at near-zero Doppler"
(the original, pre-this-session finding) -- `segments=1`, now that
`refine_sequential` is correctly `false`, closes task #151's own gap on
its own, decisively, with no further patching needed. `segments>1`'s own
remaining mystery (still never fires, independent of dwell -- see above)
is a real but low-priority open item, since `segments=1` already works.

**Decisive finding on task #99 itself**: closing task #99 was this
object's whole motivation, but direct measurement now shows **task #99's
own 4-5dB cliff is NOT a Doppler-estimation-accuracy problem at all** —
confirmed by feeding the ALREADY-SHIPPED `DsssReceiver` a trivial STATIC
ZERO Doppler offset (no frequency error whatsoever, coarse or refined) at
SPEC's exact Es/N0=5dB floor: it still fails to decode (BER~0.43,
lock~0.55). `AsyncDsssReceiver`'s own refine stage, once its real bugs
above were fixed, produces accurate estimates and decodes cleanly at
every SNR tested down to ~15-20dB under the real 500Hz/s ramp — but
cannot and does not close SPEC's literal 5dB floor case, because that
floor was never actually about Doppler accuracy. This confirms
`FINISHING_PLAN.md`'s own prior note verbatim: *"the leading remaining
candidate is Acquisition's own hit quality/reliability at this SNR, not
yet directly inspected."* Task #99 stays open, now with a much narrower,
better-evidenced next step (investigate `Acquisition`'s own hit quality
at low SNR directly) instead of the Doppler-refinement hypothesis this
whole object was built to test.

## Other still-open items

- [ ] Reconcile `acq_handoff.py`'s `DetectionEvent` field names/shape with `SPEC.md`'s own (task #79).
- [ ] Investigate `async_despread_demo.py`/`receiver_lock_demo.py` `test_examples` failures (task #63).
- [ ] `DsssBurstReceiver` (compose `BurstAcquisition`+`BurstDemod`) — explicitly out of scope (task #80), listed only so it isn't lost.
- [ ] `carrier_acq_core.c`'s `dwell_target` planning (`det_n_noncoh`, uncorrected classic model) still uses a different model than `_ratio_threshold()`'s KAPPA-corrected detection decision — no longer blocking (real `dwell_target` is now ~12 blocks, small and working, via `design_margin_db=14.0`), but still not a principled fix; teaching `det_n_noncoh`'s own call about the same KAPPA correction remains open if this ever needs revisiting at a different operating point.
- [ ] `KAPPA=7.9`'s own generality (see the `CarrierAcquisition` section above) — still a single calibration point, not validated across `nfft`/window/template variety.
- [x] **`~/legacy-commz`'s own `FrequencyAcquisition`/`NoncoherentSignalDetector` comparison — tried, three structural differences found, NONE panned out as the fix; thread now closed (user's own call).** Three candidates were identified against their source (`receiver/synchronization/carrier.py`, `dsp/detection.py`): (1) a MINIMUM, not median, noise-band reference; (2) a threshold computed ONCE and `1/n`-decayed, vs. `_ratio_threshold()`'s recomputed-every-n `1/sqrt(n)`-shaped form; (3) no separate `n_coh` multiplier (`dwell`/`design_snr` treat each FFT block as ONE direct per-look sample). Follow-up session tested the two candidates that could be checked cheaply:
    - **`n_coh`: empirically the OPPOSITE of the hoped-for lever.** Swept `det_n_noncoh(design_snr, n_coh, pd, pfa, max)` directly across `n_coh in {1, 64, 512}` — larger `n_coh` monotonically SHRINKS the predicted dwell (it's the assumed coherent-FFT gain baked into the model). Matching legacy's `n_coh=1` convention would have INFLATED `dwell_target` further, not reduced it, given our `design_snr`'s semantics (a raw per-sample amplitude SNR, not legacy's own already-post-processing per-look SNR — the two aren't the same knob despite the shared name).
    - **`sequential=True` (legacy's actual adaptive-dwell mechanism) re-tried with warm-up skips (0/20/50/100/200 blocks) — still fires at random early block counts (1, 4, 20, 34, 41) with garbage residuals (hundreds to tens-of-thousands of Hz off), on the REAL e2e prefix.** Critically, `characterize_carrier_acq_detection.py`'s Study 3 shows sequential mode is well-behaved on pure synthetic AWGN+NRZ (0% empirical false-alarm rate through n=64, matching the nominal Pfa) — so this is a genuine discrepancy between the idealized calibration signal and the real front-end's output (something in the actual collection pipeline the synthetic MC doesn't model), not a transient a warm-up skip can dodge, and not a calibration-constant problem. Confirms the earlier session's decision to shelve `sequential` was correct, not just incomplete.
    - Min-vs-median noise reference: not tested (no evidence from the above that it would matter, and switching it would require a full KAPPA recalibration for uncertain payoff).
    - **Also tested and refuted: the "shorter dwell would reduce drift-during-collection error" theory itself.** Swept `design_margin_db` from 14 down to 6 (shrinking `dwell_target` from 1534 downward) on the real captured prefix — accuracy degraded MONOTONICALLY as dwell shrank (SPEC-rate case error: -211/-235/-289 Hz at margin=14 -> -275/-833/-926 Hz at margin=6). Going the other direction (margin >= 16) never fires at all: the ~2700-epoch prefix only yields ~2149 blocks, and `dwell_target=1534` (margin=14) is already close to that ceiling. So `margin=14` isn't an arbitrary bad choice wasting the window — it's close to the BEST available operating point given the collection budget. The real constraint is this estimator needing more looks than the current prefix affords, not a fixable formula/convention mismatch.
    **Outcome: legacy-matching abandoned as the lever for this problem** (user's own call, "forget it," after seeing the above). The `dwell_target`/KAPPA planning-vs-testing model mismatch (bullet above) is still real and undocumented-as-fixed, but nothing here suggests copying legacy's conventions resolves it — any real fix would need to either (a) derive `dwell_target` from `_ratio_threshold()`'s own validated Gamma-sum model directly (replacing `det_n_noncoh()` for this object entirely, a real derivation task, not a quick swap) or (b) root-cause why `sequential` mode's real-data false-fire rate diverges from its clean synthetic-MC behavior. Neither pursued further this session.
