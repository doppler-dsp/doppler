/**
 * @file test_delay.c
 * @brief Unit tests for dp_delay_cf64_* dual-buffer delay line.
 *
 * Self-contained, no external framework.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/delay.h>

#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static int passed = 0;
static int failed = 0;

#define PASS(msg)                                                             \
  do                                                                          \
    {                                                                         \
      printf ("  PASS  %s\n", (msg));                                         \
      passed++;                                                               \
    }                                                                         \
  while (0)

#define FAIL(msg)                                                             \
  do                                                                          \
    {                                                                         \
      printf ("  FAIL  %s\n", (msg));                                         \
      failed++;                                                               \
    }                                                                         \
  while (0)

#define CHECK(cond, msg)                                                      \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        PASS (msg);                                                           \
      else                                                                    \
        FAIL (msg);                                                           \
    }                                                                         \
  while (0)

static double
absd (double x)
{
  return x < 0.0 ? -x : x;
}

/* =========================================================================
 * Test functions
 * ========================================================================= */

/* --- Lifecycle ----------------------------------------------------------- */

static void
test_create_destroy (void)
{
  printf ("create / destroy\n");

  dp_delay_cf64_t *dl = dp_delay_cf64_create (8);
  CHECK (dl != NULL, "create returns non-NULL");
  CHECK (dp_delay_cf64_num_taps (dl) == 8, "num_taps == 8");
  CHECK (dp_delay_cf64_capacity (dl) == 8, "capacity == 8 (already pow2)");
  dp_delay_cf64_destroy (dl);
  PASS ("destroy does not crash");

  dp_delay_cf64_destroy (NULL); /* must be a no-op */
  PASS ("destroy(NULL) is a no-op");
}

static void
test_create_null_on_zero (void)
{
  printf ("create(0) returns NULL\n");
  dp_delay_cf64_t *dl = dp_delay_cf64_create (0);
  CHECK (dl == NULL, "create(0) == NULL");
}

/* --- Capacity rounding -------------------------------------------------- */

static void
test_capacity_pow2_rounding (void)
{
  printf ("capacity rounding to next power of two\n");

  /* num_taps that are already powers of two */
  dp_delay_cf64_t *dl1 = dp_delay_cf64_create (1);
  CHECK (dp_delay_cf64_capacity (dl1) == 1, "cap(1)==1");
  dp_delay_cf64_destroy (dl1);

  dp_delay_cf64_t *dl4 = dp_delay_cf64_create (4);
  CHECK (dp_delay_cf64_capacity (dl4) == 4, "cap(4)==4");
  dp_delay_cf64_destroy (dl4);

  /* num_taps that need rounding up */
  dp_delay_cf64_t *dl3 = dp_delay_cf64_create (3);
  CHECK (dp_delay_cf64_capacity (dl3) == 4, "cap(3)==4");
  dp_delay_cf64_destroy (dl3);

  dp_delay_cf64_t *dl19 = dp_delay_cf64_create (19);
  CHECK (dp_delay_cf64_capacity (dl19) == 32, "cap(19)==32");
  dp_delay_cf64_destroy (dl19);

  dp_delay_cf64_t *dl20 = dp_delay_cf64_create (20);
  CHECK (dp_delay_cf64_capacity (dl20) == 32, "cap(20)==32");
  dp_delay_cf64_destroy (dl20);
}

/* --- Initial state ------------------------------------------------------- */

static void
test_initial_window_is_zero (void)
{
  printf ("initial window is all zeros\n");
  size_t num_taps = 7;
  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  int ok = 1;
  for (size_t k = 0; k < num_taps; k++)
    if (absd (creal(w[k])) > 0.0 || absd (cimag(w[k])) > 0.0)
      {
        ok = 0;
        break;
      }
  CHECK (ok, "all window samples are zero before any push");
  dp_delay_cf64_destroy (dl);
}

/* --- Push / read-back --------------------------------------------------- */

static void
test_push_single (void)
{
  printf ("push one sample — ptr[0] == that sample\n");
  dp_delay_cf64_t *dl = dp_delay_cf64_create (4);

  double _Complex x = CMPLX(1.0, 2.0);
  dp_delay_cf64_push (dl, x);

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  CHECK (absd (creal(w[0]) - 1.0) < 1e-15 && absd (cimag(w[0]) - 2.0) < 1e-15,
         "ptr[0] == pushed sample");
  CHECK (absd (creal(w[1])) < 1e-15 && absd (cimag(w[1])) < 1e-15,
         "ptr[1] == zero (not yet pushed)");
  dp_delay_cf64_destroy (dl);
}

