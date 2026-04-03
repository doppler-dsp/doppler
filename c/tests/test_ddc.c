/**
 * @file test_ddc.c
 * @brief Unit tests for dp_ddc_* Digital Down-Converter.
 *
 * Tests are self-contained: no external framework required.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/ddc.h>
#include <dp/stream.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* NCO LUT has 2^16 entries; per-sample LUT quantization error is bounded
 * by π/65536 ≈ 4.8e-5.  Two such errors compound (input tone × NCO),
 * so the instantaneous mix error can reach ~1e-4.  Use 2× margin. */
#define TOL 2e-4f

static int
cf32_near (dp_cf32_t a, dp_cf32_t b, float tol)
{
  return fabsf (a.i - b.i) <= tol && fabsf (a.q - b.q) <= tol;
}

/* Generate a complex sinusoid: out[i] = exp(j·2π·f_n·i) */
static void
make_tone (dp_cf32_t *out, size_t n, float f_n)
{
  for (size_t i = 0; i < n; i++)
    {
      float phi = 2.0f * (float)M_PI * f_n * (float)i;
      out[i].i = cosf (phi);
      out[i].q = sinf (phi);
    }
}

/* =========================================================================
 * Test 1 — create / destroy
 * ========================================================================= */

static void
test_create_destroy (void)
{
  printf ("--- create / destroy\n");

  dp_ddc_t *ddc = dp_ddc_create (0.0f, NULL);
  CHECK (ddc != NULL, "dp_ddc_create returns non-NULL");
  dp_ddc_destroy (ddc);
  PASS ("dp_ddc_destroy does not crash");

  /* NULL destroy is a no-op */
  dp_ddc_destroy (NULL);
  PASS ("dp_ddc_destroy(NULL) is safe");
}

/* =========================================================================
 * Test 2 — get_freq round-trips the constructor value
 * ========================================================================= */

static void
test_get_freq (void)
{
  printf ("--- get_freq\n");

  float freqs[] = { 0.0f, 0.25f, -0.25f, 0.49f, -0.49f };
  for (size_t k = 0; k < sizeof freqs / sizeof *freqs; k++)
    {
      dp_ddc_t *ddc = dp_ddc_create (freqs[k], NULL);
      float got = dp_ddc_get_freq (ddc);
      CHECK (fabsf (got - freqs[k]) < 1e-7f, "get_freq matches create");
      dp_ddc_destroy (ddc);
    }

  /* After set_freq */
  dp_ddc_t *ddc = dp_ddc_create (0.0f, NULL);
  dp_ddc_set_freq (ddc, 0.1f);
  CHECK (fabsf (dp_ddc_get_freq (ddc) - 0.1f) < 1e-7f,
         "get_freq reflects set_freq");
  dp_ddc_destroy (ddc);
}

/* =========================================================================
 * Test 3 — zero-frequency DDC is a passthrough (no resampler)
 * ========================================================================= */

static void
test_passthrough (void)
{
  printf ("--- zero-frequency passthrough\n");

  /* At f_n=0 the NCO emits (1, 0) every sample, so output == input. */
  enum
  {
    N = 256
  };
  dp_cf32_t in[N], out[N];
  make_tone (in, N, 0.1f); /* arbitrary input */

  dp_ddc_t *ddc = dp_ddc_create (0.0f, NULL);
  size_t n = dp_ddc_execute (ddc, in, N, out, N);
  dp_ddc_destroy (ddc);

  CHECK (n == N, "output count == input count");

  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (!cf32_near (out[i], in[i], TOL))
      {
        ok = 0;
        break;
      }
  CHECK (ok, "zero-freq DDC output matches input");
}

/* =========================================================================
 * Test 4 — frequency translation: tone at +f_n mixed to DC
 *
 * Input: complex tone at normalised frequency +f_n
 * DDC:   norm_freq = -f_n  (translate +f_n → DC)
 * Expected output: constant phasor (1, 0)
 * ========================================================================= */

