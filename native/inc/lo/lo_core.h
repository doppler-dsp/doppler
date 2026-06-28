/**
 * @file lo_core.h
 * @brief Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors.
 *
 * Wraps the integer NCO in a CF32 phasor generator.  The 32-bit phase
 * accumulator drives a static 65536-entry float sine LUT; the top 16
 * bits of the phase select the LUT index, and a quarter-cycle offset
 * (LUT_QTR = 16384) converts sin to cos without extra storage:
 *
 *   idx      = phase >> 16
 *   out(i)   = cos(θ) + j·sin(θ)
 *            = lut((idx + LUT_QTR) & 0xFFFF) + j·lut(idx)
 *
 * Output is emitted BEFORE the phase is incremented (same convention as NCO).
 * The 16-bit phase truncation gives ~96 dBc SFDR.
 *
 * The shared LUT is initialised lazily on the first lo_create() call.
 *
 * Lifecycle: lo_create → (steps / steps_ctrl / reset)* → lo_destroy
 *
 * @code
 * lo_state_t *lo = lo_create(0.25);
 * float complex out[4];
 * lo_steps(lo, 4, out);
 * // out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
 * lo_destroy(lo);
 * @endcode
 */
#ifndef LO_CORE_H
#define LO_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h> /* floor() in the lo_step_ctrl control port */
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief LO state.
   *
   * Allocate with lo_create(), or embed by value and lo_init() (see the
   * inline composition API below).  The shared 65536-entry LUT is
   * initialised lazily on the first lo_create()/lo_init() call and never
   * freed.
   */
  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } lo_state_t;

/* ---- Inline composition API (C-only; not exposed as Python methods) ----
 *
 * lo_init / lo_step let a tracking loop embed lo_state_t BY VALUE and de-rotate
 * a sample stream one sample at a time with zero call overhead — the block
 * generators below (lo_steps) stay the fast path for bulk synthesis.  The
 * shared sin LUT is exposed here so the inline step can index it directly.   */
