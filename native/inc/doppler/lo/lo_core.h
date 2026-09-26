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
 *
 * ## Spurious content — what the 16-bit index costs
 *
 * The index keeps the top 16 bits of a 32-bit phase, so unless the
 * increment is a whole number of LUT bins there is a per-sample phase
 * error, and that error is periodic: its period is set by the LOW 16
 * bits of phase_inc, NOT by the frequency. Three regimes, measured (see
 * src/doppler/source/tests/validation/lo/results.md, which regenerates them):
 *
 *   phase_inc & 0xFFFF == 0        no truncation at all; spur-free to the
 *                                  float32 floor, ~146 dBc
 *   a generic remainder            ~96 dBc, flat -- 400 random rates span
 *                                  96.32 to 96.33 dBc
 *   remainder == 0x8000 (half a    92.4 dBc: the error alternates with
 *   bin), and small-denominator    period 2 and all of it lands in one
 *   remainders near it             spur. This is the classical
 *                                  6.02*B - 3.92 phase-truncation bound.
 *
 * **The guarantee is SFDR >= 90 dBc at any frequency** (worst measured
 * 92.40, over 8 carrier positions x 2 capture lengths). The familiar
 * ~96 dBc is the TYPICAL figure, not a bound -- a design sizing its
 * spur budget must use 90.
 *
 * Amplitude quantization is not a contributor: the table is float32, so
 * |phasor| - 1 stays under 6e-08, four orders below the half-bin phase
 * error of 0.5/65536 cycles.
 *
 * The shared LUT is initialised lazily on the first dp_lo_create() call.
 *
 * Lifecycle: dp_lo_create → (steps / steps_ctrl / reset)* → dp_lo_destroy
 *
 * @code
 * dp_lo_state_t *lo = dp_lo_create(0.25);
 * float _Complex out[4];
 * dp_lo_steps (lo, 4, out, 4);
 * // out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
 * dp_lo_destroy(lo);
 * @endcode
 */
#ifndef DP_LO_CORE_H
#define DP_LO_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/nco/nco_core.h" /* nco_norm_freq_to_inc() -- the one shared cycles->phase-delta primitive */
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief LO state.
   *
   * Allocate with dp_lo_create(), or embed by value and lo_init() (see the
   * inline composition API below).  The shared 65536-entry LUT is
   * initialised lazily on the first dp_lo_create()/lo_init() call and never
   * freed.
   */
  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } dp_lo_state_t;

/* ---- Inline composition API (C-only; not exposed as Python methods) ----
 *
 * lo_init / lo_step let a tracking loop embed dp_lo_state_t BY VALUE and de-rotate
 * a sample stream one sample at a time with zero call overhead — the block
 * generators below (dp_lo_steps) stay the fast path for bulk synthesis.  The
 * shared sin LUT is exposed here so the inline step can index it directly.   */
