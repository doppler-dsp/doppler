/**
 * @file nco_core.h
 * @brief Phase-accumulator NCO, and the one float->integer boundary
 *        everything that steers one has to pass through.
 *
 * Two layers, and the order matters: the second is exact integer
 * arithmetic that C99 defines outright, so all the care lives in the
 * first.
 *
 * ## 1. The float boundary — the ONLY double->integer conversion
 *
 *   nco_phase_units   double -> uint32 phase word, total and saturating
 *   nco_norm_fold_    fold a normalised quantity into [0, 1), then convert
 *   nco_steer_scale   bound `1 + control` to a band, BEFORE converting
 *
 * The fold is unit-free, so it serves two dimensions, and each call site
 * says which through the face it calls rather than leaving it to be
 * inferred from what the result is assigned to:
 *
 *   nco_norm_freq_to_inc    cycles per sample -> a phase INCREMENT
 *   nco_norm_phase_to_word  cycles, absolute  -> a phase WORD
 *
 * "norm" is normalised to whatever the context's full scale is -- the
 * sample rate for a frequency, one period for a phase.
 *
 * C99 guarantees the integer half: unsigned arithmetic is reduced modulo
 * 2^32 for every unsigned type (6.2.5p9) and uint32_t is exactly 32 bits
 * with no padding (7.20.1.1), so wrapping, carry and borrow need no
 * reasoning at all. Undefined behaviour can enter at exactly one place --
 * a double whose truncated value the integer type cannot represent
 * (6.3.1.4) -- which makes confining that cast a STRUCTURAL rule, not a
 * stylistic one. A second conversion site anywhere forfeits the guarantee
 * no matter how careful this one is, so every double-valued phase quantity
 * does its arithmetic in double and then passes through here.
 *
 * nco_steer_scale() is the companion, and the reason the conversion should
 * almost never be the one making a decision: a conversion can only
 * saturate or floor a request that is already insane, and both are
 * symptoms. Bounding the request is the fix.
 *
 * Fold, then convert, is exact in VALUE but destroys the SIGN -- -0.25 and
 * +0.75 are the same phase word. Anything that needs direction (carry vs
 * borrow) must therefore form the signed quantity BEFORE folding.
 *
 * ## 2. nco_state_t — the 32-bit phase accumulator
 *
 * A phase register advancing by phase_inc every sample, wrapping naturally
 * at 2^32. Three output mappings, each with a matching per-sample
 * control-port variant (`_ctrl`) and a single-sample primitive
 * (nco_step_u32*):
 *
 *   nco_steps_u32        raw accumulator value  [0, 2^32)
 *   nco_steps_u32_scaled (uint64)phase * nmax >> 32  ->  [0, nmax)
 *   nco_steps_u32_ovf    raw phase + a per-sample cycle-boundary flag
 *
 * The `_ovf` flag is signed by the composite rate, not by the raw carry:
 * a forward crossing is a carry (one EXTRA output due), a backward one a
 * borrow. It serves both roles this accumulator is asked to play -- a
 * phase to synthesise from (LUT index, carrier angle) and a timing strobe
 * (a resampler asking "does this input complete an output period").
 *
 * nmax = 0 in the scaled form is identical to the raw form. reset() zeroes
 * phase only; norm_freq and nmax are unchanged. Serializable via the
 * standard bytes interface (dp_state.h).
 *
 * Lifecycle: nco_create -> (steps / reset)* -> nco_destroy. Owners that
 * want it by value (symsync, dll, resamp) embed the struct and set
 * phase/phase_inc/norm_freq directly.
 *
 * @code
 * nco_state_t *nco = nco_create(0.25, 0);
 * uint32_t out[4];
 * nco_steps_u32 (nco, 4, out, 4);
 * // out[0]=0x00000000, out[1]=0x40000000,
 * // out[2]=0x80000000, out[3]=0xC0000000
 * nco_destroy(nco);
 * @endcode
 */
