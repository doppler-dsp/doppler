/**
 * @file test_fir.c
 * @brief Unit tests for dp_fir_* complex FIR filter.
 *
 * Tests are self-contained: no external framework required.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/fir.h>
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

static int
cf32_near (dp_cf32_t a, dp_cf32_t b, float tol)
{
  return fabsf (a.i - b.i) <= tol && fabsf (a.q - b.q) <= tol;
}

/* =========================================================================
 * Test: create / destroy
 * ========================================================================= */

static void
test_lifecycle (void)
{
  printf ("lifecycle\n");

  dp_cf32_t taps[3] = { { 1, 0 }, { 0, 0 }, { 0, 0 } };
  dp_fir_t *f = dp_fir_create (taps, 3);
  CHECK (f != NULL, "dp_fir_create returns non-NULL");

  dp_fir_reset (f);
  PASS ("dp_fir_reset does not crash");

  dp_fir_destroy (f);
  PASS ("dp_fir_destroy does not crash");

  dp_fir_destroy (NULL); /* must not crash */
  PASS ("dp_fir_destroy(NULL) is safe");
}

/* =========================================================================
 * Test: identity filter (single tap h=[1+0j])
 * ========================================================================= */

static void
test_identity (void)
{
  printf ("identity filter\n");

  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_cf32_t in[8] = { { 1, 2 }, { 3, 4 },   { 5, 6 }, { 7, 8 },
                      { 9, 0 }, { -1, -2 }, { 0, 1 }, { 2, 3 } };
  dp_cf32_t out[8];

  dp_fir_execute_cf32 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    ok &= cf32_near (out[i], in[i], 1e-5f);
  CHECK (ok, "identity CF32: output == input");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: delay filter (single tap at position k)
 *
 * h = [0, 1]  →  y[n] = x[n-1]
 * ========================================================================= */

static void
test_delay (void)
{
  printf ("delay filter h=[0,1]\n");

  dp_cf32_t taps[2] = { { 0, 0 }, { 1, 0 } };
  dp_fir_t *f = dp_fir_create (taps, 2);

  dp_cf32_t in[4] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
  dp_cf32_t out[4];

  dp_fir_execute_cf32 (f, in, out, 4);

  /* y[0] = x[-1] = 0, y[1]=x[0], y[2]=x[1], y[3]=x[2] */
  dp_cf32_t expect[4] = { { 0, 0 }, { 1, 2 }, { 3, 4 }, { 5, 6 } };
  int ok = 1;
  for (int i = 0; i < 4; i++)
    ok &= cf32_near (out[i], expect[i], 1e-5f);
  CHECK (ok, "delay CF32: y[n] == x[n-1]");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: state persistence across calls (delay line)
 * ========================================================================= */

static void
test_stateful (void)
{
  printf ("stateful (delay line across calls)\n");

  /* h = [0, 1]: y[n] = x[n-1] */
  dp_cf32_t taps[2] = { { 0, 0 }, { 1, 0 } };
  dp_fir_t *f = dp_fir_create (taps, 2);

  dp_cf32_t in1[2] = { { 1, 0 }, { 2, 0 } };
  dp_cf32_t in2[2] = { { 3, 0 }, { 4, 0 } };
  dp_cf32_t out1[2], out2[2];

  dp_fir_execute_cf32 (f, in1, out1, 2);
  dp_fir_execute_cf32 (f, in2, out2, 2);

  /* First call:  y[0]=0, y[1]=1 */
  /* Second call: y[0]=2 (x[-1]=in1[1]), y[1]=3 */
  CHECK (cf32_near (out1[0], (dp_cf32_t){ 0, 0 }, 1e-5f),
         "stateful: call1 y[0]==0");
  CHECK (cf32_near (out1[1], (dp_cf32_t){ 1, 0 }, 1e-5f),
         "stateful: call1 y[1]==1");
  CHECK (cf32_near (out2[0], (dp_cf32_t){ 2, 0 }, 1e-5f),
         "stateful: call2 y[0]==2 (carries over in1[1])");
  CHECK (cf32_near (out2[1], (dp_cf32_t){ 3, 0 }, 1e-5f),
         "stateful: call2 y[1]==3");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: dp_fir_reset clears delay line
 * ========================================================================= */

static void
test_reset (void)
{
  printf ("reset clears delay line\n");

  dp_cf32_t taps[2] = { { 0, 0 }, { 1, 0 } };
  dp_fir_t *f = dp_fir_create (taps, 2);

  dp_cf32_t in1[1] = { { 99, 0 } };
  dp_cf32_t out1[1];
  dp_fir_execute_cf32 (f, in1, out1, 1);

  dp_fir_reset (f); /* clear delay — forget x[-1]=99 */

  dp_cf32_t in2[1] = { { 1, 0 } };
  dp_cf32_t out2[1];
  dp_fir_execute_cf32 (f, in2, out2, 1);

  CHECK (cf32_near (out2[0], (dp_cf32_t){ 0, 0 }, 1e-5f),
         "reset: delay cleared, y[0]==0");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: CI8 upcasting — identity tap on integer input
 * ========================================================================= */

static void
test_ci8_identity (void)
{
  printf ("CI8 identity\n");

  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_ci8_t in[8] = { { 1, 2 }, { -3, 4 },  { 5, -6 },  { 127, -128 },
                     { 0, 0 }, { 10, 20 }, { -1, -1 }, { 100, 100 } };
  dp_cf32_t out[8];

  dp_fir_execute_ci8 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-5f);
    }
  CHECK (ok, "CI8 identity: out[n] == (float)in[n]");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: CI16 upcasting — identity tap
 * ========================================================================= */

static void
test_ci16_identity (void)
{
  printf ("CI16 identity\n");

  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_ci16_t in[8]
      = { { 1000, -2000 }, { 32767, -32768 }, { 0, 1 }, { -1, 0 },
          { 500, 500 },    { -500, -500 },    { 1, 1 }, { 100, -100 } };
  dp_cf32_t out[8];

  dp_fir_execute_ci16 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-5f);
    }
  CHECK (ok, "CI16 identity: out[n] == (float)in[n]");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: CI32 upcasting — identity tap
 * ========================================================================= */

static void
test_ci32_identity (void)
{
  printf ("CI32 identity\n");

  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_ci32_t in[8]
      = { { 1000000, -2000000 }, { 0, 0 },          { -1, 1 }, { 100, -100 },
          { 500000, 500000 },    { -500, -500000 }, { 1, -1 }, { 99, 77 } };
  dp_cf32_t out[8];

  dp_fir_execute_ci32 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-3f); /* float precision */
    }
  CHECK (ok, "CI32 identity: out[n] ≈ (float)in[n]");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: complex tap rotation  h = j = (0+1j)
 *   h * x = j * (r + jq) = -q + jr
 * ========================================================================= */

static void
test_complex_tap (void)
{
  printf ("complex tap h=j (90-degree rotation)\n");

  dp_cf32_t tap = { 0.0f, 1.0f }; /* j */
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_cf32_t in[4] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 3, -4 } };
  dp_cf32_t out[4];
  dp_fir_execute_cf32 (f, in, out, 4);

  /* j*(r+jq) = jr + j²q = -q + jr */
  dp_cf32_t expect[4] = { { 0, 1 }, { -1, 0 }, { -1, 1 }, { 4, 3 } };
  int ok = 1;
  for (int i = 0; i < 4; i++)
    ok &= cf32_near (out[i], expect[i], 1e-5f);
  CHECK (ok, "complex tap h=j: y[n] == j*x[n] == (-q,r)");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: multi-tap accumulation — 3-tap moving average
 *   h = [1/3, 1/3, 1/3],  real taps only
 * ========================================================================= */

