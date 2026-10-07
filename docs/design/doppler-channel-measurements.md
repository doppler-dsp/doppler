# The Doppler channel — the measurement record

*The dated record behind [the design page](doppler-channel.md): what each step
measured, in the order it was measured, with the numbers, the wrong guesses and
what corrected them. The design page states what is; this page is why. Section
numbers are shared with it, and nothing is rewritten after the fact: a later
entry corrects an earlier one in its own words. Every number names the test or
script it came from. Two sets, the Plan timings (§5.5) and the persist-path
timings (§5.6), came from a throwaway script that is not in the tree; their
method is stated so they can be re-taken.*

*This page and the design page were written after the object existed, and
`execute_profile` after its first attempt had already been built and reverted.
The "why" phase did not come first for this object, which is part of what the
record below shows.*

______________________________________________________________________

### 4.1 The first profile, and why it was withdrawn (2026-08-21)

`execute_profile(x, ppm)` was first built in `c62452cf3` (#939). Each output
sample was mapped back to a profile index with `j·m/got`, a chord across each
internal block of the input, to pick the Doppler its carrier phase was
accumulated from. `m/got` changes with block size, so the pairing moved with
how the caller chunked the stream.

Measured on a 200 000-sample stream and a 25·cos(π·i/n) ppm profile at
`fs = 6.138 MHz`, `fc = 2.5 GHz`, fed whole and in blocks of 10 000 and
50 000: **max |difference| = 2.752e-2** against the whole-stream result, for
both block sizes. That contradicts the object's own contract (a stream fed in
blocks is the same samples as one call). It was reverted in `8e6c5ea40`.

**Why the tests missed it.** The split-resume test split the stream at `n/2`,
but both halves fit inside one internal block, so the chord was never re-cut.
A regression test here has to vary the **block size**, not only the split
point.

### 4.2 The carrier from the resampler's position (2026-10-06)

`dp_resamp_execute_ctrl_pos` returns, with each output, the input position the
accumulator already holds. The excess delay at output `k` is
`(p_k − k + 1)/fs`; the `+1` is the pipeline convention (the first tick of a
fresh stream is emitted before any input is loaded, at position −1).

Measured with the same stream and profile as §4.1, in blocks of 1000, 7777,
10 000 and 50 000: **bit-identical** to one call, all four
(`test_profile_is_independent_of_how_the_stream_was_chunked` in
`test_doppler_channel.py`; section 8a of `test_doppler_channel_core.c`). The
block sizes are not divisors of each other, and the stream is longer than one
internal block (`DOPPLER_CHANNEL_MAX_BLOCK`), so the §4.1 chord would have been
re-cut.

### 4.3 Against the scalar closed form (2026-10-06)

A flat 20 ppm profile against the scalar `doppler_ppm = 20`, `fs = 6.138 MHz`,
`fc = 2.5 GHz`, phase difference after the resampler's start-up transient:

| input samples | peak-to-peak phase difference | bound `2π·fc·2⁻³²·T` | slope     |
| ------------- | ----------------------------- | -------------------- | --------- |
| 50 000        | 0.0103 rad                    | 0.0298 rad           | 0.2013 Hz |
| 200 000       | 0.0412 rad                    | 0.1192 rad           | 0.2014 Hz |
| 800 000       | 0.1648 rad                    | 0.4767 rad           | 0.2014 Hz |

The difference is a **constant frequency**, about 0.35 of `fc·2⁻³²` (0.582 Hz),
which is what truncating a uniformly distributed fraction gives. The cause is
the resampler: `dp_resamp_execute_ctrl_push` converts the per-output step to a
32-bit phase word by truncation, so the dilation it performs differs from the
ideal by up to `2⁻³²` of a sample per output. The profile's carrier is read off
the resampler and follows what it did; the scalar's closed form is the ideal.
A ramp profile built from the scalar's `(d0, ḋ)` agrees to the same bound.
`test_flat_profile_matches_the_scalar_route_to_the_rate_quantum` and
`test_profile_ramp_matches_the_scalar_ramp` assert the bound. Filed as #1856.

### 4.4 Sabotage (2026-10-06)

Each assertion was proven able to fail:

- dropping the pipeline `+1` from the excess: worst flat-profile phase
    **1.9186 rad** against the 0.1192 bound (C section 8b);
- zeroing the position's loaded-sample term in `dp_resamp_execute_ctrl_pos`:
    all seven `eq_ctrl_pos` rates failed in `test_resamp_core.c`, and the
    flat-profile phase read 1.8773 rad;
- not clearing the profile flag on a scalar `execute()`: `offset_hz` read
    −49 999.999 against +50 000 (`test_scalar_execute_hands_offset_hz_back_to_the_closed_form`).

### 4.5 What the review of #1857 found (2026-10-06)

`profile_ok` accepted any scale above 0, but `execute_profile_max_out` sizes the
buffer for at most a 2× expansion (`2n + 2`). Measured in the review, a flat
profile at `n = 1000`:

| ppm    | outputs | expected `1000/(1+d)` |
| ------ | ------- | --------------------- |
| −4.9e5 | 1961    | 1960.8                |
| −5.5e5 | 2002    | 2222                  |
| −9e5   | 2002    | 10 000                |

