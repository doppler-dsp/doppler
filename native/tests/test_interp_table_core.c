/**
 * @file test_interp_table_core.c
 * @brief Unit tests for InterpolatedTable.
 *
 * Tests:
 *   1. Lifecycle — create rejects table_len==0, destroy handles NULL
 *   2. floor method
 *   3. nearest method (including the exact-0.5-tie case)
 *   4. linear method, including wraparound past the table's last index
 *      and negative points
 *   5. Table is copied, not aliased (caller's array can change after)
 *   6. n property
 */
#include "interp_table/interp_table_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

static inline int
almost_eq_c (double complex a, double complex b, double tol)
{
  return fabs (creal (a) - creal (b)) <= tol
         && fabs (cimag (a) - cimag (b)) <= tol;
}
#define ALMOST_EQ_C(a, b) almost_eq_c ((a), (b), 1e-9)

int
main (void)
{
  int _fails = 0;

  /* ----------------------------------------------------------------
   * 1. Lifecycle
   * ---------------------------------------------------------------- */
  {
    CHECK (interp_table_create (NULL, 0, 2) == NULL);
    interp_table_destroy (NULL); /* must not crash */

    double complex        t[3] = { 10.0, 20.0, 30.0 };
    interp_table_state_t *obj  = interp_table_create (t, 3, 2);
    CHECK (obj != NULL);
    if (!obj)
      return 1;
    CHECK (obj->n == 3);
    interp_table_reset (obj); /* no-op, must not crash */
    interp_table_destroy (obj);
  }

  /* ----------------------------------------------------------------
   * 2. floor
   * ---------------------------------------------------------------- */
  {
    double complex        t[3]  = { 10.0, 20.0, 30.0 };
    interp_table_state_t *obj   = interp_table_create (t, 3, 0);
    double                in[3] = { 0.9, 1.9, 2.9 };
    double complex        out[3];
    CHECK (interp_table_execute (obj, in, 3, out) == 3);
    CHECK (ALMOST_EQ_C (out[0], 10.0));
    CHECK (ALMOST_EQ_C (out[1], 20.0));
    CHECK (ALMOST_EQ_C (out[2], 30.0));
    interp_table_destroy (obj);
  }

  /* ----------------------------------------------------------------
   * 3. nearest -- including the exact 0.5 tie (goes to the floor index)
   * ---------------------------------------------------------------- */
  {
    double complex        t[3]  = { 10.0, 20.0, 30.0 };
    interp_table_state_t *obj   = interp_table_create (t, 3, 1);
    double                in[3] = { 0.4, 0.6, 1.5 };
    double complex        out[3];
    CHECK (interp_table_execute (obj, in, 3, out) == 3);
    CHECK (ALMOST_EQ_C (out[0], 10.0)); /* frac=0.4 <= 0.5 -> lo */
    CHECK (ALMOST_EQ_C (out[1], 20.0)); /* frac=0.6 >  0.5 -> hi */
    CHECK (ALMOST_EQ_C (out[2], 20.0)); /* frac=0.5 exactly -> lo (tie) */
    interp_table_destroy (obj);
  }

  /* ----------------------------------------------------------------
   * 4. linear -- interior point, wraparound past the last index, and a
   *    negative point (both must wrap correctly, matching Python's
   *    floor-modulo, not C's truncating %).
   * ---------------------------------------------------------------- */
  {
    double complex        t[3]  = { 10.0, 20.0, 30.0 };
    interp_table_state_t *obj   = interp_table_create (t, 3, 2);
    double                in[3] = { 0.25, 2.75, -0.5 };
    double complex        out[3];
    CHECK (interp_table_execute (obj, in, 3, out) == 3);
    CHECK (ALMOST_EQ_C (out[0], 12.5)); /* 10 + 0.25*(20-10) */
    CHECK (ALMOST_EQ_C (out[1], 15.0)); /* wraps: 30 + 0.75*(10-30) */
    CHECK (ALMOST_EQ_C (out[2], 20.0)); /* floor(-0.5)=-1 -> idx 2;
                                            frac=0.5: 30 + 0.5*(10-30) */
    interp_table_destroy (obj);
  }

  /* ----------------------------------------------------------------
   * 5. Table is copied, not aliased
   * ---------------------------------------------------------------- */
  {
    double complex        t[2] = { 1.0, 2.0 };
    interp_table_state_t *obj  = interp_table_create (t, 2, 2);
    t[0] = 999.0; /* mutate the caller's own array after create() */
    double         in[1] = { 0.0 };
    double complex out[1];
    interp_table_execute (obj, in, 1, out);
    CHECK (ALMOST_EQ_C (out[0], 1.0)); /* unaffected by the mutation */
    interp_table_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_interp_table_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_interp_table_core PASSED\n");
  return 0;
}
