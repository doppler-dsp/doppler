/**
 * @file cic_core.h
 * @brief CIC decimation filter — CIC_N stages, one-sample combs, an integer
 *        pipeline in CIC_REG_BITS-bit registers.
 *
 * **INPUT AMPLITUDE IS BOUNDED** by the filter's full scale,
 * dp_cic_get_full_scale(): CIC_PAPR_WIDE times unit amplitude as built. A
 * component at or beyond it is CLIPPED at the boundary, before any
 * filtering happens.  Unlike the library's floating-point blocks this one is
 * not scale-free — it is the one place where turning the input gain up
 * changes the answer — and the clip is silent in the sample stream: no
 * error, no NaN, just a degraded output that looks plausible.
 *
 * The input is an integer word with two ends, and a filter's **precision**
 * (dp_cic_set_precision()) names the word:
 *
 * | precision                    | word                                  | full scale, as built |
 * | ---------------------------- | ------------------------------------- | -------------------- |
 * | CIC_PRECISION_16             | CIC_PRECISION_16 bits                 | CIC_PAPR_16          |
 * | CIC_PRECISION_32             | CIC_PRECISION_32 bits, or what fits   | CIC_PAPR_WIDE        |
 * | CIC_PRECISION_64 (as built)  | all the register has left             | CIC_PAPR_WIDE        |
 *
 * - **The top is the headroom.** Full scale is a power of two above unit
 *   amplitude (dp_cic_set_headroom_bits()), because a signal's peaks are
 *   above its average: by 4 dB for a root-raised-cosine symbol stream and
 *   by 10 to 12 dB for a multi-carrier one.
 * - **The bottom is the step,** dp_cic_get_step(): a component smaller than
 *   one step reads zero. The register holds the word and the pipeline's
 *   gain of `CIC_N * log2(R)` bits, so the widest word is `CIC_REG_BITS -
 *   CIC_N * log2(R)` bits; a precision that asks for more gets that. What
 *   the headroom does not take is precision, so each bit of headroom
 *   doubles the step.
 *
 * **So check @c clipped** — a sticky flag raised by any saturating component
 * and cleared by dp_cic_reset(), following the same convention as the
 * quantizing `cvt` converters (adc, f32_to_uq15, ...).  It is free: the four
 * boundary comparisons run on every sample regardless, so recording that one
 * fired costs a register OR.  There is no reason to run a CIC without
 * checking it at least once against real input.
 *
 * (Why the input is bounded at all: the pipeline is integer, so the CF32
 * boundary is quantized.  `docs/design/cic.md` is the budget.)
 *
 * Fixed design parameters:
 *   CIC_N stages   (~19.2 dB of alias rejection a stage at f_p = 0.1 * f_out)
 *   one-sample combs (differential delay 1)
 *   R = power-of-two decimation ratio, up to CIC_R_MAX (enforced at create)
 *
 * Input/output boundary: CF32 (`float _Complex`), matching the doppler
 * default signal type.  Internally each sample is a signed integer, the
 * word above, held in a register modulo 2^CIC_REG_BITS.  The pipeline's
 * gain is `CIC_N * log2(R)` bits, and the largest sum the output decode
 * forms is `(2^word - 1) * R^CIC_N`, which is under 2^CIC_REG_BITS for
 * every word a precision gives.  At CIC_PRECISION_64 the word and the gain
 * are the register exactly: nothing is left over, and nothing needs to be,
 * since the pipeline is exact while the true result fits, and it does.
 *
 * **Every register is unsigned**, because the pipeline depends on its
 * registers wrapping and in C only an unsigned type promises that (C99
 * 6.2.5p9): a signed one that overflows is undefined. The sample goes in as
 * its value modulo 2^CIC_REG_BITS (offset binary, then the offset off again, both
 * unsigned), and comes out by adding the offset back at the pipeline's gain
 * before the normalising shift, so what is shifted is never negative. The
 * offset is carried around the pipeline and not through it, which is why a
 * filter that has only seen silence holds zeros and gives zeros: its first
 * outputs after create or reset are the filter filling with the input, and
 * nothing else.
 *
 * The unsigned modular-arithmetic CIC property guarantees exact outputs:
 * every intermediate overflow in the integrators cancels in the comb
 * stages, provided the true result fits in the register.  So the integrator/comb
 * pipeline itself needs no saturation, no range checks and no
 * floating-point — the one saturation in the block is at the CF32 encoder,
 * and it is the input bound described at the top of this file, not an
 * arithmetic guard. A NaN component is taken as over the bound: it reads as
 * the top of the range and raises @c clipped.
 *
 * With one-sample combs and CIC_N fixed, the entire comb state is CIC_N
 * registers per channel — no heap allocation beyond the state struct itself.
 *
 * Alias rejection : ~77 dB at f_p = 0.1 * f_out, CIC_N stages (independent of R)
 * Passband droop  : ~0.57 dB at f_p = 0.1 * f_out (independent of R)
 * Output precision: CF32, so a float's 24 bits of the input word
 *
 * @code
 * dp_cic_state_t *cic = dp_cic_create(16);   // R = 16
 * float _Complex  in[1024] = { 0 }, out[64];
 * size_t n_out = dp_cic_decimate(cic, in, 1024, out, 64);   // 64
 * dp_cic_destroy(cic);
 * @endcode
 */