No error was raised, and `n_in += m` counted the whole chunk although the
resampler had stopped at the cap, so the clocks would drift. The fix refuses a
sample below −5e5 ppm (`PROFILE_MIN_SCALE = 1/2`) or non-finite, so nothing is
written. The review also found `+inf` accepted (`1 + inf > 0`). The validation
pass costs about 0.2 ns per sample against 17 ns for the call (1.2%, an upper
bound that includes the binding's array setup).

### 5.1 The gap carried the signal (2026-10-06)

With Doppler on, a clean burst's gap was not silent. `fs = 1 MHz`, `on = 1000`,
`off = 3000`, 20 ppm:

| scene       | gap mean \|y\| per 500 samples, before   | after                   |
| ----------- | ---------------------------------------- | ----------------------- |
| tone        | 1.001, 1.001, 1.001, 1.001, 1.000, 1.000 | 0.022, then 0.0 exactly |
| QPSK        | 0.976 … 0.980                            | 0.020, then 0.0 exactly |
| Doppler off | 0.0                                      | 0.0                     |

The renderer chose signal-or-noise once per refill from the phase of the
*output* being drained, and a refill feeds 4096 *input* samples. With a leading
delay the mirror image happened: a burst after a delay of 64, 2000 or 5000
samples had **min |y| 0.0** over its ON region against > 0.9 after the fix
(#1858, #1859). Five tests in `test_compose.py` went red with the fix disabled.

### 5.2 `gap_noise = off` with a delay (2026-10-06)

The #1859 fix declared the delay to the channel even when the gaps are never
pulled (`render_gap` writes hard zeros), so the burst was fed `delay` samples
late and landed after its own ON region: mean |y| over `[2064, 2936)` was
**0.000** against 1.001 with the default `auto`, at `delay = 2000`. Found by
reading `render_gap` while designing the Plan's render, not by a test; the
default path was never affected, which is why the tests added with #1859 did not
see it. Fixed in #1863 with three delay cases.

### 5.3 A drawn Doppler that cannot build a channel (2026-10-06)

`doppler = −2e6` ppm: `compose()` returned **3000 samples, all zero** (it fails
the whole instance and emits `delay + off` of silence, 3 × (300 + 700)), while
the Plan returned **12 000 samples with max |y| = 1.0**, the clean signal.
Found in the review of #1864; reachable from user input, not allocation-only.
Two rules compose has that the first fix got wrong, each caught by a scene that
mixes failing and succeeding instances: a **borrowed** `persist` channel is
never rebuilt, so once a slot exists a later instance cannot fail on its own
draw (compose 48 000 samples, Plan 39 000 before the rule), and a failed
instance is shorter, which the parallel render's precomputed start offsets must
know.

### 5.4 Equal in value, different in bits (2026-10-06)

The first Plan render matched `compose()` under `np.array_equal` and failed the
C `memcmp` at sample 23: compose `(−0, 0)`, Plan `(0, 0)`. A channel's ring-out
emits negative zeros (a filter running on zeros), compose's first source
*assigns* (`out = g·y`), and the Plan accumulated into a zeroed buffer
(`0 + −0 = +0`). The first signal source of a region now assigns, and the Python
Doppler tests compare `tobytes()`.

### 5.5 Plan against `compose()` (2026-10-06)

The cache holds the signal before the channel (#1109 measured the old
clean-cache-plus-noise model **1.73 off** on a unit-power signal for a bundled
noisy source with no gaps). Timed on an 8-core WSL2 box, 400 000-sample QPSK
sources with a 20 000-sample gap and 5 000-sample delay, min of 3 composes and
5 renders, a throwaway script (a fresh `Composer` per timed `compose()`, which
is single-pass), every pair bit-identical:

| scene         | compose | render, serial | render, parallel | speedup |
| ------------- | ------- | -------------- | ---------------- | ------- |
| 1 source, ×1  | 0.011 s | 0.011 s        | 0.011 s          | 1.0×    |
| 1 source, ×4  | 0.049 s | 0.046 s        | 0.011 s          | 4.5×    |
| 1 source, ×16 | 0.199 s | —              | 0.037 s          | 5.3×    |
| 8 sources, ×1 | 0.073 s | —              | 0.017 s          | 4.2×    |
| 8 sources, ×4 | 0.292 s | 0.291 s        | 0.082 s          | 3.5×    |

The same scenes without Doppler are 1.6–2.4× (a cached re-weight is
bandwidth-bound). Serial, a Doppler render is only 1.0–1.1× `compose()`
because the channel pass dominates and `compose()` runs it serially too, so the
speed-up is entirely the fan-out. ThreadSanitizer, C suite: 148 of 148, no race
reports, with the parallel Doppler cases (5 × 1024 samples) in it.

### 5.6 The persist path (2026-10-06)

A `persist` segment cannot fan out across repeats (the channel chains them), so
it fans out across sources once per instance, gated at 4096 samples. Ten
shapes, 2 and 4 sources, 2 000 × 64 up to 400 000 × 4, min of 3 and 5:
**1.07× to 2.67×**, never slower than `compose()`; below the gate it is at
parity. The per-instance spawn does cost something (2 sources, 5 000 × 64: 1.4×
where more is available), so it is headroom rather than a loss.

______________________________________________________________________

### 7.1 Still open

- The resampler's step truncation (§4.3): #1856.
- Certification, the long-capture phase sweep and the per-sample cost split:
    #1867.
- A `background` source with Doppler in a Plan: #1865.
