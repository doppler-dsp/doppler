/**
 * @file acc_cf64_core.h
 * @brief AccCf64 component API.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * acc_cf64_state_t *obj = acc_cf64_create(0.0 + 0.0 * I);
 * acc_cf64_step(obj, 0.0 + 0.0 * I);
 * acc_cf64_destroy(obj);
 * @endcode
 */
#ifndef ACC_CF64_CORE_H
#define ACC_CF64_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief AccCf64 state.
   *
   * Allocate with acc_cf64_create().
   */
  typedef struct
  {
    double _Complex acc;
  } acc_cf64_state_t;

  /**
   * @brief Create a acc_cf64 instance.
   *
   * @param acc  Initial acc (default: 0.0 + 0.0 * I).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call acc_cf64_destroy() when done.
   */
  acc_cf64_state_t *acc_cf64_create (double _Complex acc);

  /**
   * @brief Destroy a acc_cf64 instance and release all memory.
   * @param state  May be NULL.
   */
  void acc_cf64_destroy (acc_cf64_state_t *state);

  /**
   * @brief Reset acc_cf64 to its post-create state.
   * @param state  Must be non-NULL.
   */
  void acc_cf64_reset (acc_cf64_state_t *state);

  /**
   * @brief Consume one input sample (sink; no output).
   * @param state  Must be non-NULL.
   * @param x      Input sample (double complex).
   */
  JM_FORCEINLINE JM_HOT void
  acc_cf64_step (acc_cf64_state_t *state, double complex x)
  {
    state->acc += x;
  }

  /**
   * @brief Process a block of input samples (no output).
   *
   * @param state  Component state (mutated).
   * @param input  Input array (length >= n).
   * @param n     Number of samples.
   */
  void acc_cf64_steps (acc_cf64_state_t *state, const double complex *input,
                       size_t n);

  /**
   * @brief Get current acc.
   * @param state  Must be non-NULL.
   */
  double _Complex acc_cf64_get_acc (const acc_cf64_state_t *state);

  /**
   * @brief Set acc.
   * @param state  Must be non-NULL.
   * @param acc  New value.
   */
  void acc_cf64_set_acc (acc_cf64_state_t *state, double _Complex acc);

  /**
   * @brief get.
   *
   * @param state  Must be non-NULL.
   * @return Result (double complex).
   */
  double complex acc_cf64_get (acc_cf64_state_t *state);

  /**
   * @brief dump.
   *
   * @param state  Must be non-NULL.
   * @return Result (double complex).
   */
  double complex acc_cf64_dump (acc_cf64_state_t *state);

  /**
   * @brief madd.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (double complex[]).
   * @param h  float[] parameter.
   */
  void acc_cf64_madd (acc_cf64_state_t *state, const double complex *x,
                      size_t x_len, const float *h, size_t h_len);

  /**
   * @brief add2d.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (double complex[]).
   */
  void acc_cf64_add2d (acc_cf64_state_t *state, const double complex *x,
                       size_t x_len);

  /**
   * @brief madd2d.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (double complex[]).
   * @param h  float[] parameter.
   */
  void acc_cf64_madd2d (acc_cf64_state_t *state, const double complex *x,
                        size_t x_len, const float *h, size_t h_len);

#ifdef __cplusplus
}
#endif

#endif /* ACC_CF64_CORE_H */
