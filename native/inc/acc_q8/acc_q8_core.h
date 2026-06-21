/**
 * @file acc_q8_core.h
 * @brief AccQ8 — a running 32-bit integer accumulator for Q8 (int8_t)
 * samples. Internally sums each sample into a 32-bit accumulator, which
 * can hold up to 2^24 maximum-magnitude Q8 samples before overflow. Use
 * get() for a non-destructive read, or dump() to read-and-reset in one
 * atomic call.
 *
 * Lifecycle: create -> `[step / steps / madd / reset]*` -> `[get / dump]*` ->
 * destroy
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.get()
 * 0
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
 * @brief Allocate and initialise an AccQ8 accumulator.
 * The accumulator starts at the supplied initial value and accepts Q8
 * (int8_t) samples via step(), steps(), or madd(). The 32-bit internal
 * register handles up to roughly 16 million max-magnitude samples before
 * wrap — sufficient for all standard DSP block sizes.
 *
 * @param acc  Initial accumulator value (default: 0).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call acc_q8_destroy() when done.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(10)
 * >>> obj.get_acc()
 * 10
 * @endcode
 */
acc_q8_state_t *acc_q8_create(int32_t acc);

/**
 * @brief Destroy an AccQ8 instance and release all memory.
 * Safe to call with NULL.
 *
 * @param state  May be NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.destroy()
 * @endcode
 */
void acc_q8_destroy(acc_q8_state_t *state);

/**
 * @brief Reset the accumulator to zero, mirroring the post-create state.
 * Always resets to zero regardless of the original constructor value, so
 * it is safe to call at the start of any new accumulation window.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.step(42)
 * >>> obj.reset()
 * >>> obj.get()
 * 0
 * @endcode
 */
void acc_q8_reset(acc_q8_state_t *state);

/**
 * @brief Accumulate one Q8 sample into the running total.
 * The sample is sign-extended to 32 bits before addition so negative
 * samples correctly subtract from the accumulator.
 *
 * @param state  Must be non-NULL.
 * @param x      Q8 input sample (int8_t, range `[-128, 127]`).
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.step(10)
 * >>> obj.step(20)
 * >>> obj.get()
 * 30
 * @endcode
 */
JM_FORCEINLINE JM_HOT void
acc_q8_step(acc_q8_state_t *state, int8_t x)
{
    state->acc += (int32_t)x;
}

/**
 * @brief Accumulate a contiguous block of Q8 samples.
 * Equivalent to calling step() n times; the single loop is more amenable
 * to auto-vectorisation than repeated method calls.
 *
 * @param state  Must be non-NULL.
 * @param input  Input array of int8_t samples.
 * @param n      Number of samples in input.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> import numpy as np
 * >>> obj = AccQ8(0)
 * >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
 * >>> obj.get()
 * 15
 * @endcode
 */
void acc_q8_steps(
    acc_q8_state_t *state,
    const int8_t    *input,
    size_t               n);

/**
 * @brief Read the current accumulator value without modifying it.
 * Permits repeated snapshots of the running sum mid-stream.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.step(10)
 * >>> obj.get_acc()
 * 10
 * @endcode
 */
int32_t acc_q8_get_acc(const acc_q8_state_t *state);

/**
 * @brief Overwrite the accumulator with a new value.
 * Useful for applying a bias before a new accumulation window, or for
 * restoring a checkpointed accumulator state.
 *
 * @param state  Must be non-NULL.
 * @param val    Replacement accumulator value.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> obj = AccQ8(0)
 * >>> obj.set_acc(50)
 * >>> obj.get_acc()
 * 50
 * @endcode
 */
void acc_q8_set_acc(acc_q8_state_t *state, int32_t val);



/**
 * @brief Return the current accumulated value without resetting it.
 * Mirrors get_acc() but exposed under the name used consistently across
 * all Acc-family objects in the Python API.
 *
 * @param state  Must be non-NULL.
 * @return Current accumulator value (int32_t).
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> import numpy as np
 * >>> obj = AccQ8(0)
 * >>> obj.steps(np.array([10, 20, 30], dtype=np.int8))
 * >>> obj.get()
 * 60
 * @endcode
 */
int32_t acc_q8_get(acc_q8_state_t *state);

/**
 * @brief Return the accumulated value and atomically reset it to zero.
 * Avoids the need for a separate reset() call when processing a stream
 * of non-overlapping blocks.
 *
 * @param state  Must be non-NULL.
 * @return Accumulator value before the reset (int32_t).
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> import numpy as np
 * >>> obj = AccQ8(0)
 * >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
 * >>> obj.dump()
 * 15
 * >>> obj.get()
 * 0
 * @endcode
 */
int32_t acc_q8_dump(acc_q8_state_t *state);

/**
 * @brief Multiply-accumulate over the shorter of the two arrays.
 * Computes acc += sum(`a[i]` * `b[i]`), widening int8_t inputs to int32_t
 * before accumulation to prevent intermediate overflow.
 *
 * @param state  Must be non-NULL.
 * @param a      First input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int8_t), same length as a.
 * @param b_len  Number of elements in b.
 *
 * @code
 * >>> from doppler.arith import AccQ8
 * >>> import numpy as np
 * >>> obj = AccQ8(0)
 * >>> a = np.array([10, 20, 30], dtype=np.int8)
 * >>> b = np.array([1, 2, 3], dtype=np.int8)
 * >>> obj.madd(a, b)
 * >>> obj.get()
 * 140
 * @endcode
 */
void acc_q8_madd(acc_q8_state_t *state, const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);
#ifdef __cplusplus
}
#endif

#endif /* ACC_Q8_CORE_H */