#ifndef DP_CIC_CORE_H
#define DP_CIC_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed stage count.  Alias rejection ~19.2 dB/stage at f_p=0.1. */
#define CIC_N 4

/** Bits in a pipeline register: the input word and the pipeline's gain of
 *  `CIC_N * log2(R)` bits both come out of these. */
#define CIC_REG_BITS 64u

/** @name Precisions: the input word a filter is built for.
 *  @{ */
/** A 16-bit word at every ratio. With CIC_PAPR_16 it is the filter this
 *  one was before it had a precision, output for output. */
#define CIC_PRECISION_16 16u
/** A 32-bit word, or the widest the register has left when the ratio's
 *  gain leaves less than that (past R = 256). */
#define CIC_PRECISION_32 32u
/** The widest word the register has left: `CIC_REG_BITS - CIC_N *
 *  log2(R)` bits. What dp_cic_create() builds. */
#define CIC_PRECISION_64 64u
/** The precision dp_cic_create() builds. */
#define CIC_PRECISION_DEFAULT CIC_PRECISION_64
/** @} */

/** @name Headroom: full scale over unit amplitude, a power of two.
 *  @{ */
/** Bits of headroom CIC_PRECISION_16 starts with: full scale CIC_PAPR_16. */
#define CIC_HEADROOM_BITS_16 1u
/** Bits of headroom CIC_PRECISION_32 and CIC_PRECISION_64 start with:
 *  full scale CIC_PAPR_WIDE, 12 dB, a multi-carrier signal's peaks. */
#define CIC_HEADROOM_BITS_WIDE 2u
/** Full scale at CIC_HEADROOM_BITS_16, as a voltage ratio. */
#define CIC_PAPR_16 ((float)(1u << CIC_HEADROOM_BITS_16))
/** Full scale at CIC_HEADROOM_BITS_WIDE, as a voltage ratio. */
#define CIC_PAPR_WIDE ((float)(1u << CIC_HEADROOM_BITS_WIDE))
/** @name The word's two ends, as floats, for a width @p b known when the
 *  kernel is compiled (dp_cic_decimate()).
 *  @{ */
/** The largest float the word holds: `2^(b - 1) - 1` while a float can
 *  hold that (24 bits of mantissa: `b` up to 25), and past that the float
 *  under `2^(b - 1)`. A sample scaled past it is a clip. */
#define CIC_WORD_TOP(b)                                                    \
    ((b) <= 25 ? (float)((1ull << ((b) - 1)) - 1u)                         \
               : (float)(1ull << ((b) - 1)) * (1.0f - 0x1p-24f))
/** The smallest: `-2^(b - 1)`, a float exactly. */
#define CIC_WORD_BOTTOM(b) (-(float)(1ull << ((b) - 1)))
/** Every width a word can have: a precision's, or what the register has
 *  left after a power-of-two ratio's gain, `CIC_REG_BITS - CIC_N *
 *  log2(R)`. The kernel is compiled once for each. */
#define CIC_WORD_WIDTHS(X)                                                 \
    X(16) X(20) X(24) X(28) X(32) X(36) X(40) X(44) X(48) X(52) X(56) X(60)
/** @} */

/** A word keeps its sign and at least this many bits under unity. */
#define CIC_FRACTION_BITS_MIN 1u
/** The most headroom any word takes: the narrowest word, CIC_PRECISION_16
 *  bits, less its sign and CIC_FRACTION_BITS_MIN. */
