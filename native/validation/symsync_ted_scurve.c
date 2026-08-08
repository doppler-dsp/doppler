/**
 * @file symsync_ted_scurve.c
 * @brief Validation: every timing-error detector hands the loop the SAME
 *        S-curve slope, so one `bn` means one bandwidth whichever is selected.
 *
 * The timing-side twin of carrier_nda_scurve.c, and it exists for the same
 * reason: `carrier_nda_disc` scales its output so the S-curve slope at lock is
 * 2 for every M, and a loop tuned at one M is therefore tuned at all of them.
 * The timing TEDs owe the loop filter the identical guarantee across
 * DETECTORS, and until 2026-08-08 they did not have it.
 *
 * ## What went wrong, and why a slope gate is the thing that catches it
 *
 * The two numerators have different degrees in the signal amplitude `A`:
 *
 *     gardner:  mid . (on - prev)                -- QUADRATIC
 *     dttl:     mid . (sign(on) - sign(prev))    -- LINEAR
 *
 * Both call sites used to divide by a POWER reference outside the detector,
 * which is right for Gardner and leaves DTTL's gain proportional to `1/A`. At
 * `MpskReceiverR`'s operating amplitude that ran the timing loop ~24x hot and
 * it never closed: 5 recovered symbols out of 5998. Each detector now
 * normalises itself (`gardner_ted` by `pwr_ref`, `dttl_ted` by
 * `2*sqrt(pwr_ref)`), and THIS harness is what pins the two slopes together so
 * a third detector cannot quietly arrive with the wrong one.
 *
 * ## The model
 *
 * Open-loop, so the S-curve is the detector's own and not the loop's. An I&D
 * (rectangular) matched filter has a TRIANGULAR pulse response, so the
 * matched-filter output at continuous time is
 *
 *     y(t) = A * sum_k a_k * Lambda((t - k*T) / T)
 *
 * with `Lambda` the unit triangle. Sampling that directly is exactly what the
 * cascade delivers to the TED, without needing the cascade: the on-time sample
 * of symbol k at a static timing offset `tau` (in symbols) is `y((k + tau)*T)`
 * and the transition gate is half a symbol behind it. Sweeping `tau` and
 * averaging the detector output over many random symbols IS the S-curve.
 *
 * The reference is `|on|^2 + |mid|^2` (ratesync's definition). symsync uses
 * `|on|^2`, which differs by ~1.5x on random data and so moves the
 * DTTL/Gardner slope ratio by ~1.22 -- inside this harness's tolerance, and
 * recorded as a separate pre-existing inconsistency rather than hidden by a
 * per-caller constant.
 *
 * Usage:  validate_symsync_ted_scurve [--check]
 */
#include "symsync/symsync_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/** @brief Symbols averaged per S-curve point. */
#define NSYM 20000u
/** @brief Signal amplitude. Swept, because the whole defect was gain-vs-A. */
#define NAMP 3

static uint32_t
rng (uint32_t *s)
{
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}

/** @brief Unit triangle: 1 at 0, 0 at |u| >= 1 -- the I&D pulse response. */
static double
tri (double u)
{
  double a = fabs (u);
  return a >= 1.0 ? 0.0 : 1.0 - a;
}

/**
 * @brief Matched-filter output at time `t` symbols, for the symbol stream.
 *
 * Only the two neighbours contribute (the triangle is one symbol wide either
 * side), so this is a two-term sum rather than a convolution.
 */
static float complex
mf_at (const float complex *a, size_t n, double t)
{
  long   k0     = (long)floor (t);
  double acc_re = 0.0, acc_im = 0.0;
  for (long k = k0 - 1; k <= k0 + 2; k++)
    {
      if (k < 0 || (size_t)k >= n)
        continue;
      double w = tri (t - (double)k);
      if (w == 0.0)
        continue;
      acc_re += w * (double)crealf (a[k]);
      acc_im += w * (double)cimagf (a[k]);
    }
  return (float)acc_re + (float)acc_im * I;
}

/**
 * @brief Mean detector output at a static timing offset `tau` (in symbols).
 *
 * @param a     Symbol stream.
 * @param n     Symbols.
 * @param tau   Static timing offset, symbols.
 * @param dttl  Non-zero to score DTTL, zero to score Gardner.
 * @return      The S-curve value at `tau`.
 */