#define LO_LUT_BITS 16u
#define LO_LUT_SIZE (1u << LO_LUT_BITS) /* 65536                    */
#define LO_LUT_QTR (LO_LUT_SIZE >> 2u)  /* 16384  (π/2 phase shift) */

  /**
   * @brief Shared 2^16-entry sine LUT (read-only after init).
   *
   * Filled by the first lo_create()/lo_init().  Indexed by the top 16 bits of
   * the phase accumulator; the quarter-cycle offset LO_LUT_QTR maps sin→cos.
   * Do not write.  Exposed only so lo_step() can be a header inline.
   */
  extern float lo_sin_lut[LO_LUT_SIZE];

  /**
   * @brief Initialise an LO in place (no allocation).
   *
   * The by-value counterpart to lo_create(): a tracking loop that embeds an
   * lo_state_t initialises it with lo_init() instead of owning a heap pointer.
   * Sets phase=0, derives phase_inc from norm_freq, and fills the shared LUT
   * on first use (same single-threaded caveat as lo_create()).
   *
   * @param state      LO state to initialise in place.  Must be non-NULL.
   * @param norm_freq  Normalised frequency in cycles per sample (fractional
   *                   part only).
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)            # the Python type uses lo_create internally
   * >>> lo.phase_inc
   * 1073741824
   * @endcode
   */
  void lo_init (lo_state_t *state, double norm_freq);

  /**
   * @brief Emit the current CF32 phasor, then advance the accumulator.
   *
   * Single-sample form of lo_steps(), same emit-before-increment convention
   * and bit-for-bit the same LUT math, suitable for inlining into a
   * sample-by-sample loop (e.g. carrier wipe-off ahead of a matched filter).
   * The caller must have run lo_create()/lo_init() so the LUT is populated.
   *
   * @param state  LO state.  Must be non-NULL with phase/phase_inc set.
   * @return cos(θ) + j·sin(θ) at the phase BEFORE the increment.
   * @code
   * lo_state_t lo;            // embedded by value, no heap
   * lo_init (&lo, 0.25);
   * float complex s0 = lo_step (&lo);   // 1 + 0j
   * float complex s1 = lo_step (&lo);   // 0 + 1j
   * @endcode
   */
  JM_FORCEINLINE JM_HOT float complex lo_step (lo_state_t *state)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    state->phase += state->phase_inc;
    return out;
  }

  /**
   * @brief Emit the current CF32 phasor, then advance by phase_inc + control.
   *
   * The NCO **control port** for a tracking loop: @p ctrl is a per-sample
   * frequency control in normalized cycles/sample, added on top of the centre
   * increment @c phase_inc for this step only (not persisted — the loop filter
   * holds the integrator and supplies its full output as @p ctrl each sample).
   * The LO owns the cycles→phase scaling, so the loop never touches the integer
   * phase accumulator. Same emit-before-increment convention as lo_step(); with
   * @p ctrl == 0 it is bit-identical to lo_step().
   *
   * @param state  LO state.  Must be non-NULL with phase/phase_inc set.
   * @param ctrl   Frequency control, normalized cycles/sample (any sign; the
   *               fractional cycle is taken, so it wraps correctly).
   * @return cos(θ) + j·sin(θ) at the phase BEFORE the increment.
   * @code
   * lo_state_t lo;
   * lo_init (&lo, 0.0);                 // centre at DC
   * float complex s = lo_step_ctrl (&lo, 0.01);  // step at +0.01 cyc/sample
   * @endcode
   */
  JM_FORCEINLINE JM_HOT float complex lo_step_ctrl (lo_state_t *state,
                                                    double ctrl)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    /* Fractional cycle of the control → 32-bit phase units (the LO's own
     * scaling); floor handles negative corrections by wrapping mod 1 cycle. */
    double f = ctrl - floor (ctrl);
    state->phase += state->phase_inc + (uint32_t)(f * 4294967296.0);
    return out;
  }

  /**
   * @brief Create an LO instance.
   * Allocates state, sets phase to 0, and derives phase_inc from
   * norm_freq.  Initialises the shared 65536-entry float LUT on the
   * first call (single-threaded concern: call lo_create() before
   * spawning threads that share LO instances).
   *
   * @param norm_freq  Normalised frequency in cycles per sample.
   *                   Any real value; only the fractional part matters.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(norm_freq=0.25)
   * >>> lo.phase_inc
   * 1073741824
   * @endcode
   */
  lo_state_t *lo_create (double norm_freq);

  /** Free all resources.  May be NULL (no-op). */
  void lo_destroy (lo_state_t *state);

  /**
   * @brief Zero the phase accumulator.
   * Sets phase to 0 so the next lo_steps call starts at angle 0 (1+0j).
   * norm_freq and phase_inc are unchanged.
   *
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> _ = lo.steps(2)
   * >>> lo.phase
   * 2147483648
   * >>> lo.reset()
   * >>> lo.phase
   * 0
   * >>> lo.norm_freq
   * 0.25
   * @endcode
   */
  void lo_reset (lo_state_t *state);

  /* ---- Properties ---- */

  /**
   * @brief Normalised frequency (read/write).
   * Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and
   * takes effect on the next lo_steps call; phase is NOT reset.
   *
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> lo.norm_freq
   * 0.25
   * >>> lo.norm_freq = 0.5
   * >>> lo.phase_inc
   * 2147483648
   * @endcode
   */
  double lo_get_norm_freq (const lo_state_t *state);
  void lo_set_norm_freq (lo_state_t *state, double norm_freq);

  /**
   * @brief Current phase accumulator value (read/write).
   * Returns the current integer phase in `[0, 2^32)`.  Writing overrides
   * the accumulator directly for phase-coherent frequency switching.
   *
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> lo.phase
   * 0
   * >>> lo.phase = 1073741824
   * >>> lo.phase
   * 1073741824
   * @endcode
   */
  uint32_t lo_get_phase (const lo_state_t *state);
  void lo_set_phase (lo_state_t *state, uint32_t phase);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Every composable filter exposes this triplet so a pure transducer
   * (ddc_fn / acq_fn) can serialize a channel's *mutable* state to a flat POD
   * and resume it bit-exactly on any thread/process/pod.  The blob holds only
   * what evolves per sample (here: the phase accumulator) — config (phase_inc,
   * norm_freq) is rebuilt from the descriptor.  Layout is the standard
   * envelope: [dp_state_hdr_t][uint32 phase]. */