#define CIC_HEADROOM_BITS_MAX (CIC_PRECISION_16 - 1u - CIC_FRACTION_BITS_MIN)
/** @} */

/**
 * @brief Largest decimation ratio a CIC will be built at.
 *
 * The register is CIC_REG_BITS bits and the pipeline's gain takes `CIC_N *
 * log2(R)` of them. At the cap that leaves a word of CIC_REG_BITS -
 * CIC_N * log2(CIC_R_MAX) bits, still wider than CIC_PRECISION_16; one
 * doubling further it would be CIC_PRECISION_16 exactly, with nothing for
 * the wider precisions to add. Past the cap the planner hands the residual
 * to the resampler stage.
 */
#define CIC_R_MAX 2048u

/**
 * @brief CIC filter state.
 *
 * Allocate with dp_cic_create(); free with dp_cic_destroy().
 * All comb state fits in-struct — no heap members.
 */
typedef struct {
    uint64_t integ_re[CIC_N]; /**< Integrator accumulators, real path
                                   (the sample, wrapping mod 2^64);
                                   running state. */
    uint64_t integ_im[CIC_N]; /**< Integrator accumulators, imag path;
                                   running state. */
    uint64_t comb_re[CIC_N];  /**< Previous comb input per stage, real
                                   path; running state. */
    uint64_t comb_im[CIC_N];  /**< Previous comb input per stage, imag
                                   path; running state. */
    uint32_t R;               /**< Decimation ratio, a power of two in
                                   `[2, CIC_R_MAX]`; config, set by create
                                   and dp_cic_reconfigure(). */
    uint32_t phase;           /**< Input samples since the last output,
                                   0..R-1; running state. */
    uint32_t shift;           /**< Normalizing right-shift in bits,
                                   `CIC_N * log2(R)`; derived from R. */
    uint32_t precision;       /**< CIC_PRECISION_16, _32 or _64; config,
                                   set by create (CIC_PRECISION_DEFAULT)
                                   and dp_cic_set_precision(). */
    uint32_t headroom_bits;   /**< Bits of the input word above unit
                                   amplitude; config, set with the
                                   precision and by
                                   dp_cic_set_headroom_bits(). */
    uint32_t bits;            /**< The input word's width, sign included:
                                   the precision, or `CIC_REG_BITS - shift`
                                   when that is less; derived. */
    uint64_t offset;          /**< The word's offset-binary midpoint,
                                   `2^(bits - 1)`; derived. */
    float    in_scale;        /**< Unit amplitude as an integer,
                                   `2^(bits - 1 - headroom_bits)`; derived. */
    float    out_scale;       /**< `1 / in_scale`, the step; derived. */
    uint8_t  clipped;         /**< Sticky: 1 once an input component
                                   reached the bound
                                   (dp_cic_get_full_scale());
                                   cleared by reset/reconfigure, and
                                   overwritten by set_state(). */
} dp_cic_state_t;

/**
 * @brief Create a CIC decimation filter of CIC_N stages.
 * Allocates the state struct on the heap and pre-computes the
 * normalisation right-shift (CIC_N * log2(R) bits). All integrator
 * and comb accumulators are zeroed; the first output arrives after
 * R input samples. Returns NULL for invalid R or OOM.
 * The filter is built at CIC_PRECISION_DEFAULT with that precision's
 * headroom, so its input is bounded at CIC_PAPR_WIDE
 * (dp_cic_set_precision(), dp_cic_set_headroom_bits()). A component at or
 * beyond it is clipped at the boundary before any filtering; the sample
 * stream gives no sign of it, so check the sticky @c clipped flag.
 * Unlike doppler's floating-point blocks this one is not scale-free --
 * scale the input into range first.
 *
 * @param R  Decimation ratio.  Must be a power of two from 2 to
 *           CIC_R_MAX.  Returns NULL for R=0, non-power-of-two, or a
 *           ratio above that cap.
 * @return   Heap-allocated state, or NULL on invalid R or OOM.
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> cic = CIC(R=16)
 * >>> cic.R, cic.shift
 * (16, 16)
 * @endcode
 */
dp_cic_state_t *dp_cic_create(uint32_t R);

/**
 * @brief Free the filter state.
 * @param state  State from dp_cic_create(); NULL is a no-op.
 */
void dp_cic_destroy(dp_cic_state_t *state);

