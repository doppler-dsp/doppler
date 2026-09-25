# Quantization: rules, sites, and open violations

A survey of every place doppler converts between fixed-width integers and
floating point, written to settle the rounding question for **every** kind of
conversion, not only the NCO's. It is a discussion page: it records what was
decided, what the code does today, and what still disagrees with the rule.
The decisions it proposes are not made yet.

Code links are permalinks to commit `51d34a29`, so
their line numbers stay correct as the tree moves.

## What was settled

[doppler#1117](https://github.com/doppler-dsp/doppler/issues/1117), 2026-09-12,
settled two rules, one for each side of a single question: can the converted
value leave its range?

- **An unclamped value going into wrapping (modular) arithmetic, like the
    NCO: truncate.** The rationale is
    [NCO design §7, "Why truncation, precisely"](../design/nco.md#7-why-truncation-precisely).
    Truncation has no tie to break, so it is bit-identical on every host.
    doppler builds with `-ffast-math`, and on FMA targets a rounding form like
    `+ 0.5` gets fused into the multiply, so the tie is decided by the
    compiler. Also, `(uint32_t)` of a value rounded up to 2³² is undefined
    behaviour: x86 wraps it to 0, arm64 saturates it.
- **A clamped sample quantiser: scale by 2^(N−1), clamp, then `lroundf`.**
    The clamp happens first, so the value can't wrap. `lroundf` rounds half
    away from zero regardless of the host's rounding mode (C99 §7.12.9.7), so
    it is deterministic. Truncating a bipolar signal instead costs 6.02 dB
    (6.0 dB measured on #1117) and leaves a ±1 LSB dead zone at zero.
    Full scale is 2^(N−1) in both directions: +1.0 maps one step past the top
    code and saturates, which is how the converter is defined.

## The six kinds of conversion

| #   | Kind                                | Rule                                                                             | Implemented once in                                                                                                                   | Breaks the rule                                                  |
| --- | ----------------------------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| A   | float → phase word                  | truncate: clamp at the top, or reduce mod 2³² for a resampling step              | [`nco_phase_units`][pu], [`nco_phase_units_mod`][pum], [`nco_norm_freq_to_inc`][nfi]; used by nco, lo, dll, [resamp]                  | [symsync][sym]                                                   |
| B   | phase word → table index / float    | truncate (phase truncation in the sine lookup); the conversion to float is exact | [lo lookup][lolut], [nco `nmax` scaling][nmax], [`nco_word_to_norm`][w2n]                                                             | dll's own [`/ 2³²`][dll1] [twice][dll2] (exact, only duplicated) |
| C   | float sample → integer              | 2^(N−1), clamp, `lroundf`                                                        | the six `f32_to_*_step`, e.g. [`f32_to_i16`][f2i16], [`f32_to_uq15`][f2uq15]; called by [`wfm_sink`][sink] and [`wfm_writer`][writer] | [CIC encoder][cicenc], [ADC vector path][adcv]                   |
| D   | integer sample → float              | multiply by 1/2^(N−1)                                                            | six `*_to_f32`, e.g. [`i16_to_f32`][i2f]; [`wfm_reader`][reader], [`stream`][stream]                                                  | none; int32 → float keeps only 24 bits by nature                 |
| E   | fixed-point requantise (`acc >> n`) | round half up, `(x + 2^(n−1)) >> n`, then saturate                               | `arith`, e.g. [`mul_q15`][mulq15]; [`hbdecim_q15` output][hbout]                                                                      | [CIC output shift][cicdec]                                       |
| F   | filter coefficients (design time)   | clamp, `lround`                                                                  | [`hbdecim_q15` coefficients][hbcoef]                                                                                                  | none                                                             |

Kind A already has a gate, [`check_phase_conversion_sites.py`][gate]: any
2³² constant outside `nco_core.h` fails unless it's listed in its
[allowlist][allow], and that list may only shrink. Kinds C and E have no
equivalent gate.

The cvt vector loops (`f32_to_*_steps`) all call the scalar step, so a
vector path can't round differently from its scalar one. The ADC below is
the one family where it does.

## The four violations

1. **symsync: a private, truncating phase conversion with undefined behaviour
    at `sps = 1`.** [`nominal_inc`][sym] is `(uint32_t)(4294967296.0 / s)`,
    and [`symsync_init`][symsps] turns `sps = 0` into 1. At 1 that cast is
    undefined: x86 gives 0, arm64 gives `0xFFFFFFFF`. The gate's
    [allowlist][allow] already marks it "VIOLATION", and `nco_core.h` cites it
    as one of the cases that motivated a single home. It was never fixed, and
    no issue tracks it. Proposed fix: refuse `sps < 2`. A Gardner timing
    detector needs two samples per symbol anyway, and removing the cast
    shrinks the allowlist by one.

1. **The CIC encoder truncates.** [`cic_core.h`][cicenc] clamps, then converts
    with a bare `(int16_t)sr`. It's a private copy of
    [`f32_to_uq15_step`][f2uq15] (the same offset-binary encoding, plus
    headroom) that truncates where that function rounds: the 6 dB defect #1117
    fixed everywhere else. [QUANTIZATION.md §2.4][q24] documents this
    truncation, while [§3.1][q31] says the Q15 encoder rounds. Proposed fix:
    call `f32_to_uq15_step` with scale `32768 / CIC_PAPR_HEADROOM`.

1. **The CIC output shift floors.** The [decoder][cicdec] computes
    `(uint16_t)(re >> shift)` with no rounding bias. Because the value is
    offset-binary, that floor is a −½ LSB DC offset on the signed value.
    Proposed fix: add `1 << (shift − 1)` before the shift, the same form as
    kind E. Both CIC fixes change its numbers, so its validation evidence
    has to be regenerated.

1. **The ADC's vector and scalar paths disagree.** With dither off,
    [`adc_steps`][adcv] computes its vector body in **float** (`llroundf` of a
    float product), while the tail and [`adc_step`][adcs] compute in
    **double**. They also set `clipped` at different points: the vector path
    after rounding, the scalar path before. So a sample's code depends on
    where it falls in the block. Measured with two million uniform samples:

    | bits | dBFS | samples that differ | largest difference |
    | ---- | ---- | ------------------- | ------------------ |
    | 8–24 | 0.0  | 0                   | 0                  |
    | 12   | −3.0 | 0.005%              | 1 LSB              |
    | 16   | −3.0 | 0.070%              | 1 LSB              |
    | 20   | −3.0 | 1.134%              | 1 LSB              |
    | 24   | −3.0 | 18.079%             | 1 LSB              |

    At 0 dBFS the scale is a power of two, so float and double agree exactly.
    Any other dBFS gives a scale float can't represent exactly. Proposed fix:
    do the vector multiply in double, and set `clipped` from the same value in
    both paths.

    ```text
    # the measurement, emulating both paths in numpy
    x   = uniform(-1, 1, 2e6) as float32
    sc  = 2^(bits-1) * 10^(-dBFS/20)
    ref = llround(double(sc) * double(x))     # adc_step and the tail
    vec = llround(float(sc) * x)              # the SIMD body
    count(clip(ref) != clip(vec))
    ```

Related, found in the same review: two of the twelve `cvt` cores abort on
out-of-memory where the other ten raise `MemoryError`
([doppler#1380](https://github.com/doppler-dsp/doppler/issues/1380)), and the
case for writing each converter family's kernel once is on
[just-makeit#1310](https://github.com/just-buildit/just-makeit/issues/1310).

## Documentation today

The rule is spread across three pages, and none of them covers every kind:

- [QUANTIZATION.md](../design/QUANTIZATION.md) is from July, before #1117,
    and it documents the CIC truncation next to a formula that rounds
    ([§2.4][q24] vs [§3.1][q31]).
- [Fixed-point guide §8](../guide/fixed-point.md#8-truncation-vs-rounding)
    covers only kind E.
- [NCO design §7](../design/nco.md#7-why-truncation-precisely) covers only
    kind A.

## Proposal

1. **One "Conversion rules" section in QUANTIZATION.md**, built on the table
    above: each kind, its rule, why, and the one function that implements it.
    The fixed-point guide and the NCO page link to it instead of restating
    it.
1. **One fix per violation**, as described above. The two CIC changes move
    its numbers and need its validation evidence regenerated.
1. **Gates for kinds C and E**, so they have the same single-home check kind
    A has, with an allowlist that can only shrink.

## Open decision: how ties break

The float quantisers (kind C) break ties **away from zero** (`lroundf`). The
integer requantisers (kind E) break them **upward** (`+ 2^(n−1)`).

- **Recommended: keep both and document why.** Each is the standard choice for
    its domain. With float input an exact tie is rare, and `lroundf` is
    symmetric about zero. With integer input a tie happens once every 2^n
    values, and half-up is the one-add form every DSP uses, at a cost of at
    most 2^−(n+1) LSB of bias.
- **Alternative: round half away from zero everywhere.** One rule, no bias,
    at the cost of a sign-dependent branch or select in every integer
    requantiser.

[adcs]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/adc/adc_core.h#L160-L176
[adcv]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/adc/adc_core.c#L51-L93
[allow]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/scripts/.phase-conversion-allow
[cicdec]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/cic/cic_core.h#L333-L339
[cicenc]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/cic/cic_core.h#L286-L298
[dll1]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/dll/dll_core.h#L348
[dll2]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/dll/dll_core.h#L390
[f2i16]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/f32_to_i16/f32_to_i16_core.h#L128-L136
[f2uq15]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/f32_to_uq15/f32_to_uq15_core.h#L136-L145
[gate]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/scripts/check_phase_conversion_sites.py
[hbcoef]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/hbdecim_q15/hbdecim_q15_core.c#L243-L248
[hbout]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/hbdecim_q15/hbdecim_q15_core.c#L186-L196
[i2f]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/i16_to_f32/i16_to_f32_core.h#L109
[lolut]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/lo/lo_core.c#L195-L197
[mulq15]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/arith/mul_q15.c#L13-L14
[nfi]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/nco/nco_core.h#L352
[nmax]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/nco/nco_core.h#L494
[pu]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/nco/nco_core.h#L149-L160
[pum]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/nco/nco_core.h#L198-L206
[q24]: ../design/QUANTIZATION.md#24-cast-chains-used-in-this-codebase
[q31]: ../design/QUANTIZATION.md#31-encoder
[reader]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/wfm_reader/wfm_reader_core.c#L151
[resamp]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/resamp/resamp_core.c#L167-L168
[sink]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/wfm/wfm_sink.c#L70
[stream]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/stream/stream_core.c#L174
[sym]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/symsync/symsync_core.c#L137-L141
[symsps]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/symsync/symsync_core.c#L211
[w2n]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/inc/doppler/nco/nco_core.h#L227
[writer]: https://github.com/doppler-dsp/doppler/blob/51d34a292f7607788ab69c0cb5bc78d7b167d271/native/src/wfm_writer/wfm_writer_core.c#L185
