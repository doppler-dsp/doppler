/**
 * @file nco_core.h
 * @brief Pure 32-bit phase-accumulator NCO.
 *
 * Implements a numerically-controlled oscillator whose 32-bit phase
 * register advances by phase_inc every sample and wraps naturally at
 * 2^32, giving exact integer arithmetic with no floating-point drift.
 * Three output mappings expose different views of the accumulator:
 *
 *   nco_steps_u32        raw accumulator value  `[0, 2^32)`
 *   nco_steps_u32_scaled (uint64)phase * nmax >> 32  →  [0, nmax)
 *   nco_steps_u32_ovf    raw phase + per-sample carry flag
 *
 * nmax=0 in nco_steps_u32_scaled is treated identically to
 * nco_steps_u32 (returns raw accumulator unchanged).
 *
 * Normalised-frequency → phase_inc conversion:
 *   phase_inc = floor((norm_freq mod 1.0) × 2^32)
 *
 * Negative frequencies fold correctly: −0.25 → phase_inc = 3×2^30.
 *
 * reset() zeroes phase only; norm_freq and nmax are unchanged.
 *
 * Lifecycle: nco_create → (steps / reset)* → nco_destroy
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
   * Behaviour here is total and host-independent: below zero (and NaN,
   * which the negated comparison rejects rather than passing to the cast)
   * gives 0; at or above 2^32 saturates to 2^32-1; in between it TRUNCATES
   * toward zero, so the realised value is at most one step low and never
   * high -- the convention @ref nco_norm_to_inc documents, now enforced in
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
   * @brief Normalised cycles -> uint32 phase delta, the ONE shared
   *        primitive for this conversion.
   *
   * Floor-normalises @p cycles into `[0, 1)` before scaling and
   * TRUNCATES toward zero (the bare C99 float->unsigned cast, 6.3.1.4)
   * to an integer phase step -- deliberately NOT `llround`. Every
   * caller that needs this conversion (`nco_create`/`nco_set_norm_freq`,
   * `LO`'s own phase accumulator, `Dll`'s code-phase NCO steering) MUST
   * call this inline function rather than growing its own private copy
   * -- duplicated copies of this exact formula have already drifted
   * once (one truncated while a sibling copy rounded) before being
   * consolidated here on the truncating convention.
   *
   * A 32-bit phase word can only ever represent frequency in fs/2^32
   * steps (a one-time, unavoidable quantization -- no fixed-width
   * accumulator can be exact except at those specific levels).
   * Truncation biases every phase advance low by up to a full step,
   * but it is the correct convention for a phase-accumulator NCO for
   * two reasons that outweigh the centered residual `llround` would
   * give:
   *   1. **Host-determinism.** A bare truncating cast is bit-identical
   *      on every host; `llround` is round-to-nearest, whose result at
   *      a boundary is FP-sensitive, so a closed-loop DLL fed a rounded
   *      increment converged differently on x86 vs arm64 (the loop got
   *      a slightly different step per epoch and diverged only on
   *      arm64). The increment feeds tracking loops, so it MUST be
   *      reproducible across platforms.
   *   2. **No 2^32 overflow.** `d < 1` makes `d*2^32` strictly `< 2^32`,
   *      so the cast lands in `[0, 2^32)` with no clamp. `llround` could
   *      round `d*2^32` UP to exactly `2^32` for `d ~ 0.9999...`, and
   *      `(uint32_t)2^32 == 0` freezes the NCO (x86 landed on 2^32-1,
   *      arm64 on 2^32 -- an arm64-only hang).
   * The residual is a small constant bias a carrier/code loop nulls
   * out anyway (a floor the integrator absorbs), so downstream tracking
   * is unaffected. The realised frequency is at most one step LOW,
   * never high.
   *
   * @param cycles  Any real number of cycles; only the fractional part
   *                matters. Negative values fold correctly (e.g. -0.25
   *                -> 3x2^30).
   * @return Phase delta in `[0, 2^32)`.
   */
  JM_FORCEINLINE uint32_t
  nco_norm_to_inc (double cycles)
  {
    /* The fold is in [0, 1) mathematically but NOT in floating point: for
       any cycles in [-2^-53, 0) the subtraction rounds up to exactly 1.0
       (`-1e-20 - (-1) == 1.0`), so the product reaches 2^32 and only
       nco_phase_units() keeps that out of the cast. The true fraction is
       then in (1 - 2^-53, 1), whose truncation is 2^32-1 -- the same value
       -1e-16, one representable step away, already returns. */
    double d = cycles - floor (cycles);
    return nco_phase_units (d * 4294967296.0);
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
   * the object can physically mean. symsync's [2/3, 2] is its long-standing
   * rate_est clamp of [0.5, 1.5] x sps, restated -- `inst = sps / (1+control)`
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

  /* ==================================================================
   * nco_clock — a 64-bit phase accumulator for TIMING, not synthesis
   * ================================================================== */

  /**
   * @brief A sample clock: when is the next output due, and where in the
   *        period are we?
   *
   * Distinct from @ref nco_state_t in role, not just in width. An
   * `nco_state_t` produces a PHASE to synthesise from -- a LUT index, a
   * carrier angle -- and 32 bits of it is far below the noise of anything
   * downstream. An `nco_clock_t` produces an EVENT: the strobe that says an
   * output period completed. Nothing averages a strobe away, so the clock
   * needs rate resolution the synthesis phase does not.
   *
   * Why 64 bits is the answer and 32 is not. `nco_norm_to_inc` truncates by
   * design, so a 32-bit increment is at most one 2^-32 step low. A resampler
   * is handed an EXACTLY RATIONAL rate (`m_out/sps`), which lands the wrap
   * precisely on the boundary every period: at `rate = 12/13` the ideal
   * increment is 3964585196.31, truncation is 0.31 units short per step, and
   * 13 steps land 4 units short of exactly 12 cycles. The wrap is missed and
   * the strobe never recovers its place -- 13000 inputs emit 11999 outputs
   * where a double accumulator emits 12000. One lost output permanently
   * shifts the strobe parity, which is a link-grade failure, not a rounding
   * nit.
   *
   * At 64 bits the same rate's realised error falls from 7.16e-11 to
   * 5.12e-17 cycles/sample. Note the ceiling that number reveals: 5.12e-17
   * IS the `double`'s own representation error for 12/13, so the increment
   * quantisation has vanished entirely beneath the API. The clock carries 53
   * bits of usable rate resolution, not 64, and going wider buys nothing
   * while `norm_freq` is a `double`.
   *
   * The word is deliberately NOT wired into `nco_state_t`: `symsync` and
   * `dll` embed that struct by value and serialize it, and neither needs the
   * extra bits.
   */
  typedef struct
  {
    /* These are plain comments rather than doxygen member comments, and
       deliberately so: jm harvests a field's doxygen into the .pyi
       docstring of EVERY same-named property in the project, so
       documenting norm_freq here silently rewrote Costas's,
       CarrierMpsk's, CarrierNda's and Despreader's. nco_state_t above is
       plain for the same reason. (Nor can that form even be named in
       prose here -- its opening delimiter starts a nested comment that
       doxygen never closes, which is a whole-file parse failure.) */
    uint64_t phase;     /* current accumulator value, [0, 2^64)        */
    uint64_t phase_inc; /* advance per input = frac(norm_freq) x 2^64  */
    double   norm_freq; /* SIGNED rate in cycles/input, kept pre-fold  */
  } nco_clock_t;

  /**
   * @brief Double -> 64-bit phase word: the clock's one float conversion.
   *
   * The 64-bit twin of @ref nco_phase_units, and the same total contract:
   * below zero and NaN give 0, at or above 2^64 saturates, in between it
   * truncates toward zero. See that function for why confining the cast is
   * structural rather than stylistic.
   *
   * The in-range proof is tighter here and worth stating. A `double` names
   * every integer below 2^32 exactly, so the 32-bit version's margin is
   * exact; below 2^64 it does not, and the spacing near the top is 2^11.
   * The bound still holds -- the largest `double` under 1.0 times 2^64 is
   * exactly 2^64 - 2048, representable and safely under -- but it is a
   * 2048-unit margin, not an exact one, so the guard carries the weight.
   */
  JM_FORCEINLINE uint64_t
  nco_clock_units (double units)
  {
    /* Negated form rejects NaN; see nco_phase_units(). */
    if (!(units > 0.0))
      return 0u;
    if (units >= 18446744073709551616.0)
      return 18446744073709551615u;
    return (uint64_t)units;
  }

  /** @brief Normalised cycles -> 64-bit phase delta (folds, then converts). */
  JM_FORCEINLINE uint64_t
  nco_clock_norm_to_inc (double cycles)
  {
    /* The fold rounds up to exactly 1.0 for cycles in [-2^-53, 0); the
       conversion clamps it, exactly as nco_norm_to_inc() does at 32 bits. */
    double d = cycles - floor (cycles);
    return nco_clock_units (d * 18446744073709551616.0);
  }

  /** @brief Start a clock at phase 0 running at @p norm_freq cycles/input. */
  JM_FORCEINLINE void
  nco_clock_init (nco_clock_t *c, double norm_freq)
  {
    c->phase     = 0u;
    c->norm_freq = norm_freq;
    c->phase_inc = nco_clock_norm_to_inc (norm_freq);
  }

  /** @brief Retune without restarting: phase is deliberately preserved, so a
   *         rate change mid-stream does not move the sampling instant. */
  JM_FORCEINLINE void
  nco_clock_set_rate (nco_clock_t *c, double norm_freq)
  {
    c->norm_freq = norm_freq;
    c->phase_inc = nco_clock_norm_to_inc (norm_freq);
  }

  /**
   * @brief Advance one input; report whether a period boundary was crossed.
   *
   * The 64-bit form of @ref nco_step_u32_ovf_ctrl, with the same signed
   * rule and for the same reason: the fold destroys direction, so the
   * composite advance `norm_freq + ctrl` is formed in cycles BEFORE
   * anything is folded, and its sign decides whether a crossing is a carry
   * (one EXTRA output due) or a borrow (one FEWER). Keying off @p ctrl's
   * sign is wrong in reverse -- the composite can run forward while the
   * control is negative. `|delta| >= 1` crosses every input regardless of
   * where the fraction lands, which covers both the interpolating case and
   * the exactly-unity rate a terminal resampler stage sits on.
   *
   * @param c      Clock state. Must be non-NULL.
   * @param ctrl   Per-input rate deviation, added to the base rate.
   * @param event  Out-param: 1 if this input crossed a period boundary.
   * @return Phase BEFORE the advance (the caller compares it against
   *         `c->phase` when it needs the crossing count, not just the flag).
   */
  JM_FORCEINLINE JM_HOT uint64_t
  nco_clock_tick (nco_clock_t *c, double ctrl, uint8_t *event)
  {
    uint64_t ph    = c->phase;
    uint64_t nph   = ph + c->phase_inc + nco_clock_norm_to_inc (ctrl);
    double   delta = c->norm_freq + ctrl; /* signed, pre-fold */

    c->phase = nph;
    if (delta > 0.0)
      *event = (uint8_t)(nph < ph || delta >= 1.0);
    else if (delta < 0.0)
      *event = (uint8_t)(nph > ph || delta <= -1.0);
    else
      *event = 0u;
    return ph;
  }

  /**
   * @brief Advance by an explicit increment; report the wrap.
   *
   * The output-driven counterpart to @ref nco_clock_tick. An interpolator
   * ticks once per OUTPUT and pulls an input when the phase wraps, so its
   * increment is the RECIPROCAL rate (inputs consumed per output) and is
   * supplied by the caller rather than derived from `norm_freq`; the wrap
   * means "fetch the next input", not "an output is due".
   *
   * @return 1 if the add wrapped past 2^64, else 0.
   */
  JM_FORCEINLINE JM_HOT uint8_t
  nco_clock_advance (nco_clock_t *c, uint64_t inc)
  {
    uint64_t ph = c->phase;
    c->phase    = ph + inc;
    return (uint8_t)(c->phase < ph);
  }

  /**
   * @brief Top @p bits of the phase: where in the period the clock stands.
   *
   * A uint64 word cannot leave [0, 1), so this is in `[0, 2^bits)` by
   * construction -- which is the whole point for a polyphase caller, whose
   * arm index can then never need a range check. @p bits of 0 gives 0
   * rather than an undefined 64-bit shift.
   */
  JM_FORCEINLINE size_t
  nco_clock_frac (const nco_clock_t *c, unsigned bits)
  {
    return bits ? (size_t)(c->phase >> (64u - bits)) : (size_t)0;
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
   * nco_norm_to_inc()'s own doc comment on why duplicated copies of
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
    state->phase = ph + state->phase_inc + nco_norm_to_inc (ctrl);
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
    state->phase  = ph + state->phase_inc + nco_norm_to_inc (ctrl);
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
   * **The sign must be taken before the fold.** nco_norm_to_inc() folds
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
    uint32_t ph    = state->phase;
    /* Wrapping u32 add: bit-for-bit the modulo advance the 64-bit sum
       used to produce -- only the event derivation below changes. */
    uint32_t nph   = ph + state->phase_inc + nco_norm_to_inc (ctrl);
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
   * @brief Maximum samples per call (determines pre-allocated buffer size).
   *
   * The Python extension pre-allocates output buffers of this size at
   * create time.  Requesting more samples per call is undefined behaviour.
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
   * Python's `out=` keyword writes directly into a caller-supplied
   * buffer instead of allocating a fresh one -- essential for driving
   * this from a hot per-epoch tracking loop with no per-call
   * allocation (fill `ctrl` in place, reuse the same `out` buffer every
   * call). That buffer must be sized to `steps_u32_ctrl_max_out()`,
   * NOT just `len(ctrl)` -- the returned view is still correctly
   * sliced to `len(ctrl)` regardless of the buffer's actual size.
   *
   * @param state     NCO state returned by nco_create().
   * @param ctrl      Float32 array of per-sample normalised-frequency
   *                  control offsets, any sign (the fractional cycle is
   *                  taken, so it wraps correctly).
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
   * >>> ctrl = np.full(4, 0.25, dtype=np.float32)
   * >>> out = nco.steps_u32_ctrl(ctrl)
   * >>> out.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * >>> nco.norm_freq
   * 0.0
   * @endcode
   */
  size_t nco_steps_u32_ctrl (nco_state_t *state, const float *ctrl,
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
   * @param ctrl      Float32 array of per-sample normalised-frequency
   *                  control offsets, any sign (the fractional cycle is
   *                  taken, so it wraps correctly).
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
   * >>> ctrl = np.full(4, 0.25, dtype=np.float32)
   * >>> out = nco.steps_u32_scaled_ctrl(ctrl)
   * >>> out.tolist()
   * [0, 1, 2, 3]
   * @endcode
   */
  size_t nco_steps_u32_scaled_ctrl (nco_state_t *state, const float *ctrl,
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
   * @param ctrl      Float32 array of per-sample normalised-frequency
   *                  control offsets, any sign (the fractional cycle is
   *                  taken, so it wraps correctly).
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
   * >>> ctrl = np.zeros(4, dtype=np.float32)
   * >>> ph, carry = nco.steps_u32_ovf_ctrl(ctrl)
   * >>> ph.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * >>> carry.tolist()
   * [0, 0, 0, 1]
   * @endcode
   */
  size_t nco_steps_u32_ovf_ctrl (nco_state_t *state, const float *ctrl,
                                 size_t ctrl_len, uint32_t *out,
                                 uint8_t *out1, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
