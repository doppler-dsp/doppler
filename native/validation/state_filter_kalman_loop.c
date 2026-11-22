/**
 * @file state_filter_kalman_loop.c
 * @brief What the Kalman flavor of the third-order loop delivers, where it
 *        stops being stable, and what a larger start does.
 *
 * docs/design/loop-filter.md Part III is the design; this harness measures
 * the properties it claims, over the envelope a caller meets:
 *
 *   1. Gains. The steady-state gains at the exact bandwidth `bn*t`, against
 *      the fixed-gain flavor's Butterworth gains at the same `bn*t` (the
 *      table of section 19 of the design page: they meet as `bn*t` goes to 0
 *      and differ by a few percent at the bound).
 *   2. Stability. The lower and upper loop-gain edges of the loop at the
 *      Kalman flavor's OWN steady-state gains, bisected on the exact
 *      function, at each detector delay, and the largest `bn*t` stable at the
 *      corner `K = 2`, `delay = 2`, so the flavor's constants are derived and
 *      not inherited from the first flavor's.
 *   3. The plain covariance update. The double recursion run beside a copy in
 *      `long double` (wider than double on x86; the same width on a platform
 *      where it is not, and the output says which) from the steady state and
 *      from the largest accepted start, for 10/(bn*t) updates: the gains'
 *      worst relative departure and whether the covariance stays positive
 *      definite at every update. This is why the update is kept plain.
 *   4. The start. For starts from 1 to 100 times the steady state's
 *      frequency deviation, the gains along the covariance's whole
 *      trajectory against the stability rectangle (the rule the constructor
 *      enforces), and the closed loop's response to a frequency step: peak
 *      error and settling against the fixed-gain loop, at detector slopes
 *      0.6, 0.76 and 1 and delays 0 to 2. Starts past the cap are built
 *      through dp_state_filter_kalman_init_qr(), the embedder's unguarded
 *      path, which the validated constructors do not accept: the figures past
 *      the cap are a measurement, and an embedder who passes one is outside
 *      the certified range.
 *
 * ## What is gated and what is printed
 *
 * `--check` asserts what the header claims: the delivered bandwidth is the one
 * asked for, the advertised rectangle is stable at the bound, the lower margin
 * is as stated, the plain update stays positive definite and tracks the wide
 * reference, starts up to the cap keep their whole trajectory inside the
 * rectangle while a start of ten times does not, and the high-gain start
 * settles a frequency step faster than the fixed-gain loop. The full sweep
 * prints the rest, which are findings, not promises.
 *
 * Usage:
 *   state_filter_kalman_loop              the full sweep, as tables
 *   state_filter_kalman_loop --check      the fast CI subset
 *   state_filter_kalman_loop --csv DIR    the same run, writing its CSVs to
 * DIR
 */
#include "doppler/state_filter/state_filter_core.h"
#include "doppler/state_filter_kalman/state_filter_kalman_core.h"
#include "dp_test.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef dp_state_filter_kalman_state_t kal_t;

static int g_emit;
/* the sampling table's rows, held until section 5's caps block is closed: two
 * --emit blocks cannot be open on stdout at once */
static struct samp_row
{
  double bnt, sc, dense, gap;
  size_t first, div, viol;
} g_samp[96];
static size_t g_nsamp;

/* An output table: a CSV file under `dir` (the full sweep's record), or, with
 * --emit and no directory, a `# name` block on stdout for the validator
 * (docs/dev/contributing/validation.md: the report's harness call is the
 * --check --emit run that ctest also makes). NULL when neither. */
static FILE *
open_csv (const char *dir, const char *name, const char *header)
{
  if (dir)
    {
      char path[512];
      snprintf (path, sizeof path, "%s/%s", dir, name);
      FILE *f = fopen (path, "w");
      if (f)
        fprintf (f, "%s\n", header);
      return f;
    }
  if (g_emit)
    {
      char block[128];
      snprintf (block, sizeof block, "%s", name);
      char *dot = strrchr (block, '.');
      if (dot)
        *dot = '\0';
      printf ("\n# %s\n%s\n", block, header);
      return stdout;
    }
  return NULL;
}

static void
close_csv (FILE *f)
{
  if (f && f != stdout)
    fclose (f);
}

/* ── the exact function, as the loop's stability rectangle uses it ─────── */

static int
stable_at (const double g[3], double k, double d)
{
  return dp_state_filter_loop_stable (3, g[0], g[1], g[2], d, k);
}

/* K_low and K_high of the stable interval at delay d, by bisection. */
static void
edges (const double g[3], double d, double *lo, double *hi)
{
  *lo = *hi = NAN;
  if (!stable_at (g, 1.0, d))
    return;
  double a = 1.0, b = 1e-3;
  for (int i = 0; i < 50; i++)
    {
      const double m = sqrt (a * b);
      if (stable_at (g, m, d))
        a = m;
      else
        b = m;
    }
  *lo = a;
  a   = 1.0;
  b   = 1e4;
  for (int i = 0; i < 50; i++)
    {
      const double m = sqrt (a * b);
      if (stable_at (g, m, d))
        a = m;
      else
        b = m;
    }
  *hi = a;
}

static void
kalman_gains_of_bnt (double bnt, double g[3])
{
  kal_t k;
  dp_state_filter_kalman_init_bn (&k, bnt, 1.0);
  g[0] = k.base.kp;
  g[1] = k.base.ki;
  g[2] = k.base.kr;
}

/* The rectangle the flavor advertises, on a set of gains: every loop gain
 * from K_MIN to K_MAX and every detector delay up to DELAY_MAX. Its four
 * corners (stability is one interval in K, and the delay's effect is
 * monotone over the grid the first flavor verified). */