/**
 * @brief Zero all integrator and comb accumulators; preserve R and shift.
 * The first output sample after reset arrives after R more input samples,
 * matching post-create behaviour. Use between signal bursts to eliminate
 * transient artefacts caused by residual pipeline state. Also clears the
 * sticky @c clipped flag.
 *
 * @param state  Pointer to a valid dp_cic_state_t.
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> cic = CIC(R=16)
 * >>> cic.reset()
 * >>> cic.R
 * 16
 * @endcode
 */
void dp_cic_reset(dp_cic_state_t *state);

/* Serializable state (reusable elastic-resume convention): the decimation
 * rate R, the precision and the headroom, the integrator and comb
 * accumulators, the decimation phase counter and the sticky clip flag.  R,
 * the precision and the headroom travel because dp_cic_reconfigure(),
 * dp_cic_set_precision() and dp_cic_set_headroom_bits() change
 * them (#2022's rule, doppler#2642); the shift, the word and its scales are
 * derived from the three on restore, so they cannot disagree.  `clipped` is
 * running state, not a diagnostic bolted on the side: a resumed stream that
 * silently forgot it had clipped would answer the question wrongly. */

/* Standard bytes interface (see dp_state.h): [dp_state_hdr_t][u32 R]
 * [u32 precision][u32 headroom_bits][integ_re/im, comb_re/im (CIC_N u64
 * each)][u32 phase][u8 clipped]. */
/** FourCC type tag ("CIC_") stamped in a CIC state blob's header. */
#define CIC_STATE_MAGIC DP_FOURCC ('C', 'I', 'C', '_')
/** Layout version of the CIC state blob (2 added @c clipped, 3 added R; 4:
 *  the registers hold the sample without its offset; 5: the precision and
 *  the headroom travel, and the word is theirs). */
#define CIC_STATE_VERSION 5u

/**
 * @brief Is this a precision: CIC_PRECISION_16, CIC_PRECISION_32 or
 *        CIC_PRECISION_64.
 * @param precision  The candidate.
 * @return 1 for one of the three, 0 otherwise.
 */
int dp_cic_precision_ok(uint32_t precision);

/**
 * @brief The headroom a precision starts with: CIC_HEADROOM_BITS_16 for
 *        CIC_PRECISION_16, CIC_HEADROOM_BITS_WIDE for the other two.
 *
 * The ONE rule: dp_cic_create(), dp_cic_set_precision() and every owner
 * that builds or configures a CIC (dp_RateConverter_set_cic_precision(),
 * dp_RateConverter_create()) take the headroom from here.
 *
 * @param precision  A precision dp_cic_precision_ok() accepts.
 * @return Bits of headroom.
 */
uint32_t dp_cic_precision_headroom(uint32_t precision);

/**
 * @brief Is this a headroom a CIC takes: 0 to CIC_HEADROOM_BITS_MAX bits.
 *
 * The ONE predicate: dp_cic_set_headroom_bits() and dp_cic_set_state()
 * refuse outside it, and a composition that takes the figure from a caller
 * checks it first.
 *
 * @param bits  Bits of the input word above unit amplitude.
 * @return 1 inside the domain, 0 outside it.
 */
int dp_cic_headroom_bits_ok(uint32_t bits);

/**
 * @brief Choose the input word, with that word's headroom, and reset the
 *        filter.
 *
 * CIC_PRECISION_16 is a 16-bit word with full scale CIC_PAPR_16: the filter
 * as it was before it had a precision, output for output.
 * CIC_PRECISION_32 is a 32-bit word and CIC_PRECISION_64 the widest the
 * register has left, both with full scale CIC_PAPR_WIDE; where the ratio's
 * gain leaves less than 32 bits the two are the same word
 * (dp_cic_get_bits()). The headroom is then dp_cic_set_headroom_bits()'s to
 * change.
 *
 * The registers hold samples at the old scale, so the filter is reset, as
 * dp_cic_reconfigure() resets it. A @p precision that is not one of the
 * three is refused and nothing changes.
 *
 * @param state      Pointer to a valid dp_cic_state_t.
 * @param precision  CIC_PRECISION_16, CIC_PRECISION_32 or CIC_PRECISION_64.
 * @return DP_OK, or DP_ERR_INVALID with nothing changed.
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> cic = CIC(R=64)
 * >>> cic.precision, cic.bits, cic.full_scale, cic.step
 * (64, 40, 4.0, 7.275957614183426e-12)
 * >>> cic.precision = 32
 * >>> cic.bits, cic.full_scale, cic.step
 * (32, 4.0, 1.862645149230957e-09)
 * >>> cic.precision = 16
 * >>> cic.bits, cic.full_scale, cic.step
 * (16, 2.0, 6.103515625e-05)
 * @endcode
 */
