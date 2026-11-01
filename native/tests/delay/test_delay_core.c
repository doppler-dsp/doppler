#include "doppler/delay/delay_core.h"
#include "doppler/dp_complex.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOL 1e-12

int
main (void)
{

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (4);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->num_taps == 4);
    DP_CHECK (obj->capacity == 4); /* 4 is already a power of two */
    DP_CHECK (obj->mask == 3);
    DP_CHECK (obj->buf != NULL);
    dp_delay_destroy (obj);

    dp_delay_destroy (NULL); /* must not crash */
  }

  /* ── capacity is always a power of two ─────────────────────────── */
  {
    dp_delay_state_t *a = dp_delay_create (1);
    DP_CHECK (a->capacity == 1);
    dp_delay_destroy (a);

    dp_delay_state_t *b = dp_delay_create (3);
    DP_CHECK (b->capacity == 4);
    dp_delay_destroy (b);

    dp_delay_state_t *c = dp_delay_create (5);
    DP_CHECK (c->capacity == 8);
    dp_delay_destroy (c);

    dp_delay_state_t *d = dp_delay_create (8);
    DP_CHECK (d->capacity == 8);
    dp_delay_destroy (d);
  }

  /* ── push / ptr round-trip ──────────────────────────────────────── */
  {
    /* 3-tap delay: after pushing A B C the window is [C, B, A]
     * (newest first). */
    dp_delay_state_t *obj = dp_delay_create (3);
    double _Complex win[3];

    dp_delay_push (obj, 1.0 + 0.0 * I);
    dp_delay_push (obj, 2.0 + 0.0 * I);
    dp_delay_push (obj, 3.0 + 0.0 * I);

    size_t n = dp_delay_ptr (obj, 3, win, 3);
    DP_CHECK (n == 3);
    DP_CHECK (dp_cnear (win[0], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 2.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 1.0 + 0.0 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── push_ptr returns the updated window ────────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (2);
    double _Complex win[2];

    dp_delay_push (obj, 10.0 + 0.0 * I);
    size_t n = dp_delay_push_ptr (obj, 20.0 + 0.0 * I, win, 2);
    DP_CHECK (n == 2);
    DP_CHECK (dp_cnear (win[0], 20.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 10.0 + 0.0 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── continuity across block boundaries ────────────────────────── */
  {
    /* Fill a 4-tap delay one element at a time; verify wrap-around
     * works correctly when head crosses the start of the ring. */
    dp_delay_state_t *obj = dp_delay_create (4);
    double _Complex win[4];

    for (int i = 1; i <= 8; i++)
      dp_delay_push (obj, (double)i + 0.0 * I);

    /* Last 4 pushes: 5 6 7 8 → window = [8, 7, 6, 5] */
    dp_delay_ptr (obj, 4, win, 4);
    DP_CHECK (dp_cnear (win[0], 8.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 7.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 6.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[3], 5.0 + 0.0 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── write batch pushes multiple samples ────────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (3);
    double _Complex win[3];

    /* write inserts one sample (same as push — scalar API) */
    dp_delay_write (obj, 1.0 + 0.0 * I);
    dp_delay_write (obj, 2.0 + 0.0 * I);
    dp_delay_write (obj, 3.0 + 0.0 * I);

    dp_delay_ptr (obj, 3, win, 3);
    DP_CHECK (dp_cnear (win[0], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 2.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], 1.0 + 0.0 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── reset clears the buffer and resets head ────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (4);
    double _Complex win[4];

    dp_delay_push (obj, 1.0 + 1.0 * I);
    dp_delay_push (obj, 2.0 + 2.0 * I);
    dp_delay_reset (obj);

    /* After reset everything should be zero and head should be 0. */
    DP_CHECK (obj->head == 0);
    dp_delay_ptr (obj, 4, win, 4);
    for (int i = 0; i < 4; i++)
      DP_CHECK (dp_cnear (win[i], 0.0 + 0.0 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── ptr_max_out / push_ptr_max_out ─────────────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (7);
    /* gh-607: dp_delay_ptr_max_out(n) is the tight per-call bound min(n,taps).
     */
    DP_CHECK (dp_delay_ptr_max_out (obj, 7) == 7);   /* n == num_taps      */
    DP_CHECK (dp_delay_ptr_max_out (obj, 100) == 7); /* clamped to num_taps */
    DP_CHECK (dp_delay_ptr_max_out (obj, 3) == 3);   /* tight: n < num_taps */
    DP_CHECK (dp_delay_push_ptr_max_out (obj) == 7);
    dp_delay_destroy (obj);
  }

  /* ── complex values round-trip correctly ────────────────────────── */
  {
    dp_delay_state_t *obj = dp_delay_create (2);
    double _Complex win[2];

    dp_delay_push (obj, 1.5 + 2.5 * I);
    dp_delay_push (obj, -3.0 + 4.0 * I);

    dp_delay_ptr (obj, 2, win, 2);
    DP_CHECK (dp_cnear (win[0], -3.0 + 4.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 1.5 + 2.5 * I, TOL));
    dp_delay_destroy (obj);
  }

  /* ── short out: snapshot truncates, the ring still advances ─────────
   * dp_delay_ptr() is a pure read, so a short buffer just yields fewer
   * samples.  dp_delay_push_ptr() also MUTATES: the push has to land even
   * when there is no room to report it back, or the window falls out of
   * step with the sample stream.  Both are checked here. */
  {
    dp_delay_state_t *obj = dp_delay_create (4);
    double _Complex win[4];
    const double _Complex CANARY = -999.0 - 111.0 * I;

    for (int i = 1; i <= 4; i++)
      dp_delay_push (obj, (double)i + 0.0 * I); /* window = [4, 3, 2, 1] */

    for (size_t k = 0; k < 4; k++)
      win[k] = CANARY;
    DP_CHECK (dp_delay_ptr (obj, 4, win, 2) == 2);
    DP_CHECK (dp_cnear (win[0], 4.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[1], 3.0 + 0.0 * I, TOL));
    DP_CHECK (dp_cnear (win[2], CANARY, TOL)); /* past capacity: untouched */
    DP_CHECK (dp_cnear (win[3], CANARY, TOL));

    /* max_out == 0 writes nothing. */
    for (size_t k = 0; k < 4; k++)
      win[k] = CANARY;
    DP_CHECK (dp_delay_ptr (obj, 4, win, 0) == 0);
    for (size_t k = 0; k < 4; k++)
      DP_CHECK (dp_cnear (win[k], CANARY, TOL));

    /* push_ptr with no room: the snapshot is empty, but sample 5 is in. */
    DP_CHECK (dp_delay_push_ptr (obj, 5.0 + 0.0 * I, win, 0) == 0);
    for (size_t k = 0; k < 4; k++)
      DP_CHECK (dp_cnear (win[k], CANARY, TOL));
    DP_CHECK (dp_delay_ptr (obj, 4, win, 4) == 4);
    DP_CHECK (dp_cnear (win[0], 5.0 + 0.0 * I, TOL)); /* the push landed */
    DP_CHECK (dp_cnear (win[1], 4.0 + 0.0 * I, TOL));

    dp_delay_destroy (obj);
  }

  /* serializable state — field-wise ring + head round-trips + rejects. */
  {
    dp_delay_state_t *a = dp_delay_create (4);
    dp_delay_state_t *b = dp_delay_create (4);
    DP_CHECK (a != NULL && b != NULL);
    dp_delay_push (a, 1.0 + 2.0 * I);
    dp_delay_push (a, -3.0 + 0.5 * I);
    dp_delay_push (a, 4.0 - 1.0 * I);
    DP_STATE_ROUNDTRIP_TEST (dp_delay, a, b);
    DP_CHECK (b->head == a->head);
    DP_CHECK (
        memcmp (b->buf, a->buf, 2 * a->capacity * sizeof (double _Complex))
        == 0);
    dp_delay_destroy (a);
    dp_delay_destroy (b);
  }

  DP_TEST_END ("test_delay_core");
}