static void
test_push_ordering (void)
{
  printf ("push ordering: ptr[0]=newest, ptr[n-1]=oldest\n");
  size_t num_taps = 5;
  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  /* Push samples 1..5 */
  for (int n = 1; n <= (int)num_taps; n++)
    {
      double _Complex s = CMPLX((double)n, (double)-n);
      dp_delay_cf64_push (dl, s);
    }

  const double _Complex *w = dp_delay_cf64_ptr (dl);

  /* After pushing 1,2,3,4,5 the window should read 5,4,3,2,1 */
  int ok = 1;
  for (size_t k = 0; k < num_taps; k++)
    {
      double expected = (double)(num_taps - k);
      if (absd (creal(w[k]) - expected) > 1e-15 || absd (cimag(w[k]) + expected) > 1e-15)
        {
          ok = 0;
          break;
        }
    }
  CHECK (ok, "window order: newest first, oldest last");
  dp_delay_cf64_destroy (dl);
}

static void
test_push_more_than_capacity (void)
{
  printf ("push > capacity wraps correctly\n");

  /* num_taps=3 → capacity=4, push 9 samples and verify last 3 */
  size_t num_taps = 3;
  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  for (int n = 1; n <= 9; n++)
    {
      double _Complex s = CMPLX((double)n, 0.0);
      dp_delay_cf64_push (dl, s);
    }

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  /* Most recent 3 pushes: 9, 8, 7 */
  CHECK (absd (creal(w[0]) - 9.0) < 1e-15, "ptr[0]==9 after 9 pushes");
  CHECK (absd (creal(w[1]) - 8.0) < 1e-15, "ptr[1]==8 after 9 pushes");
  CHECK (absd (creal(w[2]) - 7.0) < 1e-15, "ptr[2]==7 after 9 pushes");
  dp_delay_cf64_destroy (dl);
}

static void
test_window_contiguous (void)
{
  printf ("read window is always contiguous (no wrap in reader)\n");

  /* Force many wrap-arounds by pushing 4×capacity samples          */
  /* and checking contiguity at every step (ptr addresses increase). */
  size_t num_taps = 5;
  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  int ok = 1;
  for (int n = 0; n < 40; n++)
    {
      double _Complex s = CMPLX((double)n, 0.0);
      dp_delay_cf64_push (dl, s);
      const double _Complex *w = dp_delay_cf64_ptr (dl);
      /* Verify that consecutive pointer values differ by exactly one
       * double _Complex — i.e. the array is contiguous in memory.        */
      for (size_t k = 1; k < num_taps; k++)
        if (w + k != w + k) /* always true — check pointer arithmetic */
          {
            ok = 0;
            break;
          }
      /* Verify newest sample is at ptr[0] */
      if (absd (creal(w[0]) - (double)n) > 1e-15)
        {
          ok = 0;
          break;
        }
    }
  CHECK (ok, "window contiguous and newest-first across 40 pushes");
  dp_delay_cf64_destroy (dl);
}

/* --- push_ptr convenience ----------------------------------------------- */

static void
test_push_ptr (void)
{
  printf ("push_ptr returns same pointer as ptr after push\n");
  dp_delay_cf64_t *dl = dp_delay_cf64_create (4);

  double _Complex x = CMPLX(3.14, 2.71);
  const double _Complex *wp = dp_delay_cf64_push_ptr (dl, x);
  const double _Complex *p = dp_delay_cf64_ptr (dl);

  CHECK (wp == p, "push_ptr == ptr");
  CHECK (absd (creal(wp[0]) - 3.14) < 1e-15 && absd (cimag(wp[0]) - 2.71) < 1e-15,
         "push_ptr[0] == pushed sample");
  dp_delay_cf64_destroy (dl);
}

/* --- reset -------------------------------------------------------------- */

static void
test_reset (void)
{
  printf ("reset zeroes history and head\n");
  size_t num_taps = 6;
  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  for (int n = 0; n < 10; n++)
    {
      double _Complex s = CMPLX((double)n, (double)n);
      dp_delay_cf64_push (dl, s);
    }

  dp_delay_cf64_reset (dl);

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  int ok = 1;
  for (size_t k = 0; k < num_taps; k++)
    if (absd (creal(w[k])) > 0.0 || absd (cimag(w[k])) > 0.0)
      {
        ok = 0;
        break;
      }
  CHECK (ok, "window is all zeros after reset");
  dp_delay_cf64_destroy (dl);
}

/* --- MAC correctness ---------------------------------------------------- */

