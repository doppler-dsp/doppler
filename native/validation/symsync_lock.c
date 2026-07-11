/**
 * @file symsync_lock.c
 * @brief Monte-Carlo validation: SymbolSync's timing-lock detector actually
 *        hits its configured (pfa, pd) operating point.
 *
 * symsync_configure_lock()'s (avgs, threshold) sizing is a formula supplied
 * directly by a doppler user (not re-derived against a primary source --
 * see symsync_core.c's SYMSYNC_LOCK_DEFAULT_* comment). This harness is the
 * empirical check that formula actually delivers the (pfa, pd) it claims,
 * rather than trusting the derivation on faith:
 *
 *  - PFA: feed pure AWGN, recompute lock_signal independently per symbol
 *    (reading the object's public on-time/mid state directly, bypassing its
 *    own lockdet_step/locked hysteresis so this measures the *raw*
 *    per-block declare rate, not the sticky whole-run flag), block-average
 *    over the configured avgs looks, and count how often that average
 *    exceeds the configured threshold.
 *  - PD: same block-averaging over a real raised-cosine BPSK stream at
 *    exactly the esno_min design SNR.
 *
 * Prior finding (this file's origin): a first Monte Carlo pass found ZERO
 * false declares over 500,000 independent noise-only blocks against a
 * nominal pfa=1e-3 (which would predict ~500) -- the formula's "8"
 * variance-role scale factor is ~6x larger than this statistic's real
 * measured per-look variance (~1.33), so the real declare threshold sits at
 * ~5.4 true standard deviations above the noise mean instead of the ~3.09 a
 * true pfa=1e-3 needs. Both pfa and pd come out safe (fewer false
 * declares, more reliable true declares than the nominal numbers promise)
 * at the cost of larger-than-minimal avgs (declare latency, not
 * reliability). This harness's --check gate is intentionally loose (it
 * confirms the safe-margin direction holds, not the exact 500,000-trial
 * numbers above -- that scale is validation-script territory, not a CI
 * gate) so it stays fast and non-flaky.
 *
 * Usage: symsync_lock [--check]
 */
#include "symsync/symsync_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPS 4

static uint32_t
xorshift32 (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  return *st;
}

/* Unit-variance complex Gaussian (Box-Muller); mirrors the awgn module's
 * transform without pulling in a separate generator instance per block. */
static float complex
cgauss (uint32_t *st)
{
  uint32_t a   = xorshift32 (st);
  uint32_t b   = xorshift32 (st);
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1));
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

static int
prbs (uint32_t *st)
{
  return (xorshift32 (st) & 1u) ? -1 : 1;
}

/* Nyquist raised-cosine pulse (matched-filtered), unit symbol period T. */
static double
rc (double t, double beta, double T)
{
  double x = t / T;
  double s = (fabs (x) < 1e-9) ? 1.0 : sin (M_PI * x) / (M_PI * x);
  double d = 1.0 - (2.0 * beta * x) * (2.0 * beta * x);
  double c = (fabs (d) < 1e-9) ? M_PI / 4.0 : cos (M_PI * beta * x) / d;
  return s * c;
}

/* Same statistic as symsync_step_ted()'s lock_signal, recomputed
 * independently so the block average here is untouched by the object's own
 * lockdet_step/n_up/n_down hysteresis (which would make a raw pfa/pd
 * measurement whole-run-sticky rather than per-decision). */
static double
lock_signal (float complex y, float complex mid)
{
  double a = crealf (y) * crealf (y) + cimagf (y) * cimagf (y);
  double b = crealf (mid) * crealf (mid) + cimagf (mid) * cimagf (mid);
  return 2.0 * (a - b) / (a + b + 1e-12);
}

/* False-alarm rate: pure noise in, count block-average exceedances over
 * n_blocks independent avgs-symbol blocks. */
static double
measure_pfa (size_t avgs, double threshold, long n_blocks, uint32_t seed)
{
  symsync_state_t *s
      = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
  uint32_t st  = seed;
  long     got = 0, exceed = 0;
  double   block_sum = 0.0;
  size_t   block_n   = 0;
  while (got < n_blocks)
    {
      float complex x = cgauss (&st);
      float complex y;
      if (symsync_step (s, x, &y))
        {
          block_sum += lock_signal (y, s->mid);
          if (++block_n >= avgs)
            {
              if (block_sum / (double)avgs > threshold)
                exceed++;
              got++;
              block_sum = 0.0;
              block_n   = 0;
            }
        }
    }
  symsync_destroy (s);
  return (double)exceed / (double)n_blocks;
}

/* Detection probability: raised-cosine BPSK + AWGN at exactly esno_db,
 * count block-average exceedances over n_blocks independent avgs-symbol
 * blocks (skipping the first emitted symbol each chunk to clear the
 * acquisition transient). */
