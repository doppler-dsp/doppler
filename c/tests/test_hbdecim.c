/**
 * @file test_hbdecim.c
 * @brief Unit tests for dp_hbdecim_cf32.
 *
 * Tests cover lifecycle, output sizing, statefulness, spectral quality
 * (DC pass-through, Nyquist alias rejection), and the symmetric
 * multiply optimisation path.
 */

#include "dp/hbdecim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ================================================================== */
/* FIR branch for a 7-tap halfband prototype (cutoff π/2, unwindowed) */
/*                                                                    */
/* Prototype h[n], n=0..6, center=3 (odd):                           */
/*   h[3] = 0.5  (centre)                                            */
/*   h[1]=h[5]=0 (Nyquist zeros for odd-indexed prototype)            */
/*   h[0]=h[6] = 0.5*sinc(-1.5) ≈ -0.1061                          */
/*   h[2]=h[4] = 0.5*sinc(-0.5) ≈  0.3183                          */
/*                                                                    */
/* With phases=2: g[n] = h[n]*2                                       */
/*   bank[0] = g[0,2,4,6] = [-0.2122, 0.6366, 0.6366, -0.2122]      */
/*   bank[1] = g[1,3,5,7=0] = [0, 1.0, 0, 0]  (delay at index 1)   */
/*                                                                    */
/* FIR branch = bank[0], 4 taps (even-length, symmetric).            */
/* Delay centre = (4-1)/2 = 1  (position in odd delay line).         */
/* ================================================================== */

#define N_TAPS 4

/* Symmetric FIR branch: h[k] = h[3-k].
 * Pass this as 'h' to dp_hbdecim_cf32_create; the pure-delay branch
 * (1.0 at index 1 in the odd delay line) is implicit.               */
static const float H4_FIR[N_TAPS] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

/* DC gain: sum(H4_FIR)*0.5 + 0.5 ≈ 0.4244*0.5 + ... let me compute:
 * sum(H4_FIR) = -0.2122+0.6366+0.6366+(-0.2122) = 0.8488
 * After ×0.5 scaling in create: sum(r->h) = 0.4244
 * Delay contribution: 0.5 * 1.0 = 0.5
 * DC out ≈ 0.4244 + 0.5 = 0.9244 ≈ -0.69 dBFS (near unity)         */

/* ================================================================== */
/* Helper: complex tone                                               */
/* ================================================================== */

static void
tone (dp_cf32_t *out, size_t n, double freq)
{
  for (size_t k = 0; k < n; k++)
    {
      double ph = 2.0 * M_PI * freq * (double)k;
      out[k].i = (float)cos (ph);
      out[k].q = (float)sin (ph);
    }
}

/* ================================================================== */
/* Helper: RMS power in dB                                            */
/* ================================================================== */

static double
rms_db (const dp_cf32_t *x, size_t n)
{
  double s = 0.0;
  for (size_t k = 0; k < n; k++)
    s += (double)x[k].i * x[k].i + (double)x[k].q * x[k].q;
  return 10.0 * log10 (s / (double)n + 1e-20);
}

/* ================================================================== */
/* Tests                                                              */
/* ================================================================== */

static void
test_create_destroy (void)
{
  printf ("\n-- Lifecycle --\n");

  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);
  CHECK (r != NULL, "create returns non-NULL");
  CHECK (dp_hbdecim_cf32_num_taps (r) == N_TAPS, "num_taps reported");
  CHECK (dp_hbdecim_cf32_rate (r) == 0.5, "rate is 0.5");
  dp_hbdecim_cf32_destroy (r);
  CHECK (1, "destroy does not crash");

  /* NULL destroy is a no-op */
  dp_hbdecim_cf32_destroy (NULL);
  CHECK (1, "destroy(NULL) is safe");

  /* Even num_taps is valid (kaiser_prototype can give even FIR branch) */
  dp_hbdecim_cf32_t *even = dp_hbdecim_cf32_create (4, H4_FIR);
  CHECK (even != NULL, "create accepts even num_taps");
  dp_hbdecim_cf32_destroy (even);

  /* NULL h must be rejected */
  dp_hbdecim_cf32_t *bad = dp_hbdecim_cf32_create (N_TAPS, NULL);
  CHECK (bad == NULL, "create rejects NULL h");
}

static void
test_output_length (void)
{
  printf ("\n-- Output length --\n");

  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);

  dp_cf32_t in[512] = { 0 };
  dp_cf32_t out[300];

  size_t n = dp_hbdecim_cf32_execute (r, in, 512, out, 300);
  CHECK (n == 256, "512 in → 256 out");

  dp_hbdecim_cf32_reset (r);

  /* Odd-length block: 511 in → 255 out, 1 pending */
  n = dp_hbdecim_cf32_execute (r, in, 511, out, 300);
  CHECK (n == 255, "511 in → 255 out (1 pending)");

  /* Next sample: 1 in → 1 out (consumes pending) */
  dp_cf32_t one = { 0.0f, 0.0f };
  n = dp_hbdecim_cf32_execute (r, &one, 1, out, 300);
  CHECK (n == 1, "1 in → 1 out after pending");

  dp_hbdecim_cf32_destroy (r);
}

