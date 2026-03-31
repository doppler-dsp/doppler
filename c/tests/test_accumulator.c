/**
 * @file test_accumulator.c
 * @brief Unit tests for dp_acc_f32_* and dp_acc_cf64_* accumulators.
 *
 * Self-contained, no external framework.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/accumulator.h>

#include <math.h>
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

#define TOL_F32 1e-5f
#define TOL_F64 1e-12

static int
near_f32 (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}

static int
near_f64 (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
}

/* =========================================================================
 * Test 1 — create / destroy, initial value is zero
 * ========================================================================= */

static void
test_create_f32 (void)
{
  printf ("\nTest 1  create/destroy f32\n");
  dp_acc_f32_t *acc = dp_acc_f32_create ();
  CHECK (acc != NULL, "create returns non-NULL");
  CHECK (near_f32 (dp_acc_f32_get (acc), 0.0f, TOL_F32), "initial value is 0");
  dp_acc_f32_destroy (acc);
  dp_acc_f32_destroy (NULL); /* must not crash */
  PASS ("destroy(NULL) is safe");
}

static void
test_create_cf64 (void)
{
  printf ("\nTest 2  create/destroy cf64\n");
  dp_acc_cf64_t *acc = dp_acc_cf64_create ();
  CHECK (acc != NULL, "create returns non-NULL");
  dp_cf64_t v = dp_acc_cf64_get (acc);
  CHECK (near_f64 (v.i, 0.0, TOL_F64) && near_f64 (v.q, 0.0, TOL_F64),
         "initial value is (0,0)");
  dp_acc_cf64_destroy (acc);
  dp_acc_cf64_destroy (NULL);
  PASS ("destroy(NULL) is safe");
}

/* =========================================================================
 * Test 3 — reset
 * ========================================================================= */

static void
test_reset (void)
{
  printf ("\nTest 3  reset\n");
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_push (af, 42.0f);
  dp_acc_f32_reset (af);
  CHECK (near_f32 (dp_acc_f32_get (af), 0.0f, TOL_F32),
         "f32 reset clears to 0");
  dp_acc_f32_destroy (af);

  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_cf64_t s = { 1.0, 2.0 };
  dp_acc_cf64_push (ac, s);
  dp_acc_cf64_reset (ac);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 0.0, TOL_F64) && near_f64 (v.q, 0.0, TOL_F64),
         "cf64 reset clears to (0,0)");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 4 — dump (get + reset)
 * ========================================================================= */

static void
test_dump (void)
{
  printf ("\nTest 4  dump\n");
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_push (af, 7.5f);
  float v = dp_acc_f32_dump (af);
  CHECK (near_f32 (v, 7.5f, TOL_F32), "f32 dump returns value");
  CHECK (near_f32 (dp_acc_f32_get (af), 0.0f, TOL_F32),
         "f32 dump resets to 0");
  dp_acc_f32_destroy (af);

  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_cf64_t s = { 3.0, -4.0 };
  dp_acc_cf64_push (ac, s);
  dp_cf64_t cv = dp_acc_cf64_dump (ac);
  CHECK (near_f64 (cv.i, 3.0, TOL_F64) && near_f64 (cv.q, -4.0, TOL_F64),
         "cf64 dump returns value");
  dp_cf64_t cv2 = dp_acc_cf64_get (ac);
  CHECK (near_f64 (cv2.i, 0.0, TOL_F64) && near_f64 (cv2.q, 0.0, TOL_F64),
         "cf64 dump resets to (0,0)");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 5 — scalar push
 * ========================================================================= */

static void
test_push (void)
{
  printf ("\nTest 5  scalar push\n");
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_push (af, 1.0f);
  dp_acc_f32_push (af, 2.0f);
  dp_acc_f32_push (af, 3.0f);
  CHECK (near_f32 (dp_acc_f32_get (af), 6.0f, TOL_F32), "f32 push: 1+2+3 = 6");
  dp_acc_f32_destroy (af);

  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_cf64_t a = { 1.0, 2.0 };
  dp_cf64_t b = { 3.0, 4.0 };
  dp_acc_cf64_push (ac, a);
  dp_acc_cf64_push (ac, b);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 4.0, TOL_F64) && near_f64 (v.q, 6.0, TOL_F64),
         "cf64 push: (1+2i)+(3+4i) = (4+6i)");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 6 — 1-D add
 * ========================================================================= */

static void
test_add_1d (void)
{
  printf ("\nTest 6  1-D add\n");
  float xf[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_add (af, xf, 8);
  CHECK (near_f32 (dp_acc_f32_get (af), 36.0f, TOL_F32),
         "f32 add: sum(1..8) = 36");
  dp_acc_f32_destroy (af);

  dp_cf64_t xc[4] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_acc_cf64_add (ac, xc, 4);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 16.0, TOL_F64) && near_f64 (v.q, 20.0, TOL_F64),
         "cf64 add: sum I=1+3+5+7=16, Q=2+4+6+8=20");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 7 — 1-D madd
 * ========================================================================= */

