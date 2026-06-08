/**
 * @file test_hbdecim_core.c
 * @brief Unit tests for hbdecim_state_t.
 *
 * Mirrors c/tests/test_hbdecim.c (cf32 section only).
 *
 * Test coefficients: 4-tap FIR branch from the 7-point sinc prototype
 * with phases=2.  N=4 is even so fir_on_even=1.
 *
 *   Prototype h[n], n=0..6, centre=3:
 *     h[3]=0.5, h[1]=h[5]=0, h[0]=h[6]≈-0.1061, h[2]=h[4]≈0.3183
 *   bank[0] = g[0,2,4,6] with g=2h → [-0.2122, 0.6366, 0.6366, -0.2122]
 *   bank[1] = [0, 1.0, 0, 0]  (delay branch)
 *
 * FIR branch = bank[0]; pass this to hbdecim_create.
 */

#include "hbdecim/hbdecim_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* Minimal test harness                                                */
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

/* ================================================================== */
/* Reference 4-tap FIR branch (even-length, symmetric)                */
/* ================================================================== */

#define N_TAPS 4

static const float H4_FIR[N_TAPS] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

/* ================================================================== */
/* Helper: complex sinusoidal tone                                     */
/* ================================================================== */

static void
tone (float _Complex *out, size_t n, double freq)
{
  for (size_t k = 0; k < n; k++)
    {
      double ph = 2.0 * M_PI * freq * (double)k;
      out[k]    = CMPLXF ((float)cos (ph), (float)sin (ph));
    }
}

/* ================================================================== */
/* Helper: RMS power in dB                                             */
/* ================================================================== */

static double
rms_db (const float _Complex *x, size_t n)
{
  double s = 0.0;
  for (size_t k = 0; k < n; k++)
    s += (double)crealf (x[k]) * crealf (x[k])
         + (double)cimagf (x[k]) * cimagf (x[k]);
  return 10.0 * log10 (s / (double)n + 1e-20);
}

/* ================================================================== */
/* Tests                                                               */
/* ================================================================== */

static void
test_create_destroy (void)
{
  printf ("\n-- Lifecycle --\n");

  hbdecim_state_t *r = hbdecim_create (N_TAPS, H4_FIR);
  CHECK (r != NULL, "create returns non-NULL");
  CHECK (hbdecim_get_num_taps (r) == N_TAPS, "num_taps reported");
  CHECK (hbdecim_get_rate (r) == 0.5, "rate is 0.5");
  hbdecim_destroy (r);
  CHECK (1, "destroy does not crash");

  hbdecim_destroy (NULL);
  CHECK (1, "destroy(NULL) is safe");

  /* NULL h must be rejected */
  hbdecim_state_t *bad = hbdecim_create (N_TAPS, NULL);
  CHECK (bad == NULL, "create rejects NULL h");

  /* zero num_taps must be rejected */
  bad = hbdecim_create (0, H4_FIR);
  CHECK (bad == NULL, "create rejects zero num_taps");
}

static void
test_output_length (void)
{
  printf ("\n-- Output length --\n");

  hbdecim_state_t *r = hbdecim_create (N_TAPS, H4_FIR);

  float _Complex in[512] = { 0 };
  float _Complex out[300];

  size_t n = hbdecim_execute (r, in, 512, out, 300);
  CHECK (n == 256, "512 in -> 256 out");

  hbdecim_reset (r);

  /* Odd-length: 511 in → 255 out, 1 pending */
  n = hbdecim_execute (r, in, 511, out, 300);
  CHECK (n == 255, "511 in -> 255 out (1 pending)");

  /* Next: 1 in → 1 out (consumes pending) */
  float _Complex one = CMPLXF (0.0f, 0.0f);
  n                  = hbdecim_execute (r, &one, 1, out, 300);
  CHECK (n == 1, "1 in -> 1 out after pending");

  hbdecim_destroy (r);
}

