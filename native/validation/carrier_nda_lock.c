/**
 * @file carrier_nda_lock.c
 * @brief Monte-Carlo characterisation: the H1 mean of the NDA carrier lock
 *        statistic vs Es/N0 and M, which is what sizes its lock detector.
 *
 * `carrier_nda_disc()`'s lock output is `Re((z/|z|)^M)`. Under H0 its law is
 * known exactly and is M-independent -- zero mean, variance 1/2, because the
 * limited sample's phase is uniform (carrier_nda_core.h). Under H1 the mean
 * is NOT known in closed form here: it is the coherent gain the M-th power
 * retains once noise has rotated the limited sample, and it falls with M and
 * with decreasing Es/N0.
 *
 * That H1 mean is the one input det_dwell_gauss() / det_threshold_gauss()
 * need and the one thing not already in the tree. symsync's equivalent is a
 * fitted expression in its own `mean` line (symsync_core.c); this harness is
 * where the carrier's comes from, measured against the SHIPPED discriminator
 * rather than a re-derivation of it.
 *
 * Geometry note: symbols are generated on the **0-grid** (angles 2*pi*k/M
 * with no constellation offset), because that is what the discriminator sees
 * AT LOCK. The NDA loop's stable points are the 0-grid; the pi/4 QPSK offset
 * is applied to the symbol afterwards, by the receiver's `sym_rot`, not to
 * the discriminator's input. Generating the offset constellation here would
 * measure Re(z^M) at the loop's UNSTABLE point and report -1 at lock.
 *
 * Usage:
 *   carrier_nda_lock            full sweep, prints the table
 *   carrier_nda_lock --check    fast CI gate: the H0 law and the H1 shape
 */
#include "awgn/awgn_core.h"
#include "carrier_nda/carrier_nda_core.h"
#include "detection/detection_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NBLK 4096

/* Mean and variance of the lock statistic over `n` unit-amplitude symbols at
 * `esno_db`. `signal = 0` measures H0 (pure noise, no symbol at all).
 *
 * Noise comes from the SHIPPED generator at the SHIPPED amplitude:
 * awgn_amplitude_for_snr(esno_db, 1.0) is the per-component sigma for unit
 * symbol energy, and awgn_create() takes exactly that. Deriving it here
 * instead is what put a 3 dB error in this file's first pass -- the helper
 * exists precisely so the "is amplitude per-rail or total power" question
 * is answered once, in one place, by the code that owns the convention.
 *
 * The symbol index is a deterministic cycle rather than a random draw, and
 * that is not laziness: the M-th power STRIPS the modulation, so at lock
 * every constellation point on the 0-grid raises to the same +1. The
 * statistic is data-independent by construction -- which is the property
 * that makes this an NDA detector at all -- so cycling all M indices
 * uniformly measures exactly what a random stream would, with no second
 * PRNG to seed, scale or get wrong. */
