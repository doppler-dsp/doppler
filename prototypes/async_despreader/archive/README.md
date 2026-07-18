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