int dp_cic_set_precision(dp_cic_state_t *state, uint32_t precision);

/** @brief The precision in use.
 *  @param state  Pointer to a valid dp_cic_state_t.
 *  @return CIC_PRECISION_16, CIC_PRECISION_32 or CIC_PRECISION_64. */
uint32_t dp_cic_get_precision(const dp_cic_state_t *state);

/** @brief The input word's width in bits, sign included.
 *  @param state  Pointer to a valid dp_cic_state_t.
 *  @return The precision, or `CIC_REG_BITS - CIC_N * log2(R)` when the
 *          register has less than that left. */
uint32_t dp_cic_get_bits(const dp_cic_state_t *state);

/**
 * @brief Set how much of the input word is headroom, and reset the filter.
 *
 * @p bits of the word sit above unit amplitude, so full scale is `2^bits`
 * (dp_cic_get_full_scale()), and the rest is precision, so the step is
 * `2^bits / 2^(word - 1)` (dp_cic_get_step()): every bit of headroom is
 * 6 dB more room for peaks and 6 dB more of step. dp_cic_set_precision()
 * puts it back to the precision's own.
 *
 * The registers hold samples at the old scale, so the filter is reset. A
 * @p bits outside dp_cic_headroom_bits_ok() is refused and nothing changes.
 *
 * @param state  Pointer to a valid dp_cic_state_t.
 * @param bits   Bits of headroom, 0 to CIC_HEADROOM_BITS_MAX.
 * @return DP_OK, or DP_ERR_INVALID with nothing changed.
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> cic = CIC(R=64)
 * >>> cic.headroom_bits, cic.full_scale
 * (2, 4.0)
 * >>> cic.headroom_bits = 4       # 24 dB
 * >>> cic.full_scale, cic.step
 * (16.0, 2.9103830456733704e-11)
 * @endcode
 */
int dp_cic_set_headroom_bits(dp_cic_state_t *state, uint32_t bits);

/** @brief The headroom in use, in bits.
 *  @param state  Pointer to a valid dp_cic_state_t.
 *  @return Bits of the input word above unit amplitude. */
uint32_t dp_cic_get_headroom_bits(const dp_cic_state_t *state);

/** @brief The input bound: a component at or beyond it is clipped.
 *
 *  Exactly: a component is clipped when it scales past the word's top
 *  float, CIC_WORD_TOP(), or below its bottom. For a word wider than a
 *  float's mantissa that is at full scale; for one of 25 bits or fewer the
 *  top is `2^(bits - 1) - 1`, so a component in the last step under full
 *  scale is clipped too, which is the filter as it was at 16 bits.
 *  @param state  Pointer to a valid dp_cic_state_t.
 *  @return `2^headroom_bits`, as a voltage ratio over unit amplitude. */
double dp_cic_get_full_scale(const dp_cic_state_t *state);

/** @brief The input's step: a component smaller than this reads zero, and
 *         the output is a whole number of them.
 *  @param state  Pointer to a valid dp_cic_state_t.
 *  @return `full_scale / 2^(bits - 1)`, in the input's units. */
double dp_cic_get_step(const dp_cic_state_t *state);

/**
 * @brief Bytes dp_cic_get_state() writes (envelope + payload).
 * @param state  CIC state (unused: the size is fixed).
 * @return `sizeof(dp_state_hdr_t) + 4 + 4 * CIC_N * 8 + 4 + 1` bytes, the
 *         same for every R.
 */
size_t dp_cic_state_bytes(const dp_cic_state_t *state);
/**
 * @brief Serialize R, the integrator/comb/phase state and @c clipped into
 *        @p blob. shift is not written: it is derived from R.
 * @param state  Pointer to a valid dp_cic_state_t.
 * @param blob   Destination; at least dp_cic_state_bytes() bytes.
 */
