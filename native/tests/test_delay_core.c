#include "delay/delay_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOL 1e-12

int
main (void)
{

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    delay_state_t *obj = delay_create (4);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->num_taps == 4);
    DP_CHECK (obj->capacity == 4); /* 4 is already a power of two */
    DP_CHECK (obj->mask == 3);
    DP_CHECK (obj->buf != NULL);
    delay_destroy (obj);

    delay_destroy (NULL); /* must not crash */
  }

  /* ── capacity is always a power of two ─────────────────────────── */
  {
    delay_state_t *a = delay_create (1);
    DP_CHECK (a->capacity == 1);
    delay_destroy (a);

    delay_state_t *b = delay_create (3);
    DP_CHECK (b->capacity == 4);
    delay_destroy (b);

    delay_state_t *c = delay_create (5);
    DP_CHECK (c->capacity == 8);
    delay_destroy (c);

    delay_state_t *d = delay_create (8);
    DP_CHECK (d->capacity == 8);
    delay_destroy (d);
  }

  /* ── push / ptr round-trip ──────────────────────────────────────── */
  {
    /* 3-tap delay: after pushing A B C the window is [C, B, A]
     * (newest first). */
    delay_state_t *obj = delay_create (3);
    double complex win[3];

    delay_push (obj, 1.0 + 0.0 * I);
    delay_push (obj, 2.0 + 0.0 * I);
    delay_push (obj, 3.0 + 0.0 * I);

    size_t n = delay_ptr (obj, 3, win, 3);
    DP_CHECK (n == 3);
    DP_CHECK (dp_cnear (win[0], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 2.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 1.0 + 0.0 * I, TOL));
    delay_destroy (obj);
  }

  /* ── push_ptr returns the updated window ────────────────────────── */
  {
    delay_state_t *obj = delay_create (2);
    double complex win[2];

    delay_push (obj, 10.0 + 0.0 * I);
    size_t n = delay_push_ptr (obj, 20.0 + 0.0 * I, win, 2);
    DP_CHECK (n == 2);
    DP_CHECK (dp_cnear (win[0], 20.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 10.0 + 0.0 * I, TOL));
    delay_destroy (obj);
  }

  /* ── continuity across block boundaries ────────────────────────── */
  {
    /* Fill a 4-tap delay one element at a time; verify wrap-around
     * works correctly when head crosses the start of the ring. */
    delay_state_t *obj = delay_create (4);
    double complex win[4];

    for (int i = 1; i <= 8; i++)
      delay_push (obj, (double)i + 0.0 * I);

    /* Last 4 pushes: 5 6 7 8 → window = [8, 7, 6, 5] */
    delay_ptr (obj, 4, win, 4);
    DP_CHECK (dp_cnear (win[0], 8.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 7.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 6.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[3], 5.0 + 0.0 * I, TOL));
    delay_destroy (obj);
  }

  /* ── write batch pushes multiple samples ────────────────────────── */
  {
    delay_state_t *obj = delay_create (3);
    double complex win[3];

    /* write inserts one sample (same as push — scalar API) */
    delay_write (obj, 1.0 + 0.0 * I);
    delay_write (obj, 2.0 + 0.0 * I);
    delay_write (obj, 3.0 + 0.0 * I);

    delay_ptr (obj, 3, win, 3);
    DP_CHECK (dp_cnear (win[0], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 2.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 1.0 + 0.0 * I, TOL));
    delay_destroy (obj);
  }

  /* ── reset clears the buffer and resets head ────────────────────── */
  {
    delay_state_t *obj = delay_create (4);
    double complex win[4];

    delay_push (obj, 1.0 + 1.0 * I);
    delay_push (obj, 2.0 + 2.0 * I);
    delay_reset (obj);

    /* After reset everything should be zero and head should be 0. */
    DP_CHECK (obj->head == 0);
    delay_ptr (obj, 4, win, 4);
    for (int i = 0; i < 4; i++)
      DP_CHECK (dp_cnear (win[i], 0.0 + 0.0 * I, TOL));
    delay_destroy (obj);
  }

  /* ── ptr_max_out / push_ptr_max_out ─────────────────────────────── */
  {
    delay_state_t *obj = delay_create (7);
    /* gh-607: delay_ptr_max_out(n) is the tight per-call bound min(n,taps). */
    DP_CHECK (delay_ptr_max_out (obj, 7) == 7);   /* n == num_taps      */
    DP_CHECK (delay_ptr_max_out (obj, 100) == 7); /* clamped to num_taps */
    DP_CHECK (delay_ptr_max_out (obj, 3) == 3);   /* tight: n < num_taps */
    DP_CHECK (delay_push_ptr_max_out (obj) == 7);
    delay_destroy (obj);
  }

  /* ── complex values round-trip correctly ────────────────────────── */
  {
    delay_state_t *obj = delay_create (2);
    double complex win[2];

    delay_push (obj, 1.5 + 2.5 * I);
    delay_push (obj, -3.0 + 4.0 * I);

    delay_ptr (obj, 2, win, 2);
    DP_CHECK (dp_cnear (win[0], -3.0 + 4.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 1.5 + 2.5 * I, TOL));
    delay_destroy (obj);
  }

  /* ── short out: snapshot truncates, the ring still advances ─────────
   * delay_ptr() is a pure read, so a short buffer just yields fewer
   * samples.  delay_push_ptr() also MUTATES: the push has to land even
   * when there is no room to report it back, or the window falls out of
   * step with the sample stream.  Both are checked here. */
  {
    delay_state_t       *obj = delay_create (4);
    double complex       win[4];
    const double complex CANARY = -999.0 - 111.0 * I;

    for (int i = 1; i <= 4; i++)
      delay_push (obj, (double)i + 0.0 * I); /* window = [4, 3, 2, 1] */

    for (size_t k = 0; k < 4; k++)
      win[k] = CANARY;
    DP_CHECK (delay_ptr (obj, 4, win, 2) == 2);
    DP_CHECK (dp_cnear (win[0], 4.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], CANARY, TOL)); /* past capacity: untouched */
    DP_CHECK (dp_cnear (win[3], CANARY, TOL));

    /* max_out == 0 writes nothing. */
    for (size_t k = 0; k < 4; k++)
      win[k] = CANARY;
    DP_CHECK (delay_ptr (obj, 4, win, 0) == 0);
    for (size_t k = 0; k < 4; k++)
      DP_CHECK (dp_cnear (win[k], CANARY, TOL));

    /* push_ptr with no room: the snapshot is empty, but sample 5 is in. */
    DP_CHECK (delay_push_ptr (obj, 5.0 + 0.0 * I, win, 0) == 0);
    for (size_t k = 0; k < 4; k++)
      DP_CHECK (dp_cnear (win[k], CANARY, TOL));
    DP_CHECK (delay_ptr (obj, 4, win, 4) == 4);
    DP_CHECK (dp_cnear (win[0], 5.0 + 0.0 * I, TOL)); /* the push landed */
    DP_CHECK (dp_cnear (win[1], 4.0 + 0.0 * I, TOL));

    delay_destroy (obj);
  }

  /* serializable state — field-wise ring + head round-trips + rejects. */
  {
    delay_state_t *a = delay_create (4);
    delay_state_t *b = delay_create (4);
    DP_CHECK (a != NULL && b != NULL);
    delay_push (a, 1.0 + 2.0 * I);
    delay_push (a, -3.0 + 0.5 * I);
    delay_push (a, 4.0 - 1.0 * I);
    DP_STATE_ROUNDTRIP_TEST (delay, a, b);
    DP_CHECK (b->head == a->head);
    DP_CHECK (
        memcmp (b->buf, a->buf, 2 * a->capacity * sizeof (double _Complex))
        == 0);
    delay_destroy (a);
    delay_destroy (b);
  }

  DP_TEST_END ("test_delay_core");
}