static double
measure_pd (size_t avgs, double threshold, double esno_db, long n_blocks)
{
  const size_t   chunk_sym = 6000;
  const size_t   n         = chunk_sym * SPS;
  const double   beta = 0.35, T = SPS, span = 8 * SPS;
  float complex *rx   = malloc (n * sizeof (*rx));
  int           *bits = malloc (chunk_sym * sizeof (*bits));

  symsync_state_t *s
      = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
  uint32_t bit_st = 999u, noise_st = 4242u;
  long     got = 0, exceed = 0;
  double   block_sum = 0.0;
  size_t   block_n   = 0;
  int      warmed_up = 0;

  while (got < n_blocks)
    {
      for (size_t i = 0; i < n; i++)
        rx[i] = 0.0f;
      for (size_t k = 0; k < chunk_sym; k++)
        {
          int b    = prbs (&bit_st);
          bits[k]  = b;
          double c = (double)k * SPS + 1.3;
          if (c + span >= (double)n)
            continue;
          long lo = (long)(c - span), hi = (long)(c + span);
          if (lo < 0)
            lo = 0;
          for (long i = lo; i <= hi && i < (long)n; i++)
            rx[i] += (float)(b * rc ((double)i - c, beta, T));
        }
      double p = 0;
      for (size_t i = 0; i < n; i++)
        p += crealf (rx[i]) * crealf (rx[i]) + cimagf (rx[i]) * cimagf (rx[i]);
      p         = sqrt (p / (double)n);
      double sd = sqrt (pow (10.0, -esno_db / 10.0)) * p;
      for (size_t i = 0; i < n; i++)
        rx[i] += (float complex) (sd / sqrt (2.0)) * cgauss (&noise_st);

      for (size_t i = 0; i < n && got < n_blocks; i++)
        {
          float complex y;
          if (symsync_step (s, rx[i], &y))
            {
              if (!warmed_up)
                {
                  warmed_up = 1;
                  continue;
                }
              block_sum += lock_signal (y, s->mid);
              if (++block_n >= avgs)
                {
                  if (block_sum / (double)avgs > threshold)
                    exceed++;
                  got++;
                  block_sum = 0.0;
                  block_n   = 0;
                }
            }
        }
    }
  free (rx);
  free (bits);
  symsync_destroy (s);
  return (double)exceed / (double)n_blocks;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);

  const double rolloff = 0.35, esno_min_db = 10.0, pfa = 1e-3, pd = 0.9;

  /* Derive (avgs, threshold) exactly as symsync_configure_lock() does, so
   * this harness stays honest about what the *shipped* defaults produce
   * (not a hand-copied constant that could drift from the real formula). */
  symsync_state_t *probe
      = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
  (void)symsync_configure_lock (probe, rolloff, esno_min_db, pfa, pd);
  size_t avgs = probe->avgs;
  double threshold
      = probe->lock.up_thresh; /* configure_lock sets up == down */
  symsync_destroy (probe);

  long pfa_blocks = check ? 5000 : 500000;
  long pd_blocks  = check ? 500 : 2000;

  printf ("SymbolSync lock detector: rolloff=%.2f esno_min=%.1fdB "
          "pfa_target=%.1e pd_target=%.2f -> avgs=%zu threshold=%.4f\n",
          rolloff, esno_min_db, pfa, pd, avgs, threshold);

  double emp_pfa = measure_pfa (avgs, threshold, pfa_blocks, 12345u);
  printf ("  empirical pfa: %ld/%ld blocks false-declared (%.2e), nominal "
          "target %.1e\n",
          (long)(emp_pfa * (double)pfa_blocks), pfa_blocks, emp_pfa, pfa);

  double emp_pd = measure_pd (avgs, threshold, esno_min_db, pd_blocks);
  printf ("  empirical pd:  %ld/%ld blocks true-declared (%.4f), nominal "
          "target %.2f\n",
          (long)(emp_pd * (double)pd_blocks), pd_blocks, emp_pd, pd);

  int fail = 0;
  if (check)
    {
      /* Loose, non-flaky bounds: confirm the safe-margin direction found
       * during development, not the exact numbers (those are a longer,
       * standalone validation run's job -- see this file's doc comment). */
      if (emp_pfa > pfa)
        fail
            = 1; /* real false-alarm rate must not exceed the nominal target */
      if (emp_pd < 0.95)
        fail = 1; /* real detection rate must comfortably clear the target */
    }
  if (fail)
    {
      fprintf (stderr,
               "SymbolSync lock detector missed its (pfa, pd) target — "
               "FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: SymbolSync's lock detector meets its configured (pfa, "
            "pd) target with margin\n");
  return 0;
}
