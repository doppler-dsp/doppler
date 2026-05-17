/**
 * @file delay_core.h
 * @brief Delay component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * delay_state_t *obj = delay_create();
 * float complex y = delay_step(obj, 0.0f + 0.0f * I);
 * delay_destroy(obj);
 * @endcode
 */
#ifndef DELAY_CORE_H
#define DELAY_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Delay state.
   *
   * Dual-buffer circular delay line.  The backing store is a contiguous
   * allocation of 2*capacity elements: the first half is the live ring;
   * the second half mirrors it so that any window of num_taps consecutive
   * samples is always contiguous in memory (no wrap-around copy needed).
   *
   * Allocate with delay_create().
   */
  typedef struct
  {
    double _Complex *buf; /* 2*capacity elements; second half mirrors first */
    size_t head;          /* write pointer; decrements mod capacity */
    size_t mask;          /* capacity - 1 (power-of-two bitmask) */
    size_t num_taps;      /* window length requested at construction */
    size_t capacity;      /* smallest power-of-two >= num_taps */
  } delay_state_t;

  /**
   * @brief Create a delay instance.
   *
   * Allocates a dual-buffer circular delay line of length num_taps.
   * The internal buffer is rounded up to the next power of two for
   * efficient modular addressing.
   *
   * @param num_taps  Window length (>= 1).  Rounded up internally.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call delay_destroy() when done.
   */
  delay_state_t *delay_create (size_t num_taps);

  /**
   * @brief Destroy a delay instance and release all memory.
   * @param state  May be NULL.
   */
  void delay_destroy (delay_state_t *state);

  /**
   * @brief Reset delay to its post-create state.
   * @param state  Must be non-NULL.
   */
  void delay_reset (delay_state_t *state);

  /**
   * @brief push.
   *
   * @param state  Must be non-NULL.
   * @param x  double complex parameter.
   */
  void delay_push (delay_state_t *state, double complex x);

  /**
   * @brief ptr.
   *
   * @param state  Must be non-NULL.
   * @return Result (double complex).
   */
  size_t delay_ptr_max_out (delay_state_t *state);
  size_t delay_ptr (delay_state_t *state, size_t n, double complex *out);

  /**
   * @brief push_ptr.
   *
   * @param state  Must be non-NULL.
   * @param x  double complex parameter.
   * @return Result (double complex).
   */
  size_t delay_push_ptr_max_out (delay_state_t *state);
  size_t delay_push_ptr (delay_state_t *state, double complex x,
                         double complex *out);

  /**
   * @brief write.
   *
   * @param state  Must be non-NULL.
   * @param x      Input (double complex).
   */
  void delay_write (delay_state_t *state, double complex x);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_CORE_H */