static double
scurve_at (const float complex *a, size_t n, double tau, int dttl)
{
  /* One pass to fix the power reference, so it is the loop's converged EMA
     rather than a value that drifts across the sweep. */
  double pwr = 0.0;
  size_t cnt = 0;
  for (size_t k = 2; k + 2 < n; k++)
    {
      float complex on  = mf_at (a, n, (double)k + tau);
      float complex mid = mf_at (a, n, (double)k + tau - 0.5);
      pwr += (double)(crealf (on) * crealf (on) + cimagf (on) * cimagf (on)
                      + crealf (mid) * crealf (mid)
                      + cimagf (mid) * cimagf (mid));
      cnt++;
    }
  pwr /= (double)cnt;

  double sum = 0.0;
  for (size_t k = 2; k + 2 < n; k++)
    {
      float complex on   = mf_at (a, n, (double)k + tau);
      float complex prev = mf_at (a, n, (double)k - 1.0 + tau);
      float complex mid  = mf_at (a, n, (double)k + tau - 0.5);
      sum += dttl ? dttl_ted (mid, on, prev, pwr)
                  : gardner_ted (mid, on - prev, pwr);
    }
  return sum / (double)cnt;
}

/** @brief Central-difference slope of the S-curve at lock. */
static double
slope_at_lock (const float complex *a, size_t n, int dttl)
{
  const double h = 0.02; /* symbols; inside the triangle's linear region */
  return (scurve_at (a, n, h, dttl) - scurve_at (a, n, -h, dttl)) / (2.0 * h);
}

static int
check_case (int m, double amp, int quiet)
{
  static float complex a[NSYM];
  uint32_t             st = 12345u + (uint32_t)m;
  for (size_t k = 0; k < NSYM; k++)
    {
      unsigned ki = rng (&st) % (unsigned)m;
      double   th = 2.0 * M_PI * (double)ki / (double)m
                    + ((m == 4) ? M_PI / 4.0 : 0.0);
      a[k]        = (float)(amp * cos (th)) + (float)(amp * sin (th)) * I;
    }

  double e0_g  = scurve_at (a, NSYM, 0.0, 0);
  double e0_d  = scurve_at (a, NSYM, 0.0, 1);
  double sg    = slope_at_lock (a, NSYM, 0);
  double sd    = slope_at_lock (a, NSYM, 1);
  double ratio = sg != 0.0 ? sd / sg : 0.0;

  int fail = 0;
  /* A detector with a non-zero error AT lock drags the loop off the symbol. */
  if (fabs (e0_g) > 0.02 || fabs (e0_d) > 0.02)
    fail = 1;
  /* Both must steer the same way, or one of them is positive feedback. */
  if (!(sg > 0.0) || !(sd > 0.0))
    fail = 1;
  /* THE gate: the slopes agree, so one `bn` is one bandwidth either way.
     The band is 0.7..1.4 because the two call sites define `pwr_ref`
     differently (|on|^2 vs |on|^2 + |mid|^2), which is worth ~1.22 on its
     own -- tightening past that would be pinning that inconsistency rather
     than the normalisation. It is nowhere near loose enough to admit the
     defect this harness exists for: dividing DTTL by power instead of
     amplitude puts the ratio at 1/A, which is 2.4 at amp 0.42 and unbounded
     as the amplitude falls. */
  if (!(ratio > 0.7 && ratio < 1.4))
    fail = 1;

  if (!quiet)
    printf ("  M=%d amp=%.2f   e(0) gardner=%+.4f dttl=%+.4f   "
            "slope gardner=%6.3f dttl=%6.3f   ratio=%.3f%s\n",
            m, amp, e0_g, e0_d, sg, sd, ratio, fail ? "   <-- FAIL" : "");
  return fail;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;
  /* The amplitude sweep IS the regression test, and one row of it is the
     whole story. Sabotaged back to the power normaliser this harness reports
     ratio 1.389 / 2.778 / 11.112 at amp 1.0 / 0.5 / 0.125 -- a clean 1/A --
     and 1.389 PASSES the band below. So a single measurement at unit
     amplitude cannot see this defect at all, which is exactly how it survived
     being characterised on a unit-amplitude standalone RateSync stream while
     being fatal at the 0.5 a CIC-bounded receiver runs at. Never drop the
     sweep to one amplitude. */
  double amps[NAMP] = { 1.0, 0.5, 0.125 };
  int    ms[2]      = { 2, 4 }; /* DTTL's valid set: rectangular I/Q rails */

  printf ("Timing-TED S-curves: every detector owes the loop one slope\n");
  for (int mi = 0; mi < 2; mi++)
    for (int ai = 0; ai < NAMP; ai++)
      if (check_case (ms[mi], amps[ai], 0))
        fail = 1;

  if (fail)
    {
      fprintf (stderr, "symsync_ted_scurve FAIL: the TED slopes disagree\n");
      return check ? 1 : 0;
    }
  if (check)
    printf (
        "PASS: e(0)=0 and matched positive slope, every M and amplitude\n");
  return 0;
}
