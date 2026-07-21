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
 * nco_steps_u32(nco, 4, out);
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
    double d = cycles - floor (cycles); /* fractional cycles, [0, 1) */
    /* Truncate toward zero: the C99 float->unsigned conversion (6.3.1.4)
       discards the fractional part, and d < 1 makes d*2^32 strictly < 2^32,
       so the result is always in [0, 2^32) -- the documented contract, with
       no host-FP-sensitive rounding. (llround here could round d*2^32 UP to
       exactly 2^32 for d ~ 0.9999..., and (uint32_t)2^32 == 0 would freeze
       the NCO -- x86 landed on 2^32-1, arm64 on 2^32, hanging the closed
       loop on arm64.) */
    return (uint32_t)(d * 4294967296.0);
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
   * @brief Emit the current raw phase and this step's carry, then advance
   *        by phase_inc + ctrl.
   * Single-sample form of nco_steps_u32_ovf_ctrl(). The carry reflects
   * THIS step's true advance (phase_inc + ctrl), computed as one 64-bit
   * sum so a wrap is never missed even when ctrl itself is large.
   * @param state  NCO state.  Must be non-NULL.
   * @param ctrl   Per-sample normalised-frequency control offset.
   * @param carry  Out-param: set to 1 if this step's advance wrapped past
   *               2^32, else 0. Must be non-NULL.
   * @return Phase value BEFORE the increment.
   */
  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf_ctrl (nco_state_t *state, double ctrl, uint8_t *carry)
  {
    uint32_t ph  = state->phase;
    uint64_t sum = (uint64_t)ph + (uint64_t)state->phase_inc
                   + (uint64_t)nco_norm_to_inc (ctrl);
    *carry        = (uint8_t)((sum >> 32) != 0);
    state->phase  = (uint32_t)sum;
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
   * @return n (always).
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
  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out);

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
   * @return n (always).
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
  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out);

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
   * @return n (always).
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
                            uint8_t *out1);

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
   * @return ctrl_len (always).
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
                             size_t ctrl_len, uint32_t *out);

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
   * @return ctrl_len (always).
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
                                    size_t ctrl_len, uint32_t *out);

  size_t nco_steps_u32_ovf_ctrl_max_out (nco_state_t *state);

  /**
   * @brief Advance ctrl_len samples; raw phase + per-sample carry, with a
   *        per-sample control offset added on top of phase_inc.
   *
   * The @ref nco_steps_u32_ovf output mapping (raw phase plus a carry
   * flag marking each sample whose advance wrapped past 2^32) driven by
   * the @ref nco_steps_u32_ctrl control port -- every stepper has a
   * matching control-input counterpart. The carry reflects THIS
   * sample's true advance (`phase_inc + ctrl_inc`, added as a single
   * 64-bit sum so a wrap is never missed even when the control offset
   * itself is large), not just phase_inc alone -- needed by any
   * consumer (e.g. a coupled carrier/code tracker) that must detect a
   * period boundary while the rate is being actively steered. With
   * every `ctrl[i] == 0` this is bit-identical to nco_steps_u32_ovf().
   * Returns ctrl_len.
   *
   * @param state     NCO state returned by nco_create().
   * @param ctrl      Float32 array of per-sample normalised-frequency
   *                  control offsets, any sign (the fractional cycle is
   *                  taken, so it wraps correctly).
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Phase output buffer; must hold at least ctrl_len
   *                  uint32_t values.
   * @param out1      Carry output buffer; must hold at least ctrl_len
   *                  uint8_t values.
   * @return ctrl_len (always).
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
                                 uint8_t *out1);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
