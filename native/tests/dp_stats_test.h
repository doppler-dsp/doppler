/**
 * @file dp_stats_test.h
 * @brief Interval estimates for the probabilities a harness measures.
 *
 * A probability a caller will rely on -- a Pd, a Pfa, a share of trials --
 * is reported with its interval, not as a point
 * (docs/dev/contributing/adding-algorithms.md, phase 7). The Wilson score
 * interval is the one spelling: it stays inside [0, 1] and keeps its
 * coverage at 0 or n successes, where the normal approximation collapses to
 * a point. It was first written private to native/validation/acq_rate_pd.c;
 * this header is where a second user found it (doppler#2219's rule).
 */
#ifndef DP_STATS_TEST_H
#define DP_STATS_TEST_H

#include <math.h>

/**
 * The 95% Wilson score interval for @p k successes in @p n trials.
 *
 * @param k   Successes, 0 … n.
 * @param n   Trials, at least 1.
 * @param lo  Lower bound, written.
 * @param hi  Upper bound, written.
 */
static inline void
dp_wilson (double k, double n, double *lo, double *hi)
{
  const double z = 1.959964, p = k / n, z2 = z * z;
  const double c = (p + z2 / (2.0 * n)) / (1.0 + z2 / n);
  const double h
      = z * sqrt (p * (1.0 - p) / n + z2 / (4.0 * n * n)) / (1.0 + z2 / n);
  *lo = c - h < 0.0 ? 0.0 : c - h;
  *hi = c + h > 1.0 ? 1.0 : c + h;
}

#endif /* DP_STATS_TEST_H */
