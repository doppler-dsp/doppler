/**
 * @file test_window.c
 * @brief Unit tests for dp_kaiser_window and dp_kaiser_enbw.
 */

#include "dp/window.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================== */
/* Minimal test harness                                               */
/* ================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                      \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        {                                                                     \
          printf ("  PASS  %s\n", msg);                                       \
          g_pass++;                                                           \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          printf ("  FAIL  %s  (line %d)\n", msg, __LINE__);                  \
          g_fail++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

#define CHECK_NEAR(a, b, tol, msg)                                            \
  CHECK (fabs ((double)(a) - (double)(b)) <= (double)(tol), msg)

/* ================================================================== */
/* Tests                                                              */
/* ================================================================== */

/* A window of length 1 must always be 1.0 regardless of beta. */
static void
test_length_one (void)
{
  float w;
  dp_kaiser_window (&w, 1, 0.0f);
  CHECK_NEAR (w, 1.0f, 1e-6f, "len=1, beta=0: w[0]==1.0");

  dp_kaiser_window (&w, 1, 8.6f);
  CHECK_NEAR (w, 1.0f, 1e-6f, "len=1, beta=8.6: w[0]==1.0");
}

/* beta=0 → rectangular window: all samples must equal 1.0. */
static void
test_rectangular (void)
{
  const size_t N = 64;
  float w[64];
  dp_kaiser_window (w, N, 0.0f);

  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (fabsf (w[i] - 1.0f) > 1e-5f)
      ok = 0;
  CHECK (ok, "beta=0 → rectangular (all ones)");
}

/* The window must be symmetric: w[k] == w[N-1-k]. */
static void
test_symmetry (void)
{
  const size_t N = 128;
  float w[128];
  dp_kaiser_window (w, N, 6.0f);

  int ok = 1;
  for (size_t i = 0; i < N / 2; i++)
    if (fabsf (w[i] - w[N - 1 - i]) > 1e-6f)
      ok = 0;
  CHECK (ok, "window is symmetric");
}

/* Peak must be at the centre and equal 1.0 (by definition of Kaiser). */
static void
test_peak_at_centre (void)
{
  const size_t N = 101; /* odd length: single centre sample */
  float w[101];
  dp_kaiser_window (w, N, 6.0f);

  CHECK_NEAR (w[N / 2], 1.0f, 1e-5f, "centre sample == 1.0");

  /* All other samples must be <= 1.0 */
  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (w[i] > 1.0f + 1e-5f)
      ok = 0;
  CHECK (ok, "all samples <= 1.0");
}

/* Larger beta → smaller sidelobes → lower end values. */
static void
test_beta_monotone_ends (void)
{
  const size_t N = 64;
  float w5[64], w10[64];
  dp_kaiser_window (w5, N, 5.0f);
  dp_kaiser_window (w10, N, 10.0f);

  /* First sample gets smaller as beta grows */
  CHECK (w10[0] < w5[0], "larger beta → smaller end value");
}

/* ENBW of rectangular window (beta=0) must equal 1.0 bin exactly. */
static void
test_enbw_rectangular (void)
{
  const size_t N = 512;
  float w[512];
  dp_kaiser_window (w, N, 0.0f);
  float enbw = dp_kaiser_enbw (w, N);
  CHECK_NEAR (enbw, 1.0f, 1e-4f, "rectangular ENBW == 1.0 bin");
}

/* ENBW at beta=6 should be ~1.47 bins.
 *
 * Note: the Harris (1978) table uses α where β = π·α, so their
 * "β=2 → ENBW=1.50" entry corresponds to direct β = π·2 ≈ 6.28
 * here.  Our convention matches NumPy / SciPy: β is the direct I₀
 * argument, not scaled by π.
 */
static void
test_enbw_beta6 (void)
{
  const size_t N = 4096; /* large N → converges to asymptotic value */
  float *w = malloc (N * sizeof (float));
  dp_kaiser_window (w, N, 6.0f);
  float enbw = dp_kaiser_enbw (w, N);
  /* Reference (NumPy/SciPy): 1.467 ± 0.02 */
  CHECK (enbw > 1.44f && enbw < 1.50f, "beta=6 ENBW ≈ 1.47 bins");
  free (w);
}

/* ENBW at beta=8.6 should be ~1.72 bins. */
static void
test_enbw_beta8p6 (void)
{
  const size_t N = 4096;
  float *w = malloc (N * sizeof (float));
  dp_kaiser_window (w, N, 8.6f);
  float enbw = dp_kaiser_enbw (w, N);
  /* Reference (NumPy/SciPy): 1.722 ± 0.02 */
  CHECK (enbw > 1.70f && enbw < 1.75f, "beta=8.6 ENBW ≈ 1.72 bins");
  free (w);
}

/* ENBW increases monotonically with beta. */
static void
test_enbw_monotone (void)
{
  const size_t N = 1024;
  float w[1024];
  float prev = 0.0f;
  float betas[] = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f };
  int nb = sizeof (betas) / sizeof (betas[0]);
  int ok = 1;
  for (int i = 0; i < nb; i++)
    {
      dp_kaiser_window (w, N, betas[i]);
      float e = dp_kaiser_enbw (w, N);
      if (e <= prev)
        ok = 0;
      prev = e;
    }
  CHECK (ok, "ENBW increases monotonically with beta");
}

/* ================================================================== */
/* main                                                               */
/* ================================================================== */

int
main (void)
{
  printf ("=== test_window ===\n");

  test_length_one ();
  test_rectangular ();
  test_symmetry ();
  test_peak_at_centre ();
  test_beta_monotone_ends ();
  test_enbw_rectangular ();
  test_enbw_beta6 ();
  test_enbw_beta8p6 ();
  test_enbw_monotone ();

  printf ("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