static int
in_rectangle (const double g[3])
{
  static const double ks[2] = { STATE_FILTER_K_MIN, STATE_FILTER_K_MAX };
  for (int i = 0; i < 2; i++)
    for (int d = 0; d <= (int)STATE_FILTER_DELAY_MAX; d++)
      if (!stable_at (g, ks[i], (double)d))
        return 0;
  return 1;
}

/* ── the plain update, against a wider reference ───────────────────────── */

static void
ref_advance (long double p[6], long double q, long double r)
{
  const long double S  = p[0] + r;
  const long double a0 = p[0] + p[1], a1 = p[1] + p[3], a2 = p[2] + p[4];
  const long double b1 = p[3] + p[4], b2 = p[4] + p[5];
  const long double f00 = a0 + a1, f01 = a1 + a2, f02 = a2;
  const long double f11 = b1 + b2, f12 = b2, f22 = p[5];
  const long double K0 = (p[0] + p[1]) / S, K1 = (p[1] + p[2]) / S,
                    K2 = p[2] / S;
  p[0]                 = f00 - S * K0 * K0;
  p[1]                 = f01 - S * K0 * K1;
  p[2]                 = f02 - S * K0 * K2;
  p[3]                 = f11 - S * K1 * K1;
  p[4]                 = f12 - S * K1 * K2;
  p[5]                 = f22 - S * K2 * K2 + q;
}

static int
pd6 (const double p[6])
{
  if (!(p[0] > 0.0))
    return 0;
  const double l10 = p[1] / p[0], l20 = p[2] / p[0];
  const double d1 = p[3] - l10 * p[1];
  if (!(d1 > 0.0))
    return 0;
  const double l21 = (p[4] - l20 * p[1]) / d1;
  return (p[5] - l20 * p[2] - l21 * l21 * d1) > 0.0;
}

typedef struct
{
  size_t updates;
  double gain_dep;  /**< worst relative departure of kp, ki, kr from the ref */
  double entry_dep; /**< worst relative departure of any P entry            */
  int    pd_fail;
} plain_t;

static plain_t
plain_run (double bnt, double start, size_t cap_updates, double ref_q_scale)
{
  const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
  kal_t        k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  long double P[6];
  for (int i = 0; i < 6; i++)
    P[i] = (long double)k.p0[i];
  size_t n_up = (size_t)(10.0 / bnt);
  if (n_up > cap_updates)
    n_up = cap_updates;
  plain_t out = { n_up, 0.0, 0.0, 0 };
  for (size_t n = 0; n < n_up; n++)
    {
      dp_state_filter_kalman_update (&k, 0.0);
      ref_advance (P, (long double)q * (long double)ref_q_scale, 1.0L);
      if (!pd6 (k.p))
        out.pd_fail++;
      const long double S  = P[0] + 1.0L;
      const long double K0 = (P[0] + P[1]) / S, K1 = (P[1] + P[2]) / S,
                        K2   = P[2] / S;
      const long double g[3] = { K0 - K1 + K2, K1 - 2.0L * K2, K2 };
      const double      h[3] = { k.base.kp, k.base.ki, k.base.kr };
      for (int i = 0; i < 3; i++)
        {
          const double dep = (double)fabsl (((long double)h[i] - g[i]) / g[i]);
          if (dep > out.gain_dep)
            out.gain_dep = dep;
        }
      for (int i = 0; i < 6; i++)
        {
          const double dep
              = (double)fabsl (((long double)k.p[i] - P[i]) / P[i]);
          if (P[i] != 0.0L && dep > out.entry_dep)
            out.entry_dep = dep;
        }
    }
  return out;
}

/* ── the start: the whole trajectory against the rectangle ─────────────── */

typedef struct
{
  int    inside;   /**< every update's gains inside the rectangle          */
  double kr_peak;  /**< the largest kr over the trajectory / steady kr     */
  double kp_ratio; /**< the largest kp over the trajectory / steady kp     */
} traj_t;

/* The constructor's sampled walk with its two constants as parameters, so the
 * constants can be measured and not asserted: the first `first` updates are
 * tested one by one, then each next test is `1/div` of the elapsed updates
 * later (div 100 is 1%). The shipped values are 0 and 100. This is an
 * instrument, not a second implementation of the rule: it walks the public
 * init and update, and the harness's own corner test. */
static int
sampled_ok (double bnt, double start, size_t first, size_t div)
{
  const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
  kal_t        k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  const size_t n    = (size_t)(8.0 / bnt);
  size_t       next = first;
  for (size_t i = 0; i < n; i++)
    {
      const double g[3] = { k.base.kp, k.base.ki, k.base.kr };
      if ((i < first || i == next) && !in_rectangle (g))
        return 0;
      if (i >= first && i == next)
        next = next + next / div + 1;
      dp_state_filter_kalman_update (&k, 0.0);
    }
  return 1;
}

/* The largest start the sampled walk accepts, searched to 200. */
static double
sampled_cap (double bnt, size_t first, size_t div)
{
  double lo = 1.0, hi = 200.0;
  if (sampled_ok (bnt, hi, first, div))
    return hi;
  for (int it = 0; it < 30; it++)
    {
      const double m = sqrt (lo * hi);
      if (sampled_ok (bnt, m, first, div))
        lo = m;
      else
        hi = m;
    }
  return lo;
}

/* The largest start the constructor's own check accepts for `q`, searched
 * from 1 to the ceiling: 1 when it refuses every start. */
