# The Doppler channel — one parameter, both clocks and the carrier

`native/inc/doppler/doppler_channel/doppler_channel_core.h` applies clock
Doppler to a complex baseband stream. A real Doppler shift is not a frequency
offset: relative motion rescales the whole received time base, so carrier,
chip rate, symbol rate and frame rate all move together. This object applies
both halves of that from **one** number, so they cannot disagree.

| promise                     | what it means                                                                     | where it stops                                                                     |
| --------------------------- | --------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| **one parameter**           | the dilation and the carrier come from the same integral, so they cannot drift    | `carrier_hz = 0` dilates the clocks and leaves the carrier alone, on purpose       |
| **chunk-independent**       | feeding a stream in blocks gives the same samples as one call, bit for bit        | the output length is a property of the stream, not of the block (it varies by one) |
| **no resampling math here** | the dilation is `resamp`'s per-sample rate control; nothing is reimplemented      | the resampler's 32-bit step is the object's accuracy floor (§7)                    |
| **a profile is absolute**   | `execute_profile(x, ppm)` takes the Doppler per input sample, not as a correction | one value per sample, checked; an invalid call writes nothing                      |
| **resumable**               | `state_bytes` / `get_state` / `set_state` resume a stream bit for bit             | the blob is native-endian and unreleased; its layout may still change              |

Not to be confused with the [coarse channel](coarse-channel.md), which is a
frequency-domain channelizer. This page is the time-domain impairment.

______________________________________________________________________

## 1. Why a Doppler shift is not a frequency offset

Modelling only the carrier is the classic shortcut, and it deletes the one
error a delay-lock loop exists to track. At the 2.5 GHz carrier of the
[async DSSS receiver](async-dsss-receiver.md), its ±50 kHz uncertainty is
exactly ±20 ppm of the time base, and the same 20 ppm moves a 3.069 Mcps code
by 61.4 chips per second. A caller never converts between the two by hand,
because there is one parameter, in **ppm of the nominal time base**.

`carrier_hz` is DSP input here, not metadata. Doppler is dimensionless, and
the carrier is the only thing that converts it into Hz.

## 2. Who uses it

| caller                         | how it uses the channel                                                                              |
| ------------------------------ | ---------------------------------------------------------------------------------------------------- |
| a receiver under test          | `native/validation/` harnesses and `dp_rx_test.h` drive a receiver through a Doppler it must track   |
| `wfm_compose` and wfmgen       | a **source** carries `doppler` / `doppler_rate`; the composer's renderer runs one channel per source |
| `Plan` (`doppler.wfm.prepare`) | runs the same renderer over its cached signal, so a sweep of a Doppler scene is bit-identical (§5)   |
| a LEO pass                     | `execute_profile` takes the pass as an array; a pass is not a straight line (§4)                     |
| isolating a code loop          | `carrier_hz = 0`: the clocks dilate, the carrier stays put                                           |

The straight line `(doppler_ppm, doppler_rate_ppm_s)` is the main path for
testing. The array form exists for the case it cannot express: a 550 km
overhead pass is an S-curve from about +23.3 to −23.3 ppm (±58.2 kHz at
2.5 GHz) that departs from its own best straight line by 21% of its range.

## 3. The scalar form

The dilation is a resampling of the whole stream at output/input ratio
`1/(1+d(t))`, with `d(t) = (d0 + ḋ·t)·1e-6`. It is
`dp_resamp_execute_ctrl`, whose per-sample rate deviation tracks a ramp
exactly instead of approximating it with a ratio re-set once per block. The
channel fills `ctrl[i] = ratio(t_i) − base` for each input sample, where
`base` is the ratio the resampler was built with.

The carrier is `exp(j·2π·fc·excess(t))` with `excess(t) = ∫d dt = d0·t + ½ḋ·t²`,
evaluated closed-form from the output sample index rather than accumulated,
so a long capture stays phase-exact. It is the **integral**, not `t·d(t)`:
the latter double-counts a ramp and puts the offset at exactly twice the
intended Doppler rate, which passes every static-Doppler check and is why both
test suites assert against it.

The output is delayed by the resampler's group delay, 10.5 samples for the
built-in bank (`delay_samples`), on top of the dilation. A receiver started at
the input's phase is that far from the peak, which is at two samples per chip
five chips outside a DLL's pull-in.

## 4. The profile form

`execute_profile(x, ppm)` replaces the closed form with an array parallel to
the input. The length contract is the one `dp_resamp_execute_ctrl` already has,
so the profile is handed to the resampler rather than reduced to fit it. It is
**absolute**: the create-time scalars cancel exactly, because the kernel fills
`ctrl = ratio(ppm[i]) − base` with the same `base`.

**The carrier is read off the resampler, not integrated beside it.** The
excess delay at absolute output `k` is `(p_k − k + 1)/fs`, where `p_k` is the
input position the resampler's own accumulator reports for that output
(`dp_resamp_execute_ctrl_pos`: the newest sample under the taps plus the
fraction of an interval past it). Consequences, each of which was a defect in
an earlier shape:

- **No profile index is ever mapped to an output index.** Output `k` has the
    carrier of the position it was interpolated at, whatever call it fell in, so
    the stream cannot depend on how it was chunked.
- **No second accumulator**, so nothing can drift from the dilation.
- **No cancellation.** The integer part of `p_k − k` is exact in `int64`; only a
    fraction below one input sample is floating point.
