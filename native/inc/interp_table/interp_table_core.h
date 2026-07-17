/**
 * @file interp_table_core.h
 * @brief Periodically-extended interpolated lookup table.
 *
 * Wraps a fixed complex table of one period and evaluates it at any real
 * (possibly fractional, possibly out-of-range) position via periodic
 * (mod-length) wraparound indexing plus one of three interpolation
 * methods: nearest-below ("floor"), nearest-neighbor ("nearest"), or a
 * linear fit between the two bracketing table points ("linear", the
 * default). Purely a function of (table, method, point) -- no running
 * state, so create()/execute() is all there is to the lifecycle.
 *
 * Lifecycle: create -> execute* -> destroy
 *
 * @code
 * >>> from doppler.interp import InterpolatedTable
 * >>> import numpy as np
 * >>> table = InterpolatedTable(np.array([0.0, 1.0, 2.0],
 * dtype=np.complex128))
 * >>> table.execute(np.array([1.1]))
 * array([1.1+0.j])
 * @endcode
 */
#ifndef INTERP_TABLE_CORE_H
#define INTERP_TABLE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief InterpolatedTable state.
   *
   * Allocate with interp_table_create(). `table` is a private copy (the
   * caller's own array is not aliased or retained).
   */
  typedef struct
  {
    double complex *table;  /**< owned copy, length n                    */
    size_t          n;      /**< table length (one period)               */
    int             method; /**< 0=floor, 1=nearest, 2=linear             */
  } interp_table_state_t;

  /**
   * @brief Create an InterpolatedTable instance.
   *
   * Copies @p table internally; the caller's own array can be freed or
   * modified afterward with no effect on this instance.
   *
   * @param table      Complex table, one period, length @p table_len.
   * @param table_len  Number of elements in @p table (> 0).
   * @param method     0 = floor, 1 = nearest, 2 = linear.
   * @return Heap-allocated state, or NULL on allocation failure or
   *         table_len == 0.
   * @note Caller must call interp_table_destroy() when done.
   * @code
   * >>> from doppler.interp import InterpolatedTable
   * >>> import numpy as np
   * >>> t = InterpolatedTable(np.array([0.0, 1.0, 2.0], dtype=np.complex128),
   * method="linear")
   * >>> t.n
   * 3
   * @endcode
   */
  interp_table_state_t *interp_table_create (const double complex *table,
                                             size_t table_len, int method);

  /**
   * @brief Destroy an interp_table instance and release all memory.
   * @param state  May be NULL.
   */
  void interp_table_destroy (interp_table_state_t *state);

  /**
   * @brief No-op: InterpolatedTable is purely a function of (table,
   * method, point) with no running state to reset.
   * @param state  Must be non-NULL.
   */
  void interp_table_reset (interp_table_state_t *state);

  /**
   * @brief No fixed cap -- execute()'s output is always sized to exactly
   * match its own input length, so an `out=` buffer only ever needs to
   * be at least that many elements (never a larger, unrelated minimum).
   */
  size_t interp_table_execute_max_out (interp_table_state_t *state);

  /**
   * @brief Evaluate the table at each of @p n_in points via periodic
   * interpolation.
   *
   * Each point is wrapped mod the table length (any real value, any
   * sign) and evaluated per the configured @c method:
   *   - floor:   `table[floor(point) mod n]`
   *   - nearest: the floor or the next index, whichever `point` is
   *              closer to (an exact 0.5 tie selects the floor index)
   *   - linear:  linear interpolation between the floor index and the
   *              next one, at the fractional position between them
   *
   * @param state  Must be non-NULL.
   * @param in     Points to evaluate, length @p n_in.
   * @param n_in   Number of points.
   * @param out    Output buffer; must hold at least n_in values.
   * @return n_in (always).
   * @code
   * >>> from doppler.interp import InterpolatedTable
   * >>> import numpy as np
   * >>> ramp = InterpolatedTable(np.array([0.0, 1.0, 2.0],
   * dtype=np.complex128))
   * >>> ramp.execute(np.array([0.5, 1.1]))
   * array([0.5+0.j, 1.1+0.j])
   * @endcode
   */
  size_t interp_table_execute (interp_table_state_t *state, const double *in,
                               size_t n_in, double complex *out);

#ifdef __cplusplus
}
#endif

#endif /* INTERP_TABLE_CORE_H */
