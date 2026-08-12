#include "acc_trace/acc_trace_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOL 1e-5f

int
main (void)
{

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    DP_CHECK (acc_trace_create (0, 0, 0.1) == NULL);  /* n == 0 */
    DP_CHECK (acc_trace_create (8, -1, 0.1) == NULL); /* bad mode */
    DP_CHECK (acc_trace_create (8, 4, 0.1) == NULL);  /* bad mode */
    acc_trace_destroy (NULL);                         /* must not crash */

    acc_trace_state_t *obj = acc_trace_create (8, ACC_TRACE_MEAN, 0.1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->n == 8);
    DP_CHECK (obj->count == 0);
    DP_CHECK (acc_trace_value_max_out (obj) == 8);

    /* value before any frame → 0 (None in Python). */
    float out[8];
    DP_CHECK (acc_trace_value (obj, 8, out, 8) == 0);
    acc_trace_destroy (obj);
  }

  /* ── MEAN: average of two frames, per bin ───────────────────────────── */
  {
    acc_trace_state_t *obj  = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float              a[4] = { 1, 3, 5, 7 };
    float              b[4] = { 3, 5, 7, 9 };
    acc_trace_accumulate (obj, a, 4);
    DP_CHECK (obj->count == 1);
    acc_trace_accumulate (obj, b, 4);
    DP_CHECK (obj->count == 2);
    float out[4];
    DP_CHECK (acc_trace_value (obj, 4, out, 4) == 4);
    const float want[4] = { 2, 4, 6, 8 };
    for (int i = 0; i < 4; i++)
      DP_CHECK (fabsf (out[i] - want[i]) < TOL);

    /* reset clears the running trace and counter. */
    acc_trace_reset (obj);
    DP_CHECK (obj->count == 0);
    DP_CHECK (acc_trace_value (obj, 4, out, 4) == 0);
    acc_trace_destroy (obj);
  }

  /* ── MEAN is order-independent and stable over three frames ─────────── */
  {
    acc_trace_state_t *obj   = acc_trace_create (2, ACC_TRACE_MEAN, 0.1);
    float              f0[2] = { 0, 30 };
    float              f1[2] = { 6, 60 };
    float              f2[2] = { 9, 90 };
    acc_trace_accumulate (obj, f0, 2);
    acc_trace_accumulate (obj, f1, 2);
    acc_trace_accumulate (obj, f2, 2);
    float out[2];
    acc_trace_value (obj, 2, out, 2);
    DP_CHECK (fabsf (out[0] - 5.0f) < TOL);  /* (0+6+9)/3    */
    DP_CHECK (fabsf (out[1] - 60.0f) < TOL); /* (30+60+90)/3 */
    acc_trace_destroy (obj);
  }

  /* ── EXP: seed then single update with alpha = 0.5 ──────────────────── */
  {
    acc_trace_state_t *obj  = acc_trace_create (2, ACC_TRACE_EXP, 0.5);
    float              s[2] = { 10, 20 };
    float              u[2] = { 2, 4 };
    acc_trace_accumulate (obj, s, 2); /* seeds acc = s */
    acc_trace_accumulate (obj, u, 2); /* 0.5*u + 0.5*s */
    float out[2];
    acc_trace_value (obj, 2, out, 2);
    DP_CHECK (fabsf (out[0] - 6.0f) < TOL);  /* 0.5*2 + 0.5*10 */
    DP_CHECK (fabsf (out[1] - 12.0f) < TOL); /* 0.5*4 + 0.5*20 */
    acc_trace_destroy (obj);
  }

  /* ── MAXHOLD / MINHOLD per bin ──────────────────────────────────────── */
  {
    acc_trace_state_t *mx    = acc_trace_create (3, ACC_TRACE_MAXHOLD, 0.1);
    acc_trace_state_t *mn    = acc_trace_create (3, ACC_TRACE_MINHOLD, 0.1);
    float              p0[3] = { 1, 5, 2 };
    float              p1[3] = { 4, 3, 6 };
    acc_trace_accumulate (mx, p0, 3);
    acc_trace_accumulate (mx, p1, 3);
    acc_trace_accumulate (mn, p0, 3);
    acc_trace_accumulate (mn, p1, 3);
    float omx[3], omn[3];
    acc_trace_value (mx, 3, omx, 3);
    acc_trace_value (mn, 3, omn, 3);
    const float wmx[3] = { 4, 5, 6 };
    const float wmn[3] = { 1, 3, 2 };
    for (int i = 0; i < 3; i++)
      {
        DP_CHECK (fabsf (omx[i] - wmx[i]) < TOL);
        DP_CHECK (fabsf (omn[i] - wmn[i]) < TOL);
      }
    acc_trace_destroy (mx);
    acc_trace_destroy (mn);
  }

  /* ── short frame is ignored ─────────────────────────────────────────── */
  {
    acc_trace_state_t *obj     = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float              shrt[2] = { 1, 2 };
    acc_trace_accumulate (obj, shrt, 2); /* p_len < n → no-op */
    DP_CHECK (obj->count == 0);
    acc_trace_destroy (obj);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    acc_trace_state_t *a    = acc_trace_create (8, 0, 0.1);
    const float        p[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    float              out[8];
    DP_CHECK (a != NULL);
    acc_trace_accumulate (a, p, 8);

    for (int i = 0; i < 8; i++)
      out[i] = 42.0f;
    DP_CHECK (acc_trace_value (a, 8, out, 3) == 3);
    for (int i = 3; i < 8; i++)
      DP_CHECK (out[i] == 42.0f); /* tail untouched */

    /* Zero capacity emits nothing. */
    for (int i = 0; i < 8; i++)
      out[i] = 42.0f;
    DP_CHECK (acc_trace_value (a, 8, out, 0) == 0);
    for (int i = 0; i < 8; i++)
      DP_CHECK (out[i] == 42.0f);
    acc_trace_destroy (a);
  }

  /* serializable state — field-wise trace + count round-trips + rejects. */
  {
    acc_trace_state_t *a = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    acc_trace_state_t *b = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    DP_CHECK (a != NULL && b != NULL);
    const float f1[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float f2[4] = { 0.5f, -1.0f, 2.0f, 0.0f };
    acc_trace_accumulate (a, f1, 4);
    acc_trace_accumulate (a, f2, 4);
    DP_STATE_ROUNDTRIP_TEST (acc_trace, a, b);
    DP_CHECK (b->count == a->count);
    DP_CHECK (memcmp (b->acc, a->acc, a->n * sizeof (double)) == 0);
    acc_trace_destroy (a);
    acc_trace_destroy (b);
  }

  DP_TEST_END ("test_acc_trace_core");
}
