/**
 * @file boxcar_core.h
 * @brief Boxcar (rectangular) moving-average filter — cf32, fixed window.
 *
 * A sliding-window moving average over the last `len` complex samples: one
 * output per input sample (no rate change). Each step adds the new sample and
 * subtracts the sample leaving the window, so it is O(1) per sample regardless
 * of window length (a running window sum, not a re-summed convolution). The
 * output is the window mean times an optional output `gain`: `gain·(Σ window)/
 * len`. Because the step must multiply by `1/len` anyway, the gain is folded
 * into a single cached `scale = gain/len`, so applying it is free — a
 * composing loop (e.g. a carrier arm with an AGC) can push its gain into the
 * boxcar and avoid a second multiply.
 *
 * The delay ring is a **fixed in-struct array** (`BOXCAR_MAX_LEN`), so the
 * state is pointer-free POD: it embeds by value into a composing object (a
 * carrier loop's I/Q arm, a smoother ahead of a detector) and serializes as a
 * whole-struct snapshot. A window longer than `BOXCAR_MAX_LEN` is rejected at
 * create/init time. (A bounded window sum also stays numerically clean —
 * unlike a never-reset CIC integrator-comb, whose integrator drifts in float.)
 *
 * Until the ring fills (the first `len-1` samples after a reset) the ring
 * holds zeros, so the average is taken over a partial window and the output
 * ramps in.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.filter import MovingAverage
 * >>> ma = MovingAverage(2)                       # 2-sample window, unit gain
 * >>> ma.steps(np.ones(3, np.complex64)).real.tolist()
 * [0.5, 1.0, 1.0]
 * >>> ma2 = MovingAverage(2, gain=2.0)            # gain folded into the mean
 * >>> ma2.steps(np.ones(3, np.complex64)).real.tolist()
 * [1.0, 2.0, 2.0]
 * @endcode
 */
#ifndef BOXCAR_CORE_H
#define BOXCAR_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /* Maximum window length. The delay ring is a fixed in-struct array so the
   * state stays pointer-free POD (embed-by-value + whole-struct
   * serialization); a longer window is rejected at create/init time. */
#define BOXCAR_MAX_LEN 64

  /**
   * @brief Boxcar moving-average state (cf32).
   *
   * Pointer-free POD. Allocate with boxcar_create(), or embed by value and
   * boxcar_init(). The accumulator and ring are internal; read `len`/`gain`
   * for the configured window and output gain.
   */
  typedef struct
  {
    size_t len;        /**< window length (1 .. BOXCAR_MAX_LEN).         */
    size_t pos;        /**< ring write index (0 .. len-1).               */
    double inv_len;    /**< cached 1 / len.                              */
    double gain;       /**< output gain applied to the mean.            */
    float  scale;      /**< cached (float)(gain / len) — the per-sample
                            applied multiply.                            */
    float complex acc; /**< running sum over the window.                 */
    float complex ring[BOXCAR_MAX_LEN]; /**< delay line.                 */
  } boxcar_state_t;

  /**
   * @brief Slide the window by one sample; return the gained moving average.
   *
   * O(1): add @p x, drop the sample leaving the window, return
   * `acc · scale` (= `gain · acc / len`) — one multiply.
   *
   * @param s  Boxcar state. Must be non-NULL.
   * @param x  One input sample.
   * @return The gained window mean after admitting @p x.
   */
  JM_FORCEINLINE JM_HOT float complex
  boxcar_step (boxcar_state_t *s, float complex x)
  {
    s->acc += x - s->ring[s->pos];
    s->ring[s->pos] = x;
    if (++s->pos >= s->len)
      s->pos = 0;
    return s->acc * s->scale;
  }

  /**
   * @brief Set the output gain; refresh the cached scale.
   * @param s     Boxcar state. Must be non-NULL.
   * @param gain  New output gain (folded into `scale = gain / len`).
   */
  JM_FORCEINLINE void
  boxcar_set_gain (boxcar_state_t *s, double gain)
  {
    s->gain  = gain;
    s->scale = (float)(gain * s->inv_len);
  }

  /** @brief Current output gain. */
  JM_FORCEINLINE double
  boxcar_get_gain (const boxcar_state_t *s)
  {
    return s->gain;
  }

  /**
   * @brief Initialise a boxcar in place (no allocation).
   * @param s     State to initialise. Must be non-NULL.
   * @param len   Window length; clamped to `[1, BOXCAR_MAX_LEN]`.
   * @param gain  Output gain (folded into the averaging scale).
   */
  void boxcar_init (boxcar_state_t *s, size_t len, double gain);

  /**
   * @brief Create a boxcar instance.
   * @param len   Window length (1 .. BOXCAR_MAX_LEN; default 4).
   * @param gain  Output gain (default 1.0).
   * @return Heap state, or NULL on invalid length / allocation failure.
   * @note Caller must call boxcar_destroy() when done.
   */
  boxcar_state_t *boxcar_create (size_t len, double gain);

  /** @brief Destroy a boxcar instance. @param s May be NULL. */
  void boxcar_destroy (boxcar_state_t *s);

  /** @brief Clear the window (zero the ring and the running sum); keep config.
   */
  void boxcar_reset (boxcar_state_t *s);

  /**
   * @brief Filter a block: write the gained moving average of each sample.
   * @param s       Boxcar state. Must be non-NULL.
   * @param input   Input samples.
   * @param output  Output (gained window means); may alias @p input.
   * @param n       Number of samples.
   */
  void boxcar_steps (boxcar_state_t *s, const float complex *input,
                     float complex *output, size_t n);

  /* ── Serializable state (standard bytes interface; see dp_state.h)
   * ────────── Pointer-free POD struct, so a whole-struct snapshot resumes
   * exactly. */
#define BOXCAR_STATE_MAGIC DP_FOURCC ('B', 'O', 'X', 'C')
#define BOXCAR_STATE_VERSION 1u

  /** @brief Serialized-state byte size. */
  size_t boxcar_state_bytes (const boxcar_state_t *s);
  /** @brief Serialize the full state into @p blob. */
  void boxcar_get_state (const boxcar_state_t *s, void *blob);
  /** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects.
   */
  int boxcar_set_state (boxcar_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BOXCAR_CORE_H */
