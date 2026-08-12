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
#include "dp_test.h"
#include "interp_table/interp_table_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int
main (void)
{

  /* ----------------------------------------------------------------
   * 1. Lifecycle
   * ---------------------------------------------------------------- */
  {
    DP_CHECK (interp_table_create (NULL, 0, 2) == NULL);
    interp_table_destroy (NULL); /* must not crash */

    double complex        t[3] = { 10.0, 20.0, 30.0 };
    interp_table_state_t *obj  = interp_table_create (t, 3, 2);
    DP_CHECK (obj != NULL);
    if (!obj)
      return 1;
    DP_CHECK (obj->n == 3);
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
    DP_CHECK (interp_table_execute (obj, in, 3, out, 3) == 3);
    DP_CHECK (dp_cnear (out[0], 10.0, 1e-9));
    DP_CHECK (dp_cnear (out[1], 20.0, 1e-9));
    DP_CHECK (dp_cnear (out[2], 30.0, 1e-9));
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
    DP_CHECK (interp_table_execute (obj, in, 3, out, 3) == 3);
    DP_CHECK (dp_cnear (out[0], 10.0, 1e-9)); /* frac=0.4 <= 0.5 -> lo */
    DP_CHECK (dp_cnear (out[1], 20.0, 1e-9)); /* frac=0.6 >  0.5 -> hi */
    DP_CHECK (
        dp_cnear (out[2], 20.0, 1e-9)); /* frac=0.5 exactly -> lo (tie) */
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
    DP_CHECK (interp_table_execute (obj, in, 3, out, 3) == 3);
    DP_CHECK (dp_cnear (out[0], 12.5, 1e-9)); /* 10 + 0.25*(20-10) */
    DP_CHECK (dp_cnear (out[1], 15.0, 1e-9)); /* wraps: 30 + 0.75*(10-30) */
    DP_CHECK (dp_cnear (out[2], 20.0, 1e-9)); /* floor(-0.5)=-1 -> idx 2;
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
    interp_table_execute (obj, in, 1, out, 1);
    DP_CHECK (dp_cnear (out[0], 1.0, 1e-9)); /* unaffected by the mutation */
    interp_table_destroy (obj);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* interp_table is 1:1 and stateless, so a truncated call simply drops
     * the tail: the emitted prefix is bit-identical to an unclamped run
     * and there is no delay line left mid-stream. */
    double complex        tab[4] = { 1.0, 2.0, 3.0, 4.0 };
    interp_table_state_t *obj    = interp_table_create (tab, 4, 2);
    const double          in[6]  = { 0.0, 1.0, 2.0, 3.0, 0.5, 1.5 };
    double complex        out[6], full[6];
    DP_CHECK (obj != NULL);
    for (int i = 0; i < 6; i++)
      out[i] = 42.0 + 42.0 * I;

    DP_CHECK (interp_table_execute (obj, in, 6, full, 6) == 6);
    DP_CHECK (interp_table_execute (obj, in, 6, out, 2) == 2);
    for (int i = 0; i < 2; i++)
      DP_CHECK (dp_cnear (out[i], creal (full[i]), 1e-9));
    for (int i = 2; i < 6; i++)
      DP_CHECK (out[i] == 42.0 + 42.0 * I); /* tail untouched */

    /* Zero capacity emits nothing. */
    DP_CHECK (interp_table_execute (obj, in, 6, out, 0) == 0);
    DP_CHECK (out[0] == full[0]); /* still holds the earlier write */
    interp_table_destroy (obj);
  }

  DP_TEST_END ("test_interp_table_core");
}