static void
test_stateful (void)
{
  printf ("\n-- Statefulness --\n");

  hbdecim_state_t *r1 = hbdecim_create (N_TAPS, H4_FIR);
  hbdecim_state_t *r2 = hbdecim_create (N_TAPS, H4_FIR);

  float _Complex in[512];
  tone (in, 512, 0.1);

  float _Complex full[260], half_a[140], half_b[140];
  size_t nf = hbdecim_execute (r1, in, 512, full, 260);
  size_t na = hbdecim_execute (r2, in, 256, half_a, 140);
  size_t nb = hbdecim_execute (r2, in + 256, 256, half_b, 140);

  CHECK (nf == na + nb, "two half-blocks == one full block (count)");

  int    match = 1;
  size_t check = (nf < 10) ? nf : 10;
  for (size_t k = 0; k < check; k++)
    {
      float ref_i = (k < na) ? crealf (half_a[k]) : crealf (half_b[k - na]);
      float ref_q = (k < na) ? cimagf (half_a[k]) : cimagf (half_b[k - na]);
      if (fabsf (crealf (full[k]) - ref_i) > 1e-5f
          || fabsf (cimagf (full[k]) - ref_q) > 1e-5f)
        {
          match = 0;
          break;
        }
    }
  CHECK (match, "half-block outputs match full-block outputs");

  hbdecim_destroy (r1);
  hbdecim_destroy (r2);
}

static void
test_reset (void)
{
  printf ("\n-- Reset --\n");

  hbdecim_state_t *r = hbdecim_create (N_TAPS, H4_FIR);

  float _Complex in[64], out1[40], out2[40];
  tone (in, 64, 0.05);

  size_t n1 = hbdecim_execute (r, in, 64, out1, 40);
  hbdecim_reset (r);
  size_t n2 = hbdecim_execute (r, in, 64, out2, 40);

  CHECK (n1 == n2, "reset: same output count");

  int    match = 1;
  size_t check = (n1 < 5) ? n1 : 5;
  for (size_t k = 0; k < check; k++)
    {
      if (fabsf (crealf (out1[k]) - crealf (out2[k])) > 1e-5f
          || fabsf (cimagf (out1[k]) - cimagf (out2[k])) > 1e-5f)
        {
          match = 0;
          break;
        }
    }
  CHECK (match, "reset: outputs reproduce");

  hbdecim_destroy (r);
}

static void
test_dc_passthrough (void)
{
  printf ("\n-- Spectral: DC pass-through --\n");

  hbdecim_state_t *r = hbdecim_create (N_TAPS, H4_FIR);

  float _Complex in[2048], out[1040];
  for (size_t k = 0; k < 2048; k++)
    in[k] = CMPLXF (1.0f, 0.0f);

  size_t n = hbdecim_execute (r, in, 2048, out, 1040);

  /* Skip initial transient; the 4-tap prototype has modest gain
   * (~-0.7 dB); allow ±2 dB.                                     */
  size_t skip = N_TAPS;
  if (skip >= n)
    skip = 0;
  double pwr = rms_db (out + skip, n - skip);
  CHECK (pwr > -2.0 && pwr < 2.0, "DC power near 0 dBFS");

  hbdecim_destroy (r);
}

static void
test_alias_rejection (void)
{
  printf ("\n-- Spectral: alias rejection --\n");

  hbdecim_state_t *r = hbdecim_create (N_TAPS, H4_FIR);

  float _Complex pass_in[2048], stop_in[2048];
  float _Complex out_p[1040], out_s[1040];

  tone (pass_in, 2048, 0.05); /* well inside passband          */
  tone (stop_in, 2048, 0.45); /* near Nyquist, deep stopband   */

  size_t np = hbdecim_execute (r, pass_in, 2048, out_p, 1040);
  hbdecim_reset (r);
  size_t ns = hbdecim_execute (r, stop_in, 2048, out_s, 1040);

  size_t skip  = N_TAPS;
  size_t use_p = (np > skip) ? np - skip : 1;
  size_t use_s = (ns > skip) ? ns - skip : 1;
  double pwr_p = rms_db (out_p + skip, use_p);
  double pwr_s = rms_db (out_s + skip, use_s);

  /* 4-tap prototype is modest; require ≥ 6 dB attenuation.
   * Production quality is validated by the Python spectral tests
   * which use kaiser_prototype(phases=2) coefficients.           */
  CHECK (pwr_p - pwr_s > 6.0, "stopband tone attenuated > 6 dB");

  hbdecim_destroy (r);
}

/* ================================================================== */
/* main                                                                */
/* ================================================================== */

int
main (void)
{
  printf ("=== hbdecim_core unit tests ===\n");

  test_create_destroy ();
  test_output_length ();
  test_stateful ();
  test_reset ();
  test_dc_passthrough ();
  test_alias_rejection ();

  printf ("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
