#include "delay/delay_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

#define TOL 1e-12

static inline int
ceq (double complex a, double complex b)
{
  return fabs (creal (a) - creal (b)) < TOL
         && fabs (cimag (a) - cimag (b)) < TOL;
}

int
main (void)
{
  int _fails = 0;

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    delay_state_t *obj = delay_create (4);
    CHECK (obj != NULL);
    CHECK (obj->num_taps == 4);
    CHECK (obj->capacity == 4); /* 4 is already a power of two */
    CHECK (obj->mask == 3);
    CHECK (obj->buf != NULL);
    delay_destroy (obj);

    delay_destroy (NULL); /* must not crash */
  }

  /* ── capacity is always a power of two ─────────────────────────── */
  {
    delay_state_t *a = delay_create (1);
    CHECK (a->capacity == 1);
    delay_destroy (a);

    delay_state_t *b = delay_create (3);
    CHECK (b->capacity == 4);
    delay_destroy (b);

    delay_state_t *c = delay_create (5);
    CHECK (c->capacity == 8);
    delay_destroy (c);

    delay_state_t *d = delay_create (8);
    CHECK (d->capacity == 8);
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

    size_t n = delay_ptr (obj, 3, win);
    CHECK (n == 3);
    CHECK (ceq (win[0], 3.0 + 0.0 * I));
    CHECK (ceq (win[1], 2.0 + 0.0 * I));
    CHECK (ceq (win[2], 1.0 + 0.0 * I));
    delay_destroy (obj);
  }

  /* ── push_ptr returns the updated window ────────────────────────── */
  {
    delay_state_t *obj = delay_create (2);
    double complex win[2];

    delay_push (obj, 10.0 + 0.0 * I);
    size_t n = delay_push_ptr (obj, 20.0 + 0.0 * I, win);
    CHECK (n == 2);
    CHECK (ceq (win[0], 20.0 + 0.0 * I));
    CHECK (ceq (win[1], 10.0 + 0.0 * I));
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
    delay_ptr (obj, 4, win);
    CHECK (ceq (win[0], 8.0 + 0.0 * I));
    CHECK (ceq (win[1], 7.0 + 0.0 * I));
    CHECK (ceq (win[2], 6.0 + 0.0 * I));
    CHECK (ceq (win[3], 5.0 + 0.0 * I));
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

    delay_ptr (obj, 3, win);
    CHECK (ceq (win[0], 3.0 + 0.0 * I));
    CHECK (ceq (win[1], 2.0 + 0.0 * I));
    CHECK (ceq (win[2], 1.0 + 0.0 * I));
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
    CHECK (obj->head == 0);
    delay_ptr (obj, 4, win);
    for (int i = 0; i < 4; i++)
      CHECK (ceq (win[i], 0.0 + 0.0 * I));
    delay_destroy (obj);
  }

  /* ── ptr_max_out / push_ptr_max_out ─────────────────────────────── */
  {
    delay_state_t *obj = delay_create (7);
    CHECK (delay_ptr_max_out (obj) == 7);
    CHECK (delay_push_ptr_max_out (obj) == 7);
    delay_destroy (obj);
  }

  /* ── complex values round-trip correctly ────────────────────────── */
  {
    delay_state_t *obj = delay_create (2);
    double complex win[2];

    delay_push (obj, 1.5 + 2.5 * I);
    delay_push (obj, -3.0 + 4.0 * I);

    delay_ptr (obj, 2, win);
    CHECK (ceq (win[0], -3.0 + 4.0 * I));
    CHECK (ceq (win[1], 1.5 + 2.5 * I));
    delay_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_delay_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_delay_core PASSED\n");
  return 0;
}
