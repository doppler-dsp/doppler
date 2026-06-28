/**
 * @file acc_q15_core.h
 * @brief AccQ15 — a running 64-bit integer accumulator for Q15 (int16_t)
 * samples. Internally sums each sample into a 64-bit accumulator, which
 * prevents overflow even for very long block lengths. Use get() to read the
 * running total non-destructively, or dump() to read-and-reset in one call.
 *
 * Lifecycle: create -> `[step / steps / madd / reset]*` -> `[get / dump]*` ->
 * destroy
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.get()
 * 0
 * @endcode
 */
#ifndef ACC_Q15_CORE_H
#define ACC_Q15_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"
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
 * @brief Allocate and initialise an AccQ15 accumulator.
 * The accumulator starts at the supplied initial value and may be driven
 * sample-by-sample (step), in bulk (steps), or via multiply-accumulate
 * (madd). The internal register is a 64-bit signed integer so it will not
 * overflow in any realistic DSP workload.
 *
 * @param acc  Initial accumulator value (default: 0).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call acc_q15_destroy() when done.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(100)
 * >>> obj.get_acc()
 * 100
 * @endcode
 */
acc_q15_state_t *acc_q15_create(int64_t acc);

/**
 * @brief Destroy an AccQ15 instance and release all memory.
 * Safe to call with NULL.
 *
 * @param state  May be NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.destroy()
 * @endcode
 */
void acc_q15_destroy(acc_q15_state_t *state);

/**
 * @brief Reset the accumulator to zero, mirroring the post-create state.
 * Does not re-initialise to the constructor's acc value — always resets
 * to zero, matching the default initial state for a clean sweep.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.step(42)
 * >>> obj.reset()
 * >>> obj.get()
 * 0
 * @endcode
 */
void acc_q15_reset(acc_q15_state_t *state);

/**
 * @brief Accumulate one Q15 sample into the running total.
 * The sample is sign-extended to 64 bits before addition, ensuring that
 * negative samples subtract correctly from the accumulator without wrap.
 *
 * @param state  Must be non-NULL.
 * @param x      Q15 input sample (int16_t, range `[-32768, 32767]`).
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.step(100)
 * >>> obj.step(200)
 * >>> obj.get()
 * 300
 * @endcode
 */
JM_FORCEINLINE JM_HOT void
acc_q15_step(acc_q15_state_t *state, int16_t x)
{
    state->acc += (int64_t)x;
}

/**
 * @brief Accumulate a contiguous block of Q15 samples.
 * Equivalent to calling step() n times but faster for large arrays because
 * the loop can be auto-vectorised by the compiler.
 *
 * @param state  Must be non-NULL.
 * @param input  Input array of int16_t samples.
 * @param n      Number of samples in input.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> import numpy as np
 * >>> obj = AccQ15(0)
 * >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
 * >>> obj.get()
 * 15
 * @endcode
 */
void acc_q15_steps(
    acc_q15_state_t *state,
    const int16_t    *input,
    size_t               n);

/**
 * @brief Read the current accumulator value without modifying it.
 * Use this when you need to snapshot the running total mid-stream and
 * continue accumulating afterward.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.step(100)
 * >>> obj.get()
 * 100
 * >>> obj.step(200)
 * >>> obj.get()
 * 300
 * @endcode
 */
int64_t acc_q15_get_acc(const acc_q15_state_t *state);

/**
 * @brief Overwrite the accumulator with a new value.
 * Useful for setting a bias before a new accumulation window, or for
 * restoring a previously checkpointed value.
 *
 * @param state  Must be non-NULL.
 * @param val    Replacement accumulator value.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> obj = AccQ15(0)
 * >>> obj.set_acc(1000)
 * >>> obj.get_acc()
 * 1000
 * @endcode
 */
void acc_q15_set_acc(acc_q15_state_t *state, int64_t val);



/**
 * @brief Return the current accumulated value without resetting it.
 * Identical to reading the acc field directly; exists as a named method so
 * the Python binding can expose it consistently with dump().
 *
 * @param state  Must be non-NULL.
 * @return Current accumulator value (int64_t).
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> import numpy as np
 * >>> obj = AccQ15(0)
 * >>> obj.steps(np.array([10, 20, 30], dtype=np.int16))
 * >>> obj.get()
 * 60
 * @endcode
 */
int64_t acc_q15_get(acc_q15_state_t *state);

/**
 * @brief Return the accumulated value and atomically reset it to zero.
 * Ideal for block-based processing where each block hands off its sum and
 * then starts fresh, avoiding a separate reset() call.
 *
 * @param state  Must be non-NULL.
 * @return Accumulator value before the reset (int64_t).
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> import numpy as np
 * >>> obj = AccQ15(0)
 * >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
 * >>> obj.dump()
 * 15
 * >>> obj.get()
 * 0
 * @endcode
 */
int64_t acc_q15_dump(acc_q15_state_t *state);

/**
 * @brief Multiply-accumulate over the shorter of the two arrays.
 * Computes acc += sum(`a[i]` * `b[i]`), using SIMD (AVX2 when available) to
 * process multiple products per cycle, making this efficient for FIR filter
 * energy computation and dot-product accumulation across blocks.
 *
 * @param state  Must be non-NULL.
 * @param a      First input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int16_t), same length as a.
 * @param b_len  Number of elements in b.
 *
 * @code
 * >>> from doppler.arith import AccQ15
 * >>> import numpy as np
 * >>> obj = AccQ15(0)
 * >>> a = np.array([100, 200, 300], dtype=np.int16)
 * >>> b = np.array([10, 20, 30], dtype=np.int16)
 * >>> obj.madd(a, b)
 * >>> obj.get()
 * 14000
 * @endcode
 */
void acc_q15_madd(acc_q15_state_t *state, const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the running 64-bit accumulator resumes exactly into an
 * identically-built instance. */
#define ACC_Q15_STATE_MAGIC DP_FOURCC ('A', 'C', '1', '5')
#define ACC_Q15_STATE_VERSION 1u
size_t acc_q15_state_bytes (const acc_q15_state_t *state);
void   acc_q15_get_state (const acc_q15_state_t *state, void *blob);
int    acc_q15_set_state (acc_q15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_Q15_CORE_H */