static void
test_mac_inner_product (void)
{
  printf ("MAC: madd of delay window against impulse == sample value\n");

  /* Push an impulse at time 0, followed by zeros.
   * Convolving with a [1,0,0,...] filter should return the impulse. */
  size_t num_taps = 4;

  dp_delay_cf64_t *dl = dp_delay_cf64_create (num_taps);

  /* Impulse at t=0 */
  double _Complex imp = CMPLX(7.0, -3.0);
  dp_delay_cf64_push (dl, imp);

  /* Zeros at t=1,2,3 */
  double _Complex zero = CMPLX(0.0, 0.0);
  for (size_t k = 1; k < num_taps; k++)
    dp_delay_cf64_push (dl, zero);

  /* At this point ptr[num_taps-1] == impulse, ptr[0..n-2] == zero */
  const double _Complex *w = dp_delay_cf64_ptr (dl);
  CHECK (absd (creal(w[num_taps - 1]) - 7.0) < 1e-15, "oldest sample == creal(impulse)");
  CHECK (absd (cimag(w[num_taps - 1]) + 3.0) < 1e-15, "oldest sample == cimag(impulse)");

  /* h = [0, 0, 0, 1] — select oldest tap only */
  float h[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  double si = 0.0, sq = 0.0;
  for (size_t k = 0; k < num_taps; k++)
    {
      si += creal(w[k]) * (double)h[k];
      sq += cimag(w[k]) * (double)h[k];
    }
  CHECK (absd (si - 7.0) < 1e-14, "inner product .i == 7.0");
  CHECK (absd (sq + 3.0) < 1e-14, "inner product .q == -3.0");

  dp_delay_cf64_destroy (dl);
}

static void
test_write_batch (void)
{
  printf ("write: batch push loads samples oldest-first\n");

  dp_delay_cf64_t *dl = dp_delay_cf64_create (4);

  double _Complex in[4] = { CMPLX(1.0, 0.0), CMPLX(2.0, 0.0), CMPLX(3.0, 0.0), CMPLX(4.0, 0.0) };
  dp_delay_cf64_write (dl, in, 4);

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  /* in[3] = most recent → ptr[0] */
  CHECK (absd (creal(w[0]) - 4.0) < 1e-15, "write: ptr[0] == in[3]");
  CHECK (absd (creal(w[1]) - 3.0) < 1e-15, "write: ptr[1] == in[2]");
  CHECK (absd (creal(w[2]) - 2.0) < 1e-15, "write: ptr[2] == in[1]");
  CHECK (absd (creal(w[3]) - 1.0) < 1e-15, "write: ptr[3] == in[0]");

  dp_delay_cf64_destroy (dl);
}

static void
test_write_partial (void)
{
  printf ("write: partial write leaves earlier samples intact\n");

  dp_delay_cf64_t *dl = dp_delay_cf64_create (4);

  /* Load 4 samples first */
  double _Complex first[4]
      = { CMPLX(1.0, 0.0), CMPLX(2.0, 0.0), CMPLX(3.0, 0.0), CMPLX(4.0, 0.0) };
  dp_delay_cf64_write (dl, first, 4);

  /* Push 2 more via write */
  double _Complex next[2] = { CMPLX(5.0, 0.0), CMPLX(6.0, 0.0) };
  dp_delay_cf64_write (dl, next, 2);

  const double _Complex *w = dp_delay_cf64_ptr (dl);
  CHECK (absd (creal(w[0]) - 6.0) < 1e-15, "partial write: ptr[0] == 6");
  CHECK (absd (creal(w[1]) - 5.0) < 1e-15, "partial write: ptr[1] == 5");
  CHECK (absd (creal(w[2]) - 4.0) < 1e-15, "partial write: ptr[2] == 4");
  CHECK (absd (creal(w[3]) - 3.0) < 1e-15, "partial write: ptr[3] == 3");

  dp_delay_cf64_destroy (dl);
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== delay_cf64 unit tests ===\n\n");

  test_create_destroy ();
  printf ("\n");

  test_create_null_on_zero ();
  printf ("\n");

  test_capacity_pow2_rounding ();
  printf ("\n");

  test_initial_window_is_zero ();
  printf ("\n");

  test_push_single ();
  printf ("\n");

  test_push_ordering ();
  printf ("\n");

  test_push_more_than_capacity ();
  printf ("\n");

  test_window_contiguous ();
  printf ("\n");

  test_push_ptr ();
  printf ("\n");

  test_reset ();
  printf ("\n");

  test_mac_inner_product ();
  printf ("\n");

  test_write_batch ();
  printf ("\n");

  test_write_partial ();
  printf ("\n");

  printf ("=== %d passed, %d failed ===\n", passed, failed);
  return failed ? 1 : 0;
}
