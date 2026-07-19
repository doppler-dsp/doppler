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

## Other still-open items

- [ ] Reconcile `acq_handoff.py`'s `DetectionEvent` field names/shape with `SPEC.md`'s own (task #79).
- [ ] Investigate `async_despread_demo.py`/`receiver_lock_demo.py` `test_examples` failures (task #63).
- [ ] `DsssBurstReceiver` (compose `BurstAcquisition`+`BurstDemod`) — explicitly out of scope (task #80), listed only so it isn't lost.
