/**
 * @file carrier_nda_pullin.c
 * @brief Monte-Carlo validation: the NDA carrier loop acquires a carrier
 *        frequency offset with NO data and NO symbol timing, and its
 * closed-loop jitter scales with the loop bandwidth.
 *
 * The headline capability: the M-th-power discriminator strips the data and
 * runs on a fixed-rate (N×/symbol) arm integrate-and-dump, so it locks a bare
 * (unmodulated) carrier and a modulated carrier before timing is established.
 *
 * Validated, per M in {2,4,8}:
 *   - cold-start frequency pull-in range on an unmodulated carrier;
 *   - lock on modulated M-PSK data with NO symbol timing (random symbols,
 *     arm I&D not aligned to symbols);
 *   - closed-loop frequency error variance grows with bn.
 *
 * Usage:  carrier_nda_pullin [--check]
 */
#include "carrier_nda/carrier_nda_core.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

static uint32_t
xs (uint32_t *s)
{
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}
static float
gauss (uint32_t *s)
{
  double r1 = (xs (s) + 1.0) / 4294967297.0;
  double r2 = (xs (s) + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (TWOPI * r2));
}

/* Acquire an unmodulated carrier step f0? (cold start, light noise). */
static int
acq_unmod (int m, double f0, double bn, size_t n, float sigma, uint32_t seed)
{
  carrier_nda_state_t *c  = carrier_nda_create (bn, 0.707, 0.0, 8, 4, m);
  uint32_t             ns = seed;
  float complex        d;
  for (size_t k = 0; k < n; k++)
    {
      float complex x = (float complex)cexp (I * TWOPI * f0 * (double)k)
                        + sigma * gauss (&ns) + sigma * gauss (&ns) * I;
      double        pe, lk;
      d = carrier_nda_wipeoff (c, x);
      if (carrier_nda_arm_step (c, d, &pe, &lk))
        {
          c->lock += CARRIER_NDA_LOCK_ALPHA * (lk - c->lock);
          carrier_nda_steer (c, pe);
        }
    }
  int ok = fabs (carrier_nda_get_norm_freq (c) - f0) < 5e-4
           && carrier_nda_get_lock (c) > 0.5 * c->lock_scale;
  carrier_nda_destroy (c);
  return ok;
}

/* Largest acquired unmodulated-carrier step on a coarse grid. */
static double
pull_in_range (int m, double bn)
{
  double best = 0.0;
  for (double f0 = 0.0005; f0 <= 0.01001; f0 += 0.0005)
    {
      if (acq_unmod (m, f0, bn, 60000, 0.05f, 11u))
        best = f0;
      else
        break;
    }
  return best;
}

/* Lock on modulated M-PSK data with NO symbol timing; return tracked-freq err.
 */
static double
acq_moddata (int m, double f0, float sigma, double *out_lock)
{
  int                  sps  = 8;
  size_t               nsym = 8000;
  carrier_nda_state_t *c    = carrier_nda_create (0.01, 0.707, 0.0, sps, 4, m);
  uint32_t             ds = 3u, ns = 9u;
  for (size_t s = 0; s < nsym; s++)
    {
      float complex a = mpsk_constellation ((int)(xs (&ds) % (uint32_t)m), m);
      for (int i = 0; i < sps; i++)
        {
          size_t        k = s * (size_t)sps + (size_t)i;
          float complex x
              = a * (float complex)cexp (I * TWOPI * f0 * (double)k)
                + sigma * gauss (&ns) + sigma * gauss (&ns) * I;
          double        pe, lk;
          float complex d = carrier_nda_wipeoff (c, x);
          if (carrier_nda_arm_step (c, d, &pe, &lk))
            {
              c->lock += CARRIER_NDA_LOCK_ALPHA * (lk - c->lock);
              carrier_nda_steer (c, pe);
            }
        }
    }
  *out_lock  = carrier_nda_get_lock (c) / c->lock_scale; /* normalize to ~1 */
  double err = fabs (carrier_nda_get_norm_freq (c) - f0);
  carrier_nda_destroy (c);
  return err;
}

/* Closed-loop tracked-frequency variance on an unmodulated carrier, vs bn. */
static double
freq_var (int m, double bn, float sigma, size_t n)
{
  carrier_nda_state_t *c    = carrier_nda_create (bn, 0.707, 0.002, 8, 4, m);
  uint32_t             ns   = 21u;
  size_t               warm = n / 2;
  double               mu = 0, m2 = 0;
  long                 cnt = 0;
  for (size_t k = 0; k < n; k++)
    {
      float complex x = (float complex)cexp (I * TWOPI * 0.002 * (double)k)
                        + sigma * gauss (&ns) + sigma * gauss (&ns) * I;
      double        pe, lk;
      float complex d = carrier_nda_wipeoff (c, x);
      if (carrier_nda_arm_step (c, d, &pe, &lk))
        carrier_nda_steer (c, pe);
      if (k >= warm)
        {
          double f = carrier_nda_get_norm_freq (c);
          mu += f;
          m2 += f * f;
          cnt++;
        }
    }
  carrier_nda_destroy (c);
  return m2 / cnt - (mu / cnt) * (mu / cnt);
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;
  int ms[]  = { 2, 4, 8 };

  printf ("NDA cold-start pull-in (unmodulated carrier, no data/timing)\n");
  for (int i = 0; i < 3; i++)
    {
      double rng = pull_in_range (ms[i], 0.01);
      printf ("  M=%d  pull-in range >= %.4f cyc/sample\n", ms[i], rng);
      if (check && !(rng >= 0.002))
        fail = 1;
    }

  printf ("Lock on modulated data with NO symbol timing (f0=0.001)\n");
  for (int i = 0; i < 3; i++)
    {
      double lk, err = acq_moddata (ms[i], 0.001, 0.1f, &lk);
      printf ("  M=%d  freq_err=%.2e  lock=%.3f\n", ms[i], err, lk);
      if (check && !(err < 5e-4 && lk > 0.5))
        fail = 1;
    }

  printf ("Closed-loop freq jitter grows with bn (M=4, unmodulated)\n");
  double prev = 0.0;
  int    grew = 1;
  for (double bn = 0.005; bn <= 0.0201; bn *= 2)
    {
      double v = freq_var (4, bn, 0.2f, 200000);
      printf ("  bn=%.3f  var(f)=%.3e\n", bn, v);
      if (prev > 0 && !(v > prev))
        grew = 0;
      prev = v;
    }
  if (check && !grew)
    fail = 1;

  if (fail)
    {
      fprintf (stderr, "NDA pull-in/jitter deviates from theory — FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: cold-start pull-in (no data/timing) per M; jitter grows "
            "with bn\n");
  return 0;
}
