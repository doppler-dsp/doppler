# Async despreader prototype

## The bug

`native/src/dll/dll_core.c`'s `segments > 1` path (`Dll(..., segments=K)`)
splits each code epoch into `K` sub-epoch partial correlations, with a
one-epoch-deep lookback so a mid-epoch data transition doesn't collapse
the whole epoch's correlation. Long-run stress testing (SF=1023, sps=2,
`EPOCHS_PER_SYMBOL=10/9`, several thousand epochs) found it diverges:
`last_error` creeps steadily in magnitude, eventually saturates
`DLL_DISC_CLAMP`, and the despread output blows up by an order of
magnitude and never recovers -- reproducible with **zero added noise**,
so it isn't a low-SNR sensitivity issue.

An earlier scratchpad Python reimplementation of the design doc's own
pseudocode (`docs/design/async-despreader-working-design.md`) showed the
*same* divergence under the same stress, at every tie-break rule tried
(the design doc's bare `argmax`, a margin-gated version, and a
ratchet-chain margin-gated version) -- so this isn't a C-porting-specific
bug, and it isn't the tie-break rule either. Sweeping `windows` (segments)
alone showed the divergence appears at `windows >= 2` and grows worse
with more windows; `windows=1` (no chunking at all) never diverges.

## The fix

A working DSSS despreader implementation reviewed for comparison
implements essentially the same algorithm and is proven robust over long
transmissions in its own test suite and examples. `despreader.py` in this
folder is a faithful, sample-level pure-Python port of its core loop
(`_find_max_power` / `_get_window` / its `LoopFilter`), stripped of
numba, acquisition, the state machine, and carrier coupling. Validated
**rock solid** (`validate_stress.py`) across:

- `windows` = 6, 11, 22 (both a hand-picked size and the design doc's own
  `max_error_db`-derived size)
- `bn` = 0.002 through 0.02
- with and without a residual carrier
- with and without -8 dB additive noise
- runs up to 12,000 symbols (4x the length that originally diverged)

Two differences from the earlier (buggy) reimplementations turned out to
matter -- see `despreader.py`'s module docstring for the full
explanation:

1. Sample-level `get_window` reconstruction (shift the raw per-sample
   signal/early/late buffers, then average the reconstructed product),
   not "pre-sum into per-chunk sums, then recombine whole chunk-sums."
2. The working implementation's exact discrete-time 2nd-order loop-filter form and rate
   scaling (divide the FULL proportional+integral output by the number
   of samples in the upcoming epoch), not "integrator alone as the
   sustained rate, plus the proportional term spread over an extra
   factor of `sf`" (a scheme arrived at this session while fixing a
   different, already-resolved bug).

## Downstream pipeline (`validate_pipeline.py`)

