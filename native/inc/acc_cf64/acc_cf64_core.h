/**
 * @file acc_cf64_core.h
 * @brief AccCf64 component API.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * acc_cf64_state_t *obj = acc_cf64_create(0.0 + 0.0 * I);
 * acc_cf64_step(obj, 1.0 + 0.5 * I);
 * double complex v = acc_cf64_get(obj);  // v == 1.0 + 0.5 * I
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
   * @brief Double-precision complex scalar accumulator.
   * Maintains one running complex sum (``acc``) across calls to
   * ``step``, ``steps``, ``madd``, ``add2d``, and ``madd2d``. The
   * signal path is double-precision complex (128-bit per sample);
   * coefficient arrays for ``madd``/``madd2d`` are single-precision
   * float to match typical FIR weight storage. Use ``get`` to read
   * without side-effects or ``dump`` to read and zero atomically.
   *
   * @param acc  Initial accumulator value (default: 0j).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call acc_cf64_destroy() when done.
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.get_acc()
   * 0j
   * >>> obj.set_acc(3+4j)
   * >>> obj.get_acc()
   * (3+4j)
   * >>> obj.reset()
   * >>> obj.get_acc()
   * 0j
   * @endcode
   */
  acc_cf64_state_t *acc_cf64_create (double _Complex acc);

  /**
   * @brief Release all memory owned by an AccCf64 instance.
   * Passing NULL is safe; the function is a no-op in that case.
   * After this call the pointer must not be used.
   */
  void acc_cf64_destroy (acc_cf64_state_t *state);

  /**
   * @brief Zero the accumulator, restoring the same state as a fresh
   * ``AccCf64(0j)`` — regardless of the value supplied to
   * ``acc_cf64_create``. Both the real and imaginary parts are set to
   * 0.0. Subsequent ``get`` / ``dump`` calls return ``0j`` until new
   * samples are processed.
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.step(3+2j)
   * >>> obj.reset()
   * >>> obj.get_acc()
   * 0j
   * @endcode
   */
  void acc_cf64_reset (acc_cf64_state_t *state);

  /**
   * @brief Add one complex sample to the running sum (``acc += x``).
   * This is the hot-path entry for sample-by-sample processing.
   * For block inputs prefer ``acc_cf64_steps`` to amortise call overhead.
   *
   * @param state  Must be non-NULL.
   * @param x      Input sample (complex).
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.step(3+2j)
   * >>> obj.get()
   * (3+2j)
   * @endcode
   */
  JM_FORCEINLINE JM_HOT void
  acc_cf64_step (acc_cf64_state_t *state, double complex x)
  {
    state->acc += x;
  }

  /**
   * @brief Add all samples in ``input`` to the running sum.
   * Equivalent to calling ``acc_cf64_step`` for each element; iterates
   * element-by-element over double-precision complex samples.
   *
   * @param state  Must be non-NULL.
   * @param input  Input samples (complex128 array).
   * @param n      Number of elements in ``input``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.steps(np.array([1+0j, 2+1j, 3+2j], dtype=np.complex128))
   * >>> obj.get()
   * (6+3j)
   * @endcode
   */
  void acc_cf64_steps (acc_cf64_state_t *state, const double complex *input,
                       size_t n);

  /**
   * @brief Return the current accumulator value without modifying state.
   * Use this when you need to read the running sum mid-accumulation
   * without disturbing it. For a read-and-reset in one call use
   * ``acc_cf64_dump``.
   *
   * @return Current value of ``acc`` (complex).
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.step(1+2j)
   * >>> obj.get_acc()
   * (1+2j)
   * @endcode
   */
  double _Complex acc_cf64_get_acc (const acc_cf64_state_t *state);

  /**
   * @brief Overwrite the accumulator with a new complex value.
   * Useful for seeding the accumulator to a known baseline before
   * processing a new segment without a full ``reset``.
   *
   * @param state  Must be non-NULL.
   * @param acc    New accumulator value (complex).
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.set_acc(5+6j)
   * >>> obj.get_acc()
   * (5+6j)
   * @endcode
   */
  void acc_cf64_set_acc (acc_cf64_state_t *state, double _Complex acc);

  /**
   * @brief Return the current accumulated sum without resetting state.
   * Identical to reading the ``acc`` property directly; retained as an
   * explicit method so call sites that need the value can be uniform
   * with ``dump`` without a conditional.
   *
   * @return Current value of ``acc`` (complex).
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.step(2+0j)
   * >>> obj.step(0+3j)
   * >>> obj.get()
   * (2+3j)
   * @endcode
   */
  double complex acc_cf64_get (acc_cf64_state_t *state);

  /**
   * @brief Return the accumulated sum and atomically reset it to zero.
   * This is the canonical "drain" primitive: read the period total, then
   * start a fresh accumulation interval without a separate ``reset``
   * call. Both real and imaginary parts are zeroed unconditionally.
   *
   * @return Value of ``acc`` just before the reset (complex).
   * @code
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> obj.step(3+2j)
   * >>> obj.step(1+1j)
   * >>> obj.dump()
   * (4+3j)
   * >>> obj.get()
   * 0j
   * @endcode
   */
  double complex acc_cf64_dump (acc_cf64_state_t *state);

  /**
   * @brief Dot-product accumulate with complex signal and float weights:
   * ``acc += sum(x[i] * h[i])`` for ``i`` in
   * ``0 .. min(x_len, h_len) - 1``. The signal array ``x`` is
   * double-precision complex; the coefficient array ``h`` is
   * single-precision float (widened to double before multiplication).
   * The shorter of the two arrays limits iteration.
   *
   * @param state  Must be non-NULL.
   * @param x      Complex signal samples (complex128 array).
   * @param x_len  Number of elements in ``x``.
   * @param h      Real coefficient / weight array (float32 array).
   * @param h_len  Number of elements in ``h``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
   * >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
   * >>> obj.madd(x, h)
   * >>> obj.get()
   * (5+0j)
   * @endcode
   */
  void acc_cf64_madd (acc_cf64_state_t *state, const double complex *x,
                      size_t x_len, const float *h, size_t h_len);

  /**
   * @brief Sum all elements of a (logically) 2-D complex array into the
   * accumulator. The array is treated as a flat C-order buffer of
   * ``x_len`` complex128 samples regardless of the original shape; the
   * caller is responsible for passing the total element count.
   *
   * @param state  Must be non-NULL.
   * @param x      Input array (complex128, any shape — passed as flat buffer).
   * @param x_len  Number of elements in ``x``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> grid = np.array([[1+0j, 2+0j], [3+0j, 4+0j]], dtype=np.complex128)
   * >>> obj.add2d(grid)
   * >>> obj.get()
   * (10+0j)
   * @endcode
   */
  void acc_cf64_add2d (acc_cf64_state_t *state, const double complex *x,
                       size_t x_len);

  /**
   * @brief Dot-product accumulate over a flat 2-D complex buffer:
   * ``acc += sum(x[i] * h[i])`` for ``i`` in
   * ``0 .. min(x_len, h_len) - 1``. Combines ``add2d`` and ``madd``
   * semantics for 2-D data — a complex signal grid is weighted
   * element-wise by a real coefficient buffer and folded into the
   * running sum.
   *
   * @param state  Must be non-NULL.
   * @param x      Complex signal samples (complex128, flat buffer).
   * @param x_len  Number of elements in ``x``.
   * @param h      Real coefficient / weight array (float32).
   * @param h_len  Number of elements in ``h``.
   * @code
   * >>> import numpy as np
   * >>> from doppler.accumulator import AccCf64
   * >>> obj = AccCf64(0j)
   * >>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
   * >>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
   * >>> obj.madd2d(x, h)
   * >>> obj.get()
   * (5+0j)
   * @endcode
   */
  void acc_cf64_madd2d (acc_cf64_state_t *state, const double complex *x,
                        size_t x_len, const float *h, size_t h_len);

#ifdef __cplusplus
}
#endif

#endif /* ACC_CF64_CORE_H */
