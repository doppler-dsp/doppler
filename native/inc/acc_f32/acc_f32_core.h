/**
 * @file acc_f32_core.h
 * @brief AccF32 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * acc_f32_state_t *obj = acc_f32_create(0.0f);
 * acc_f32_step(obj, 0.0f);
 * acc_f32_destroy(obj);
 * @endcode
 */
#ifndef ACC_F32_CORE_H
#define ACC_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief AccF32 state.
   *
   * Allocate with acc_f32_create().
   */
  typedef struct
  {
    float acc;
  } acc_f32_state_t;

  /**
   * @brief Create a acc_f32 instance.
   *
   * @param acc  Initial acc (default: 0.0f).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call acc_f32_destroy() when done.
   */
  acc_f32_state_t *acc_f32_create (float acc);

  /**
   * @brief Destroy a acc_f32 instance and release all memory.
   * @param state  May be NULL.
   */
  void acc_f32_destroy (acc_f32_state_t *state);

  /**
   * @brief Reset acc_f32 to its post-create state.
   * @param state  Must be non-NULL.
   */
  void acc_f32_reset (acc_f32_state_t *state);

  /**
   * @brief Consume one input sample (sink; no output).
   * @param state  Must be non-NULL.
   * @param x      Input sample (float).
   */
  JM_FORCEINLINE JM_HOT void
  acc_f32_step (acc_f32_state_t *state, float x)
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
  void acc_f32_steps (acc_f32_state_t *state, const float *input, size_t n);

  /**
   * @brief Get current acc.
   * @param state  Must be non-NULL.
   */
  float acc_f32_get_acc (const acc_f32_state_t *state);

  /**
   * @brief Set acc.
   * @param state  Must be non-NULL.
   * @param acc  New value.
   */
  void acc_f32_set_acc (acc_f32_state_t *state, float acc);

  /**
   * @brief get.
   *
   * @param state  Must be non-NULL.
   * @return Result (float).
   */
  float acc_f32_get (acc_f32_state_t *state);

  /**
   * @brief dump.
   *
   * @param state  Must be non-NULL.
   * @return Result (float).
   */
  float acc_f32_dump (acc_f32_state_t *state);

  /**
   * @brief madd.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (float[]).
   * @param h  float[] parameter.
   */
  void acc_f32_madd (acc_f32_state_t *state, const float *x, size_t x_len,
                     const float *h, size_t h_len);

  /**
   * @brief add2d.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (float[]).
   */
  void acc_f32_add2d (acc_f32_state_t *state, const float *x, size_t x_len);

  /**
   * @brief madd2d.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (float[]).
   * @param h  float[] parameter.
   */
  void acc_f32_madd2d (acc_f32_state_t *state, const float *x, size_t x_len,
                       const float *h, size_t h_len);

#ifdef __cplusplus
}
#endif

#endif /* ACC_F32_CORE_H */