void dp_cic_get_state(const dp_cic_state_t *state, void *blob);
/**
 * @brief Restore R, the integrator/comb/phase state and @c clipped from
 *        @p blob.
 *
 * R is the writer's, as dp_cic_reconfigure() would set it, and shift is
 * recomputed from it, so a blob restores into a CIC built with any R
 * (doppler#2642).  A blob whose R is not one dp_cic_create() accepts, or
 * whose phase is not below that R, is refused and @p state is unchanged.
 *
 * @param state  Pointer to a valid dp_cic_state_t.
 * @param blob   A blob from dp_cic_get_state().
 * @return DP_OK, or DP_ERR_INVALID if the envelope rejects, R is invalid or
 *         the phase is not below R.
 */
int dp_cic_set_state(dp_cic_state_t *state, const void *blob);

/**
 * @brief Upper bound on decimate output — returns 0 (lazy-alloc signal).
 *
 * The Python extension allocates n_in elements on the first call.
 * Since n_in >= ceil(n_in/R) = n_out for all R >= 1, the buffer is
 * always large enough as long as block size stays consistent.
 *
 * @param state  CIC state (unused).
 * @return Always 0, meaning "no fixed cap": size the output to the input
 *         length.
 */
size_t dp_cic_decimate_max_out(dp_cic_state_t *state);

/**
 * @brief The filter's response to a constant input, from its own geometry.
 *
 * A CIC's pipeline gain is `R^N`, and this implementation removes it with a
 * right-shift of `N*log2(R)` bits, so the DC gain is `R^N / 2^shift` — one
 * exactly, whenever the shift matches R. Computed from `R` and the stored
 * shift rather than measured, so a mismatch between the two is visible
 * without running a signal through the filter.
 *
 * @param state State. Must be non-NULL.
 * @return The DC gain. 1.0 for every power-of-two R the filter accepts.
 *
 * @code
 * dp_cic_state_t *c = dp_cic_create (32);
 * printf ("%.4f\n", dp_cic_dc_gain (c));   // 1.0000
 * dp_cic_destroy (c);
 * @endcode
 */
double dp_cic_dc_gain(const dp_cic_state_t *state);

/**
 * @brief The decimator's group delay, in its INPUT samples: where in the
 *        input timeline an output sample's content sits.
 *
 * Output `k` of a fresh stream carries the input at index
 * `k * R - group_delay`. From the structure, not measured: the response is
 * CIC_N boxcars of `R` inputs in a row, `CIC_N * (R - 1) + 1` inputs long
 * and symmetric, so its center is `CIC_N * (R - 1) / 2` behind the newest
 * input; and output `k` is formed as input `k * R + R - 1` arrives, which
 * is `R - 1` after the instant `k * R`. The difference is the delay, the
 * same at every frequency.
 *
 * @param state State. Must be non-NULL.
 * @return `(R - 1) * (CIC_N - 2) / 2`, in input samples: `R - 1` at
 *         CIC_N 4.
 *
 * @code
 * dp_cic_state_t *c = dp_cic_create (8);
 * printf ("%.1f\n", dp_cic_get_group_delay (c));   // 7.0
 * dp_cic_destroy (c);
 * @endcode
 */
double dp_cic_get_group_delay(const dp_cic_state_t *state);

#ifndef __cplusplus
/**
 * @brief dp_cic_decimate() for a word of @p b bits: the kernel.
 *
 * Force-inlined, and called only with a literal @p b, so each width is
 * compiled with its two ends as constants (CIC_WORD_TOP(),
 * CIC_WORD_BOTTOM()). That is what makes the clamp branch-free: with the
 * ends in variables the compiler cannot rule out that one is NaN and keeps
 * a branch per comparison, and the loop ran at half the rate (measured on
 * bench_cic_core, R = 32). dp_cic_decimate() picks the width.
 *
 * @param state    As dp_cic_decimate().
 * @param in       As dp_cic_decimate().
 * @param n_in     As dp_cic_decimate().
 * @param out      As dp_cic_decimate().
 * @param max_out  As dp_cic_decimate().
 * @param b        The word's width, `state->bits`, as a literal.
 * @return As dp_cic_decimate().
 */
