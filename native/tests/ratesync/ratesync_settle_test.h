/**
 * @file ratesync_settle_test.h
 * @brief RateSync run from a stated timing offset, and the error it has left.
 *
 * Shared by the pin (test_ratesync_core.c) and the sweep
 * (native/validation/ratesync_settle.c), so both measure one thing.
 *
 * Noise-free, default bn and zeta unless a caller passes another bn,
 * Gardner. The error of an emitted symbol is the input instant the terminal
 * stage read it at (dp_ratesync_next_read()) less the stimulus' eye, in
 * symbols. One run cannot show a 2% transient: Gardner's own data jitter on
 * a shaped stream is several hundredths of a symbol. So the error is
 * averaged over data patterns at one offset, which leaves the
 * deterministic transient. The start is the mean error at symbol 0 and the
 * residual the mean error at symbol n, both from where the mean ends up,
 * which absorbs the loop's constant offset of about a thousandth of a
 * symbol.
 */
#ifndef RATESYNC_SETTLE_TEST_H
#define RATESYNC_SETTLE_TEST_H

#include "doppler/ratesync/ratesync_core.h"
#include "doppler/ratesync/ratesync_private.h"
#include "dp_tx_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The bound's width in standard errors: a count of comparisons, worked in
   test_ratesync_core.c above `test_settle_time_from_an_offset`. */
#define RS_SETTLE_K 4.0
#define RS_SETTLE_END 450  /* last symbol held to the bound             */
#define RS_SETTLE_TAIL 150 /* symbols after it, whose mean is the final */
#define RS_SETTLE_N (RS_SETTLE_END + RS_SETTLE_TAIL)

/* One run at loop bandwidth `bn`: the timing error of the first `want`
   symbols, unwrapped, and the detector's output beside each when `ted` is
   not NULL. A rectangle is sample-and-hold, so its center is half a symbol
   less half a sample past the origin, and `tau * sps` has to be a whole
   number of samples. Returns 1 when all were emitted. */
static inline int
rs_settle_run (int pulse, double sps, size_t m, double bn, double tau,
               uint32_t seed, size_t want, double *e, double *ted)
{
  dp_tx_cfg_t c = dp_tx_defaults ();
  c.sps         = sps;
  c.beta        = 0.35;
  c.span        = 8;
  c.tau         = tau;
  c.seed        = seed;
  c.nsym        = want + 40u;
  double eye    = ((double)c.span + tau) * sps;
  if (pulse == RATESYNC_PULSE_IANDD)
    {
      c.pulse = DP_TX_NRZ;
      eye += 0.5 * (sps - 1.0);
    }
  size_t               n = 0;
  float _Complex      *x = dp_tx_make (&c, NULL, &n);
  dp_ratesync_state_t *s = dp_ratesync_create (
      sps, pulse, c.beta, c.span, m, 1024, bn, 0.707, RATESYNC_TED_GARDNER);
  size_t ne = 0;
  for (size_t i = 0; x && s && i < n && ne < want; i++)
    {
      const ratesync_loop_t *l    = &s->loop;
      const double           next = dp_ratesync_next_read (s);
      /* one output period, and how many outputs ahead the next strobe is */
      const double per = s->decim / (l->term_rate + l->ctrl);
      const size_t k   = (l->m - l->out_count % l->m) % l->m;
      float _Complex y;
      if (ratesync_step (s, x[i], &y))
        {
          double d = (next + (double)k * per - eye) / sps;
          d -= floor (d + 0.5);
          if (ne)
            d -= floor (d - e[ne - 1] + 0.5);
          if (ted)
            ted[ne] = dp_ratesync_get_timing_error (s);
          e[ne++] = d;
        }
    }
  free (x);
  dp_ratesync_destroy (s);
  return ne == want;
}

typedef struct
{
  double start; /* mean error at symbol 0, from where it ends       */
  double worst; /* largest |mean residual| in [from, RS_SETTLE_END) */
  int    from;  /* ceil(settle_time) of an object at the defaults   */
  int    over;  /* symbols there over 2% of the start + K se        */
} rs_settle_t;

/* `seeds` data patterns at one offset, the loop at `bn`; `from` is read
   from an object at the default bn, which is the figure under test.
   Returns 1, or 0 when a run came up short. */
static inline int
rs_settle_measure (int pulse, double sps, size_t m, double tau, int seeds,
                   rs_settle_t *r)
{
  static double e[RS_SETTLE_N], sum[RS_SETTLE_N], sq[RS_SETTLE_N];
  memset (sum, 0, sizeof sum);
  memset (sq, 0, sizeof sq);
  dp_ratesync_state_t *s = dp_ratesync_create (
      sps, pulse, 0.35, 8, m, 1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  if (!s)
    return 0;
  r->from = (int)ceil (dp_ratesync_get_settle_time (s));
  dp_ratesync_destroy (s);
  for (int k = 0; k < seeds; k++)
    {
      if (!rs_settle_run (pulse, sps, m, 0.01, tau, 7u + 31u * (uint32_t)k,
                          RS_SETTLE_N, e, NULL))
        return 0;
      /* each run against where it ends itself, so the noise of that
         reference is in the standard error; this also takes out a run that
         settled a whole symbol over */
      double end = 0.0;
      for (int n = RS_SETTLE_END; n < RS_SETTLE_N; n++)
        end += e[n] / RS_SETTLE_TAIL;
      for (int n = 0; n < RS_SETTLE_N; n++)
        {
          sum[n] += e[n] - end;
          sq[n] += (e[n] - end) * (e[n] - end);
        }
    }
  r->start = sum[0] / seeds;
  r->over  = 0;
  r->worst = 0.0;
  for (int n = r->from; n < RS_SETTLE_END; n++)
    {
      const double mean = sum[n] / seeds;
      const double se   = sqrt (fabs (sq[n] / seeds - mean * mean) / seeds);
      const double left = fabs (mean);
      r->over += left > 0.02 * fabs (r->start) + RS_SETTLE_K * se;
      if (left > r->worst)
        r->worst = left;
    }
  return 1;
}

#endif /* RATESYNC_SETTLE_TEST_H */