#define LO_LUT_BITS 16u
#define LO_LUT_SIZE (1u << LO_LUT_BITS) /* 65536                    */
#define LO_LUT_QTR (LO_LUT_SIZE >> 2u)  /* 16384  (π/2 phase shift) */

  /**
   * @brief Shared 2^16-entry sine LUT (read-only after init).
   *
   * Filled by the first dp_lo_create()/lo_init().  Indexed by the top 16 bits of
   * the phase accumulator; the quarter-cycle offset LO_LUT_QTR maps sin→cos.
   * Do not write.  Exposed only so lo_step() can be a header inline.
   */
  extern float lo_sin_lut[LO_LUT_SIZE];

  /**
   * @brief Initialise an LO in place (no allocation).
   *
   * The by-value counterpart to dp_lo_create(): a tracking loop that embeds an
   * dp_lo_state_t initialises it with lo_init() instead of owning a heap pointer.
   * Sets phase=0, derives phase_inc from norm_freq, and fills the shared LUT
   * on first use (same single-threaded caveat as dp_lo_create()).
   *
   * @param state      LO state to initialise in place.  Must be non-NULL.
   * @param norm_freq  Normalised frequency in cycles per sample (fractional
   *                   part only).
   * @code
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)          # the Python type calls dp_lo_create
   * >>> lo.phase_inc
   * 1073741824
   * @endcode
   */
  void lo_init (dp_lo_state_t *state, double norm_freq);

  /**
   * @brief Emit the current CF32 phasor, then advance the accumulator.
   *
   * Single-sample form of dp_lo_steps(), same emit-before-increment convention
   * and bit-for-bit the same LUT math, suitable for inlining into a
   * sample-by-sample loop (e.g. carrier wipe-off ahead of a matched filter).
   * The caller must have run dp_lo_create()/lo_init() so the LUT is populated.
   *
   * @param state  LO state.  Must be non-NULL with phase/phase_inc set.
   * @return cos(θ) + j·sin(θ) at the phase BEFORE the increment.
   * @code
   * dp_lo_state_t lo;            // embedded by value, no heap
   * lo_init (&lo, 0.25);
   * float _Complex s0 = lo_step (&lo);   // 1 + 0j
   * float _Complex s1 = lo_step (&lo);   // 0 + 1j
   * @endcode
   */
  JM_FORCEINLINE JM_HOT float _Complex lo_step (dp_lo_state_t *state)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float _Complex out
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
   * dp_lo_state_t lo;
   * lo_init (&lo, 0.0);                 // centre at DC
   * float _Complex s = lo_step_ctrl (&lo, 0.01);  // step at +0.01 cyc/sample
   * @endcode
   */
  JM_FORCEINLINE JM_HOT float _Complex lo_step_ctrl (dp_lo_state_t *state,
                                                    double ctrl)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float _Complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    /* nco_norm_freq_to_inc() is the ONE shared cycles->phase-delta
     * primitive, and it TRUNCATES -- see nco_core.h for why rounding would
     * make the increment differ by host. This comment claimed the opposite
     * ("rounds, not truncates") from the consolidation until an audit
     * caught it; test_lo_core.c section 21 now pins truncation on this
     * path, so the prose cannot drift away from the code again. */
    state->phase += state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return out;
  }

  /**
   * @brief Create an LO instance.
   * Allocates state, sets phase to 0, and derives phase_inc from
   * norm_freq.  Initialises the shared 65536-entry float LUT on the
   * first call (single-threaded concern: call dp_lo_create() before
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
  dp_lo_state_t *dp_lo_create (double norm_freq);

  /** Free all resources.  May be NULL (no-op). */
  void dp_lo_destroy (dp_lo_state_t *state);

  /**
   * @brief Zero the phase accumulator.
   * Sets phase to 0 so the next dp_lo_steps call starts at angle 0 (1+0j).
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
  void dp_lo_reset (dp_lo_state_t *state);

  /* ---- Properties ---- */

  /**
   * @brief Normalised frequency (read/write).
   * Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and
   * takes effect on the next dp_lo_steps call; phase is NOT reset.
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
  double dp_lo_get_norm_freq (const dp_lo_state_t *state);
  void dp_lo_set_norm_freq (dp_lo_state_t *state, double norm_freq);

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
  uint32_t dp_lo_get_phase (const dp_lo_state_t *state);
  void dp_lo_set_phase (dp_lo_state_t *state, uint32_t phase);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Every composable filter exposes this triplet so a pure transducer
   * (ddc_fn / acq_fn) can serialize a channel's *mutable* state to a flat POD
   * and resume it bit-exactly on any thread/process/pod.  The blob holds only
   * what evolves per sample (here: the phase accumulator) — config (phase_inc,
   * norm_freq) is rebuilt from the descriptor.  Layout is the standard
   * envelope: [dp_state_hdr_t][uint32 phase]. */
#define LO_STATE_MAGIC DP_FOURCC ('L', 'O', '_', '_')
#define LO_STATE_VERSION 1u

  /** @brief Bytes dp_lo_get_state() writes for @p state (envelope + payload). */
  size_t dp_lo_state_bytes (const dp_lo_state_t *state);
  /** @brief Serialize @p state's mutable state into @p blob (>= dp_lo_state_bytes). */
  void dp_lo_get_state (const dp_lo_state_t *state, void *blob);
  /** @brief Restore mutable state from @p blob.
   *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
  int dp_lo_set_state (dp_lo_state_t *state, const void *blob);

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
  uint32_t dp_lo_get_phase_inc (const dp_lo_state_t *state);

  /* ---- Block generators ---- */

  /**
   * @brief Maximum samples per call (determines pre-allocated buffer size).
   */
  size_t dp_lo_steps_max_out (dp_lo_state_t *state);

  /**
   * @brief Generate n CF32 phasors at the current norm_freq.
   * Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE
   * the accumulator is advanced, giving a unit-magnitude complex
   * sinusoid via the 65536-entry LUT.  SFDR is ≥ 90 dBc at any
   * frequency and ~96 dBc at a typical one — see the file header for
   * why those are two different numbers.  Returns n.
   *
   * @param state  LO state returned by dp_lo_create().
   * @param n      Number of phasors to generate.
   * @param out    Output buffer; must hold at least n float _Complex values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(n, max_out) samples.
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
  size_t dp_lo_steps (dp_lo_state_t *state, size_t n, float _Complex *out,
                   size_t max_out);

  size_t dp_lo_steps_ctrl_max_out (dp_lo_state_t *state);

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
   * @param state     LO state returned by dp_lo_create().
   * @param ctrl      Per-sample normalised-frequency deviations in
   *                  `double`.  Only the fractional part of each element
   *                  contributes.  See dp_nco_steps_u32_ctrl() on why the
   *                  port is `double` and not float32.
   * @param ctrl_len  Number of elements in ctrl; equals output length.
   * @param out       Output buffer; must hold at least ctrl_len float _Complex
   *                  values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(ctrl_len, max_out) samples.
   * @code
   * >>> import numpy as np
   * >>> from doppler.source import LO
   * >>> lo = LO(0.25)
   * >>> ctrl = np.zeros(4, dtype=np.float64)
   * >>> out = lo.steps_ctrl(ctrl)
   * >>> out.dtype
   * dtype('complex64')
   * >>> out.shape
   * (4,)
   * >>> [round(float(abs(c)), 4) for c in out]
   * [1.0, 1.0, 1.0, 1.0]
   * @endcode
   */
  size_t dp_lo_steps_ctrl (dp_lo_state_t *state, const double *ctrl, size_t ctrl_len,
                        float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
