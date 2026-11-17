/**
 * @file state_filter_loop.c
 * @brief What the third-order state filter delivers, and where it stops
 *        being stable.
 *
 * The filter's gains are solved so that the exact noise bandwidth of the
 * discrete loop is `bn*t` (docs/design/loop-filter.md section 11.1). This
 * harness measures the loop that results, three independent ways, over the
 * envelope a caller meets:
 *
 *   1. Bandwidth. Parseval on the impulse response of the REAL
 *      `dp_state_filter_step` (no model of it), the exact function
 *      `dp_state_filter_loop_bandwidth`, and, as a third route that shares
 *      neither, the variance of the oscillator phase when the detector sees
 *      white Gaussian noise (`Var(phi) = 2*Bn*sigma^2`).
 *   2. Stability. The lower and upper loop-gain edges `K_low`, `K_high` at
 *      each `bn*t` and detector delay, bisected on the exact test; the
 *      stable gains at each delay counted as intervals (one is the claim);
 *      and the rectangle the automatic case advertises.
 *   3. Transients. The phase error to a frequency step and to a frequency
 *      ramp, next to the second-order loop at the same `bn`.
 *   4. The failure. The growth per update of the unstable mode just past
 *      each edge.
 *
 * ## What is gated and what is printed
 *
 * `--check` asserts what the header CLAIMS: the delivered bandwidth is the
 * one asked for at a detector delay of zero (by impulse and by noise), the
 * exact function agrees with simulation at integer delays, the advertised
 * rectangle is stable at the bound with one stable interval per delay, the
 * ramp is nulled, and the bound is tight. The sweep prints the rest: the
 * edges over the whole `bn*t` and delay map, the transient metrics and the
 * failure rates, which are findings, not promises.
 *
 * Usage:
 *   state_filter_loop           the full sweep, as tables
 *   state_filter_loop --check   the fast CI subset, asserting the claims
 *   state_filter_loop --check --emit
 *                               the same run, with its measurements as CSV
 *                               blocks for the report
 */
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/state_filter/state_filter_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIST 16

static int g_emit;

typedef double (*step_fn) (void *, double);

static double
sf_step (void *s, double e)
{
  return dp_state_filter_step ((dp_state_filter_state_t *)s, e);
}

static double
lf_step (void *s, double e)
{
  return dp_loop_filter_step ((dp_loop_filter_state_t *)s, e);
}

/* Bn = (1/2) sum phi^2 for a unit impulse into the closed loop, the detector
 * reading its error `delay` updates late (a fractional delay is linear
 * interpolation of the error between two updates). `tail` is the share of
 * the sum that landed in the last half: a converged answer is near zero. */
static double
bn_parseval (step_fn step, void *s, double delay, double gain, size_t n_max,
             double *tail)
{
  const int    k       = (int)floor (delay);
  const double f       = delay - (double)k;
  double       r[HIST] = { 0 };
  double       phi = 0.0, sum = 0.0, half = 0.0;
  for (size_t n = 0; n < n_max; n++)
    {
      if (n == n_max / 2)
        half = sum;
      for (int i = HIST - 1; i > 0; i--)
        r[i] = r[i - 1];
      r[0]           = ((n == 0) ? 1.0 : 0.0) - phi;
      const double e = (1.0 - f) * r[k] + f * r[k + 1];
      phi += step (s, gain * e);
      sum += phi * phi;
    }
  if (tail)
    *tail = sum > 0.0 ? (sum - half) / sum : 0.0;
  return 0.5 * sum;
}

/* The same loop driven by white Gaussian noise of unit variance: the
 * oscillator phase has variance 2*Bn. The third route to the bandwidth,
 * the one a physical receiver would see. Returns Var(phi) / 2. */
static double
bn_noise (dp_state_filter_state_t *sf, size_t n, uint32_t seed)
{
  uint32_t     rng = seed;
  double       phi = 0.0, sum = 0.0;
  size_t       used = 0;
  const size_t warm = 20000;
  for (size_t i = 0; i < n + warm; i++)
    {
      const double w = dp_gauss (&rng);
      phi += dp_state_filter_step (sf, w - phi);
      if (i >= warm)
        {
          sum += phi * phi;
          used++;
        }
    }
  return 0.5 * sum / (double)used;
}

