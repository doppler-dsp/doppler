/**
 * @file acc_q8_core.h
 * @brief AccQ8 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * acc_q8_state_t *obj = acc_q8_create(0);
 * acc_q8_step(obj, 0);
 * acc_q8_destroy(obj);
 * @endcode
 */
#ifndef ACC_Q8_CORE_H
#define ACC_Q8_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AccQ8 state.
 *
 * Allocate with acc_q8_create().
 */
typedef struct {
    int32_t acc;
} acc_q8_state_t;

/**
 * @brief Create a acc_q8 instance.
 *
 * @param acc  Initial acc (default: 0).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call acc_q8_destroy() when done.
 */
acc_q8_state_t *acc_q8_create(int32_t acc);

/**
 * @brief Destroy a acc_q8 instance and release all memory.
 * @param state  May be NULL.
 */
void acc_q8_destroy(acc_q8_state_t *state);

/**
 * @brief Reset AccQ8 to its post-create state.
 * @param state  Must be non-NULL.
 */
void acc_q8_reset(acc_q8_state_t *state);

/**
 * @brief Consume one input sample (sink; no output).
 * @param state  Must be non-NULL.
 * @param x      Input sample (int8_t).
 */
JM_FORCEINLINE JM_HOT void
acc_q8_step(acc_q8_state_t *state, int8_t x)
{
    state->acc += (int32_t)x;
}

/**
 * @brief Process a block of input samples (no output).
 *
 * @param state  Component state (mutated).
 * @param input  Input array (length >= n).
 * @param n     Number of samples.
 */
void acc_q8_steps(
    acc_q8_state_t *state,
    const int8_t    *input,
    size_t               n);

/**
 * @brief Get current acc.
 * @param state  Must be non-NULL.
 */
int32_t acc_q8_get_acc(const acc_q8_state_t *state);

/**
 * @brief Set acc.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void acc_q8_set_acc(acc_q8_state_t *state, int32_t val);



int32_t acc_q8_get(acc_q8_state_t *state);
int32_t acc_q8_dump(acc_q8_state_t *state);
void acc_q8_madd(acc_q8_state_t *state, const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);
#ifdef __cplusplus
}
#endif

#endif /* ACC_Q8_CORE_H */
