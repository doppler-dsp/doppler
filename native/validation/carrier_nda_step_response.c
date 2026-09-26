/**
 * @file carrier_nda_step_response.c
 * @brief Validation: the NDA carrier loop's closed-loop frequency step
 *        response converges to the target for BOTH a constant-modulus signal
 *        and a pulse-shaped (RRC) signal.
 *
 * A carrier offset is a frequency step at t=0. Because the loop filter is a
 * type-2 (PI) integrator, the steady-state frequency error is driven to ~0
 * regardless of the discriminator gain — so the loop locks even on a
 * pulse-shaped (RRC) stream, where the raw M-th-power discriminator's coherent
 * gain (S-curve slope) collapses ~80x (Sg^4 is tiny). The collapse only slows
 * pull-in; it does not stop lock. Constant-modulus pulls in fast; RRC pulls in
 * slower with more steady-state jitter, but both settle on the target.
 *
 * Validated (noiseless step f0 = 5e-4, n = 4 arm dumps/symbol):
 *   - constant-modulus M-PSK (M in {2,4,8}): steady-state freq error -> 0;
 *   - root-raised-cosine QPSK: steady-state freq error -> 0 (it locks), and
 *     pulls in no faster than the constant-modulus case (gain collapse).
 *
 * Usage:  carrier_nda_step_response [--check]
 */
#include "dp_rng_test.h"

#include "doppler/carrier_nda/carrier_nda_core.h"
#include "doppler/dp_complex.h"
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/wfm/wfm_dsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586
#define SPS 8
#define RRC_BETA 0.35
#define RRC_SPAN 8

/* Build a noiseless M-PSK signal at SPS samples/symbol with a carrier step f0.
 * kind 0 = constant-modulus (rectangular); kind 1 = root-raised-cosine. */
static void
build_sig (int kind, int m, double f0, size_t nsym, float complex *rx,
           uint32_t seed)
{
  size_t   N  = nsym * (size_t)SPS;
  uint32_t ds = seed;
  if (kind == 0)
    {
      for (size_t s = 0; s < nsym; s++)
        {
          float complex a
              = mpsk_constellation ((int)(dp_xs32 (&ds) % (uint32_t)m), m);
          for (int i = 0; i < SPS; i++)
            rx[s * (size_t)SPS + (size_t)i] = a;
        }
    }
  else
    {
      /* wfm_rrc_taps' span is ONE-sided: RRC_SPAN symbols end to end. */
      int    ntap = (int)wfm_rrc_ntaps (SPS, RRC_SPAN / 2);
      float *h    = malloc ((size_t)ntap * sizeof (*h));
      wfm_rrc_taps (RRC_BETA, SPS, RRC_SPAN / 2, h);
      float complex *up = calloc (N, sizeof (*up));
      for (size_t s = 0; s < nsym; s++)
        up[s * (size_t)SPS]
            = mpsk_constellation ((int)(dp_xs32 (&ds) % (uint32_t)m), m);
      for (size_t k = 0; k < N; k++)
        {
          float complex acc = 0.0f;
          for (int j = 0; j < ntap; j++)
            {
              long idx = (long)k - j;
              if (idx >= 0 && (size_t)idx < N)
                acc += up[idx] * h[j];
            }
          rx[k] = acc;
        }
      free (up);
      free (h);
    }
  for (size_t k = 0; k < N; k++)
    rx[k] *= (float complex)cexp (I * TWOPI * f0 * (double)k);
}

/* Run the step and return (final freq error, settle index). Settle index is
 * the first sample where |norm_freq - f0| < 10% f0, probed every 64 samples.
 */
static void
step_response (int kind, int m, double bn, double f0, size_t nsym,
               double *ferr, size_t *settle)
{
  size_t         N  = nsym * (size_t)SPS;
  float complex *rx = malloc (N * sizeof (*rx));
  build_sig (kind, m, f0, nsym, rx, 31u);
  dp_carrier_nda_state_t *c
      = dp_carrier_nda_create (bn, 0.707, 0.0, SPS, 4, m);
  float complex o[64];
  *settle = N;
  for (size_t i = 0; i + 64 <= N; i += 64)
    {
      dp_carrier_nda_steps (c, rx + i, 64, o, 64);
      if (*settle == N
          && fabs (dp_carrier_nda_get_norm_freq (c) - f0) < 0.1 * f0)
        *settle = i;
    }
  *ferr = fabs (dp_carrier_nda_get_norm_freq (c) - f0);
  dp_carrier_nda_destroy (c);
  free (rx);
}

int
main (int argc, char **argv)
{
  int    check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int    fail  = 0;
  double f0    = 5e-4;
  double bn    = 0.002;

  printf ("NDA closed-loop frequency step response (f0=%.1e, noiseless)\n",
          f0);

  printf ("Constant-modulus M-PSK:\n");
  double ferr;
  size_t settle, settle_cm4 = 0;
  int    ms[] = { 2, 4, 8 };
  for (int i = 0; i < 3; i++)
    {
      step_response (0, ms[i], bn, f0, 8000, &ferr, &settle);
      printf ("  M=%d  steady-state err=%.2e  settle=%zu samples\n", ms[i],
              ferr, settle);
      if (ms[i] == 4)
        settle_cm4 = settle;
      if (check && !(ferr < 5e-4 && settle < (size_t)8000 * SPS))
        fail = 1;
    }

  printf ("Root-raised-cosine QPSK (the gain-collapse case):\n");
  step_response (1, 4, bn, f0, 8000, &ferr, &settle);
  printf ("  M=4  steady-state err=%.2e  settle=%zu samples\n", ferr, settle);
  /* It locks (steady-state error driven to ~0 by the type-2 integrator) ... */
  if (check && !(ferr < 1e-3))
    fail = 1;
  /* ... and pulls in no faster than constant-modulus (the slope collapse). */
  if (check && settle_cm4 > 0 && !(settle >= settle_cm4))
    fail = 1;

  if (fail)
    {
      fprintf (stderr, "NDA step response deviates — FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: loop locks on constant-modulus and RRC; RRC pulls in "
            "slower\n");
  return 0;
}
