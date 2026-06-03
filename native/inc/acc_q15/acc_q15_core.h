/**
 * @file acc_q15_core.h
 * @brief AccQ15 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * acc_q15_state_t *obj = acc_q15_create(0);
 * acc_q15_step(obj, 0);
 * acc_q15_destroy(obj);
 * @endcode
 */
#ifndef ACC_Q15_CORE_H
#define ACC_Q15_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AccQ15 state.
 *
 * Allocate with acc_q15_create().
 */
typedef struct {
    int64_t acc;
} acc_q15_state_t;

/**
 * @brief Create a acc_q15 instance.
 *
 * @param acc  Initial acc (default: 0).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call acc_q15_destroy() when done.
 */
acc_q15_state_t *acc_q15_create(int64_t acc);

/**
 * @brief Destroy a acc_q15 instance and release all memory.
 * @param state  May be NULL.
 */
void acc_q15_destroy(acc_q15_state_t *state);

/**
 * @brief Reset AccQ15 to its post-create state.
 * @param state  Must be non-NULL.
 */
void acc_q15_reset(acc_q15_state_t *state);

/**
 * @brief Consume one input sample (sink; no output).
 * @param state  Must be non-NULL.
 * @param x      Input sample (int16_t).
 */
JM_FORCEINLINE JM_HOT void
acc_q15_step(acc_q15_state_t *state, int16_t x)
{
    state->acc += (int64_t)x;
}

/**
 * @brief Process a block of input samples (no output).
 *
 * @param state  Component state (mutated).
 * @param input  Input array (length >= n).
 * @param n     Number of samples.
 */
void acc_q15_steps(
    acc_q15_state_t *state,
    const int16_t    *input,
    size_t               n);

/**
 * @brief Get current acc.
 * @param state  Must be non-NULL.
 */
int64_t acc_q15_get_acc(const acc_q15_state_t *state);

/**
 * @brief Set acc.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void acc_q15_set_acc(acc_q15_state_t *state, int64_t val);



int64_t acc_q15_get(acc_q15_state_t *state);
int64_t acc_q15_dump(acc_q15_state_t *state);
void acc_q15_madd(acc_q15_state_t *state, const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);
#ifdef __cplusplus
}
#endif

#endif /* ACC_Q15_CORE_H */