static void
fresh (dp_state_filter_state_t *sf, double bnt)
{
  memset (sf, 0, sizeof *sf);
  dp_state_filter_init (sf, bnt, 1.0);
}

/* The edges of the stable interval of the loop gain, by bisection on the
 * exact test. A NaN edge means the interval does not reach that side of
 * the search range [0.01, 1000]. */
static void
gain_edges (const dp_state_filter_state_t *sf, double delay, double *lo,
            double *hi)
{
  *lo = *hi = NAN;
  if (!dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay, 1.0))
    return;
  double a = 1.0, b = 0.01;
  if (!dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay, b))
    {
      for (int i = 0; i < 40; i++)
        {
          const double m = sqrt (a * b);
          if (dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay,
                                           m))
            a = m;
          else
            b = m;
        }
      *lo = a;
    }
  a = 1.0;
  b = 1000.0;
  if (!dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay, b))
    {
      for (int i = 0; i < 40; i++)
        {
          const double m = sqrt (a * b);
          if (dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay,
                                           m))
            a = m;
          else
            b = m;
        }
      *hi = a;
    }
}

/* How many times the stability of the loop changes along a log-spaced sweep
 * of the loop gain at one delay: 2 is one stable interval (in at the lower
 * edge, out at the upper). */
static int
gain_transitions (const dp_state_filter_state_t *sf, double delay)
{
  int prev = -1, changes = 0;
  for (int j = 0; j <= 400; j++)
    {
      const double K = 0.02 * pow (5000.0, (double)j / 400.0);
      const int    st
          = dp_state_filter_loop_stable (3, sf->kp, sf->ki, sf->kr, delay, K);
      if (prev >= 0 && st != prev)
        changes++;
      prev = st;
    }
  return changes;
}

/* The phase error to a frequency step (w[n] = n) into the closed loop:
 * its peak, and the updates until it stays within 5% of that peak... of
 * its own peak for good, which for a step that must return to zero is the
 * settling time. */
static void
step_response (step_fn step, void *s, size_t n, double *peak, size_t *settle)
{
  double  phi = 0.0, pk = 0.0;
  double *e = malloc (n * sizeof *e);
  if (!e)
    exit (1);
  for (size_t i = 0; i < n; i++)
    {
      e[i] = (double)i - phi;
      phi += step (s, e[i]);
      if (fabs (e[i]) > pk)
        pk = fabs (e[i]);
    }
  size_t last = 0;
  for (size_t i = 0; i < n; i++)
    if (fabs (e[i]) > 0.05 * pk)
      last = i;
  *peak   = pk;
  *settle = last + 1;
  free (e);
}

/* The error after `n` updates of a frequency ramp of r per update per
 * update: the steady value if the loop settles. */
static double
ramp_error (step_fn step, void *s, double r, size_t n)
{
  double f = 0.0, w = 0.0, phi = 0.0, e = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      f += r;
      w += f;
      e = w - phi;
      phi += step (s, e);
    }
  return e;
}

/* Growth per update of the unstable mode at gain K: the log of the ratio of
 * the loop's peak phase over two late windows. */
static double
growth_rate (const dp_state_filter_state_t *base, double delay, double K)
{
  dp_state_filter_state_t sf      = *base;
  const int               k       = (int)floor (delay);
  const double            f       = delay - (double)k;
  double                  r[HIST] = { 0 };
  double                  phi = 0.0, pk1 = 0.0, pk2 = 0.0;
  const int               n1 = 3000, n2 = 6000;
  for (int n = 0; n < n2; n++)
    {
      for (int i = HIST - 1; i > 0; i--)
        r[i] = r[i - 1];
      r[0]           = ((n == 0) ? 1.0 : 0.0) - phi;
      const double e = (1.0 - f) * r[k] + f * r[k + 1];
      phi += dp_state_filter_step (&sf, K * e);
      if (n >= n1 / 2 && n < n1 && fabs (phi) > pk1)
        pk1 = fabs (phi);
      if (n >= n2 - n1 / 2 && fabs (phi) > pk2)
        pk2 = fabs (phi);
      if (!isfinite (phi) || fabs (phi) > 1e150)
        return NAN; /* grows too fast to measure over this window */
    }
  /* the two windows' centres are n2 - n1 updates apart */
  return (pk1 > 0.0 && pk2 > 0.0) ? log (pk2 / pk1) / (double)(n2 - n1) : NAN;
}

