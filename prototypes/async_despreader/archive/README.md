# Archive

Two different things live here, both kept for reference rather than
deleted:

1. **Historical scripts** — the DLL `segments > 1` divergence
   investigation and the piece-by-piece migration to C-backed doppler
   objects that preceded the coupled carrier+code tracker. The full
   narrative (the bug, the fix, the SNR-sweep resolution, each
   migration step) is told in prose in `../README.md`'s "The bug"
   through "Migrating to C-backed doppler objects" sections — this
   file doesn't repeat it, just indexes which file is which:
   - `despreader.py` — the pure-Python reference implementation
     (`SimpleAsyncDespreader`) the whole divergence fix was validated
     against. Still imported by `validate_stress.py` below; not
     imported by anything in the current design.
   - `despreader_nco.py`, `despreader_lf.py`, `despreader_interp.py` —
     the three incremental C-backed swaps (NCO, LoopFilter,
     InterpolatedTable), each validated bit-exact/clean against the
     previous step.
   - `compare_nco.py`, `compare_lf.py`, `compare_interp.py` — the
     diff harnesses for each swap above.
   - `validate_stress.py` — the original long-run divergence stress
     test for `despreader.py`. Its geometry constants and signal
     generator (`code()`/`signal()`) were extracted into
     `../signal_gen.py` for the current design's validation scripts to
     use without pulling in `despreader.py` — this file is otherwise
     unchanged and still runnable standalone (`python validate_stress.py`
     from within this directory).
   - `validate_pipeline.py` — the `despreader.py` -> `Costas` ->
     `Resampler` -> `SymbolSync` pipeline validation from the same
     era.

2. **Discarded ideas for the coupled carrier+code tracker /
   Doppler-rate problem** — tried (or reasoned through) and dropped.
   Each entry says what was tried, what was found, and points at the
   actual script if it's worth re-running.

## Solving Doppler resolution purely inside `Acquisition`

**Idea**: instead of a separate Python frequency-refinement bridge,
tune `Acquisition`'s own `reps`/`max_noncoh` (more coherent depth, more
non-coherent looks) to get a Doppler estimate directly accurate enough
to seed the coupled tracker.

**Tried**: `acq_reps_noncoh_sweep.py` swept real
`Acquisition.configure_search_raw(doppler_bins, n_noncoh)` combos
(D=8/16/32, nc=1/6/12/24/48) under real async 1800 sps data at
Es/N0=2/5 dB.

**Found, and dropped**: `hit_rate` climbs with `nc` but `success_rate`
(correct bin) plateaus at 0.25-0.62 and never converges toward 1.0
even at `nc=48` — categorically different from genuine noise-averaging
convergence. Matches a pre-existing documented finding
(`docs/design/dsss-acquisition.md`'s "ceiling (b) fails hard, not
gracefully," commit `a244a0b4`): async data toggling close to
`Acquisition`'s own slow-time sampling rate aliases broadband energy
across the entire Doppler-bin axis — a structural, deterministic
mislock that no amount of non-coherent averaging fixes. This is exactly
why the separate bridge (`../freq_refine.py`) operates on the
POST-DESPREAD stream instead, where the data rate sits comfortably
below that stream's own Nyquist.

## 4x-ing Acquisition's slow-time FFT size (extending real coherent depth)

**Idea**: since finer Doppler resolution needs a longer coherent
window, what if the slow-time FFT (`doppler_bins`) were simply made 4x
longer?

**Tried**: direct D-vs-4D comparison at fixed `n_noncoh`, same script
above.

**Found, and dropped**: no benefit, and often slightly worse (e.g.
`success_rate` 0.33 -> 0.25, RMS error 160 -> 286 Hz going from D=4 to
D=16 at Es/N0=5 dB, nc=6). Extending `D` extends the coherent window in
TIME, which spans MORE data-bit transitions — the aliasing floor gets
worse at roughly the same rate the nominal bin width improves, so the
two effects cancel (or aliasing wins). Confirms the limiter isn't bin
width, it's the data modulation's own broadband leakage.

## Zero-padding ("stack zeros horizontally") / bin-rolling Acquisition's native slow-time axis

**Idea**: instead of extending real `D` (which worsens aliasing, per
above), zero-pad the SAME real per-epoch correlation values before the
slow-time FFT — sharpens an already-correct bin the same way
`freq_refine.py`'s own `zero_pad` sharpens its squared spectrum,
without adding more real (aliasing-exposed) samples. Then "roll bins
for the 2D" — some form of circular-shift/alignment trick across the
full (Doppler x code-phase) frame.

**Status: reasoned through live, dropped before any code ran.** The
user worked through the mechanism and concluded "won't work" — the data
modulation's own broadband content is baked into the real per-epoch
correlation values BEFORE any transform; padding only interpolates
that same already-aliased spectrum more finely, exactly like it failed
to fix `freq_refine.py`'s own squared-spectrum gross errors (see the
next entry). A genie-code-phase isolation harness
(`acq_slowtime_zeropad_test.py`) was written to test the zero-pad half
of this but never run — left here, ready if ever worth revisiting, but
not part of the active design.

## Matched-filtering the FFT power spectrum against the known Dirichlet mainlobe