JM_FORCEINLINE JM_HOT size_t
cic_decimate_word(dp_cic_state_t *state, const float _Complex *in,
                  size_t n_in, float _Complex *out, size_t max_out,
                  const unsigned b)
{
    const uint32_t R      = state->R;
    const uint32_t shift  = state->shift;
    const float    scale  = state->in_scale;
    const float    hi     = CIC_WORD_TOP(b);
    const float    lo     = CIC_WORD_BOTTOM(b);
    const uint64_t offset = (uint64_t)1 << (b - 1u);
    /* Read once: `out` is a float array and the compiler must otherwise
       assume each output it stores could be this field, and reload it. */
    const float    step   = state->out_scale;
    const uint64_t bias   = offset << shift;
    size_t n_out = 0;
    int    clip  = 0;   /* accumulated in a register; folded in once below */
    /* The filter's registers, in locals for the whole call and written back
       once at the end: the loop then touches no memory but the input and
       the output, so where the state sits beside the buffers cannot slow
       it (measured: storing them every sample, one benchmark program ran
       the converter's CIC plan a quarter slower than another with the
       same objects). */
    uint64_t ir[CIC_N], ii[CIC_N], cr[CIC_N], ci[CIC_N];
    for (int k = 0; k < CIC_N; k++) {
        ir[k] = state->integ_re[k];
        ii[k] = state->integ_im[k];
        cr[k] = state->comb_re[k];
        ci[k] = state->comb_im[k];
    }
    uint32_t phase = state->phase;

    for (size_t i = 0; i < n_in; i++) {
        /* CF32 → the word, saturated. The four comparisons run regardless,
           so noting that one fired costs a register OR — which is the whole
           reason `clipped` exists rather than a line of documentation
           asking callers to be careful. The upper one is written as "not
           at or under", so a NaN, which fails every comparison, lands on
           the top of the range and is counted as a clip: the conversion to
           an integer below is never handed a value it cannot hold. */
        float sr = crealf(in[i]) * scale;
        float si = cimagf(in[i]) * scale;
        if (!(sr <= hi)) { sr = hi; clip = 1; }
        if (sr < lo)     { sr = lo; clip = 1; }
        if (!(si <= hi)) { si = hi; clip = 1; }
        if (si < lo)     { si = lo; clip = 1; }
        /* Into the unsigned registers as the value itself, modulo 2^64:
           through offset binary, which is never negative (the word is
           under CIC_REG_BITS - 1 bits, so the sum cannot leave an int64),
           and the offset straight back off in unsigned arithmetic. No
           register here is signed. A zero sample is a zero word, so a
           filter that has only seen silence holds zeros. */
        uint64_t re = (uint64_t)((int64_t)sr + (int64_t)offset) - offset;
        uint64_t im = (uint64_t)((int64_t)si + (int64_t)offset) - offset;

        /* CIC_N integrators — unsigned wrap-around is intentional. The
           count is a constant, so the loops below unroll. */
        for (int k = 0; k < CIC_N; k++) {
            re = ir[k] += re;
            im = ii[k] += im;
        }

        if (++phase < R)
            continue;
        phase = 0;

        /* CIC_N one-sample combs: y = x - prev; prev = x. */
        for (int k = 0; k < CIC_N; k++) {
            uint64_t t;
            t = cr[k]; cr[k] = re; re -= t;
            t = ci[k]; ci[k] = im; im -= t;
        }

        /* Capacity reached: keep running the filter (the integrator and
           comb state must stay in step with the input stream) but stop
           writing.  Guarding the store, not the loop, is the difference
           between a truncated block and a corrupted filter. */
        if (n_out >= max_out)
            continue;

        /* → CF32. The registers hold the filtered sum modulo 2^64, which
           is negative as often as not. Adding the offset at the pipeline's
           gain makes it the offset-binary sum, in [0, 2^b * R^CIC_N),
           which fits; the right shift then normalises it and floors it,
           an unsigned shift of a non-negative value. What is left is a
           word, so it and the offset are both in an int64 and their
           difference is the sample. */
        out[n_out++] = CMPLXF(
            (float)((int64_t)((re + bias) >> shift) - (int64_t)offset)
                * step,
            (float)((int64_t)((im + bias) >> shift) - (int64_t)offset)
                * step);
    }
    for (int k = 0; k < CIC_N; k++) {
        state->integ_re[k] = ir[k];
        state->integ_im[k] = ii[k];
        state->comb_re[k]  = cr[k];
        state->comb_im[k]  = ci[k];
    }
    state->phase = phase;
    state->clipped |= (uint8_t)clip;   /* sticky; cleared by reset() */
    return n_out;
}

/** One case of dp_cic_decimate()'s width switch: the kernel with @p b as
 *  a literal. */