static void
emit_block (const char *name, const char *header)
{
  if (g_emit)
    printf ("\n#BLOCK %s\n%s\n", name, header);
}

int
main (int argc, char **argv)
{
  int check = 0;
  for (int i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--check") == 0)
        check = 1;
      else if (strcmp (argv[i], "--emit") == 0)
        g_emit = 1;
    }
  const double bnts[] = { 0.001, 0.005, 0.01, 0.02, 0.04, 0.08, 0.12 };
  const size_t nb     = sizeof bnts / sizeof *bnts;
  dp_state_filter_state_t sf;

  /* 1 ── Bandwidth ──────────────────────────────────────────────────────── */
  if (!check)
    {
      printf (
          "# Delivered noise bandwidth against bn*t, by the real step's\n"
          "# impulse response (parseval), the exact function (exact) and\n"
          "# white noise (noise); and what a detector delay does to it.\n\n");
      printf ("%9s %5s %12s %12s %12s\n", "bn*t", "delay", "parseval", "exact",
              "exact/bn*t");
    }
  emit_block ("bandwidth", "bnt,delay,parseval,exact,noise");
  for (size_t i = 0; i < nb; i++)
    for (int d = 0; d <= 2; d++)
      {
        fresh (&sf, bnts[i]);
        double       tail;
        const double ex
            = dp_state_filter_loop_bandwidth (3, sf.kp, sf.ki, sf.kr, d, 1.0);
        const size_t n_sim = (size_t)(1000.0 / bnts[i]) + 20000;
        const double pv    = bn_parseval (sf_step, &sf, d, 1.0, n_sim, &tail);
        double       noise = NAN;
        if (d == 0 && bnts[i] >= 0.01)
          {
            fresh (&sf, bnts[i]);
            noise = bn_noise (&sf, 2000000, 0x5EEDu + (uint32_t)i);
          }
        if (g_emit)
          printf ("%.6g,%d,%.9g,%.9g,%.9g\n", bnts[i], d, pv, ex, noise);
        else if (!check)
          printf ("%9.4g %5d %12.7g %12.7g %12.7f\n", bnts[i], d, pv, ex,
                  ex / bnts[i]);
        if (check)
          {
            /* Exact function against the real step, at every cell. */
            DP_CHECK (tail < 1e-8);
            DP_CHECK_NEAR (pv / ex - 1.0, 0.0, 1e-6);
            if (d == 0) /* the bandwidth asked for is the one delivered */
              DP_CHECK_NEAR (ex / bnts[i] - 1.0, 0.0, 1e-8);
            if (d == 0 && !isnan (noise))
              /* ...and by noise: 2e6 samples, relative std near 1%. */
              DP_CHECK_NEAR (noise / bnts[i] - 1.0, 0.0, 0.06);
          }
      }

  /* 2 ── Stability ──────────────────────────────────────────────────────── */
  if (!check)
    printf ("\n# The stable loop-gain interval [K_low, K_high] at each bn*t "
            "and detector delay\n# (integer delays are exact; the fractions "
            "are the interpolation model),\n# and the number of changes of "
            "stability along K (2 = one stable interval).\n\n%9s %5s %9s %9s "
            "%7s\n",
            "bn*t", "delay", "K_low", "K_high", "changes");
  emit_block ("edges", "bnt,delay,k_low,k_high,changes");
  const double dels[] = { 0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
  for (size_t i = 0; i < nb; i++)
    for (size_t j = 0; j < sizeof dels / sizeof *dels; j++)
      {
        fresh (&sf, bnts[i]);
        double lo, hi;
        gain_edges (&sf, dels[j], &lo, &hi);
        const int ch = gain_transitions (&sf, dels[j]);
        if (g_emit)
          printf ("%.6g,%.2f,%.6g,%.6g,%d\n", bnts[i], dels[j], lo, hi, ch);
        else if (!check)
          printf ("%9.4g %5.2f %9.4f %9.2f %7d\n", bnts[i], dels[j], lo, hi,
                  ch);
        if (check && dels[j] <= STATE_FILTER_DELAY_MAX)
          DP_CHECK (ch <= 2); /* one stable interval, or none */
      }

  if (check)
    {
      /* The rectangle at the bound, on a fine grid. */
      fresh (&sf, STATE_FILTER_BNT_MAX);
      int unstable = 0;
      for (int di = 0; di <= 80; di++)
        {
          const double d = STATE_FILTER_DELAY_MAX * di / 80.0;
          for (int gi = 0; gi <= 30; gi++)
            {
              const double K = STATE_FILTER_K_MIN
                               * pow (STATE_FILTER_K_MAX / STATE_FILTER_K_MIN,
                                      (double)gi / 30.0);
              unstable += !dp_state_filter_loop_stable (3, sf.kp, sf.ki, sf.kr,
                                                        d, K);
            }
        }
      DP_CHECK (unstable == 0);
      /* The bound is tight: a loop 10% wider loses the upper corner. */
      fresh (&sf, 1.1 * STATE_FILTER_BNT_MAX / 0.95);
      DP_CHECK (!dp_state_filter_loop_stable (
          3, sf.kp, sf.ki, sf.kr, STATE_FILTER_DELAY_MAX, STATE_FILTER_K_MAX));
    }

  /* 3 ── Transients ─────────────────────────────────────────────────────── */
  if (!check)
    printf ("\n# Phase error to a frequency step (peak, updates to settle to "
            "5%% of peak)\n# and the steady error to a frequency ramp, "
            "against the second-order loop at\n# the same bn "
            "(zeta = 0.707).\n\n%9s %8s %10s %8s | %10s %8s | %12s %12s\n",
            "bn*t", "order", "peak", "settle", "peak", "settle", "ramp err 2",
            "ramp err 3");
  emit_block ("transient", "bnt,peak3,settle3,peak2,settle2,rampe3,rampe2");
  for (size_t i = 0; i < nb && bnts[i] <= 0.08; i++)
    {
      const size_t n = (size_t)(60.0 / bnts[i]) + 2000;
      double       p3, p2;
      size_t       s3, s2;
      fresh (&sf, bnts[i]);
      step_response (sf_step, &sf, n, &p3, &s3);
      dp_loop_filter_state_t lf;
      memset (&lf, 0, sizeof lf);
      dp_loop_filter_init (&lf, bnts[i], 0.707, 1.0);
      step_response (lf_step, &lf, n, &p2, &s2);
      const double r = 1e-7 / (bnts[i] * bnts[i]); /* scale with the loop */
      fresh (&sf, bnts[i]);
      const double re3 = ramp_error (sf_step, &sf, r, n * 4);
      memset (&lf, 0, sizeof lf);
      dp_loop_filter_init (&lf, bnts[i], 0.707, 1.0);
      const double re2 = ramp_error (lf_step, &lf, r, n * 4);
      if (g_emit)
        printf ("%.6g,%.6g,%zu,%.6g,%zu,%.6g,%.6g\n", bnts[i], p3, s3, p2, s2,
                re3, re2);
      else if (!check)
        printf ("%9.4g %8s %10.3f %8zu | %10.3f %8zu | %12.3e %12.3e\n",
                bnts[i], "3 | 2", p3, s3, p2, s2, re2, re3);
      if (check)
        {
          /* The ramp is nulled by the third order and not by the second:
           * the claim the object exists for. */
          DP_CHECK (fabs (re3) < 1e-4 * fabs (re2));
          DP_CHECK_NEAR (
              re2 / dp_state_filter_steady_error (2, lf.ki, 0.0, 2, r) - 1.0,
              0.0, 1e-3);
        }
    }

  /* 4 ── The failure ────────────────────────────────────────────────────── */
  if (!check)
    {
      printf ("\n# Growth per update (log of the peak-phase ratio) of the "
              "unstable mode just\n# past each gain edge, at delay 1.\n\n"
              "%9s %9s %12s %9s %12s\n",
              "bn*t", "K_low*.8", "growth", "K_high*1.2", "growth");
      for (size_t i = 0; i < nb; i++)
        {
          fresh (&sf, bnts[i]);
          double lo, hi;
          gain_edges (&sf, 1.0, &lo, &hi);
          printf ("%9.4g %9.4f %12.3e %9.2f %12.3e\n", bnts[i], 0.8 * lo,
                  growth_rate (&sf, 1.0, 0.8 * lo), 1.2 * hi,
                  growth_rate (&sf, 1.0, 1.2 * hi));
        }
    }

  if (check)
    {
      DP_TEST_EMIT_END ("state_filter_loop");
    }
  return 0;
}