static void
test_madd_1d (void)
{
  printf ("\nTest 7  1-D madd\n");
  /* dot([1,2,3],[4,5,6]) = 4+10+18 = 32 */
  float x[3] = { 1.0f, 2.0f, 3.0f };
  float h[3] = { 4.0f, 5.0f, 6.0f };
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_madd (af, x, h, 3);
  CHECK (near_f32 (dp_acc_f32_get (af), 32.0f, TOL_F32),
         "f32 madd: [1,2,3]·[4,5,6] = 32");
  dp_acc_f32_destroy (af);

  /* complex x = [(1,2),(3,4)], real h = [2,3]
   * I:  1*2 + 3*3 = 2+9 = 11
   * Q:  2*2 + 4*3 = 4+12 = 16 */
  dp_cf64_t xc[2] = { { 1.0, 2.0 }, { 3.0, 4.0 } };
  float hc[2] = { 2.0f, 3.0f };
  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_acc_cf64_madd (ac, xc, hc, 2);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 11.0, TOL_F64) && near_f64 (v.q, 16.0, TOL_F64),
         "cf64 madd: I=11, Q=16");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 8 — accumulation across calls (state preserved)
 * ========================================================================= */

static void
test_accumulation (void)
{
  printf ("\nTest 8  accumulation across calls\n");
  dp_acc_f32_t *af = dp_acc_f32_create ();
  float x[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  dp_acc_f32_add (af, x, 4);  /* +4 */
  dp_acc_f32_add (af, x, 4);  /* +4 = 8 */
  dp_acc_f32_push (af, 2.0f); /* +2 = 10 */
  CHECK (near_f32 (dp_acc_f32_get (af), 10.0f, TOL_F32),
         "f32 state preserved across calls: 4+4+2 = 10");
  dp_acc_f32_destroy (af);
}

/* =========================================================================
 * Test 9 — 2-D add
 * ========================================================================= */

static void
test_add_2d (void)
{
  printf ("\nTest 9  2-D add\n");
  /* 2×3 matrix: sum = 1+2+3+4+5+6 = 21 */
  float x[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_add2d (af, &x[0][0], 2, 3);
  CHECK (near_f32 (dp_acc_f32_get (af), 21.0f, TOL_F32),
         "f32 add2d: sum of 2×3 matrix = 21");
  dp_acc_f32_destroy (af);

  dp_cf64_t xc[2][2] = { { { 1, 2 }, { 3, 4 } }, { { 5, 6 }, { 7, 8 } } };
  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_acc_cf64_add2d (ac, &xc[0][0], 2, 2);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 16.0, TOL_F64) && near_f64 (v.q, 20.0, TOL_F64),
         "cf64 add2d: I=1+3+5+7=16, Q=2+4+6+8=20");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 10 — 2-D madd
 * ========================================================================= */

static void
test_madd_2d (void)
{
  printf ("\nTest 10  2-D madd\n");
  /* 2×2 element-wise: [[1,2],[3,4]] · [[1,1],[1,1]] = 1+2+3+4 = 10 */
  float x[2][2] = { { 1, 2 }, { 3, 4 } };
  float h[2][2] = { { 1, 1 }, { 1, 1 } };
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_madd2d (af, &x[0][0], &h[0][0], 2, 2);
  CHECK (near_f32 (dp_acc_f32_get (af), 10.0f, TOL_F32),
         "f32 madd2d: [[1,2],[3,4]]·ones = 10");

  /* with non-unity weights: [[1,2],[3,4]] · [[2,2],[2,2]] = 20 */
  float h2[2][2] = { { 2, 2 }, { 2, 2 } };
  dp_acc_f32_reset (af);
  dp_acc_f32_madd2d (af, &x[0][0], &h2[0][0], 2, 2);
  CHECK (near_f32 (dp_acc_f32_get (af), 20.0f, TOL_F32),
         "f32 madd2d: [[1,2],[3,4]]·2*ones = 20");
  dp_acc_f32_destroy (af);

  /* cf64 × real: 2×2, x=[(1,2),(3,4),(5,6),(7,8)], h=all 1
   * I=1+3+5+7=16, Q=2+4+6+8=20 */
  dp_cf64_t xc[2][2] = { { { 1, 2 }, { 3, 4 } }, { { 5, 6 }, { 7, 8 } } };
  float hc[2][2] = { { 1, 1 }, { 1, 1 } };
  dp_acc_cf64_t *ac = dp_acc_cf64_create ();
  dp_acc_cf64_madd2d (ac, &xc[0][0], &hc[0][0], 2, 2);
  dp_cf64_t v = dp_acc_cf64_get (ac);
  CHECK (near_f64 (v.i, 16.0, TOL_F64) && near_f64 (v.q, 20.0, TOL_F64),
         "cf64 madd2d: I=16, Q=20");
  dp_acc_cf64_destroy (ac);
}

/* =========================================================================
 * Test 11 — zero-length (n=0) is a no-op
 * ========================================================================= */

static void
test_zero_length (void)
{
  printf ("\nTest 11  zero-length (n=0) no-op\n");
  dp_acc_f32_t *af = dp_acc_f32_create ();
  dp_acc_f32_push (af, 5.0f);
  dp_acc_f32_add (af, NULL, 0);
  dp_acc_f32_madd (af, NULL, NULL, 0);
  dp_acc_f32_add2d (af, NULL, 0, 0);
  dp_acc_f32_madd2d (af, NULL, NULL, 0, 0);
  CHECK (near_f32 (dp_acc_f32_get (af), 5.0f, TOL_F32),
         "f32 zero-length ops leave value unchanged");
  dp_acc_f32_destroy (af);
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== accumulator unit tests ===\n");

  test_create_f32 ();
  test_create_cf64 ();
  test_reset ();
  test_dump ();
  test_push ();
  test_add_1d ();
  test_madd_1d ();
  test_accumulation ();
  test_add_2d ();
  test_madd_2d ();
  test_zero_length ();

  printf ("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