#define LO_STATE_MAGIC DP_FOURCC ('L', 'O', '_', '_')
#define LO_STATE_VERSION 1u

  /** @brief Bytes lo_get_state() writes for @p state (envelope + payload). */
  size_t lo_state_bytes (const lo_state_t *state);
  /** @brief Serialize @p state's mutable state into @p blob (>= lo_state_bytes). */
  void lo_get_state (const lo_state_t *state, void *blob);
  /** @brief Restore mutable state from @p blob.
   *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
  int lo_set_state (lo_state_t *state, const void *blob);

  /**
   * @brief Per-sample phase increment (read-only).
   * Derived from norm_freq as floor(frac(norm_freq) × 2^32).  A freq
   * of 0.25 gives phase_inc = 1073741824 (0x40000000).
   *
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> lo.phase_inc
   * 1073741824
   * @endcode
   */
  uint32_t lo_get_phase_inc (const lo_state_t *state);

  /* ---- Block generators ---- */

  /**
   * @brief Maximum samples per call (determines pre-allocated buffer size).
   */
  size_t lo_steps_max_out (lo_state_t *state);

  /**
   * @brief Generate n CF32 phasors at the current norm_freq.
   * Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE
   * the accumulator is advanced, giving a unit-magnitude complex
   * sinusoid via the 65536-entry LUT.  SFDR ≈ 96 dBc.  Returns n.
   *
   * @param state  LO state returned by lo_create().
   * @param n      Number of phasors to generate.
   * @param out    Output buffer; must hold at least n float complex values.
   * @return n (always).
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> out = lo.steps(4)
   * >>> out.dtype
   * dtype('complex64')
   * >>> out.shape
   * (4,)
   * >>> [round(float(abs(c)), 4) for c in out]
   * [1.0, 1.0, 1.0, 1.0]
   * @endcode
   */
  size_t lo_steps (lo_state_t *state, size_t n, float complex *out);

  size_t lo_steps_ctrl_max_out (lo_state_t *state);

  /**
   * @brief Generate CF32 phasors with per-sample FM deviation.
   * For each sample i, `ctrl[i]`'s fractional part is converted to a
   * delta phase-increment (delta = floor(frac(`ctrl[i]`) × 2^32)) that
   * is added on top of the base phase_inc for that one step only.  The
   * base norm_freq and phase_inc are NOT modified; the deviation is
   * transient per sample, making this the natural API for FM synthesis
   * and frequency-hopping.  Output length equals ctrl_len.  Returns
   * ctrl_len.
   *
   * @param state     LO state returned by lo_create().
   * @param ctrl      Float32 array of per-sample normalised-frequency
   *                  deviations.  Only the fractional part of each element
   *                  contributes.
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Output buffer; must hold at least ctrl_len float complex
   *                  values.
   * @return ctrl_len (always).
   * @code
   * >>> import numpy as np
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> ctrl = np.zeros(4, dtype=np.float32)
   * >>> out = lo.steps_ctrl(ctrl)
   * >>> out.dtype
   * dtype('complex64')
   * >>> out.shape
   * (4,)
   * >>> [round(float(abs(c)), 4) for c in out]
   * [1.0, 1.0, 1.0, 1.0]
   * @endcode
   */
  size_t lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
                        float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