static double
accepted_cap (double q)
{
  double alo = 1.0, ahi = STATE_FILTER_KALMAN_START_MAX;
  if (dp_state_filter_kalman_params_ok_qr (q, 1.0, 1.0, ahi))
    return ahi;
  for (int it = 0; it < 30; it++)
    {
      const double m = 0.5 * (alo + ahi);
      if (dp_state_filter_kalman_params_ok_qr (q, 1.0, 1.0, m))
        alo = m;
      else
        ahi = m;
    }
  return alo;
}

/* The update at which a start FIRST leaves the rectangle (every update
 * tested); the walk length when it never does. */
static size_t
first_violation (double bnt, double start)
{
  const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
  kal_t        k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  const size_t n = (size_t)(8.0 / bnt);
  for (size_t i = 0; i < n; i++)
    {
      const double g[3] = { k.base.kp, k.base.ki, k.base.kr };
      if (!in_rectangle (g))
        return i;
      dp_state_filter_kalman_update (&k, 0.0);
    }
  return n;
}

/* `dense` tests every update (and stops at the first violation); otherwise
 * the first 100 updates and then every tenth, as the starts table has always
 * done. The cap search uses the dense form: it is the reference the shipped
 * constructor's sampled walk is measured against. */
static traj_t
trajectory (double bnt, double start, int dense)
{
  const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
  kal_t        k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  double g0[3];
  kalman_gains_of_bnt (bnt, g0);
  traj_t       t = { 1, 1.0, 1.0 };
  const size_t n = (size_t)(8.0 / bnt);
  for (size_t i = 0; i < n; i++)
    {
      const double g[3] = { k.base.kp, k.base.ki, k.base.kr };
      if ((dense || i < 100 || i % 10 == 0) && t.inside && !in_rectangle (g))
        {
          t.inside = 0;
          if (dense)
            break;
        }
      if (g[2] / g0[2] > t.kr_peak)
        t.kr_peak = g[2] / g0[2];
      if (g[0] / g0[0] > t.kp_ratio)
        t.kp_ratio = g[0] / g0[0];
      dp_state_filter_kalman_update (&k, 0.0);
    }
  return t;
}

/* ── the closed loop: a frequency step, through a detector ─────────────── */

typedef struct
{
  int    ok;     /**< bounded, and settled to the step                    */
  double peak;   /**< peak |phase error|                                  */
  int    settle; /**< updates until |error| stays under 5% of its peak    */
} step_t;

#define MAXD 4

/* One closed loop around a filter: the detector reads the phase error
 * `delay` updates late and scales it by `slope`. `kalman` selects the filter
 * (a kal_t, else a dp_state_filter_state_t). */
static step_t
closed_step (void *flt, int kalman, double slope, int delay, size_t n)
{
  const double f0 = 1e-3; /* the frequency step, cycles per update */
  double       hist[MAXD + 1];
  for (int i = 0; i <= MAXD; i++)
    hist[i] = 0.0;
  double *err      = malloc (n * sizeof *err);
  double  phase_in = 0.0, phase_osc = 0.0;
  step_t  out = { 1, 0.0, 0 };
  for (size_t i = 0; i < n; i++)
    {
      phase_in += f0;
      const double e = phase_in - phase_osc;
      err[i]         = e;
      for (int j = MAXD; j > 0; j--)
        hist[j] = hist[j - 1];
      hist[0]           = e;
      const double meas = slope * hist[delay];
      const double u
          = kalman
                ? dp_state_filter_kalman_step ((kal_t *)flt, meas)
                : dp_state_filter_step ((dp_state_filter_state_t *)flt, meas);
      phase_osc += u;
      if (!(fabs (e) < 1e6 * f0))
        {
          out.ok = 0;
          break;
        }
    }
  double pk = 0.0;
  for (size_t i = 0; i < n; i++)
    if (fabs (err[i]) > pk)
      pk = fabs (err[i]);
  out.peak = pk;
  int last = 0;
  for (size_t i = 0; i < n; i++)
    if (fabs (err[i]) > 0.05 * pk)
      last = (int)i;
  out.settle = last + 1;
  if (out.ok && fabs (err[n - 1]) > 1e-3 * pk)
    out.ok = 0; /* did not settle inside the run */
  free (err);
  return out;
}

/* The time the gains take to settle: the last update at which any of kp, ki,
 * kr is more than 1% from its steady value, times bn*t. The start rule walks
 * 8/(bn*t) updates; this is the measurement that horizon rests on. */
static double
settle_vs (double bnt, double start, double bnt_ref)
{
  const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
  kal_t        k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  double g0[3];
  kalman_gains_of_bnt (bnt_ref, g0);
  size_t       last = 0;
  const size_t n    = (size_t)(30.0 / bnt);
  for (size_t i = 0; i < n; i++)
    {
      const double g[3] = { k.base.kp, k.base.ki, k.base.kr };
      for (int j = 0; j < 3; j++)
        if (fabs (g[j] / g0[j] - 1.0) > 0.01)
          last = i;
      dp_state_filter_kalman_update (&k, 0.0);
    }
  return (double)last * bnt;
}

static double
settle_bnt (double bnt, double start)
{
  return settle_vs (bnt, start, bnt);
}