#ifndef NCO_CORE_H
#define NCO_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Double -> phase word: the ONLY float-to-integer conversion in
   *        the phase-accumulator family.
   *
   * C99 guarantees the integer half of a phase accumulator outright --
   * unsigned arithmetic is reduced modulo 2^N for every unsigned type
   * (6.2.5p9) and `uintN_t` is exactly N bits with no padding (7.20.1.1) --
   * so wrapping, carry and borrow need no reasoning at any width.
   * Undefined behaviour can enter at exactly one place: a `double` whose
   * truncated value the integer type cannot represent (6.3.1.4). That makes
   * confining the conversion a STRUCTURAL rule rather than a stylistic one.
   * A second site anywhere forfeits the guarantee no matter how careful this
   * function is, so every `double`-valued phase quantity -- a folded
   * frequency, a reciprocal `2^32 / rate`, a steered `inc * (1 + control)`
   * -- does its arithmetic in `double` and then passes through here.
   *
   * Three real sites proved the point before this existed, each computing
   * its own cast and each undefined at its boundary: `resamp` at
   * `rate == 1.0` and `symsync` at `sps == 1` both produced **0** on x86
   * (a phase increment that never advances -- a dead NCO) where arm64
   * saturates, and `symsync`'s loop steer did not clamp but WRAPPED, so a
   * control asking to speed up returned roughly a ninth of the correct
   * increment.
   *
   * `resamp` needs the OTHER answer at that boundary, and it gets it from
   * @ref nco_phase_units_mod rather than from a private cast. Under the
   * interpolating rule, outputs are emitted every tick and the phase word
   * only decides when to LOAD, so a step of one whole period per tick --
   * rate 1 -- is an ordinary operating point whose exact encoding is 0,
   * not a limit to be clamped. Saturation here remains right for a phase
   * ACCUMULATOR, which is what this function converts for.
   *
   * Two behaviours, ONE home. This paragraph used to argue the opposite --
   * that resamp should keep the modular cast "in its own `resamp_step_inc`" so
   * it would not be consolidated back -- and it was right about the
   * BEHAVIOUR and wrong about the HOME. The faces above already show one
   * fold serving two dimensions; one cast can likewise serve two boundary
   * conventions. The cost of the private home was not hypothetical: a file
   * that already owned "the conversion" locally grew a SECOND one beside
   * it -- `resamp_execute_ctrl_push`'s `(uint32_t)(frac * 2^32 + 0.5)` --
   * whose rounding could carry past 2^32 into the undefined cast, stalling
   * the interpolator for any composite rate in (1.0, 1.0 + 1.16e-10]. A
   * Doppler ramp crosses that window at closest approach, and it cost a
   * receiver its lock there.
   *
   * Behaviour here is total and host-independent: below zero (and NaN,
   * which the negated comparison rejects rather than passing to the cast)
   * gives 0; at or above 2^32 saturates to 2^32-1; in between it TRUNCATES
   * toward zero, so the realised value is at most one step low and never
   * high -- the convention @ref nco_norm_fold_ documents, now enforced in
   * one place. Saturation is the honest answer at the limit: a phase word
   * cannot express more than one cycle per sample, and clamping says so
   * where a wrap would silently invert the caller's intent.
   *
   * @param units  A phase quantity already scaled to phase-word units
   *               (i.e. cycles x 2^32). Any value, including NaN.
   * @return `units` truncated into `[0, 2^32)`.
   */
  JM_FORCEINLINE uint32_t
  nco_phase_units (double units)
  {
    /* Establish 6.3.1.4's precondition rather than assume it. The negated
       form is deliberate: every comparison with NaN is false, so `!(u > 0)`
       rejects NaN where `u < 0.0` would wave it through to the cast. */
    if (!(units > 0.0))
      return 0u;
    if (units >= 4294967296.0)
      return 4294967295u;
    return (uint32_t)units;
  }

  /**
   * @brief Total, MODULAR double -> phase word: one whole period is 0.
   *
   * The resampling twin of @ref nco_phase_units. Same single float
   * boundary, opposite answer at the top of the range, and the choice
   * belongs to the CALLER's dimension rather than to the value:
   *
   *   - A phase ACCUMULATOR cannot express more than one cycle per sample,
   *     so asking for more is out of range and @ref nco_phase_units clamps
   *     -- saying so, where a wrap would silently invert the intent.
   *   - A resampling STEP of one whole period per tick is not out of range
   *     at all: it is rate 1, an ordinary operating point, and its exact
   *     encoding is 0. Under the interpolator's rule (emit every tick,
   *     load when the accumulator fails to advance) a step of 0 means the
   *     accumulator never advances, so `u(k) <= u(k-1)` holds every tick,
   *     one input is consumed per output, and one arm serves throughout --
   *     which is precisely what rate 1 should do.
   *
   * Clamping that case instead is wrong but undramatic, and the measured
   * number is worth keeping because a comment once guessed it: 2^32-1
   * emits 4097 outputs for 4096 inputs (1.000244) where 0 emits exactly
   * 4096. A slow drift, not a doubling.
   *
   * Total for every double. NaN and anything <= 0 give 0 (the negated
   * comparison is what rejects NaN, as above). Below 2^32 the bare cast is
   * defined and truncates toward zero. At or above 2^32 the value is
   * reduced modulo one period first -- `fmod` is exact, so the reduction
   * introduces no error of its own, and it keeps the face total instead of
   * trading one undefined cast for an assumed range. Callers converting a
   * rate reciprocal reach it well inside 2^32 anyway; the reduction is
   * there so no caller has to prove that before calling.
   *
   * @param units  A step already scaled to phase-word units (cycles x
   *               2^32). Any value, including NaN.
   * @return `units` reduced modulo 2^32 and truncated toward zero.
   */
  JM_FORCEINLINE uint32_t
  nco_phase_units_mod (double units)
  {
    if (!(units > 0.0))
      return 0u;
    if (units < 4294967296.0)
      return (uint32_t)units;
    return (uint32_t)fmod (units, 4294967296.0);
  }

  /**
   * @brief The INVERSE: a phase word back to normalised cycles in [0, 1).
   *
   * uint32 -> double is exact and total -- every phase word is
   * representable, so unlike the forward direction there is no boundary,
   * no undefined case and nothing to decide. It lives here anyway, for the
   * duller reason: `word / 2^32` was written out at four sites, and a
   * scale factor spelled by hand four times is a scale factor that can be
   * mistyped in one of them. The forward faces are confined because they
   * can trap; this one is shared because it is duplication.
   *
   * Read it back in whatever unit the caller's full scale is -- multiply
   * by `sf` for chips, by a symbol period for symbols. Callers doing that
   * are exactly the ones this saves from restating 2^32.
   *
   * @param word  Any phase word.
   * @return `word / 2^32`, in `[0, 1)`.
   */
  JM_FORCEINLINE double
  nco_word_to_norm (uint32_t word)
  {
    return (double)word / 4294967296.0;
  }

  /**
   * @brief Fold a normalised quantity into `[0, 1)` and scale it to a
   *        32-bit phase word -- the ONE shared body behind both faces.
   *
   * Call @ref nco_norm_freq_to_inc or @ref nco_norm_phase_to_word
   * instead of this: they are this function, and picking one is how a
   * call site states which dimension it holds. The conversion itself is
   * unit-free -- fold modulo one, scale by 2^32 -- so it cannot tell a
   * frequency (cycles per SAMPLE, landing on `phase_inc`) from a phase
   * (cycles, absolute, landing on `phase`), and both are live: three
   * sites in the library convert an absolute angle, two of them the
   * proportional path of a carrier loop turning `kp*e` radians into a
   * phase-word nudge.
   *
   * Floor-normalises @p norm into `[0, 1)` before scaling and
   * TRUNCATES toward zero (the bare C99 float->unsigned cast, 6.3.1.4)
   * to an integer phase step -- deliberately NOT `llround`. Every
   * caller that needs this conversion (`nco_create`/`nco_set_norm_freq`,
   * `LO`'s own phase accumulator, `Dll`'s code-phase NCO steering) MUST
   * go through one of the two faces rather than growing its own private
   * copy -- duplicated copies of this exact formula have already drifted
   * once (one truncated while a sibling copy rounded) before being
   * consolidated here on the truncating convention.
   *
   * A 32-bit phase word can only ever represent frequency in fs/2^32
   * steps (a one-time, unavoidable quantization -- no fixed-width
   * accumulator can be exact except at those specific levels). Note the
   * accumulator truncating is inherent, but quantizing phase_inc ONCE at
   * setup is a separate choice, and it is made for reproducibility rather
   * than accuracy.
   *
   * **Truncation is the only quantization with no tie to break and
   * nothing to contract.** That, and not a general claim about rounding
   * being sloppy, is why it is the convention here. doppler compiles with
   * `-ffast-math` project-wide (CMakeLists.txt), under which the tie
   * behaviour of any rounding form is at the compiler's discretion.
   * Disassembled at the project's own flags:
   *
   *     (uint32_t)(d*2^32)        v2: mulsd, cvttsd2si    v3: vmulsd
   *     (uint32_t)(d*2^32 + 0.5)  v2: mulsd, addsd        v3: vfmadd132sd
   *     (uint32_t)llround(d*2^32) v2: mulsd, addsd        v3: vmulsd, vaddsd
   *
   * The `+ 0.5` form CONTRACTS into a single fused multiply-add on any
   * target with baseline FMA -- arm64, and x86-64-v3 -- while staying a
   * separate multiply-then-add on the x86-64-v2 baseline doppler ships.
   * One rounding versus two, disagreeing at exact ties: a live
   * x86-vs-arm64 divergence at this project's exact target configuration.
   * `llround` is not even a libm call under these flags; the compiler
   * rewrites it into that same shape, differently again at each ISA
   * level. The increment feeds closed tracking loops, so a constant that
   * differs by host is precisely the reproducibility problem. If the
   * half-LSB were ever worth having, it would have to be rounded in
   * INTEGER arithmetic, where no compiler flag can reinterpret it.
   *
   * A rounding form can also reach exactly 2^32 (verified: `llround` of
   * `nextafter(1,0) * 2^32` is 2^32 on the nose), which as a bare cast is
   * the undefined conversion that once froze this NCO on arm64. That is
   * no longer an argument against rounding -- @ref nco_phase_units
   * saturates, so the value cannot reach a cast at all -- but it is why
   * the guard has to exist regardless of which convention is chosen.
   *
   * Historical note, since the prose predates the evidence: this function
   * was born truncating (84c46503) and `llround` was never live in it, so
   * the earlier text here described a recollection rather than a fix this
   * file ever made. The mechanism above is checkable instead.
   *
   * The residual is a small constant bias a carrier/code loop nulls
   * out anyway (a floor the integrator absorbs), so downstream tracking
   * is unaffected. The realised frequency is at most one step LOW,
   * never high.
   *
   * **Total, and it answers 0 where @ref nco_phase_units saturates.**
   * That looks like a divergence and is not: the two take different
   * units. `nco_phase_units` is handed a quantity ALREADY in phase-word
   * units, so infinitely many of them saturate, which is the honest
   * answer for a phase word. This function is handed a normalised RATE,
   * and an infinite rate has no fractional part, so no phase word
   * represents it -- 0, a stopped oscillator, is the honest answer here.
   *
   * Infinity is not even a special case. `1.0`, `2^32` and `1e300` all
   * give 0 by the same rule, their fractional part being exactly zero,
   * and NaN gives 0 because @ref nco_phase_units rejects it. It is
   * "only the fractional part matters" applied at the top of the range,
   * exactly as a sub-LSB rate is that rule applied at the bottom. Both
   * ends are pinned in `test_nco_core.c` (§C29 and §13), side by side
   * with the saturating case so the pair reads as intended rather than
   * being rediscovered as a defect.
   *
   * @param norm  Any real normalised quantity; only the fractional part
   *              matters. Negative values fold correctly (e.g. -0.25
   *              -> 3x2^30).
   * @return Phase word in `[0, 2^32)`.
   */
  JM_FORCEINLINE uint32_t
  nco_norm_fold_ (double norm)
  {
    /* The fold is in [0, 1) mathematically but NOT in floating point: for
       any norm in [-2^-53, 0) the subtraction rounds up to exactly 1.0
       (`-1e-20 - (-1) == 1.0`), so the product reaches 2^32 and only
       nco_phase_units() keeps that out of the cast. The true fraction is
       then in (1 - 2^-53, 1), whose truncation is 2^32-1 -- the same value
       -1e-16, one representable step away, already returns. */
    double d = norm - floor (norm);
    return nco_phase_units (d * 4294967296.0);
  }

  /**
   * @brief Normalised FREQUENCY -> per-sample phase increment.
   *
   * @p norm_freq is in cycles per SAMPLE, normalised to the sample rate;
   * the result belongs in `phase_inc` (or is added to one, as the `ctrl`
   * ports do). The conversion is @ref nco_norm_fold_ -- see it for the
   * fold, the truncating convention and why this is the only
   * double->integer boundary in the family.
   *
   * @param norm_freq  Cycles per sample; any sign, any magnitude (only
   *                   the fractional part survives the fold).
   * @return Phase increment in `[0, 2^32)`.
   */
  JM_FORCEINLINE uint32_t
  nco_norm_freq_to_inc (double norm_freq)
  {
    return nco_norm_fold_ (norm_freq);
  }

  /**
   * @brief Normalised PHASE -> absolute phase word.
   *
   * @p norm_phase is an angle in cycles, normalised to one period -- a
   * carrier loop's `kp*e / 2pi` proportional nudge, a code loop's
   * `seed_chip / sf` starting offset. The result is a position on the
   * accumulator, not a rate: it belongs in `phase`, or is added to one.
   * The conversion is @ref nco_norm_fold_.
   *
   * The fold is exact in value but destroys the SIGN (-0.25 and +0.75 are
   * the same word), which is exactly right here -- a phase is modular, so
   * a negative angle IS the positive one that lands in the same place, and
   * adding the word to `phase` retreats by wrapping. A caller needing
   * DIRECTION (carry versus borrow) must form the signed quantity before
   * this call; see @ref nco_step_u32_ovf_ctrl.
   *
   * @param norm_phase  Cycles; any sign, any magnitude.
   * @return Phase word in `[0, 2^32)`.
   */
  JM_FORCEINLINE uint32_t
  nco_norm_phase_to_word (double norm_phase)
  {
    return nco_norm_fold_ (norm_phase);
  }

  /**
   * @brief Bound a steered rate to a band about nominal.
   *
   * The companion to @ref nco_phase_units, and the reason that one should
   * almost never be doing real work: a conversion can only ever saturate or
   * floor an already-insane request, which are both symptoms. Bounding the
   * REQUEST is the fix, and every steered oscillator in the library forms it
   * the same way -- nominal x (1 + control) -- so it belongs here rather than
   * being open-coded per object.
   *
   * That saturate-or-floor distinction is not academic. Routing symsync's
   * timing steer through nco_phase_units() correctly killed an undefined
   * cast, and turned a negative product into 0 -- a STOPPED timing NCO, which
   * never strobes again and so can never recover. The undefined cast it
   * replaced wrapped to a huge increment, slipping a cycle and recovering.
   * Timing acquisition is non-linear and does reach control < -1, so the
   * honest conversion was strictly worse than the undefined one until the
   * command itself was bounded.
   *
   * The BAND is the caller's policy, not this function's: it is set by what
   * the object can physically mean. symsync's `[2/3, 2]` is its long-standing
   * rate_est clamp of `[0.5, 1.5]` x sps, restated -- `inst = sps/(1+control)`
   * is monotone, so the two are the same constraint seen from either end.
   *
   * @param control  Fractional rate deviation; the steer is `1 + control`.
   * @param lo       Lower bound on the scale (e.g. 2/3 for -33%). Must be > 0
   *                 for a rate that cannot run backwards.
   * @param hi       Upper bound on the scale (e.g. 2.0 for +100%).
   * @return `1 + control` clamped to `[lo, hi]`; NaN gives @p lo.
   */
  JM_FORCEINLINE double
  nco_steer_scale (double control, double lo, double hi)
  {
    double scale = 1.0 + control;
    /* Negated, so NaN lands on lo rather than sailing through -- the same
       reasoning as nco_phase_units(). */
    if (!(scale > lo))
      return lo;
    if (scale > hi)
      return hi;
    return scale;
  }