**Idea**: `freq_refine.py`'s bridge picks the raw `argmax` of an
accumulated `|FFT|^2` spectrum. Since a genuine tone's energy (once
zero-padded) is spread across `~zero_pad` bins in a KNOWN shape (the
rectangular window's own Dirichlet kernel), cross-correlating the
power spectrum against that known shape before picking the peak should
suppress a single isolated noise-bin spike relative to a real,
multi-bin-wide peak — closer to the true maximum-likelihood statistic.

**Tried**: `use_mf=True` / `_matched_filter_power` in
`../freq_refine.py`, measured head-to-head against not using it across
every Es/N0/collection-length combination in `../improve_low_snr.py`.

**Found, and dropped (kept as an off-by-default, documented-negative
opt-in in `../freq_refine.py`, not deleted)**: equal or slightly WORSE
`found_right_peak` every time (e.g. 2 dB/300 epochs: 0.25 -> 0.17 with
matched filtering on). Plausible cause: the squared signal's noise
cross-terms are themselves correlated across nearby bins (same
finite-length DFT), so local bin-averaging doesn't discriminate signal
from this particular noise the way it would for independent per-bin
noise. **Multi-look non-coherent averaging (longer collection) is the
lever that actually works** — see `../README.md` / `../improve_low_snr.py`.

## The nested-loop rearchitecture campaign (CHECKPOINTS 16-23) and its superseding cleanup

**Idea**: a working reference implementation (`~/legacy-commz`) showed
the intended carrier-tracking architecture is a nested two-rate loop
(raw-rate fused code+carrier replica, always-on code-rate aiding from
the carrier estimate, a low-rate PSD-matched-filter refinement, a
closed Costas loop running on a RESAMPLED low-rate stream that feeds
back into the raw-rate replica) rather than the once-per-epoch,
no-aiding, no-resample design this story had used up to that point.

**Tried**: fixed `aid_code`'s formula to aid from the full current
carrier estimate, not just the tracked deviation (`despreader_coupled
.py`, still in place); added an opt-in `car_update_windows` per-window
carrier-loop mode (`despreader_coupled.py`, still in place); built a
`carrier_resample_track.py` external harness mirroring the reference's
own resample-then-track carrier loop exactly (anti-alias-filtered
`RateConverter` down to the demod rate, THEN a Costas discriminator,
matching `~/legacy-commz`'s `Despreader.step()`).

**Found, and (partially) dropped**: none of these fixed the BER~0.5
"never locks" failure this whole campaign was chasing — confirmed
against both a synthetic sweep AND a real `~/legacy-commz`-generated
capture (bit-identical Gold-1023 code verified between the two
projects). Pulling `MpskReceiver`'s own telemetry eventually found the
real cause: its symbol-timing loop never locks on either signal,
independent of anything upstream — a separate, still-open bug entirely
outside Acquisition/PSDMF/`CoupledAsyncDespreader` (out of this
folder's current scope; see `../FINISHING_PLAN.md`).
`carrier_resample_track.py` is archived here because, once the real
cause was found downstream, its own hypothesis (raw per-window
discriminator too coarse/unfiltered) was never actually the problem —
`car_update_windows=True` alone (no resample stage) despreads cleanly,
confirmed by a direct genie-aided BER check
(`../e2e_acq_to_despreader.py`).

**Two REAL bugs the cleanup's genie-aided despread-level test DID
find, isolated from the `MpskReceiver` confound for the first time**:
(1) `aid_code=True` combined with a nonzero static Doppler offset on
this story's own synthetic signal generators (`make_ramp_signal`,
`signal_gen.signal()`) is catastrophic, not a mild regression —
because neither generator gives the Doppler offset a matching
code-rate change, `aid_code` injects a real but physically-unwarranted
correction that misaligns the code loop over a long run (worked around
in `e2e_acq_to_despreader.py` by disabling `aid_code` for that one
case; see task #101). (2) the real SPEC rate (500 Hz/s) alone,
independent of any static offset or `aid_code`, ALSO produces BER~0.5
— likely because PSDMF's one-shot batch refine doesn't account for the
residual moving ~450 Hz during its own ~0.9s collection window, but
this is a hypothesis, not yet confirmed. (2) is still open.

**Archived alongside this campaign** (superseded by
`../e2e_acq_to_despreader.py`, ONE clean end-to-end test replacing
several overlapping/exploratory scripts):
`spec_full_characterization_prototype.py` (three overlapping trial
functions — coarse-seed-only, PSDMF-seeded zero-impairment, and the
CHECKPOINT-17 static-offset sweep — all now covered by the one new
test), `doppler_rate_test.py`/`doppler_rate_floor_test.py` (the
FLL-assist-vs-static-batch characterization that led to `bn_fll_car`,
now integrated into `despreader_coupled.py` and no longer needing a
standalone sweep), `pullin_sweep.py` (the empirical pull-in gate that
predates the real Acquisition handoff — `acq_handoff.py` replaced the
hand-chosen `seed_gap_hz` this measured against), `characterize_snr.py`
/`improve_low_snr.py` (the PSDMF Es/N0 characterization behind the
multi-look-averaging finding above, superseded by the one new test's
own scoring), `bench_freq_bank.py` (the roll-FFT-vs-mixer-bank
benchmark for task #70, long since resolved and shipped), and
`signal_gen.py` (the decoupled-code-phase signal generator all of the
above depended on — no longer imported by anything in this folder now
that `acq_handoff.py`'s own demo, which used it, has been trimmed down
to pure Acquisition->handoff glue).