static void
test_moving_average (void)
{
  printf ("3-tap moving average\n");

  float w = 1.0f / 3.0f;
  dp_cf32_t taps[3] = { { w, 0 }, { w, 0 }, { w, 0 } };
  dp_fir_t *f = dp_fir_create (taps, 3);

  /* Input: impulse at n=0 */
  dp_cf32_t in[6]
      = { { 3, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
  dp_cf32_t out[6];
  dp_fir_execute_cf32 (f, in, out, 6);

  /* Impulse response of 3-tap MA: [1, 1, 1, 0, 0, 0] */
  dp_cf32_t expect[6]
      = { { 1, 0 }, { 1, 0 }, { 1, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
  int ok = 1;
  for (int i = 0; i < 6; i++)
    ok &= cf32_near (out[i], expect[i], 1e-5f);
  CHECK (ok, "3-tap MA: impulse response [1,1,1,0,0,0]");

  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: large block (exercises SIMD path)
 * ========================================================================= */

static void
test_large_block (void)
{
  printf ("large block (1024 samples, exercises SIMD)\n");

  /* h = [1]: pure delay-0, identity */
  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_cf32_t *in = (dp_cf32_t *)malloc (1024 * sizeof (dp_cf32_t));
  dp_cf32_t *out = (dp_cf32_t *)malloc (1024 * sizeof (dp_cf32_t));

  for (int i = 0; i < 1024; i++)
    {
      in[i].i = (float)(i % 127);
      in[i].q = (float)(-(i % 63));
    }

  dp_fir_execute_cf32 (f, in, out, 1024);

  int ok = 1;
  for (int i = 0; i < 1024; i++)
    ok &= cf32_near (out[i], in[i], 1e-5f);
  CHECK (ok, "large block identity: 1024 samples match");

  free (in);
  free (out);
  dp_fir_destroy (f);
}

/* =========================================================================
 * Test: CI16 large block (exercises SIMD upcast path)
 * ========================================================================= */

static void
test_ci16_large (void)
{
  printf ("CI16 large block (SIMD upcast path)\n");

  dp_cf32_t tap = { 1.0f, 0.0f };
  dp_fir_t *f = dp_fir_create (&tap, 1);

  dp_ci16_t *in = (dp_ci16_t *)malloc (256 * sizeof (dp_ci16_t));
  dp_cf32_t *out = (dp_cf32_t *)malloc (256 * sizeof (dp_cf32_t));

  for (int i = 0; i < 256; i++)
    {
      in[i].i = (int16_t)(i * 100 - 12800);
      in[i].q = (int16_t)(-i * 50 + 6400);
    }

  dp_fir_execute_ci16 (f, in, out, 256);

  int ok = 1;
  for (int i = 0; i < 256; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-3f);
    }
  CHECK (ok, "CI16 large block: 256 samples match");

  free (in);
  free (out);
  dp_fir_destroy (f);
}

/* =========================================================================
 * Real-tap tests
 * ========================================================================= */

/* Identity: h=[1.0], real tap, CF32 signal */
static void
test_real_identity (void)
{
  printf ("real-tap identity (h=[1.0], CF32)\n");

  float tap = 1.0f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);
  CHECK (f != NULL, "dp_fir_create_real returns non-NULL");

  dp_cf32_t in[8] = { { 1, 2 }, { 3, 4 },   { 5, 6 }, { 7, 8 },
                      { 9, 0 }, { -1, -2 }, { 0, 1 }, { 2, 3 } };
  dp_cf32_t out[8];
  dp_fir_execute_real_cf32 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    ok &= cf32_near (out[i], in[i], 1e-5f);
  CHECK (ok, "real identity CF32: output == input");

  dp_fir_destroy (f);
}

/* Scale: h=[0.5] — real tap halves magnitude of both I and Q */
static void
test_real_scale (void)
{
  printf ("real-tap scale (h=[0.5])\n");

  float tap = 0.5f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);

  dp_cf32_t in[4] = { { 4, -2 }, { 0, 8 }, { -6, 6 }, { 10, 0 } };
  dp_cf32_t out[4];
  dp_fir_execute_real_cf32 (f, in, out, 4);

  int ok = 1;
  for (int i = 0; i < 4; i++)
    {
      dp_cf32_t expected = { in[i].i * 0.5f, in[i].q * 0.5f };
      ok &= cf32_near (out[i], expected, 1e-6f);
    }
  CHECK (ok, "real scale: out[n] == 0.5 * in[n]");

  dp_fir_destroy (f);
}

/* 3-tap moving average with real taps on complex signal.
 * Verify I and Q channels are averaged independently. */
static void
test_real_moving_average (void)
{
  printf ("real-tap 3-tap moving average\n");

  float w = 1.0f / 3.0f;
  float taps[3] = { w, w, w };
  dp_fir_t *f = dp_fir_create_real (taps, 3);

  /* Impulse on I, step on Q */
  dp_cf32_t in[6]
      = { { 3, 3 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
  dp_cf32_t out[6];
  dp_fir_execute_real_cf32 (f, in, out, 6);

  dp_cf32_t expect[6]
      = { { 1, 1 }, { 1, 1 }, { 1, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
  int ok = 1;
  for (int i = 0; i < 6; i++)
    ok &= cf32_near (out[i], expect[i], 1e-5f);
  CHECK (ok, "real 3-tap MA: I and Q averaged independently");

  dp_fir_destroy (f);
}

/* Stateful: delay line carries over across calls */
static void
test_real_stateful (void)
{
  printf ("real-tap stateful (delay line across calls)\n");

  /* h=[0, 1]: y[n] = x[n-1] */
  float taps[2] = { 0.0f, 1.0f };
  dp_fir_t *f = dp_fir_create_real (taps, 2);

  dp_cf32_t in1[2] = { { 1, 10 }, { 2, 20 } };
  dp_cf32_t in2[2] = { { 3, 30 }, { 4, 40 } };
  dp_cf32_t out1[2], out2[2];

  dp_fir_execute_real_cf32 (f, in1, out1, 2);
  dp_fir_execute_real_cf32 (f, in2, out2, 2);

  CHECK (cf32_near (out1[0], (dp_cf32_t){ 0, 0 }, 1e-5f),
         "real stateful: call1 y[0]==0");
  CHECK (cf32_near (out1[1], (dp_cf32_t){ 1, 10 }, 1e-5f),
         "real stateful: call1 y[1]==in[0]");
  CHECK (cf32_near (out2[0], (dp_cf32_t){ 2, 20 }, 1e-5f),
         "real stateful: call2 y[0]==in1[1] (carries over)");
  CHECK (cf32_near (out2[1], (dp_cf32_t){ 3, 30 }, 1e-5f),
         "real stateful: call2 y[1]==in2[0]");

  dp_fir_destroy (f);
}

/* Real-tap vs complex-tap equivalence:
 * dp_fir_create_real(h) must equal dp_fir_create with {h,0} taps. */
static void
test_real_vs_complex_equiv (void)
{
  printf ("real-tap vs complex-tap equivalence\n");

  float rtaps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
  dp_cf32_t ctaps[5]
      = { { 0.1f, 0 }, { 0.2f, 0 }, { 0.4f, 0 }, { 0.2f, 0 }, { 0.1f, 0 } };

  dp_fir_t *rf = dp_fir_create_real (rtaps, 5);
  dp_fir_t *cf = dp_fir_create (ctaps, 5);

  dp_cf32_t in[16];
  for (int i = 0; i < 16; i++)
    {
      in[i].i = (float)(i * 3 - 20);
      in[i].q = (float)(-(i * 2) + 15);
    }

  dp_cf32_t rout[16], cout[16];
  dp_fir_execute_real_cf32 (rf, in, rout, 16);
  dp_fir_execute_cf32 (cf, in, cout, 16);

  int ok = 1;
  for (int i = 0; i < 16; i++)
    ok &= cf32_near (rout[i], cout[i], 1e-4f);
  CHECK (ok, "real-tap == complex-tap with zero-imag coefficients");

  dp_fir_destroy (rf);
  dp_fir_destroy (cf);
}

/* CI8 input through real-tap filter */
static void
test_real_ci8 (void)
{
  printf ("real-tap CI8 identity\n");

  float tap = 1.0f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);

  dp_ci8_t in[8] = { { 1, 2 }, { -3, 4 },  { 5, -6 },  { 127, -128 },
                     { 0, 0 }, { 10, 20 }, { -1, -1 }, { 100, 100 } };
  dp_cf32_t out[8];
  dp_fir_execute_real_ci8 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-5f);
    }
  CHECK (ok, "real CI8 identity: out[n] == (float)in[n]");

  dp_fir_destroy (f);
}

/* CI16 input through real-tap filter */
static void
test_real_ci16 (void)
{
  printf ("real-tap CI16 identity\n");

  float tap = 1.0f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);

  dp_ci16_t in[8]
      = { { 1000, -2000 }, { 32767, -32768 }, { 0, 1 }, { -1, 0 },
          { 500, 500 },    { -500, -500 },    { 1, 1 }, { 100, -100 } };
  dp_cf32_t out[8];
  dp_fir_execute_real_ci16 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-5f);
    }
  CHECK (ok, "real CI16 identity: out[n] == (float)in[n]");

  dp_fir_destroy (f);
}

