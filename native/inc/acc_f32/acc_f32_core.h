/**
 * @file acc_f32_core.h
 * @brief AccF32 component API.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * acc_f32_state_t *obj = acc_f32_create(0.0f);
 * acc_f32_step(obj, 1.0f);
 * float v = acc_f32_get(obj);   // v == 1.0
 * acc_f32_destroy(obj);
 * @endcode
 */
#ifndef ACC_F32_CORE_H
#define ACC_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"

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
   * @brief Single-precision floating-point scalar accumulator.
   * Maintains one running sum (``acc``) that persists across calls to
   * ``step``, ``steps``, ``madd``, ``add2d``, and ``madd2d``. Use
   * ``get`` to read without side-effects or ``dump`` to read and
   * atomically zero in a single call.
   *
   * @param acc  Initial accumulator value (default: 0.0).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call acc_f32_destroy() when done.
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.get_acc()
   * 0.0
   * >>> obj.set_acc(5.0)
   * >>> obj.get_acc()
   * 5.0
   * >>> obj.reset()
   * >>> obj.get_acc()
   * 0.0
   * @endcode
   */
  acc_f32_state_t *acc_f32_create (float acc);

  /**
   * @brief Release all memory owned by an AccF32 instance.
   * Passing NULL is safe; the function is a no-op in that case.
   * After this call the pointer must not be used.
   */
  void acc_f32_destroy (acc_f32_state_t *state);

  /**
   * @brief Zero the accumulator, restoring the same state as a fresh
   * ``AccF32(0.0)`` — regardless of the value supplied to
   * ``acc_f32_create``. Subsequent ``get`` / ``dump`` calls return
   * ``0.0`` until new samples are processed.
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.step(7.0)
   * >>> obj.reset()
   * >>> obj.get_acc()
   * 0.0
   * @endcode
   */
  void acc_f32_reset (acc_f32_state_t *state);

  /**
   * @brief Add one sample to the running sum (``acc += x``).
   * This is the hot-path entry point for sample-by-sample processing.
   * For block inputs prefer ``acc_f32_steps`` to amortise call overhead
   * and allow auto-vectorisation.
   *
   * @param state  Must be non-NULL.
   * @param x      Input sample (float).
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.step(3.0)
   * >>> obj.get()
   * 3.0
   * @endcode
   */
  JM_FORCEINLINE JM_HOT void
  acc_f32_step (acc_f32_state_t *state, float x)
  {
    state->acc += x;
  }

  /**
   * @brief Add all samples in ``input`` to the running sum.
   * Equivalent to calling ``acc_f32_step`` for each element, but
   * SIMD-vectorised on platforms that provide it (AVX-512 / AVX2 / SSE2).
   * The loop uses JM_RESTRICT so the compiler can assume no aliasing
   * between ``state`` and ``input``.
   *
   * @param state  Must be non-NULL.
   * @param input  Input samples (float32 array).
   * @param n      Number of elements in ``input``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.steps(np.array([1.0, 2.0, 3.0], dtype=np.float32))
   * >>> obj.get()
   * 6.0
   * @endcode
   */
  void acc_f32_steps (acc_f32_state_t *state, const float *input, size_t n);

  /**
   * @brief Return the current accumulator value without modifying state.
   * Use this when you need to read the running sum mid-accumulation
   * without disturbing it. For a read-and-reset in one call use
   * ``acc_f32_dump``.
   *
   * @return Current value of ``acc`` (float).
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.step(4.0)
   * >>> obj.get_acc()
   * 4.0
   * @endcode
   */
  float acc_f32_get_acc (const acc_f32_state_t *state);

  /**
   * @brief Overwrite the accumulator with a new value.
   * Useful for seeding the accumulator to a known baseline before
   * processing a new segment without a full ``reset``.
   *
   * @param state  Must be non-NULL.
   * @param acc    New accumulator value.
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.set_acc(10.0)
   * >>> obj.get_acc()
   * 10.0
   * @endcode
   */
  void acc_f32_set_acc (acc_f32_state_t *state, float acc);

  /**
   * @brief Return the current accumulated sum without resetting state.
   * Identical to reading the ``acc`` property directly; retained as an
   * explicit method so call sites that need the value can be uniform
   * with ``dump`` without a conditional.
   *
   * @return Current value of ``acc`` (float).
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.step(2.0)
   * >>> obj.step(3.0)
   * >>> obj.get()
   * 5.0
   * @endcode
   */
  float acc_f32_get (acc_f32_state_t *state);

  /**
   * @brief Return the accumulated sum and atomically reset it to zero.
   * This is the canonical "drain" primitive: read the period total, then
   * start a fresh accumulation interval without a separate ``reset``
   * call. The zero-reset is unconditional and always writes 0.0f.
   *
   * @return Value of ``acc`` just before the reset (float).
   * @code
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> obj.step(3.0)
   * >>> obj.step(4.0)
   * >>> obj.dump()
   * 7.0
   * >>> obj.get()
   * 0.0
   * @endcode
   */
  float acc_f32_dump (acc_f32_state_t *state);

  /**
   * @brief Dot-product accumulate: ``acc += sum(x[i] * h[i])`` for
   * ``i`` in ``0 .. min(x_len, h_len) - 1``. The shorter of the two
   * arrays limits the iteration count; no out-of-bounds access occurs.
   * Typical use: apply a short FIR weight vector to one block of
   * signal samples and fold the result into a running total.
   *
   * @param state  Must be non-NULL.
   * @param x      Signal samples (float32 array).
   * @param x_len  Number of elements in ``x``.
   * @param h      Coefficient / weight array (float32 array).
   * @param h_len  Number of elements in ``h``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
   * >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
   * >>> obj.madd(x, h)
   * >>> obj.get()
   * 5.0
   * @endcode
   */
  void acc_f32_madd (acc_f32_state_t *state, const float *x, size_t x_len,
                     const float *h, size_t h_len);

  /**
   * @brief Sum all elements of a (logically) 2-D float array into the
   * accumulator. The array is treated as a flat C-order buffer of
   * ``x_len`` floats regardless of the original shape; the caller is
   * responsible for passing the total element count.
   *
   * @param state  Must be non-NULL.
   * @param x      Input array (float32, any shape — passed as flat buffer).
   * @param x_len  Number of elements in ``x``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> grid = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float32)
   * >>> obj.add2d(grid)
   * >>> obj.get()
   * 10.0
   * @endcode
   */
  void acc_f32_add2d (acc_f32_state_t *state, const float *x, size_t x_len);

  /**
   * @brief Dot-product accumulate over a flat 2-D buffer:
   * ``acc += sum(x[i] * h[i])`` for ``i`` in
   * ``0 .. min(x_len, h_len) - 1``. Combines ``add2d`` and ``madd``
   * semantics — a 2-D signal array is weighted element-wise by a
   * coefficient buffer and the scalar total is folded into the running
   * sum.
   *
   * @param state  Must be non-NULL.
   * @param x      Signal samples (float32, flat buffer of the 2-D array).
   * @param x_len  Number of elements in ``x``.
   * @param h      Coefficient / weight array (float32).
   * @param h_len  Number of elements in ``h``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccF32
   * >>> obj = AccF32(0.0)
   * >>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
   * >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
   * >>> obj.madd2d(x, h)
   * >>> obj.get()
   * 5.0
   * @endcode
   */
  void acc_f32_madd2d (acc_f32_state_t *state, const float *x, size_t x_len,
                       const float *h, size_t h_len);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
   * Whole-struct POD snapshot (pointer-free); the running accumulator resumes exactly into an
   * identically-built instance. */
#define ACC_F32_STATE_MAGIC DP_FOURCC ('A', 'C', 'C', 'F')
#define ACC_F32_STATE_VERSION 1u
  size_t acc_f32_state_bytes (const acc_f32_state_t *state);
  void    acc_f32_get_state (const acc_f32_state_t *state, void *blob);
  int     acc_f32_set_state (acc_f32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_F32_CORE_H */
