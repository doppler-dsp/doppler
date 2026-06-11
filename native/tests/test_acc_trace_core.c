#include "acc_trace/acc_trace_core.h"
#include <math.h>
#include <stdio.h>

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

#define TOL 1e-5f

int
main (void)
{
  int _fails = 0;

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    CHECK (acc_trace_create (0, 0, 0.1) == NULL);  /* n == 0 */
    CHECK (acc_trace_create (8, -1, 0.1) == NULL); /* bad mode */
    CHECK (acc_trace_create (8, 4, 0.1) == NULL);  /* bad mode */
    acc_trace_destroy (NULL);                      /* must not crash */

    acc_trace_state_t *obj = acc_trace_create (8, ACC_TRACE_MEAN, 0.1);
    CHECK (obj != NULL);
    CHECK (obj->n == 8);
    CHECK (obj->count == 0);
    CHECK (acc_trace_value_max_out (obj) == 8);

    /* value before any frame → 0 (None in Python). */
    float out[8];
    CHECK (acc_trace_value (obj, 8, out) == 0);
    acc_trace_destroy (obj);
  }

  /* ── MEAN: average of two frames, per bin ───────────────────────────── */
  {
    acc_trace_state_t *obj  = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float              a[4] = { 1, 3, 5, 7 };
    float              b[4] = { 3, 5, 7, 9 };
    acc_trace_accumulate (obj, a, 4);
    CHECK (obj->count == 1);
    acc_trace_accumulate (obj, b, 4);
    CHECK (obj->count == 2);
    float out[4];
    CHECK (acc_trace_value (obj, 4, out) == 4);
    const float want[4] = { 2, 4, 6, 8 };
    for (int i = 0; i < 4; i++)
      CHECK (fabsf (out[i] - want[i]) < TOL);

    /* reset clears the running trace and counter. */
    acc_trace_reset (obj);
    CHECK (obj->count == 0);
    CHECK (acc_trace_value (obj, 4, out) == 0);
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
    acc_trace_value (obj, 2, out);
    CHECK (fabsf (out[0] - 5.0f) < TOL);  /* (0+6+9)/3    */
    CHECK (fabsf (out[1] - 60.0f) < TOL); /* (30+60+90)/3 */
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
    acc_trace_value (obj, 2, out);
    CHECK (fabsf (out[0] - 6.0f) < TOL);  /* 0.5*2 + 0.5*10 */
    CHECK (fabsf (out[1] - 12.0f) < TOL); /* 0.5*4 + 0.5*20 */
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
    acc_trace_value (mx, 3, omx);
    acc_trace_value (mn, 3, omn);
    const float wmx[3] = { 4, 5, 6 };
    const float wmn[3] = { 1, 3, 2 };
    for (int i = 0; i < 3; i++)
      {
        CHECK (fabsf (omx[i] - wmx[i]) < TOL);
        CHECK (fabsf (omn[i] - wmn[i]) < TOL);
      }
    acc_trace_destroy (mx);
    acc_trace_destroy (mn);
  }

  /* ── short frame is ignored ─────────────────────────────────────────── */
  {
    acc_trace_state_t *obj     = acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float              shrt[2] = { 1, 2 };
    acc_trace_accumulate (obj, shrt, 2); /* p_len < n → no-op */
    CHECK (obj->count == 0);
    acc_trace_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_acc_trace_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acc_trace_core PASSED\n");
  return 0;
}