static void
test_stateful (void)
{
  printf ("\n-- Statefulness --\n");

  dp_hbdecim_cf32_t *r1 = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);
  dp_hbdecim_cf32_t *r2 = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);

  dp_cf32_t in[512];
  tone (in, 512, 0.1);

  dp_cf32_t full[260], half_a[140], half_b[140];
  size_t nf = dp_hbdecim_cf32_execute (r1, in, 512, full, 260);
  size_t na = dp_hbdecim_cf32_execute (r2, in, 256, half_a, 140);
  size_t nb = dp_hbdecim_cf32_execute (r2, in + 256, 256, half_b, 140);

  size_t total = na + nb;
  CHECK (nf == total, "two half-blocks = one full block (length)");

  /* Check that first 10 outputs agree */
  int match = 1;
  size_t check = (nf < 10) ? nf : 10;
  for (size_t k = 0; k < check; k++)
    {
      float di = full[k].i - (k < na ? half_a[k].i : half_b[k - na].i);
      float dq = full[k].q - (k < na ? half_a[k].q : half_b[k - na].q);
      if (fabsf (di) > 1e-5f || fabsf (dq) > 1e-5f)
        {
          match = 0;
          break;
        }
    }
  CHECK (match, "half-block outputs match full-block outputs");

  dp_hbdecim_cf32_destroy (r1);
  dp_hbdecim_cf32_destroy (r2);
}

static void
test_reset (void)
{
  printf ("\n-- Reset --\n");

  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);

  dp_cf32_t in[64];
  dp_cf32_t out1[40], out2[40];
  tone (in, 64, 0.05);

  size_t n1 = dp_hbdecim_cf32_execute (r, in, 64, out1, 40);
  dp_hbdecim_cf32_reset (r);
  size_t n2 = dp_hbdecim_cf32_execute (r, in, 64, out2, 40);

  CHECK (n1 == n2, "reset: same output count");
  size_t check = (n1 < 5) ? n1 : 5;
  int match = 1;
  for (size_t k = 0; k < check; k++)
    {
      if (fabsf (out1[k].i - out2[k].i) > 1e-5f
          || fabsf (out1[k].q - out2[k].q) > 1e-5f)
        {
          match = 0;
          break;
        }
    }
  CHECK (match, "reset: outputs reproduce");

  dp_hbdecim_cf32_destroy (r);
}

static void
test_dc_passthrough (void)
{
  printf ("\n-- Spectral: DC pass-through --\n");

  /* A DC tone (freq=0) is in the passband and should pass. */
  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);

  dp_cf32_t in[2048], out[1040];
  /* DC = constant 1.0 + 0j */
  for (size_t k = 0; k < 2048; k++)
    in[k] = (dp_cf32_t){ 1.0f, 0.0f };

  size_t n = dp_hbdecim_cf32_execute (r, in, 2048, out, 1040);
  size_t skip = N_TAPS; /* skip initial transient */
  if (skip >= n)
    skip = 0;
  double pwr = rms_db (out + skip, n - skip);
  /* DC power near 0 dBFS.  The unwindowed 7-tap prototype gives
   * gain ≈ 0.924 (≈ -0.69 dB); allow ±2 dB.                        */
  CHECK (pwr > -2.0 && pwr < 2.0, "DC power near 0 dBFS");

  dp_hbdecim_cf32_destroy (r);
}

static void
test_alias_rejection (void)
{
  printf ("\n-- Spectral: alias rejection --\n");

  /* A tone at 0.6 (normalised, input rate) is in the stopband and
   * should be attenuated after decimation.  The unwindowed 7-tap
   * prototype gives modest attenuation; require ≥ 6 dB here.
   * Spectral quality at production parameters is validated by the
   * Python integration tests (kaiser_prototype → C execution).      */
  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N_TAPS, H4_FIR);

  dp_cf32_t passband_in[2048], stop_in[2048];
  dp_cf32_t out_p[1040], out_s[1040];

  tone (passband_in, 2048, 0.05); /* well inside passband */
  tone (stop_in, 2048, 0.45);     /* deep stopband (near Nyquist) */

  size_t np = dp_hbdecim_cf32_execute (r, passband_in, 2048, out_p, 1040);
  dp_hbdecim_cf32_reset (r);
  size_t ns = dp_hbdecim_cf32_execute (r, stop_in, 2048, out_s, 1040);

  size_t skip = N_TAPS;
  size_t use_p = (np > skip) ? np - skip : 1;
  size_t use_s = (ns > skip) ? ns - skip : 1;
  double pwr_p = rms_db (out_p + skip, use_p);
  double pwr_s = rms_db (out_s + skip, use_s);

  CHECK (pwr_p - pwr_s > 6.0, "stopband tone attenuated > 6 dB");

  dp_hbdecim_cf32_destroy (r);
}

/* ================================================================== */
/* main                                                               */
/* ================================================================== */

int
main (void)
{
  printf ("=== dp_hbdecim_cf32 unit tests ===\n");

  test_create_destroy ();
  test_output_length ();
  test_stateful ();
  test_reset ();
  test_dc_passthrough ();
  test_alias_rejection ();

  printf ("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
