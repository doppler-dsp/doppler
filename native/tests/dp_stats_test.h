/**
 * @file dp_stats_test.h
 * @brief Interval estimates for the probabilities a harness measures, and
 *        the histograms its reports are drawn from.
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
#include <stdint.h>
#include <stdio.h>

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

/** The most bins a dp_hist_t holds. */
#define DP_HIST_MAX 128u

/**
 * A fixed-edge histogram: @c nb equal bins over [@c lo, @c hi), with the
 * values below and above counted apart, so a tail outside the range is
 * still counted, not lost. NaN is not counted. The bin counts are what a
 * report draws from, so a figure is computed here, in C, from the run.
 */
typedef struct
{
  double   lo, hi;
  size_t   nb;
  uint64_t c[DP_HIST_MAX];
  uint64_t under, over;
} dp_hist_t;

/** Empty @p h over [@p lo, @p hi) in @p nb bins (1 to DP_HIST_MAX). */
static inline void
dp_hist_init (dp_hist_t *h, double lo, double hi, size_t nb)
{
  *h    = (dp_hist_t){ 0 };
  h->lo = lo;
  h->hi = hi;
  h->nb = nb > DP_HIST_MAX ? DP_HIST_MAX : (nb ? nb : 1);
}

/** Count @p v in @p h. */
static inline void
dp_hist_add (dp_hist_t *h, double v)
{
  if (isnan (v))
    return;
  if (v < h->lo)
    h->under++;
  else if (v >= h->hi)
    h->over++;
  else
    {
      size_t b = (size_t)((v - h->lo) / (h->hi - h->lo) * (double)h->nb);
      h->c[b < h->nb ? b : h->nb - 1]++;
    }
}

/**
 * Print @p h as CSV rows `<prefix>,<bin_lo>,<bin_hi>,<count>`, one a bin,
 * the under and over counts as the bins `-inf,lo` and `hi,inf`. Bins with
 * no count are printed too, so a row's absence never reads as a zero.
 */
static inline void
dp_hist_emit (const dp_hist_t *h, const char *prefix)
{
  const double w = (h->hi - h->lo) / (double)h->nb;
  printf ("%s,-inf,%.6g,%llu\n", prefix, h->lo, (unsigned long long)h->under);
  for (size_t b = 0; b < h->nb; b++)
    printf ("%s,%.6g,%.6g,%llu\n", prefix, h->lo + (double)b * w,
            h->lo + (double)(b + 1) * w, (unsigned long long)h->c[b]);
  printf ("%s,%.6g,inf,%llu\n", prefix, h->hi, (unsigned long long)h->over);
}

#endif /* DP_STATS_TEST_H */