int
main (int argc, char **argv)
{
  int         check = 0;
  const char *csv   = NULL;
  for (int i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--check") == 0)
        check = 1;
      else if (strcmp (argv[i], "--emit") == 0)
        g_emit = 1;
      else if (strcmp (argv[i], "--csv") == 0 && i + 1 < argc)
        csv = argv[++i];
    }

  if (!check)
    printf ("long double mantissa: %d bits (double: %d)\n\n", LDBL_MANT_DIG,
            DBL_MANT_DIG);

  /* 1 ── Gains: Riccati against Butterworth ─────────────────────────────── */
  {
    FILE *f = open_csv (csv, "gains.csv",
                        "bnt,kp,ki,kr,kp_rel,ki_rel,kr_rel,bn_err");
    if (!check)
      printf ("%9s %12s %12s %12s %8s %8s %8s\n", "bn*t", "kp", "ki", "kr",
              "dkp%", "dki%", "dkr%");
    const double bnts[]
        = { 1e-6, 1e-4, 1e-3, 0.005, 0.01, 0.02, 0.04, 0.08, 0.12 };
    for (size_t i = 0; i < sizeof bnts / sizeof *bnts; i++)
      {
        double g[3];
        kalman_gains_of_bnt (bnts[i], g);
        dp_state_filter_state_t b;
        dp_state_filter_init (&b, bnts[i], 1.0);
        const double rel[3]
            = { g[0] / b.kp - 1.0, g[1] / b.ki - 1.0, g[2] / b.kr - 1.0 };
        const double bn_err
            = dp_state_filter_loop_bandwidth (3, g[0], g[1], g[2], 0.0, 1.0)
                  / bnts[i]
              - 1.0;
        /* the delivered bandwidth is the one asked for */
        DP_CHECK (fabs (bn_err) < 1e-8);
        if (!check)
          printf ("%9.1e %12.5e %12.5e %12.5e %8.2f %8.2f %8.2f\n", bnts[i],
                  g[0], g[1], g[2], 100 * rel[0], 100 * rel[1], 100 * rel[2]);
        if (f)
          fprintf (f, "%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.3g\n",
                   bnts[i], g[0], g[1], g[2], rel[0], rel[1], rel[2], bn_err);
        /* they meet as bn*t goes to 0 and differ by a few percent at the
         * bound: the kr gap is under 1% below 0.01 and under 10% at 0.12 */
        if (bnts[i] <= 0.01)
          DP_CHECK (fabs (rel[2]) < 0.01);
        if (bnts[i] <= 0.12)
          DP_CHECK (fabs (rel[2]) < 0.10 && fabs (rel[0]) < 0.06);

        /* CONTROLS: each assertion above, evaluated on a named wrong case,
         * must fail, or it cannot tell a right answer from a wrong one.
         * (1) the bandwidth check on gains with kr 0.1% off: */
        {
          const double gp[3] = { g[0], g[1], g[2] * 1.001 };
          const double e     = dp_state_filter_loop_bandwidth (3, gp[0], gp[1],
                                                               gp[2], 0.0, 1.0)
                                   / bnts[i]
                               - 1.0;
          DP_CHECK (!(fabs (e) < 1e-8));
        }
        /* (3) the 10% kr check against the Butterworth gains of a loop ten
         * times as wide: */
        if (bnts[i] * 10.0 <= 0.12)
          {
            dp_state_filter_state_t w;
            dp_state_filter_init (&w, bnts[i] * 10.0, 1.0);
            DP_CHECK (!(fabs (g[2] / w.kr - 1.0) < 0.10));
          }
        /* (4) the 1% kr check at the bound, where the two flavors differ by
         * 9%: */
        if (bnts[i] == 0.12)
          DP_CHECK (!(fabs (rel[2]) < 0.01));
      }
    close_csv (f);
  }

  /* the smallest bn*t the flavor accepts is the smallest this block measures
   * the exact bandwidth at (1e-6, error under 1e-8, and the plain update's
   * sweep record runs to it): nothing below is claimed */
  DP_CHECK (STATE_FILTER_KALMAN_BNT_MIN >= 1e-6);

  /* 2 ── Stability at the Kalman flavor's own gains ─────────────────────── */
  {
    FILE *f = open_csv (csv, "edges.csv", "bnt,delay,k_low,k_high");
    if (!check)
      printf ("\n%9s %5s %9s %9s\n", "bn*t", "delay", "K_low", "K_high");
    const double bnts[]   = { 0.001, 0.01, 0.04, 0.08, 0.12, 0.13, 0.14 };
    double       best_bnt = 0.0; /* the largest with 5% headroom at K_MAX */
    double       best_bnt_nohead = 0.0; /* ... with no headroom asked */
    for (size_t i = 0; i < sizeof bnts / sizeof *bnts; i++)
      {
        double g[3];
        /* past the flavor's bound the constructor refuses: build it
         * unguarded to measure where the stability actually ends */
        {
          kal_t  k;
          double qq = 0.0;
          /* ratio_for_bandwidth refuses above the bound: solve by bisection
           * on q here, with the same steady-state solve */
          double lo = -60.0, hi = 5.0;
          for (int it = 0; it < 100; it++)
            {
              const double mid = 0.5 * (lo + hi);
              double       p[6], gg[3];
              dp_state_filter_kalman_steady (exp (mid), 1.0, p, gg);
              const double bn = dp_state_filter_loop_bandwidth (
                  3, gg[0], gg[1], gg[2], 0.0, 1.0);
              if (bn < bnts[i])
                lo = mid;
              else
                hi = mid;
            }
          qq = exp (0.5 * (lo + hi));
          dp_state_filter_kalman_init_qr (&k, qq, 1.0, 1.0, 1.0);
          g[0] = k.base.kp;
          g[1] = k.base.ki;
          g[2] = k.base.kr;
        }
        for (int d = 0; d <= 2; d++)
          {
            double lo, hi;
            edges (g, (double)d, &lo, &hi);
            if (!check)
              printf ("%9.3f %5d %9.4f %9.3f\n", bnts[i], d, lo, hi);
            if (f)
              fprintf (f, "%.10g,%d,%.10g,%.10g\n", bnts[i], d, lo, hi);
            /* BNT_MAX is the largest bn*t on this grid whose upper edge at
             * the worst delay keeps 5% headroom over K_MAX (a 5% error in
             * loop gain or detector slope stays stable): 2.18 against 2 at
             * 0.12, 2.004 at 0.13 */
            if (d == 2 && bnts[i] >= 0.12 && hi >= 1.05 * STATE_FILTER_K_MAX)
              best_bnt = bnts[i];
            /* CONTROL (8): the upper edge past the bound is under K_MAX, so
             * the claim's assertion fails there (1.85 at 0.14) */
            if (bnts[i] == 0.14 && d == 2)
              DP_CHECK (!(hi > STATE_FILTER_K_MAX));
            if (d == 2 && bnts[i] >= 0.12 && hi >= STATE_FILTER_K_MAX)
              best_bnt_nohead = bnts[i];
            if (bnts[i] == 0.12)
              {
                /* the advertised rectangle holds at the bound, with the
                 * lower margin the page states: 2.5 dB under K_MIN at the
                 * worst delay (the first flavor has 4.1) */
                DP_CHECK (lo < STATE_FILTER_K_MIN);
                DP_CHECK (hi > STATE_FILTER_K_MAX);
                if (d == 2)
                  {
                    const double margin_db
                        = 20.0 * log10 (STATE_FILTER_K_MIN / lo);
                    DP_CHECK (margin_db > 2.0);
                  }
              }
          }
        DP_CHECK (bnts[i] != 0.12 || in_rectangle (g));
        /* CONTROLS at the bound (5, 6): the same assertions on gains with kp
         * 3 times too large (not in the rectangle) and 0.85 times (the lower
         * edge rises until the margin is under 2 dB) must fail */
        if (bnts[i] == 0.12)
          {
            const double big[3]    = { 3.0 * g[0], g[1], g[2] };
            const double narrow[3] = { 0.85 * g[0], g[1], g[2] };
            DP_CHECK (!in_rectangle (big));
            double lo2, hi2;
            edges (narrow, 2.0, &lo2, &hi2);
            DP_CHECK (!(20.0 * log10 (STATE_FILTER_K_MIN / lo2) > 2.0));
          }
      }
    DP_CHECK (best_bnt == STATE_FILTER_KALMAN_BNT_MAX);
    /* CONTROL (21): with no headroom asked, the same selection lands on a
     * different bn*t (0.13), so the 5% is what chooses 0.12 */
    DP_CHECK (best_bnt_nohead != STATE_FILTER_KALMAN_BNT_MAX);
    close_csv (f);

    /* the largest bn*t stable at K = 2 and delay 2: the flavor's own bound,
     * derived and not inherited (the first flavor's is 0.1248) */
    double lo = 0.05, hi = 0.30;
    for (int it = 0; it < 40; it++)
      {
        const double m = 0.5 * (lo + hi);
        double       a = -60.0, b = 5.0;
        for (int j = 0; j < 100; j++)
          {
            const double mid = 0.5 * (a + b);
            double       p[6], gg[3];
            dp_state_filter_kalman_steady (exp (mid), 1.0, p, gg);
            if (dp_state_filter_loop_bandwidth (3, gg[0], gg[1], gg[2], 0.0,
                                                1.0)
                < m)
              a = mid;
            else
              b = mid;
          }
        double p[6], gg[3];
        dp_state_filter_kalman_steady (exp (0.5 * (a + b)), 1.0, p, gg);
        if (stable_at (gg, STATE_FILTER_K_MAX, STATE_FILTER_DELAY_MAX))
          lo = m;
        else
          hi = m;
      }
    if (!check)
      printf ("\nlargest bn*t stable at K = 2, delay = 2: %.4f "
              "(the first flavor's: 0.1248)\n",
              lo);
    /* the constant is under the derived bound, and the bound is near the
     * value the page states */
    DP_CHECK (STATE_FILTER_KALMAN_BNT_MAX < lo);
    DP_CHECK (lo > 0.125 && lo < 0.135);
  }

  /* 3 ── The plain covariance update, against a wider reference ─────────── */
  {
    FILE *f = open_csv (csv, "plain_update.csv",
                        "bnt,start,updates,gain_dep,entry_dep,pd_fail,"
                        "ldbl_bits");
    if (!check)
      printf ("\n%9s %6s %10s %12s %12s %8s\n", "bn*t", "start", "updates",
              "gain dep", "worst P", "PD fail");
    const double bnts[]   = { 0.12, 0.01, 1e-3, 1e-4, 1e-5, 1e-6 };
    const double starts[] = { 1.0, STATE_FILTER_KALMAN_START_MAX };
    for (size_t i = 0; i < sizeof bnts / sizeof *bnts; i++)
      for (size_t j = 0; j < 2; j++)
        {
          /* --check: the fast half of the range; the sweep does all, the
           * slow end capped at 2e6 updates (the number is printed) */
          if (check && bnts[i] < 1e-4)
            continue;
          const plain_t r = plain_run (bnts[i], starts[j], 2000000u, 1.0);
          if (!check)
            printf ("%9.1e %6.1f %10zu %12.3e %12.3e %8d\n", bnts[i],
                    starts[j], r.updates, r.gain_dep, r.entry_dep, r.pd_fail);
          if (f)
            fprintf (f, "%.10g,%.1f,%zu,%.4g,%.4g,%d,%d\n", bnts[i], starts[j],
                     r.updates, r.gain_dep, r.entry_dep, r.pd_fail,
                     LDBL_MANT_DIG);
          /* the plain form holds: positive definite at every update, and the
           * gains within 1e-7 of the wider reference (the entry that loses
           * digits is p12; the gains do not feel it) */
          DP_CHECK (r.pd_fail == 0);
          DP_CHECK (r.gain_dep < 1e-7);
        }
    close_csv (f);
    /* CONTROLS (16, 17). The gain check on a reference whose process noise is
     * 1% off must fail at the same bound, and the positive-definite test must
     * refuse a matrix that is not (a negative variance, and a covariance
     * with a correlation above 1): */
    {
      const plain_t w = plain_run (1e-3, 5.0, 2000000u, 1.01);
      DP_CHECK (!(w.gain_dep < 1e-11));
      const double neg[6] = { 1, 0, 0, -1, 0, 1 };
      const double ind[6] = { 1, 2, 0, 1, 0, 1 };
      DP_CHECK (!pd6 (neg));
      DP_CHECK (!pd6 (ind));
    }
  }

  /* 4 ── The start ──────────────────────────────────────────────────────── */
  {
    FILE *f
        = open_csv (csv, "starts.csv", "bnt,start,inside,kr_peak,kp_ratio");
    FILE *g = open_csv (csv, "relaxed.csv",
                        "bnt,start,slope,delay,ok,settle,peak,fixed_settle");
    if (!check)
      printf ("\n%9s %6s %7s %8s %8s\n", "bn*t", "start", "inside", "kr pk",
              "kp pk");
    const double bnts[]   = { 0.01, 0.04 };
    const double starts[] = { 1.0, 2.0, 4.0, 5.0, 10.0, 30.0, 100.0 };
    for (size_t i = 0; i < 2; i++)
      {
        for (size_t j = 0; j < sizeof starts / sizeof *starts; j++)
          {
            const traj_t t = trajectory (bnts[i], starts[j], 0);
            if (!check)
              printf ("%9.3f %6.1f %7s %8.3f %8.3f\n", bnts[i], starts[j],
                      t.inside ? "yes" : "NO", t.kr_peak, t.kp_ratio);
            if (f)
              fprintf (f, "%.10g,%.1f,%d,%.6g,%.6g\n", bnts[i], starts[j],
                       t.inside, t.kr_peak, t.kp_ratio);
            /* the rule the constructor enforces: every start up to the cap
             * keeps its whole trajectory inside the rectangle, and a start
             * of ten times does not, so the cap is not vacuous */
            if (starts[j] <= STATE_FILTER_KALMAN_START_MAX)
              DP_CHECK (t.inside);
            if (starts[j] >= 10.0)
              DP_CHECK (!t.inside);
          }

        /* the closed loop, noise-free, through a detector with a slope and a
         * delay; the fixed-gain loop at the same bn*t beside it */
        const size_t n = (size_t)(60.0 / bnts[i]);
        if (!check)
          printf ("\n bn*t %.3f: step response (settle, peak) through slope "
                  "and delay\n%6s %6s %6s | %9s %9s\n",
                  bnts[i], "start", "slope", "delay", "settle", "fixed");
        static const double slopes[] = { 1.0, 0.76, 0.6 };
        for (size_t j = 0; j < sizeof starts / sizeof *starts; j++)
          for (size_t s = 0; s < 3; s++)
            for (int d = 0; d <= 2; d++)
              {
                if (check && !(s == 0 && d == 0))
                  continue;
                double q
                    = dp_state_filter_kalman_ratio_for_bandwidth (bnts[i]);
                kal_t k;
                dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, starts[j]);
                dp_state_filter_state_t b;
                memset (&b, 0, sizeof b); /* init leaves integ and rate */
                dp_state_filter_init (&b, bnts[i], 1.0);
                const step_t a  = closed_step (&k, 1, slopes[s], d, n);
                const step_t fx = closed_step (&b, 0, slopes[s], d, n);
                if (!check)
                  printf ("%6.1f %6.2f %6d | %9d%s %9d%s\n", starts[j],
                          slopes[s], d, a.settle, a.ok ? " " : "!", fx.settle,
                          fx.ok ? " " : "!");
                if (g)
                  fprintf (g, "%.10g,%.1f,%.2f,%d,%d,%d,%.6g,%d\n", bnts[i],
                           starts[j], slopes[s], d, a.ok, a.settle, a.peak,
                           fx.settle);
                if (check && s == 0 && d == 0 && starts[j] <= 5.0)
                  {
                    DP_CHECK (a.ok);
                    /* the high-gain start settles a frequency step faster
                     * than the fixed-gain loop at the same bandwidth */
                    if (starts[j] >= 4.0)
                      DP_CHECK ((double)a.settle
                                < (2.0 / 3.0) * (double)fx.settle);
                    /* CONTROL (14): a start of 1 is the steady state, which
                     * settles as the fixed-gain loop does, so the same
                     * assertion must fail on it */
                    if (starts[j] == 1.0)
                      DP_CHECK (!((double)a.settle
                                  < (2.0 / 3.0) * (double)fx.settle));
                  }
              }
        /* CONTROL (15): the settles-to-the-step assertion on a loop that
         * cannot: a detector slope of 500 puts the loop gain far past the
         * upper edge, and the step is not settled */
        {
          kal_t kc;
          dp_state_filter_kalman_init_qr (
              &kc, dp_state_filter_kalman_ratio_for_bandwidth (bnts[i]), 1.0,
              1.0, 1.0);
          DP_CHECK (!closed_step (&kc, 1, 500.0, 2, n).ok);
        }
      }
    close_csv (f);
    close_csv (g);
  }

  /* 5 ── The cap, as a function of bn*t ────────────────────────────────── */
  {
    FILE *f = open_csv (csv, "caps.csv", "bnt,dense_cap,accepted_cap,binding");
    if (!check)
      printf ("\n%9s %12s %12s %s\n", "bn*t", "rule's cap", "accepted",
              "(accepted is the rule's cap up to the ceiling "
              "STATE_FILTER_KALMAN_START_MAX)");
    static const double bnts[]
        = { 1e-4, 3e-4, 1e-3, 3e-3, 0.01, 0.02, 0.04, 0.06, 0.08, 0.1, 0.12 };
    double min_cap = 1e30; /* the smallest rule cap where the ceiling binds */
    double min_cap_all = 1e30; /* ... over every bn*t, rule-bound included */
    for (size_t i = 0; i < sizeof bnts / sizeof *bnts; i++)
      {
        /* --check: the ceiling row where the rule's cap is smallest (0.04),
         * one rule row (0.06) and the bound (0.12); the sweep does every
         * row. The dense search walks every update, so each row costs. */
        if (check && bnts[i] != 0.04 && bnts[i] != 0.06 && bnts[i] != 0.12)
          continue;
        /* the rule's cap: the largest start whose whole trajectory stays in
         * the rectangle (dense: every update), searched up to 200 */
        double lo = 1.0, hi = 200.0;
        if (trajectory (bnts[i], hi, 1).inside)
          lo = hi;
        else
          for (int it = 0; it < 20; it++)
            {
              const double m = sqrt (lo * hi);
              if (trajectory (bnts[i], m, 1).inside)
                lo = m;
              else
                hi = m;
            }
        const double dense_cap = lo;
        /* what the constructor accepts: the same search on the shipped
         * check, which has the ceiling */
        /* The constructor-facing checks are built 1e-9 INSIDE the row's
         * bn*t: params_ok_qr recomputes the bandwidth from q, and at the
         * bound (0.12) one ulp of rounding in that recomputation puts it
         * over BNT_MAX on a platform with another libm, which refuses every
         * start and reads as a cap of 1. 1e-9 is five orders over the
         * measured error of that recomputation (6e-15) and changes no digit
         * of the table. */
        const double q = dp_state_filter_kalman_ratio_for_bandwidth (
            bnts[i] * (1.0 - 1e-9));
        const double alo = accepted_cap (q);
        const char  *binding
            = dense_cap > STATE_FILTER_KALMAN_START_MAX ? "ceiling" : "rule";
        if (!check)
          printf ("%9.1e %12.3f %12.3f %s\n", bnts[i], dense_cap, alo,
                  binding);
        if (f)
          fprintf (f, "%.10g,%.6g,%.6g,%s\n", bnts[i], dense_cap, alo,
                   binding);
        /* the constructor never accepts past the rule's cap or the ceiling */
        if (bnts[i] <= 0.04 && dense_cap < min_cap)
          min_cap = dense_cap;
        if (dense_cap < min_cap_all)
          min_cap_all = dense_cap;
        /* CONTROL (12): a minimal pair at the cap where the rule binds: the
         * constructor's check accepts a start 1% under the dense cap and
         * refuses one 1% over it, so "never accepts past the cap" can fail */
        if (dense_cap <= STATE_FILTER_KALMAN_START_MAX)
          {
            const double q2 = q;
            if (!dp_state_filter_kalman_params_ok_qr (q2, 1.0, 1.0,
                                                      dense_cap * 0.99))
              {
                /* say which row and by how much, so a platform failure is
                 * readable from its log */
                double p6[6], g3[3];
                dp_state_filter_kalman_steady (q2, 1.0, p6, g3);
                fprintf (stderr,
                         "control 12: bn*t %.10g dense cap %.10g: the "
                         "constructor refuses 0.99 of it (bandwidth/bn*t - 1 "
                         "= %.3e)\n",
                         bnts[i], dense_cap,
                         dp_state_filter_loop_bandwidth (3, g3[0], g3[1],
                                                         g3[2], 0.0, 1.0)
                                 / bnts[i]
                             - 1.0);
              }
            DP_CHECK (dp_state_filter_kalman_params_ok_qr (q2, 1.0, 1.0,
                                                           dense_cap * 0.99));
            DP_CHECK (!dp_state_filter_kalman_params_ok_qr (q2, 1.0, 1.0,
                                                            dense_cap * 1.01));
          }
        /* CONTROL (13): at the bound the accepted cap is under 3, so the
         * "at least 3 times" assertion fails there */
        if (bnts[i] == 0.12)
          DP_CHECK (!(alo > 3.0));
        /* the constructor accepts up to the rule's cap or the ceiling,
         * whichever is smaller: the lower bound is what a constructor that
         * refuses every start fails (its cap reads 1), where the upper bound
         * passes */
        const double want = fmin (dense_cap, STATE_FILTER_KALMAN_START_MAX);
        DP_CHECK (alo >= 0.999 * want);
        /* CONTROL (12b): the same lower-bound check on a constructor that
         * refuses every start (q = 0 is outside its domain) must fail */
        DP_CHECK (!(accepted_cap (0.0) >= 0.999 * want));
        DP_CHECK (alo <= dense_cap * 1.0001);
        DP_CHECK (alo <= STATE_FILTER_KALMAN_START_MAX * 1.0001);

        /* ── the sampling constants, measured ─────────────────────────────
         * The shipped walk tests each next update 1% of the elapsed updates
         * after the last (at least one later: so the first 100 are all
         * tested and there is no separate head). The rows below are the cap
         * each choice gives against the dense one: a sampled walk can only
         * miss a violation, so its cap is at or above the dense one, and the
         * gap is what the constants cost. The shipped pair is gated; the
         * coarser pairs are the record that shows the gap opening. */
        static const struct
        {
          size_t first, div;
        } cfg[]           = { { 0, 100 }, { 100, 100 }, { 0, 10 },
                              { 0, 2 },   { 10, 10 },   { 100, 10 } };
        const size_t viol = dense_cap < 200.0
                                ? first_violation (bnts[i], dense_cap * 1.001)
                                : 0;
        for (size_t c = 0; c < sizeof cfg / sizeof *cfg; c++)
          {
            const double sc  = sampled_cap (bnts[i], cfg[c].first, cfg[c].div);
            const double gap = sc / dense_cap - 1.0;
            g_samp[g_nsamp++] = (struct samp_row){
              bnts[i],      sc,         dense_cap,        gap,
              cfg[c].first, cfg[c].div, c == 0 ? viol : 0
            };
            if (c == 0)
              {
                /* the shipped constants cost under 0.1% of the cap, and
                 * that is ten times under the room the ceiling leaves where
                 * it binds (the cap is at least 5.9 against 5) */
                DP_CHECK (gap >= -1e-9 && gap < 1e-3);
                if (dense_cap > STATE_FILTER_KALMAN_START_MAX)
                  DP_CHECK (gap * 10.0
                            < dense_cap / STATE_FILTER_KALMAN_START_MAX - 1.0);
                /* where the rule binds the walk is short, and the first
                 * violation at the cap falls inside the first 100 updates,
                 * where every update is tested whatever the step */
                if (dense_cap <= STATE_FILTER_KALMAN_START_MAX)
                  DP_CHECK (viol < 100);
                /* and the first violation, at the cap, comes inside 1/(bn*t)
                 * updates: the walk's 8/(bn*t) has a wide margin there */
                if (dense_cap < 200.0)
                  {
                    DP_CHECK ((double)viol * bnts[i] < 1.0);
                    /* CONTROL (19): a start 10% UNDER the cap never leaves
                     * the rectangle (the walk length comes back), and the
                     * same assertion fails on it */
                    DP_CHECK (
                        !((double)first_violation (bnts[i], dense_cap * 0.9)
                              * bnts[i]
                          < 1.0));
                  }
              }
          }
      }
    /* START_MAX is the integer part of the smallest rule cap up to 0.04 (5.90
     * at 0.04): the largest whole number the rule allows everywhere the
     * ceiling binds */
    DP_CHECK (floor (min_cap) == STATE_FILTER_KALMAN_START_MAX);
    /* CONTROL (20): over every bn*t the smallest cap has another integer
     * part, so the equality is what picks out the ceiling-bound range */
    DP_CHECK (floor (min_cap_all) != STATE_FILTER_KALMAN_START_MAX);
    close_csv (f);
    FILE *fs = open_csv (csv, "sampling.csv",
                         "bnt,first,div,sampled_cap,dense_cap,gap,viol");
    for (size_t r = 0; r < g_nsamp; r++)
      {
        if (!check)
          printf ("%9.1e first %3zu 1/%-3zu sampled %8.4f dense %8.4f gap "
                  "%9.2e\n",
                  g_samp[r].bnt, g_samp[r].first, g_samp[r].div, g_samp[r].sc,
                  g_samp[r].dense, g_samp[r].gap);
        if (fs)
          fprintf (fs, "%.10g,%zu,%zu,%.6g,%.6g,%.4g,%zu\n", g_samp[r].bnt,
                   g_samp[r].first, g_samp[r].div, g_samp[r].sc,
                   g_samp[r].dense, g_samp[r].gap, g_samp[r].viol);
      }
    close_csv (fs);
    /* CONTROL (18): a 1/10 step accepts a start the dense walk refuses, so the
     * 0.1% assertion on the step's cap can fail. At 1e-3 the dense cap is
     * 6.056 and the 1/10 step's is 6.079 (the sweep's sampling table); 6.07
     * is between them. One walk each, not a search. */
    DP_CHECK (!trajectory (1e-3, 6.07, 1).inside);
    DP_CHECK (sampled_ok (1e-3, 6.07, 0, 10));
    DP_CHECK (!sampled_ok (1e-3, 6.07, 0, 100));
  }

  /* 6 ── The start rule's horizon is measured ──────────────────────────────
   */
  {
    FILE *f = open_csv (csv, "settle.csv", "bnt,start,settle_times_bnt");
    static const double bnts[]   = { 1e-3, 0.01, 0.04, 0.12 };
    static const double starts[] = { 1.0, 2.0, 4.0, 5.0 };
    double              worst    = 0.0;
    for (size_t i = 0; i < 4; i++)
      for (size_t j = 0; j < 4; j++)
        {
          if (bnts[i] >= 0.12 && starts[j] > 1.0)
            continue; /* the bound accepts no start above 1 */
          const double st = settle_bnt (bnts[i], starts[j]);
          if (st > worst)
            worst = st;
          if (f)
            fprintf (f, "%.10g,%.1f,%.4f\n", bnts[i], starts[j], st);
        }
    if (!check)
      printf ("\nlargest gain-settling time to 1%%: %.2f / (bn*t) updates "
              "(the start rule walks 8)\n",
              worst);
    /* the walk covers the settling with margin (the measured worst is about
     * 4.5), and once within 1% of steady the gains are inside the rectangle
     * by more than that: the steady state's own margin is 8% at the bound */
    DP_CHECK (worst * 1.5 < 8.0);
    /* CONTROL (7): the settle measure against the steady state of a loop ten
     * times as wide never settles, and the same assertion fails on it */
    DP_CHECK (!(settle_vs (0.01, 4.0, 0.1) * 1.5 < 8.0));
    close_csv (f);
  }

  if (g_emit && !csv)
    printf ("\n# end\nstatus\n"); /* the epilogue below is not a row */
  DP_TEST_END ("state_filter_kalman_loop");
}