/* CI32 input through real-tap filter */
static void
test_real_ci32 (void)
{
  printf ("real-tap CI32 identity\n");

  float tap = 1.0f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);

  dp_ci32_t in[8]
      = { { 1000000, -2000000 }, { 0, 0 },          { -1, 1 }, { 100, -100 },
          { 500000, 500000 },    { -500, -500000 }, { 1, -1 }, { 99, 77 } };
  dp_cf32_t out[8];
  dp_fir_execute_real_ci32 (f, in, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      dp_cf32_t expected = { (float)in[i].i, (float)in[i].q };
      ok &= cf32_near (out[i], expected, 1e-3f);
    }
  CHECK (ok, "real CI32 identity: out[n] ≈ (float)in[n]");

  dp_fir_destroy (f);
}

/* Large block: exercises SIMD path, real taps */
static void
test_real_large_block (void)
{
  printf ("real-tap large block (1024 samples, SIMD)\n");

  float tap = 1.0f;
  dp_fir_t *f = dp_fir_create_real (&tap, 1);

  dp_cf32_t *in = (dp_cf32_t *)malloc (1024 * sizeof (dp_cf32_t));
  dp_cf32_t *out = (dp_cf32_t *)malloc (1024 * sizeof (dp_cf32_t));

  for (int i = 0; i < 1024; i++)
    {
      in[i].i = (float)(i % 127);
      in[i].q = (float)(-(i % 63));
    }

  dp_fir_execute_real_cf32 (f, in, out, 1024);

  int ok = 1;
  for (int i = 0; i < 1024; i++)
    ok &= cf32_near (out[i], in[i], 1e-5f);
  CHECK (ok, "real large block identity: 1024 samples match");

  free (in);
  free (out);
  dp_fir_destroy (f);
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== FIR unit tests ===\n");

  test_lifecycle ();
  test_identity ();
  test_delay ();
  test_stateful ();
  test_reset ();
  test_ci8_identity ();
  test_ci16_identity ();
  test_ci32_identity ();
  test_complex_tap ();
  test_moving_average ();
  test_large_block ();
  test_ci16_large ();

  printf ("\n--- real-tap tests ---\n");
  test_real_identity ();
  test_real_scale ();
  test_real_moving_average ();
  test_real_stateful ();
  test_real_vs_complex_equiv ();
  test_real_ci8 ();
  test_real_ci16 ();
  test_real_ci32 ();
  test_real_large_block ();

  printf ("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