- **A profile of zeros pays no multiply.** The phase is exactly zero.

A profile sample below −5e5 ppm (a scale under ½) is refused with nothing
written, as is a non-finite one: that is the 2× expansion the output buffer is
sized for, and at −1e6 ppm time stops, which `create()` already refuses for the
scalar. The whole profile is validated before any output is produced, so a bad
call writes nothing rather than a valid prefix.

**A refusal is a negative return, never 0.** `dp_doppler_channel_execute_profile`
returns the sample count, or `DP_ERR_INVALID` when it refuses. 0 stays a valid
answer: a one-sample call returns it 333 times in 1000 at +5e5 ppm and 500 in
1000 at +1e6 ppm (measured, §4.7), so the sign alone tells a refusal from an
empty result, to a C caller and to a binding. The clocks do not move on a
refusal, and Python raises `ValueError`. The convention for count-returning
calls that can refuse is
[#1869](https://github.com/doppler-dsp/doppler/issues/1869).

Mixing the two calls on one stream is coherent: both advance the same clocks,
and `offset_hz` reports whichever drove the most recent call.

## 5. Composition: the renderer, and the Plan

A scene source with Doppler renders through one channel, in
`wfm_compose.c`. The channel's input and output clocks are decoupled by the
dilation, so the renderer follows the **input** timeline — `delay` noise-only
samples, `on` signal samples, then noise-only — counted in input samples, never
the phase of the output being drained. With `gap_noise = off` the gaps are never
pulled, so the channel's input starts at ON.

- **The lifetime is selectable.** `per_instance`: the channel dies with each
    repeat, the repeated-trial shape that composes with a ranged Doppler redrawn
    per instance. `persist`: one continuous pass across the segment's gaps and
    repeats, keyed by (segment, source), the only lifetime under which
    `doppler_rate` accumulates across a multi-burst scene.
- **The channel runs through the gaps.** An emitter does not stop moving because
    its burst ended, so during a gap the thing propagating is the noise floor the
    segment already carries.
- **A drawn Doppler that cannot build a channel fails the whole instance**:
    `delay + off` of silence, no ON region. A *borrowed* `persist` channel is
    never rebuilt, so only the instance that has to create it can fail.

`Plan` caches each source's clean on-time **before** the channel and runs the
channel at render time over it, through the same renderer
(`dp_wfm_render_from_feed`), so there is one implementation of the dilation, the
holdover and the input timeline rather than two. A Doppler render is therefore
bit-identical to `compose()` and is **not** a pure re-weight of the cache; it
fans the channel out across repeats, or across sources when a `persist` channel
chains the repeats. A background source with Doppler is refused
([#1865](https://github.com/doppler-dsp/doppler/issues/1865)): the fold sums
those before any channel could run.

## 6. State

Running state is the two sample clocks (`n_in`, `n_out`), the profile's last
`d` and a flag saying a profile has driven the stream (these two only so
`offset_hz` survives a resume), plus the resampler's own blob. Configuration is
restored by `create()`. A profile's carrier needs no state of its own: it is a
function of the resampler's position, which the blob already carries.

## 7. What is not known yet

- **Whether the resampler's 32-bit step matters.** It truncates, so the
    dilation it performs differs from the ideal by up to `2⁻³²` of a sample per
    output, always the same way. Against the scalar closed form that is a
    constant frequency of at most `fc·2⁻³²` (0.58 Hz at 2.5 GHz); measured
    0.20 Hz. Probably immaterial against ppm-scale tracking; recorded in
    [#1856](https://github.com/doppler-dsp/doppler/issues/1856).
- **Where a long capture stops being phase-exact.** The scalar carrier is a
    double evaluated from the sample index and reduced to one turn before a
    `float` `cexpf`. The header states ~1e-8 cycles of representation error at
    1000 s, 20 ppm, 2.5 GHz; that is arithmetic, not a sweep.
- **How the per-sample cost splits.** The channel alone costs about 13 ns per
    sample (`bench_doppler_channel_core`: 13.1 for the closed form, 13.9–14.2
    for the array form, 1.06–1.08×), and a Plan render of a Doppler scene about
    24 ns per output sample end to end. How the channel's 13 ns divides between
    the resampler and the carrier's `cexpf` has not been measured.
- **The size of the scalar form's input-time approximation.** `ctrl[i]` is
    evaluated at `n_in/fs`, not at the true receive time, which differ by the
    dilation itself (~1e-5 relative). The induced error in `d` is
    ~1e-5·ḋ·t, far below the ppm the parameter is quoted in, but it has not
    been bounded by measurement.
- **Which claims a caller may rely on.** The object has tests but no
    certification report: no header claim has been enumerated and mapped onto
    its C tests, and no sweep has been run
    ([#1867](https://github.com/doppler-dsp/doppler/issues/1867)).

The dated record behind each of these is in
[the measurements](doppler-channel-measurements.md), with the section numbers
shared with this page.

## See also

- [Gallery: Doppler Channel](../gallery/doppler-channel.md) — the scalar form,
    measured.
- [Gallery: a Doppler profile](../gallery/doppler-channel-profile.md) — the
    array form, driven by one cosine period.
- [State serialization](state-serialization.md) — the blob envelope.
- [Resampler](RESAMPLER.md) — the per-sample rate control underneath.