static void
test_frequency_translation (void)
{
  printf ("--- frequency translation\n");

  enum
  {
    N = 512
  };
  dp_cf32_t in[N], out[N];

  float test_freqs[] = { 0.25f, -0.25f, 0.1f, -0.1f, 0.3f };

  for (size_t k = 0; k < sizeof test_freqs / sizeof *test_freqs; k++)
    {
      float f_n = test_freqs[k];
      make_tone (in, N, f_n);

      dp_ddc_t *ddc = dp_ddc_create (-f_n, NULL);
      size_t n = dp_ddc_execute (ddc, in, N, out, N);
      dp_ddc_destroy (ddc);

      CHECK (n == N, "output count == input count (translation)");

      /* Skip the first sample (phase ramp starts at 0 regardless) */
      dp_cf32_t dc = { 1.0f, 0.0f };
      int ok = 1;
      for (size_t i = 1; i < N; i++)
        if (!cf32_near (out[i], dc, TOL))
          {
            ok = 0;
            fprintf (stderr, "    f_n=%.3f  i=%zu  got (%.6f, %.6f)\n", f_n, i,
                     out[i].i, out[i].q);
            break;
          }
      CHECK (ok, "translated tone is DC (1, 0)");
    }
}

/* =========================================================================
 * Test 5 — set_freq retunes NCO seamlessly
 * ========================================================================= */

static void
test_retune (void)
{
  printf ("--- retune via set_freq\n");

  enum
  {
    N = 256
  };
  dp_cf32_t in[N], out[N];

  /* Start at -0.25 to cancel a +0.25 tone */
  make_tone (in, N, 0.25f);
  dp_ddc_t *ddc = dp_ddc_create (-0.25f, NULL);
  dp_ddc_execute (ddc, in, N, out, N);

  int ok1 = 1;
  dp_cf32_t dc = { 1.0f, 0.0f };
  for (size_t i = 1; i < N; i++)
    if (!cf32_near (out[i], dc, TOL))
      {
        ok1 = 0;
        break;
      }
  CHECK (ok1, "pre-retune: tone at +0.25 down-converted to DC");

  /* Retune to -0.1; now process a +0.1 tone and expect DC */
  dp_ddc_set_freq (ddc, -0.1f);
  CHECK (fabsf (dp_ddc_get_freq (ddc) - (-0.1f)) < 1e-7f,
         "set_freq reflected in get_freq");

  make_tone (in, N, 0.1f);
  dp_ddc_execute (ddc, in, N, out, N);
  dp_ddc_destroy (ddc);

  int ok2 = 1;
  for (size_t i = 1; i < N; i++)
    if (!cf32_near (out[i], dc, TOL))
      {
        ok2 = 0;
        break;
      }
  CHECK (ok2, "post-retune: tone at +0.1 down-converted to DC");
}

/* =========================================================================
 * Test 6 — reset clears NCO phase
 * ========================================================================= */

static void
test_reset (void)
{
  printf ("--- reset\n");

  enum
  {
    N = 64
  };
  dp_cf32_t in[N], out1[N], out2[N];
  make_tone (in, N, 0.25f);

  /* First run */
  dp_ddc_t *ddc = dp_ddc_create (-0.25f, NULL);
  dp_ddc_execute (ddc, in, N, out1, N);

  /* Advance phase by processing N more samples, then reset */
  dp_ddc_execute (ddc, in, N, out2, N);
  dp_ddc_reset (ddc);

  /* After reset, output should match the first run */
  dp_ddc_execute (ddc, in, N, out2, N);
  dp_ddc_destroy (ddc);

  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (!cf32_near (out1[i], out2[i], TOL))
      {
        ok = 0;
        break;
      }
  CHECK (ok, "reset restores initial state");
}

/* =========================================================================
 * Test 7 — max_out clips output when smaller than num_in
 * ========================================================================= */

static void
test_max_out_clip (void)
{
  printf ("--- max_out clipping (no resampler)\n");

  enum
  {
    N = 128,
    CAP = 64
  };
  dp_cf32_t in[N], out[CAP];
  make_tone (in, N, 0.0f);

  dp_ddc_t *ddc = dp_ddc_create (0.0f, NULL);
  size_t n = dp_ddc_execute (ddc, in, N, out, CAP);
  dp_ddc_destroy (ddc);

  CHECK (n == CAP, "output count capped at max_out");
}

/* =========================================================================
 * Test 8 — zero num_in returns 0 without crashing
 * ========================================================================= */

static void
test_empty_block (void)
{
  printf ("--- empty block\n");

  dp_cf32_t out[4];
  dp_ddc_t *ddc = dp_ddc_create (0.1f, NULL);
  size_t n = dp_ddc_execute (ddc, NULL, 0, out, 4);
  dp_ddc_destroy (ddc);

  CHECK (n == 0, "empty block returns 0");
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== DDC unit tests ===\n");

  test_create_destroy ();
  test_get_freq ();
  test_passthrough ();
  test_frequency_translation ();
  test_retune ();
  test_reset ();
  test_max_out_clip ();
  test_empty_block ();

  printf ("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