#define CIC_WORD_CASE(b)                                                   \
    case b:                                                                \
        return cic_decimate_word(state, in, n_in, out, max_out, b);

/**
 * @brief Decimate a block of CF32 samples through the CIC pipeline.
 * Each sample is converted to the filter's integer word
 * (dp_cic_get_bits()), pushed through CIC_N
 * integrators (unsigned wrapping), and when the phase counter reaches R
 * the integrated value is passed through CIC_N one-sample combs and
 * converted back to CF32.  State persists between calls. A fresh or reset
 * filter starts from silence: its first CIC_N - 1 outputs are the filter
 * filling with the input, and silence in gives exact zeros out from the
 * first one.
 * Feeding blocks that are multiples of R gives predictable output
 * counts (exactly n_in/R samples per block).
 *
 * @note **Input amplitude is bounded** by dp_cic_get_full_scale(), 4.0 as
 * built: a component scaled past the word's top, or below its bottom, is
 * clipped at the boundary before filtering; the sample stream gives no
 * sign of it, so check the sticky @c clipped flag. Scale the input into
 * range first; see the file header.
 *
 * @param state  Pointer to a valid dp_cic_state_t.
 * @param in     CF32 input block, inside the full scale (clipped otherwise).
 * @param n_in   Number of input samples.
 * @param out    Output buffer; must hold at least max_out elements.
 * @param max_out Capacity of @p out in samples.  Normally n_in (the loosest
 *               bound: at most one output per input).  If it is smaller the
 *               integrators and combs still advance over every input sample
 *               -- the pipeline is a running filter and cannot be left
 *               half-fed -- but emission stops, so the samples past the
 *               capacity are dropped rather than written past the end.
 * @return       Number of CF32 samples written to @p out:
 *               min(floor((phase + n_in) / R), max_out), where phase is
 *               the input count carried from the previous call.
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> import numpy as np
 * >>> cic = CIC(R=16)
 * >>> y = cic.decimate(np.zeros(64, dtype=np.complex64))
 * >>> y.tolist(), y.dtype                 # silence in, zeros from the first
 * ([0j, 0j, 0j, 0j], dtype('complex64'))
 * >>> ones = cic.decimate(np.ones(96, dtype=np.complex64))
 * >>> [round(float(v.real), 3) for v in ones]  # then the filter fills
 * [0.059, 0.562, 0.972, 1.0, 1.0, 1.0]
 * @endcode
 */
JM_FORCEINLINE JM_HOT size_t
dp_cic_decimate(dp_cic_state_t *state, const float _Complex *in,
             size_t n_in, float _Complex *out, size_t max_out)
{
    /* One call of the kernel for each width, each with its width as a
       literal: a width the switch does not name is one no filter has
       (CIC_WORD_WIDTHS() is every one derive() can give). */
    switch (state->bits) {
        CIC_WORD_WIDTHS(CIC_WORD_CASE)
    default:
        return 0;
    }
}
#undef CIC_WORD_CASE
#else
/* From C++ this is a call into the library's out-of-line copy, which
   cic_core.c emits from the inlined definition above (the `extern` idiom
   of util's saturate.c), so there is one body: that body uses the C99
   complex library, which C++ does not have (dp_complex.h, "From C++"). A
   plain comment, because this block's Doxygen is also the Python
   docstring. */
size_t dp_cic_decimate(dp_cic_state_t *state, const float _Complex *in,
                       size_t n_in, float _Complex *out, size_t max_out);
#endif /* !__cplusplus */

/**
 * @brief Change the decimation ratio in place and reset all filter state.
 * Recomputes the normalisation shift (CIC_N * log2(R)) and zeros all
 * accumulators so the filter behaves exactly like a freshly created
 * one with the new R. Silently ignores R values that are not a
 * power-of-two in `[2, 2048]` (`CIC_R_MAX`) — the state is left unchanged in
 * that case.
 *
 * @param state  Pointer to a valid dp_cic_state_t.
 * @param R      New decimation ratio.  Same constraints as dp_cic_create().
 *
 * @code
 * >>> from doppler.resample import CIC
 * >>> cic = CIC(R=4)
 * >>> cic.reconfigure(8)
 * >>> cic.R, cic.shift
 * (8, 12)
 * @endcode
 */
void dp_cic_reconfigure(dp_cic_state_t *state, uint32_t R);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
