/**
 * @file lockdet_verify.c
 * @brief Monte-Carlo validation: lockdet's verify counts compound the way
 *        the header says, and its declare latency is det_verify_delay().
 *
 * lockdet_core.h makes two probabilistic claims that nothing executed:
 *
 *   "at per-look false-alarm rate p the false-declare rate is p^n_up"
 *   "predict the declare latency with det_verify_delay()"
 *
 * Both are the contract the carrier and timing lock indicators are sized
 * against -- det_verify_count() picks n_up FROM the first, and the second is
 * the number a receiver reports so a caller knows how long the lamp takes to
 * light. A decision rule whose compounding is off by a factor turns a
 * 1e-5 budget into something else entirely, silently, because a lock
 * detector that declares too eagerly still looks like a working detector.
 *
 * Method. Feed the detector pure noise -- a real Gaussian look stream from
 * the shipped **awgn** generator, not a private one -- with the threshold
 * placed at det_q_inv(p) sigmas so the per-look hit probability is exactly
 * the p the claim is stated in. Then measure:
 *
 *   - the false-declare RATE: declares per look, against the exact
 *     p^n_up (1-p)/(1-p^n_up) -- p^n_up alone is its p -> 0 limit;
 *   - the declare LATENCY: mean looks to the first declare, against
 *     det_verify_delay(p, n_up).
 *
 * One noise stream answers both, because they are the same process seen two
 * ways. The detector is reset after each declare so every trial measures a
 * fresh run rather than the sticky flag.
 *
 * Why awgn rather than a local Box-Muller: this file's whole subject is
 * whether a rate comes out right, and a private generator with a slightly
 * wrong variance moves that rate without failing anything. doppler ships one
 * noise source whose amplitude is documented as the per-component standard
 * deviation; a harness that measures probabilities has no business rolling
 * its own. (native/tests/ was consolidated onto dp_rng_test.h for exactly
 * this reason; native/validation/ was not, and a hand-rolled cgauss here is
 * what put a 3 dB error into carrier_nda_lock.c's first pass.)
 *
 * Usage:
 *   lockdet_verify            full table over (p, n_up)
 *   lockdet_verify --check    fast CI gate on the default cell
 */
#include "awgn/awgn_core.h"
#include "detection/detection_core.h"
#include "lockdet/lockdet_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLK 4096

/* Measure declares-per-look and mean-looks-to-declare on a noise-only
 * stream whose per-look hit probability is `p`.
 *
 * The threshold is det_q_inv(p) * sigma with sigma = 1, so "hit" means the
 * look landed in the upper-p tail of the very distribution awgn is drawing
 * from -- the per-look rate is set by construction rather than tuned. */
static void
measure (double p, uint32_t n_up, size_t looks, uint64_t seed,
         double *rate_out, double *latency_out)
{
  double         thr = det_q_inv (p);
  awgn_state_t  *g   = awgn_create (seed, 1.0f); /* per-component sd = 1 */
  float complex *buf = malloc (BLK * sizeof *buf);
  if (!g || !buf)
    {
      *rate_out = *latency_out = -1.0;
      free (buf);
      awgn_destroy (g);
      return;
    }
  lockdet_state_t d;
  memset (&d, 0, sizeof d);
  /* n_down = 1 is irrelevant here: the detector is reset on every declare,
     so it never spends a look in the locked state. */
  lockdet_init (&d, thr, thr, n_up, 1);

  size_t declares = 0, run = 0, run_total = 0;
  for (size_t done = 0; done < looks;)
    {
      size_t n = awgn_generate (g, BLK, buf, BLK);
      for (size_t i = 0; i < n && done < looks; i++, done++)
        {
          run++;
          /* One real Gaussian look. Taking the real part is a choice of
             rail, not a scaling: awgn's amplitude IS the per-component sd,
             so this look is N(0, 1) exactly. */
          if (lockdet_step (&d, (double)crealf (buf[i])))
            {
              declares++;
              run_total += run;
              run = 0;
              lockdet_reset (&d);
            }
        }
    }
  *rate_out    = (double)declares / (double)looks;
  *latency_out = declares ? (double)run_total / (double)declares : 0.0;
  free (buf);
  awgn_destroy (g);
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);

  if (check)
    {
      /* One cell, enough looks for ~5% precision on the rate. p = 0.2 and
         n_up = 3 gives p^3 = 8e-3, so 4e5 looks yield ~3200 declares. */
      double rate, lat;
      measure (0.2, 3, 400000u, 20260813u, &rate, &lat);
      /* The EXACT declare rate is the reciprocal of the mean waiting time,
         p^n (1-p)/(1-p^n) -- not p^n, which the header states and which is
         its p -> 0 limit. At p = 0.2 the two differ by 19%, so asserting
         p^n here would fail on correct code. */
      double want_lat  = det_verify_delay (0.2, 3);
      double want_rate = 1.0 / want_lat;
      int    fail      = 0;
      if (!(fabs (rate - want_rate) < 0.12 * want_rate))
        {
          printf ("FAIL rate %.5f want %.5f (1/det_verify_delay)\n", rate,
                  want_rate);
          fail = 1;
        }
      if (!(fabs (lat - want_lat) < 0.12 * want_lat))
        {
          printf ("FAIL latency %.1f want %.1f (det_verify_delay)\n", lat,
                  want_lat);
          fail = 1;
        }
      if (!fail)
        printf ("lockdet_verify: OK (rate %.5f vs %.5f, latency %.1f vs "
                "%.1f)\n",
                rate, want_rate, lat, want_lat);
      return fail;
    }

  const double   ps[]   = { 0.5, 0.3, 0.2, 0.1 };
  const uint32_t nups[] = { 1, 2, 3, 4 };
  printf ("lockdet verify-count compounding, noise-only looks from awgn\n");
  printf (
      "threshold = det_q_inv(p) sigma, so the per-look hit rate IS p.\n\n");
  printf ("    p   n_up      p^n_up       exact   measured   approx err   "
          "latency   det_verify_delay\n");
  printf ("  ---   ----   ---------   ---------   --------   ----------   "
          "--------   ----------------\n");
  for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
    for (size_t j = 0; j < sizeof (nups) / sizeof (nups[0]); j++)
      {
        double rate, lat;
        double want = pow (ps[i], (double)nups[j]);
        /* Scale the run so every cell collects a comparable number of
           declares rather than a comparable number of looks. */
        size_t looks = (size_t)(3000.0 / want);
        if (looks > 40000000u)
          looks = 40000000u;
        measure (ps[i], nups[j], looks, 991u + (uint64_t)(i * 16 + j), &rate,
                 &lat);
        double delay = det_verify_delay (ps[i], (int)nups[j]);
        double exact = 1.0 / delay;
        printf ("  %.1f   %4u   %9.6f   %9.6f   %8.6f   %+9.1f%%   "
                "%8.1f   %16.1f\n",
                ps[i], nups[j], want, exact, rate,
                100.0 * (want - exact) / exact, lat, delay);
      }
  return 0;
}