Feeding the despreader's chunk-rate output through `Costas` ->
`Resampler` -> `SymbolSync` (matching
`test_async_dsss_receiver.py`'s composition) needed one more fix:
`NOMINAL_RATE = windows * EPOCHS_PER_SYMBOL` is not an integer (e.g.
`6 * 10/9 = 6.667`), and feeding that fractional rate straight into
`SymbolSync` (as the original test does) tracks correctly most of the
time but was seen to fail at one specific code-phase offset (BER ~0.10).
Explicitly resampling to a clean integer samples/symbol (`Resampler`)
*before* `SymbolSync` fixed it — matching the
`feedback_despread_resample_demod_separation` memory note (don't let one
stage's tuning parameter double as a downstream stage's rate
requirement).

With that resample step in place: **BER=0.0** across every code-phase
offset and symbol-clock-drift scenario tested, at the nominal Es/N0 ~
22.6 dB operating point.

## SNR sweep: resolved (was mis-measured, not a real gap)

An initial short-run (1200-symbol) test at Es/N0 ~ 10.6 dB ("near noise
floor") gave BER ~0.12 -- worse than the original test's `<0.02` target,
and worth investigating given theoretical BPSK BER at that Es/N0 is
~3.9e-6 (`Q(sqrt(2*Eb/N0))`). Isolating the despreader alone (genie
timing, no carrier) confirmed it captures full processing gain: 0 errors
in ~4000 symbols at Es/N0=10dB. The ~0.12 BER traced to `Costas`/
`SymbolSync` not having settled -- both need several inverse-loop-
bandwidths (`1/bn` chunks/symbols at `bn=0.02`) before their output is
trustworthy, and a naive "discard the first quarter" warm-up on a short
run undercounts this at low SNR, especially since `Costas` runs at the
*chunk* rate where per-sample SNR is much lower than the eventual
matched-filtered symbol SNR. With a properly sized warm-up and a longer
run (`validate_pipeline.py`'s `SNR_SWEEP_WARMUP`/`SNR_SWEEP_NSYM`):
**BER=0.0 (0 errors in 6191 symbols) at Es/N0=10dB**, matching the
theoretical floor.

## Migrating to C-backed doppler objects, piece by piece

`despreader.py` stays the pure-Python original -- untouched, so it keeps
serving as the known-good reference. Each piece being swapped for a real
doppler C extension gets its own copy (`despreader_<piece>.py`) plus a
`compare_<piece>.py` that runs both on identical input and diffs the
outputs and internal loop state, so a regression is caught immediately
rather than discovered several pieces later.

**Step 1 -- `NCO` (`despreader_nco.py`, `compare_nco.py`): done.** Code-
phase tracking's `phase`/`phase_inc`/`% 1.0` float bookkeeping is
replaced by `doppler.source.NCO` (`native/inc/nco/nco_core.h`): each
epoch sets `norm_freq = code_rate / tsamps` and calls
`steps_u32(tsamps)`, converting the raw `[0, 2**32)` ramp to a chip
position via `phase / 2**32 * sf`. Re-ran `validate_stress.py`'s full
sweep against `despreader_nco.py` directly: **CLEAN** at every
`windows`/`bn`/noise config, same as the pure-Python original.
`compare_nco.py` confirms the two are not bit-exact (`NCO`'s `phase_inc
= floor(frac(norm_freq) * 2**32)` truncation gives it a real, finite
rate resolution -- `2**-32 * tsamps ~= 4.8e-7` in `code_rate` units for
`tsamps=2046`), but the divergence is **exactly the predicted
quantization floor**, not a functional regression: `code_rate_diff` sits
at 1.9e-7 to 4.8e-7 across all 18 swept configs (windows=6/11/22,
bn=0.002/0.01/0.02, with/without -8 dB noise), and every config reports
`MATCH (within NCO quantization)`.

Superseded the initial version of this step, which overwrote `norm_freq`
every epoch (`nco.norm_freq = code_rate / tsamps`) -- correct, but not
the established doppler idiom for a loop-filter-driven oscillator. Now
uses the real control port instead: `phase_inc`/`norm_freq` are set ONCE
at construction to the TRUE nominal rate and never touched again; each
epoch's tracked deviation is passed as a per-sample `ctrl` array to
`nco.steps_u32_ctrl(ctrl)`, mirroring `lo_step_ctrl`/`lo_steps_ctrl`
(`native/inc/lo/lo_core.h`, used e.g. by `carrier_nda_core.h`) --
**`NCO` didn't have this control port at all until this work added it**
(`nco_steps_u32_ctrl`/`objects/nco.toml`, `native/tests/test_nco_core.c`
test 8): the primary use case for a fixed-point NCO -- driving it from a
tracking loop's per-sample correction without ever touching its
configured rate -- was missing from doppler's most foundational
component's Python surface.

Adding it surfaced a real bug: jm's default `variable_output` codegen
(cached buffer + gh-437 weakref-gated retire) leaks unboundedly under
the natural `x = obj.method(...)` loop pattern -- confirmed leaking
~12 KB/call, which is what took the whole WSL VM down mid-session before
being traced and fixed. `NCOObj_steps_u32_ctrl` (`native/src/source/
source_ext_nco.c`) now allocates a fresh array per call by default
(matching its siblings `steps_u32`/`steps_u32_scaled`/`steps_u32_ovf`),
with a permanent leak regression test
(`test_steps_u32_ctrl_no_leak_in_tight_loop` in
`src/doppler/source/tests/test_nco.py`). Filed
[jm#493](https://github.com/just-buildit/just-makeit/issues/493)
separately for `[project] c_style = "clang-format"` reformatting sacred
`native/inc/**` headers with no exclusion (found while trying to
eliminate the manual `clang-format -i` step this addition needed).

**Both `despreader_nco.py` and `despreader_lf.py` now also avoid
per-epoch allocation entirely**: the `ctrl` (float32) and phase-out
(uint32) buffers are allocated ONCE in `__init__` and reused every epoch
via `steps_u32_ctrl`'s `out=` parameter (`ctrl_buf.fill(...)` in place,
`nco.steps_u32_ctrl(ctrl_buf, out=phase_buf)`) -- no cache/retire
bookkeeping involved (`out=` always writes directly into the caller's
own array), so this is safe by construction, not just empirically
leak-free. Note `out=`'s buffer must be sized to
`steps_u32_ctrl_max_out()` (65536), not just this epoch's `tsamps` --
the returned view is still correctly sliced to `tsamps`. Re-validated
after this change: `compare_nco.py` (all 18 MATCH, unchanged),
`compare_lf.py` (all 18 CLEAN, unchanged), and the full pipeline BER
check (still 0.0 at Es/N0=22.6dB and 10dB) -- confirms the optimization
is behavior-preserving.

**Step 2 -- `LoopFilter` (`despreader_lf.py`, `compare_lf.py`): done, and
load-bearing for the C port.** The code loop's `LoopFilter` is swapped
to the real `doppler.track.LoopFilter` (`native/inc/loop_filter/
loop_filter_core.h`) -- the SAME engine already used by every other
doppler tracking loop, including the currently-committed C `Dll`. Unlike
the NCO swap, this one is a genuinely *different filter algorithm*, not
just lower precision: the pure-Python filter is a discrete-time
bilinear-mapped design whose recursion carries the previous input
forward (`out = state + kp*(x - last_in) + ki*last_in`); the real
`LoopFilter` is the "standard" Stephens & Thomas form used everywhere
else in doppler (`integ += ki*x; return integ + kp*x`), with `kp`/`ki`
from a different closed-form derivation. Two things came out of the
comparison:

1. At the validated `bn` range (0.002-0.02, `zeta=0.707`), the two
   derivations' `kp`/`ki` turn out to be numerically very close (within
   ~0.03% at `bn=0.02`, closer at smaller `bn`) -- both are discrete
   approximations of the same continuous-time 2nd-order design, and they
   converge as `bn -> 0`. This is NOT something to rely on in general
   (they're still a different z-transform of the loop), but it explains
   why swapping the filter barely moved the numbers here.
2. Keeping the VALIDATED scaling (`code_rate = 1.0 + lf.step(e) /
   tsamps`, not the currently-committed C `dll_update()`'s "integrator
   alone as rate + kp*e spread over an extra factor of sf*sps"): the
   real `LoopFilter` is **just as stable** across the full
   `validate_stress.py` sweep (windows=6/11/22, bn=0.002/0.01/0.02,
   with/without -8 dB noise, 12000 symbols) -- every config CLEAN, code
   rate and output magnitude matching the NCO-only version to 6 decimal
   places. The full downstream pipeline (`Costas` -> `Resampler` ->
   `SymbolSync`) also still hits **BER=0.0** at both Es/N0=22.6dB and
   Es/N0=10dB with the real `LoopFilter` swapped in.

**This is the key finding for the eventual C port**: the divergence bug
was never about `loop_filter_core.h`'s internal recursion form -- that
engine is fine as-is. It was specifically the *scaling formula* wrapped
around it in `dll_update()`. The C fix (next-steps item 3 below) can
reuse the existing shared loop filter engine untouched and just correct
how its output is applied, rather than introducing a new filter design
into C.

## Next steps

1. Continue the piece-by-piece C-backed migration: replica generation
   next (a real `doppler` LUT/interpolator object) -- its own
   `despreader_<piece>.py` + `compare_<piece>.py` pair, same pattern as
   steps 1-2 above.
2. Update `test_async_dsss_receiver.py` to use a valid `windows` divisor
   (its current `K=8` does not evenly divide `TE=2046` -- neither this
   prototype nor `dll_core.c`'s C implementation support that), the
   explicit resample step, and a properly sized warm-up in its low-SNR
   assertions.
3. Port the two fixes above (sample-level reconstruction, exact
   loop-filter scaling) into `native/src/dll/dll_core.c`'s `segments > 1`
   branch, with the same stress scenarios as C regression tests.
4. Separately: the working implementation reviewed for comparison
   couples carrier tracking into the despread replica itself (its local
   oscillator, driven by the Costas loop, multiplies the local code
   replica before despreading), so its code loop is coherent, not
   carrier-blind like `Dll`. That's a distinct architectural question
   (flagged mid-session: carrier acquisition/tracking needs to happen
   before or coincident with despreading so the carrier doesn't walk
   out of the correlation bandwidth over long transmissions) -- out of
   scope for this prototype, which targets the chunked-lookback
   divergence specifically and reproduces it even with zero carrier at
   all.