static void
measure (int m, double esno_db, int signal, size_t n, uint64_t seed,
         double *mean_out, double *var_out)
{
  float amp = signal ? awgn_amplitude_for_snr ((float)esno_db, 1.0f) : 1.0f;
  awgn_state_t *g = awgn_create (seed, amp);
  float complex buf[NBLK];
  double        s1 = 0.0, s2 = 0.0;
  if (!g)
    {
      *mean_out = *var_out = -1.0;
      return;
    }
  for (size_t done = 0; done < n;)
    {
      size_t got = awgn_generate (g, NBLK, buf, NBLK);
      for (size_t i = 0; i < got && done < n; i++, done++)
        {
          float complex z = buf[i];
          if (signal)
            {
              /* 0-grid constellation point: what the loop locks TO. */
              double th = 2.0 * M_PI * (double)(done % (size_t)m) / (double)m;
              z += (float)cos (th) + (float)sin (th) * I;
            }
          double pe, lk;
          carrier_nda_disc (z, m, &pe, &lk);
          s1 += lk;
          s2 += lk * lk;
        }
    }
  double mean = s1 / (double)n;
  *mean_out   = mean;
  *var_out    = s2 / (double)n - mean * mean;
  awgn_destroy (g);
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int ms[3] = { 2, 4, 8 };

  if (check)
    {
      int fail = 0;
      /* H0 is the analytic anchor: zero mean and variance 1/2 at EVERY M.
         CARRIER_NDA_LOCK_NORM_SD is derived from that 1/2, so if this
         drifts the whole threshold chain is wrong. */
      for (int i = 0; i < 3; i++)
        {
          double mu, var;
          measure (ms[i], 0.0, 0, 400000u, 12345u + (uint32_t)i, &mu, &var);
          if (fabs (mu) > 0.01 || fabs (var - 0.5) > 0.01)
            {
              printf (
                  "FAIL m=%d H0: mean %.4f (want 0), var %.4f (want 0.5)\n",
                  ms[i], mu, var);
              fail = 1;
            }
        }
      /* H1 shape: the mean must be positive, rise with Es/N0, and fall with
         M at a fixed Es/N0. A detector sized on a mean that did any of those
         backwards would be sized backwards. */
      for (int i = 0; i < 3; i++)
        {
          double lo_mu, hi_mu, v;
          measure (ms[i], 4.0, 1, 200000u, 777u + (uint32_t)i, &lo_mu, &v);
          measure (ms[i], 20.0, 1, 200000u, 778u + (uint32_t)i, &hi_mu, &v);
          if (!(lo_mu > 0.0) || !(hi_mu > lo_mu))
            {
              printf ("FAIL m=%d H1: mean %.4f @4dB, %.4f @20dB\n", ms[i],
                      lo_mu, hi_mu);
              fail = 1;
            }
        }
      {
        double m2, m8, v;
        measure (2, 4.0, 1, 200000u, 4242u, &m2, &v);
        measure (8, 4.0, 1, 200000u, 4243u, &m8, &v);
        if (!(m2 > m8))
          {
            printf ("FAIL: mean should fall with M (%.4f vs %.4f)\n", m2, m8);
            fail = 1;
          }
      }
      printf (check && !fail ? "carrier_nda_lock: OK\n" : "");
      return fail;
    }

  /* Full sweep: the H1 mean, and what it costs to detect at the receiver's
     default indicator spec. */
  const double esnos[] = { 0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 14.0, 20.0 };
  const double pd = 0.99, pfa = 1e-5;
  printf ("NDA carrier lock statistic Re((z/|z|)^M), 0-grid symbols\n");
  printf ("H0: mean 0, var 1/2 at every M (analytic).\n");
  printf ("Dwell/threshold from det_dwell_gauss/det_threshold_gauss at "
          "pd=%.2f pfa=%.0e, var=0.5.\n\n",
          pd, pfa);
  printf ("  M   Es/N0    H1 mean    H1 var     dwell   thresh   "
          "declare (syms)\n");
  printf ("  --  ------   --------   --------   -----   ------   "
          "--------------\n");
  for (int i = 0; i < 3; i++)
    {
      for (size_t j = 0; j < sizeof (esnos) / sizeof (esnos[0]); j++)
        {
          double mu, var;
          measure (ms[i], esnos[j], 1, 2000000u, 31u + (uint32_t)(i * 100 + j),
                   &mu, &var);
          int    dwell = det_dwell_gauss (mu, 0.5, pd, pfa);
          double thr   = det_threshold_gauss (mu, pd, pfa);
          /* One decision per dwell-block; n_up = 1 as symsync defaults. */
          double lat = (dwell > 0) ? (double)dwell : 0.0;
          if (dwell > 0)
            printf ("  %d   %5.1f    %8.4f   %8.4f   %5d   %6.4f   %10.0f\n",
                    ms[i], esnos[j], mu, var, dwell, thr, lat);
          else
            printf ("  %d   %5.1f    %8.4f   %8.4f       -        -   "
                    "unreachable\n",
                    ms[i], esnos[j], mu, var);
        }
      printf ("\n");
    }
  return 0;
}