/**
 * @brief Wrapping add with carry detection.
 *
 * NCO_ADD_OVF(a, b, res) computes *res = a + b and returns 1 if the
 * addition wrapped (carry out), 0 otherwise.  Branchless on x86/AArch64.
 */
#if defined(__GNUC__) || defined(__clang__)
#define NCO_ADD_OVF(a, b, res)                                                \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),             \
                                    (uint32_t *)(res)))
#else
static inline uint8_t
nco_add_ovf_ (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define NCO_ADD_OVF(a, b, res) nco_add_ovf_ ((a), (b), (res))
#endif

  /**
   * @brief NCO state.
   *
   * Allocate with nco_create().  All fields are managed by the library;
   * read phase and phase_inc via the property accessors.
   */
  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)         */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double   norm_freq; /* normalised frequency (cycles/sample)          */
    uint32_t nmax;      /* wrap target for steps_u32_scaled; 0 = raw   */
  } nco_state_t;

  /**
   * @brief Emit the current raw phase, then advance the accumulator.
   *
   * Single-sample form, suitable for inlining into another module's own
   * per-sample loop (e.g. a code-tracking loop's phase steer) with zero
   * call overhead -- the canonical primitive every batch stepper below
   * and every OTHER module embedding an nco_state_t by value should
   * compose, rather than reimplementing this advance inline (see
   * nco_norm_fold_()'s own doc comment on why duplicated copies of
   * this exact class of arithmetic have already drifted once).
   *
   * @param state  NCO state.  Must be non-NULL.
   * @return Phase value BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32 (nco_state_t *state)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc;
    return ph;
  }

  /**
   * @brief Emit the current phase scaled to `[0, nmax)`, then advance.
   * Single-sample form of nco_steps_u32_scaled() -- see that function's
   * doc comment for the scaling identity and the nmax==0 special case.
   * @param state  NCO state.  Must be non-NULL.
   * @return Scaled phase value (or raw, if nmax == 0) BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled (nco_state_t *state)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc;
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  /**
   * @brief Emit the current raw phase and this step's carry, then advance.
   * Single-sample form of nco_steps_u32_ovf().
   * @param state  NCO state.  Must be non-NULL.
   * @param carry  Out-param: set to 1 if this step's advance wrapped past
   *               2^32, else 0. Must be non-NULL.
   * @return Phase value BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf (nco_state_t *state, uint8_t *carry)
  {
    uint32_t ph = state->phase;
    *carry      = NCO_ADD_OVF (ph, state->phase_inc, &state->phase);
    return ph;
  }

  /**
   * @brief Emit the current raw phase, then advance by phase_inc + ctrl.
   * Single-sample form of nco_steps_u32_ctrl() -- the control port for a
   * tracking loop, see that function's doc comment. phase_inc/norm_freq
   * are never modified; only the running phase advances.
   * @param state  NCO state.  Must be non-NULL.
   * @param ctrl   Per-sample normalised-frequency control offset, any
   *               sign (the fractional cycle is taken, so it wraps
   *               correctly).
   * @return Phase value BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ctrl (nco_state_t *state, double ctrl)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return ph;
  }

  /**
   * @brief Emit the current phase scaled to `[0, nmax)`, then advance by
   *        phase_inc + ctrl.
   * Single-sample form of nco_steps_u32_scaled_ctrl().
   * @param state  NCO state.  Must be non-NULL.
   * @param ctrl   Per-sample normalised-frequency control offset.
   * @return Scaled phase value (or raw, if nmax == 0) BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled_ctrl (nco_state_t *state, double ctrl)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  /**
   * @brief Emit the current raw phase and this step's cycle-boundary
   *        event, then advance by phase_inc + ctrl.
   *
   * Single-sample form of nco_steps_u32_ovf_ctrl(). The event flags
   * THIS step's true advance crossing a full-cycle boundary, in the
   * direction the composite rate is going: a **carry** when the phase
   * runs forward past 2^32 (one EXTRA output/load for the consumer),
   * a **borrow** when it runs backward past 0 (one FEWER). A steered
   * consumer knows the sign of its own control, so one boolean carries
   * both senses.
   *
   * **The sign must be taken before the fold.** nco_norm_fold_() folds
   * bipolar to unipolar by construction (-0.25 -> 3x2^30), which keeps
   * the modulo phase exact but destroys the direction: retreating by
   * 0.25 cycles is indistinguishable, in the accumulator, from
   * advancing by 0.75, and the bare 64-bit sum's bit 32 then sets on
   * nearly every step (norm_freq=0.5 steered by ctrl=-0.25 is a
   * 0.25 cyc/sample composite -- 2 boundary crossings in 8 steps --
   * yet reports 8). So the signed advance is formed as
   * `delta = norm_freq + ctrl` in cycles, ahead of any folding.
   *
   * Keying off the sign of @p ctrl alone would be wrong for the same
   * reason in reverse: the composite can run forward while the control
   * is negative (norm_freq=0.5, ctrl=-1e-4 is a legitimate +0.4999
   * step). Only the composite's sign decides.
   *
   * Given that sign, the event is the accumulator's own wrap -- forward
   * `phase_new < phase_old`, backward `phase_new > phase_old` -- plus
   * the whole-cycle term: |delta| >= 1 crosses a boundary every step
   * regardless of where the fractional part lands, which is both the
   * multi-wrap case (0.9 + 0.9) and the exactly-unity case a resampler
   * running at rate 1.0 sits on (fractional advance 0, one output per
   * input). `delta == 0` is free-running and never events.
   *
   * @param state  NCO state.  Must be non-NULL.
   * @param ctrl   Per-sample normalised-frequency control offset.
   * @param carry  Out-param: set to 1 if this step's advance crossed a
   *               cycle boundary (carry if the composite rate is
   *               positive, borrow if negative), else 0. Must be
   *               non-NULL.
   * @return Phase value BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf_ctrl (nco_state_t *state, double ctrl, uint8_t *carry)
  {
    uint32_t ph = state->phase;
    /* Wrapping u32 add: bit-for-bit the modulo advance the 64-bit sum
       used to produce -- only the event derivation below changes. */
    uint32_t nph   = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    double   delta = state->norm_freq + ctrl; /* signed, pre-fold */

    state->phase = nph;
    if (delta > 0.0)
      *carry = (uint8_t)(nph < ph || delta >= 1.0);
    else if (delta < 0.0)
      *carry = (uint8_t)(nph > ph || delta <= -1.0);
    else
      *carry = 0;
    return ph;
  }

  /**
   * @brief Create an NCO instance.
   * Allocates and initialises the phase accumulator to zero, converts
   * norm_freq to the integer phase_inc = floor(frac(norm_freq) × 2^32),
   * and stores nmax for scaled output.  The NCO is immediately ready to
   * call nco_steps_u32 / nco_steps_u32_scaled / nco_steps_u32_ovf.
   *
   * @param norm_freq  Normalised frequency in cycles per sample.
   *                   Any real value; only the fractional part matters.
   *                   Negative values fold correctly (−0.25 → 3×2^30).
   * @param nmax       Wrap target for nco_steps_u32_scaled.
   *                   Pass 0 to return the raw 32-bit accumulator.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(norm_freq=0.25, nmax=0)
   * >>> nco.phase_inc
   * 1073741824
   * @endcode
   */
  nco_state_t *nco_create (double norm_freq, uint32_t nmax);

  /** Free all resources.  May be NULL (no-op). */
  void nco_destroy (nco_state_t *state);

  /**
   * @brief Zero the phase accumulator.
   * Sets phase to 0 so the next nco_steps_u32 call starts from the
   * beginning of the cycle.  norm_freq, phase_inc, and nmax are
   * unchanged; the NCO is ready to generate samples again immediately.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> _ = nco.steps_u32(2)
   * >>> nco.phase
   * 2147483648
   * >>> nco.reset()
   * >>> nco.phase
   * 0
   * >>> nco.norm_freq
   * 0.25
   * @endcode
   */
  void nco_reset (nco_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Only the running phase accumulator is serialized; phase_inc / nmax are
   * config restored by the constructor.  Envelope: [dp_state_hdr_t][u32
   * phase].
   */
#define NCO_STATE_MAGIC DP_FOURCC ('N', 'C', 'O', '_')
#define NCO_STATE_VERSION 1u

  /** @brief Serialized-state byte size. */
  size_t nco_state_bytes (const nco_state_t *state);
  /** @brief Serialize the phase accumulator into @p blob. */
  void nco_get_state (const nco_state_t *state, void *blob);
  /** @brief Restore phase; DP_OK, or DP_ERR_INVALID if the envelope rejects.
   */
  int nco_set_state (nco_state_t *state, const void *blob);

  /* ---- Properties ---- */

  /**
   * @brief Normalised frequency (read/write).
   * Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and
   * takes effect on the next nco_steps_* call; phase is NOT reset.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.norm_freq
   * 0.25
   * >>> nco.norm_freq = 0.5
   * >>> nco.phase_inc
   * 2147483648
   * @endcode
   */
  double nco_get_norm_freq (const nco_state_t *state);
  void   nco_set_norm_freq (nco_state_t *state, double norm_freq);

  /**
   * @brief Current phase accumulator value (read/write).
   * Reading returns the current integer phase in `[0, 2^32)`.  Writing
   * overrides the accumulator directly, allowing arbitrary phase offsets
   * without re-creating the NCO.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.phase
   * 0
   * >>> nco.phase = 2147483648
   * >>> nco.phase
   * 2147483648
   * @endcode
   */
  uint32_t nco_get_phase (const nco_state_t *state);
  void     nco_set_phase (nco_state_t *state, uint32_t phase);

  /**
   * @brief Per-sample phase increment (read-only).
   * Derived from norm_freq as floor(frac(norm_freq) × 2^32).  Updated
   * automatically whenever norm_freq is written.  A freq of 0.25 gives
   * phase_inc = 1073741824 (0x40000000).
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.phase_inc
   * 1073741824
   * @endcode
   */
  uint32_t nco_get_phase_inc (const nco_state_t *state);

  /* ---- Block generators ---- */

  /**
   * @brief Pre-allocation hint: the buffer size the binding starts with.
   *
   * NOT a limit on the call, and it used to say it was ("requesting more
   * samples per call is undefined behaviour").  That was the contract
   * before `pass_capacity` (jm gh-138) started telling the kernel the
   * caller's capacity: every stepper now clamps to its own @c max_out
   * argument and returns what it actually wrote, and the Python binding
   * grows its buffer on demand.  Measured: all three faces return 70000
   * correct samples for a 70000-sample request.  Size an @c out= buffer
   * with this, or ignore it and let the binding allocate.
   */
  size_t nco_steps_u32_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; write raw uint32 accumulator values.
   * Each element is the phase value BEFORE the increment fires, so
   * `out[0]` is the phase at the moment of the call.  The accumulator
   * wraps silently at 2^32, giving the full-resolution integer ramp
   * that the scaled and carry variants derive from.  Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Output buffer; must hold at least n uint32_t values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(n, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> out = nco.steps_u32(4)
   * >>> out.dtype
   * dtype('uint32')
   * >>> out.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * @endcode
   */
  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out,
                        size_t max_out);

  size_t nco_steps_u32_scaled_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; values scaled to `[0, nmax)`.
   * Uses the branchless fixed-point identity
   *   `out[i]` = (uint64_t)phase * nmax >> 32
   * to map the full accumulator range uniformly onto [0, nmax) without
   * a modulo operation.  When nmax == 0 falls back to the raw accumulator
   * (identical to nco_steps_u32).  Useful for polyphase filter bank
   * indexing and direct LUT addressing.  Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Output buffer; must hold at least n uint32_t values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(n, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 4)
   * >>> out = nco.steps_u32_scaled(4)
   * >>> out.dtype
   * dtype('uint32')
   * >>> out.tolist()
   * [0, 1, 2, 3]
   * @endcode
   */
  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out,
                               size_t max_out);

  size_t nco_steps_u32_ovf_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; write raw phase values and per-sample carry.
   * Identical to nco_steps_u32 for the phase array, but simultaneously
   * fills a parallel uint8 carry buffer: `out1[i]` is 1 if the add that
   * produced `out[i]`'s post-increment phase wrapped past 2^32, else 0.
   * The carry marks the exact boundary of one input period and is the
   * primitive for polyphase sample-clock and rational resampling engines.
   * Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Phase output buffer; must hold at least n uint32_t values.
   * @param out1   Carry output buffer; must hold at least n uint8_t values.
   * @param max_out Capacity of @p out and @p out1 in elements (both receive
   *                the same count). Emission stops there, so the return
   *                value is the number actually written.
   * @return min(n, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.5, 0)
   * >>> ph, carry = nco.steps_u32_ovf(4)
   * >>> ph.tolist()
   * [0, 2147483648, 0, 2147483648]
   * >>> carry.tolist()
   * [0, 1, 0, 1]
   * >>> carry.dtype
   * dtype('uint8')
   * @endcode
   */
  size_t nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1, size_t max_out);

  size_t nco_steps_u32_ctrl_max_out (nco_state_t *state);

  /**
   * @brief Advance ctrl_len samples; raw phase, with a per-sample control
   *        offset added on top of the fixed phase_inc (not persisted).
   *
   * The NCO **control port** for a tracking loop: @p ctrl is a per-sample
   * frequency control in normalised cycles/sample, added to the centre
   * increment @c phase_inc for that step only. @c phase_inc / @c norm_freq
   * are NEVER modified by this call -- only the running @c phase advances,
   * by `phase_inc + ctrl_inc` each sample -- so a loop filter can drive the
   * NCO with its full per-sample output (integrator + proportional term)
   * without the caller ever touching the NCO's own configured rate. Mirrors
   * `lo_step_ctrl`/`lo_steps_ctrl` (native/inc/lo/lo_core.h), which does
   * this for the CF32 phasor output; this is the same control-port pattern
   * for NCO's raw phase output. With every `ctrl[i] == 0` this is
   * bit-identical to nco_steps_u32(). Returns ctrl_len.
   *
   * Python's `out=` keyword writes into a caller-supplied buffer instead
   * of allocating a fresh one.  This used to claim it was "essential for
   * a hot per-epoch tracking loop"; measured, it is worth 0-25% below
   * 8192 samples and nothing at or above it, so reach for it only if a
   * profile says so.
   *
   * The buffer must be sized to `steps_u32_ctrl_max_out()`, NOT just
   * `len(ctrl)` -- so a 64-sample call still needs a 65536-element
   * buffer, which is most of what makes `out=` poor value here.  That is
   * this header's doing, not the binding's: `*_max_out(state)` takes only
   * the state, so it is a bound over ALL calls and cannot say what THIS
   * one needs.  A generated binding may accept a request-sized buffer
   * only where the bound is declared per-call (a `max_out(state, n)`
   * prototype).  The returned view is correctly sliced to `len(ctrl)`
   * regardless of the buffer's size.
   *
   * @param state     NCO state returned by nco_create().
   * @param ctrl      Per-sample normalised-frequency control offsets in
   *                  `double`, any sign (the fractional cycle is taken, so
   *                  it wraps correctly).  `double` because that is the
   *                  width the conversion works in and every scalar steer
   *                  site already uses; a float32 port quantized the
   *                  request before the fold ever saw it, so the same
   *                  commanded rate landed on a different phase word
   *                  depending on which face it entered by.
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Output buffer; must hold at least ctrl_len uint32_t
   *                  values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(ctrl_len, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> import numpy as np
   * >>> nco = NCO(norm_freq=0.0, nmax=0)
   * >>> ctrl = np.full(4, 0.25, dtype=np.float64)
   * >>> out = nco.steps_u32_ctrl(ctrl)
   * >>> out.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * >>> nco.norm_freq
   * 0.0
   * @endcode
   */
  size_t nco_steps_u32_ctrl (nco_state_t *state, const double *ctrl,
                             size_t ctrl_len, uint32_t *out,
                             size_t max_out);

  size_t nco_steps_u32_scaled_ctrl_max_out (nco_state_t *state);

  /**
   * @brief Advance ctrl_len samples; values scaled to `[0, nmax)`, with a
   *        per-sample control offset added on top of phase_inc.
   *
   * The @ref nco_steps_u32_scaled output mapping (nmax=0 falls back to
   * the raw accumulator) driven by the @ref nco_steps_u32_ctrl control
   * port -- every stepper has a matching control-input counterpart, so
   * a tracking loop can drive LUT-indexed output (nmax = table length)
   * exactly as it would raw phase output, without ever touching
   * phase_inc/norm_freq. With every `ctrl[i] == 0` this is bit-identical
   * to nco_steps_u32_scaled(). Returns ctrl_len.
   *
   * @param state     NCO state returned by nco_create().
   * @param ctrl      Per-sample normalised-frequency control offsets in
   *                  `double`, any sign (the fractional cycle is taken, so
   *                  it wraps correctly).  `double` because that is the
   *                  width the conversion works in and every scalar steer
   *                  site already uses; a float32 port quantized the
   *                  request before the fold ever saw it, so the same
   *                  commanded rate landed on a different phase word
   *                  depending on which face it entered by.
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Output buffer; must hold at least ctrl_len uint32_t
   *                  values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(ctrl_len, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> import numpy as np
   * >>> nco = NCO(norm_freq=0.0, nmax=4)
   * >>> ctrl = np.full(4, 0.25, dtype=np.float64)
   * >>> out = nco.steps_u32_scaled_ctrl(ctrl)
   * >>> out.tolist()
   * [0, 1, 2, 3]
   * @endcode
   */
  size_t nco_steps_u32_scaled_ctrl (nco_state_t *state, const double *ctrl,
                                    size_t ctrl_len, uint32_t *out,
                                    size_t max_out);

  size_t nco_steps_u32_ovf_ctrl_max_out (nco_state_t *state);

  /**
   * @brief Advance ctrl_len samples; raw phase + per-sample carry, with a
   *        per-sample control offset added on top of phase_inc.
   *
   * The @ref nco_steps_u32_ovf output mapping (raw phase plus a flag
   * marking each sample whose advance crossed a cycle boundary) driven
   * by the @ref nco_steps_u32_ctrl control port -- every stepper has a
   * matching control-input counterpart. The flag reflects THIS sample's
   * true SIGNED advance (`norm_freq + ctrl`, formed in cycles before
   * either term is folded into the accumulator), not just phase_inc
   * alone -- needed by any consumer (e.g. a coupled carrier/code
   * tracker, or a resampler asking "does this input produce an output")
   * that must detect a period boundary while the rate is being actively
   * steered. A forward crossing is a carry (one EXTRA output/load), a
   * backward one a borrow (one FEWER); see @ref nco_step_u32_ovf_ctrl
   * for why the sign cannot be recovered after the fold, nor taken from
   * `ctrl` alone. With every `ctrl[i] == 0` and `norm_freq` in [0, 1)
   * this is bit-identical to nco_steps_u32_ovf(). Returns ctrl_len.
   *
   * @param state     NCO state returned by nco_create().
   * @param ctrl      Per-sample normalised-frequency control offsets in
   *                  `double`, any sign (the fractional cycle is taken, so
   *                  it wraps correctly).  `double` because that is the
   *                  width the conversion works in and every scalar steer
   *                  site already uses; a float32 port quantized the
   *                  request before the fold ever saw it, so the same
   *                  commanded rate landed on a different phase word
   *                  depending on which face it entered by.
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Phase output buffer; must hold at least ctrl_len
   *                  uint32_t values.
   * @param out1      Cycle-boundary event buffer (carry when the
   *                  composite rate is positive, borrow when negative);
   *                  must hold at least ctrl_len uint8_t values.
   * @param max_out Capacity of @p out and @p out1 in elements (both receive
   *                the same count). Emission stops there, so the return
   *                value is the number actually written.
   * @return min(ctrl_len, max_out) samples.
   * @code
   * >>> from doppler.source import NCO
   * >>> import numpy as np
   * >>> nco = NCO(norm_freq=0.25, nmax=0)
   * >>> ctrl = np.zeros(4, dtype=np.float64)
   * >>> ph, carry = nco.steps_u32_ovf_ctrl(ctrl)
   * >>> ph.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * >>> carry.tolist()
   * [0, 0, 0, 1]
   * @endcode
   */
  size_t nco_steps_u32_ovf_ctrl (nco_state_t *state, const double *ctrl,
                                 size_t ctrl_len, uint32_t *out,
                                 uint8_t *out1, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
