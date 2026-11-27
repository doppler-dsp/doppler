/**
 * @file ratesync_seed.c
 * @brief Validation: handing a feedforward timing estimate to RateSync's
 *        matched cascade, with the pieces as shipped and nothing in the loop.
 *
 * docs/design/ratesync-timing.md §9.3 says how an estimate reaches the loop:
 * the estimate's `eye` is an index into the input both converters are
 * handed, the tracking converter's group delay says which of its outputs
 * carries that sample, the nearest output is the on-time one (a count), and
 * what is left, under half an output period, goes to the terminal stage as
 * control over one symbol. This harness does exactly that from outside, on
 * the public calls, and measures where the on-time outputs land.
 *
 *   - **Stimulus.** dp_tx_make(): random +-1 symbols, a root-raised cosine
 *     (roll-off 0.35, span 8) or a held rectangle, at a real number of
 *     samples a symbol and a fractional offset `tau`.
 *   - **Estimate.** dp_timing_est_create_matched() on the first block of
 *     `L` symbols past its own lead-in.
 *   - **Tracking cascade.** dp_RateConverter_create_matched() at `m / sps`,
 *     the cascade RateSync builds, fed the same samples through
 *     dp_RateConverter_execute_ctrl_push() with zero control until the
 *     estimate is in hand.
 *   - **Seed.** `j_on = (eye + group_delay) * rate` is the on-time output's
 *     index, not whole. `j* = floor (j_on)` picks the outputs
 *     `j = j* (mod m)`; `r = j_on - j*`, in [0, 1), is moved by one symbol
 *     of control, always a stretch: a control that shortened an interval
 *     below one terminal input would emit two outputs in one call, and a
 *     call has one control.
 *     The terminal stage sets the interval to its next output, `1 / (R +
 *     ctrl)` of its inputs, as each output is emitted (resamp_core.c,
 *     ctrl_push_impl), so `ctrl = -R r / (m + r)` held while `m` outputs
 *     are emitted stretches each interval by `r / m` of itself and the
 *     symbol by `r` outputs, exactly. `R` is the terminal stage's rate.
 *   - **Where an output is.** A second cascade of the same plan takes the
 *     same control and a slow tone in place of the data. The phase of its
 *     output `j` is the input time that output reads, through the cascade
 *     and not from a formula.
 *
 * Columns: `est` is the estimate's error against the stimulus, `seed` is
 * how far the on-time outputs sit from where the estimate pointed (the
 * hand-over alone), `miss` is the on-time outputs against the stimulus
 * (the two together), all in symbols; `r` is the remainder in outputs;
 * `eye` is `mean (|Re y|)^2 / mean (y^2)` over the on-time outputs of the
 * data cascade after the seed, 1 for an open eye, and `off` is the same
 * over the outputs the seed did not pick, shifted half a symbol when
 * `m = 1`.
 *
 * `bank_sps` is the symbol's length in the terminal bank's own samples. The
 * planner decimates to `m` samples a symbol ahead of that stage, so at
 * `m = 1` and a power-of-two `sps` the bank has one sample a symbol and no
 * arm of it is a matched filter: those rows read an eye of 0.72 to 0.90
 * with the timing right. They are printed and not held.
 *
 * `--check` holds every row whose bank has two or more samples a symbol:
 * `|seed|` within SEED_TOL_RECT or SEED_TOL_RRC, and the eye at or above
 * EYE_MIN. The rectangle's bound is the bank's phase step and the probe's
 * own reading; the root-raised cosine's adds how far an arm's
 * low-frequency delay sits from its pulse's center (doppler#3368, 0.005 to
 * 0.009 symbols at span 8), since the probe reads the first and the
 * estimate names the second.
 *
 * Usage:  validate_ratesync_seed [--check]
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/ratesync/ratesync_core.h"
#include "doppler/timing_est/timing_est_core.h"
#include "dp_tx_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define L_SYM 256    /* symbols in the estimate's block           */
#define POST_SYM 400 /* symbols run after the seed                */
#define SKIP_SYM 40  /* of those, left for the bank to turn over  */
#define NPHASE 1024u
#define TONE_CYC 0.02 /* the probe tone, cycles a symbol          */
#define SEED 7u
#define SEED_TOL_RECT 0.002 /* symbols */
#define SEED_TOL_RRC 0.010  /* symbols */
#define EYE_MIN 0.97
#define BANK_SPS_MIN 2.0 /* rows held */

static double
wrap (double v)
{
  return v - floor (v + 0.5);
}

typedef struct
{
  int    pulse; /* RC_PULSE_* */
  double sps;
  size_t m;
  double tau;
} row_t;

typedef struct
{
  double est, seed, miss, r, eye, off, quality, bank_sps;
  int    ok;
} res_t;

/* mean (|Re y|)^2 / mean (Re y ^2) over y[j0], y[j0 + step], ... */
static double
eye_ratio (const float _Complex *y, size_t j0, size_t n, size_t step)
{
  double a = 0.0, p = 0.0;
  size_t c = 0;
  for (size_t j = j0; j < n; j += step, c++)
    {
      double v = creal (y[j]);
      a += fabs (v);
      p += v * v;
    }
  return (c && p > 0.0) ? (a / (double)c) * (a / (double)c) / (p / (double)c)
                        : 0.0;
}

static res_t
run_row (const row_t *w)
{
  res_t                  res  = { 0 };
  const double           rate = (double)w->m / w->sps;
  dp_timing_est_state_t *te
      = dp_timing_est_create_matched (w->sps, w->pulse, 0.35, 8);
  dp_RateConverter_state_t *rd = dp_RateConverter_create_matched (
      rate, 1, w->pulse, 0.35, 8, (double)w->m, NPHASE);
  dp_RateConverter_state_t *rt = dp_RateConverter_create_matched (
      rate, 1, w->pulse, 0.35, 8, (double)w->m, NPHASE);
  if (!te || !rd || !rt)
    return res;

  /* the terminal stage's own rate, as the loop reads it */
  ratesync_loop_t lp;
  memset (&lp, 0, sizeof lp);
  dp_ratesync_loop_bind_cascade (&lp, rd);
  /* the most inputs the seed's symbol can take: half again a symbol */
  const size_t n_ctl = (size_t)(1.5 * w->sps) + 2;

  const size_t n_est
      = (size_t)(((double)te->lead + 4.0 * L_SYM + 2.0) * w->sps / 4.0) + 8;
  const size_t n_all = n_est + n_ctl + (size_t)(POST_SYM * w->sps);

  dp_tx_cfg_t cfg     = dp_tx_defaults ();
  cfg.pulse           = w->pulse == RC_PULSE_RRC ? DP_TX_RRC : DP_TX_NRZ;
  cfg.sps             = w->sps;
  cfg.tau             = w->tau;
  cfg.nsym            = (size_t)((double)n_all / w->sps) + 2 * 8 + 4;
  cfg.seed            = SEED;
  size_t          n_x = 0;
  float _Complex *x   = dp_tx_make (&cfg, NULL, &n_x);
  const size_t    cap = (size_t)((double)n_all * rate) + 64;
  float _Complex *yd  = malloc (cap * sizeof *yd);
  double         *t   = malloc (cap * sizeof *t);
  if (!x || !yd || !t || n_x < n_all)
    goto done;

  /* an eye center of the stimulus, on the input's timeline */
  const double lead_in = 8.0 * w->sps;
  const double truth
      = w->pulse == RC_PULSE_RRC
            ? lead_in + w->tau * w->sps
            /* held: symbol k is the samples from the first at or past
               its edge, sps of them */
            : ceil (lead_in + w->tau * w->sps) + 0.5 * (w->sps - 1.0);

  dp_timing_estimate_t e = dp_timing_est_estimate (te, x, n_est);
  res.quality            = e.quality;
  res.bank_sps           = rd->bank_sps;
  res.est                = wrap ((e.eye - truth) / w->sps);

  const double gd   = dp_RateConverter_get_group_delay (rd);
  const double j_on = (e.eye + gd) * rate;
  const double jst  = floor (j_on);
  res.r             = j_on - jst;
  const double ctrl = -lp.term_rate * res.r / ((double)w->m + res.r);

  /* both cascades, the same control; the probe's phase, unwrapped */
  const double wt = 2.0 * M_PI * TONE_CYC / w->sps;
  size_t       nd = 0, nt = 0, dosed = 0;
  double       ph = 0.0, prev = 0.0;
  float _Complex o[8];
  for (size_t i = 0; i < n_all; i++)
    {
      /* held until m outputs have been emitted under it */
      double c = (i >= n_est && dosed < w->m) ? ctrl : 0.0;
      size_t k = dp_RateConverter_execute_ctrl_push (rd, x[i], c, o, 8);
      if (i >= n_est)
        dosed += k;
      for (size_t q = 0; q < k && nd < cap; q++)
        yd[nd++] = o[q];
      float _Complex tone = CMPLXF ((float)(0.5 * cos (wt * (double)i)),
                                    (float)(0.5 * sin (wt * (double)i)));
      k = dp_RateConverter_execute_ctrl_push (rt, tone, c, o, 8);
      for (size_t q = 0; q < k && nt < cap; q++)
        {
          double a = carg (o[q]);
          if (nt == 0)
            ph = a;
          else
            ph += wrap ((a - prev) / (2.0 * M_PI)) * 2.0 * M_PI;
          prev    = a;
          t[nt++] = ph / wt; /* modulo one tone period, fixed below */
        }
    }
  if (nd != nt || nd < (size_t)((double)(n_est + n_ctl) * rate) + 8)
    goto done;

  /* The unwrap starts while the bank fills, so the probe's time is known
     to a whole number of tone periods: take them out against the plan's
     own delay at an output the fill is long past. */
  {
    const size_t jr  = (size_t)((double)n_est * rate) - 4;
    const double per = w->sps / TONE_CYC;
    const double off
        = floor ((((double)jr / rate - gd) - t[jr]) / per + 0.5) * per;
    for (size_t j = 0; j < nt; j++)
      t[j] += off;
  }

  /* on-time outputs after the seed and the bank's turn-over */
  const size_t j0 = (size_t)((double)(n_est + n_ctl) * rate) + SKIP_SYM * w->m;
  size_t       jf = j0;
  while (((long long)jf - (long long)jst) % (long long)w->m != 0)
    jf++;
  double sm = 0.0, ss = 0.0;
  size_t c = 0;
  for (size_t j = jf; j < nt; j += w->m, c++)
    {
      sm += wrap ((t[j] - truth) / w->sps);
      ss += wrap ((t[j] - e.eye) / w->sps);
    }
  if (!c)
    goto done;
  res.miss = sm / (double)c;
  res.seed = ss / (double)c;
  res.eye  = eye_ratio (yd, jf, nd, w->m);
  res.off  = w->m > 1 ? eye_ratio (yd, jf + w->m / 2, nd, w->m) : 0.0;
  res.ok   = 1;

done:
  free (x);
  free (yd);
  free (t);
  dp_timing_est_destroy (te);
  dp_RateConverter_destroy (rd);
  dp_RateConverter_destroy (rt);
  return res;
}

int
main (int argc, char **argv)
{
  const int           check   = argc > 1 && strcmp (argv[1], "--check") == 0;
  static const double sps_l[] = { 4.0, 5.0, 8.0, 17.333, 32.0, 67.3 };
  static const size_t m_l[]   = { 1, 2, 4 };
  static const double tau_l[] = { -0.2, 0.0, 0.2, 0.37 };
  int                 fail = 0, held = 0, red = 0;

  printf ("# pulse sps m tau bank_sps quality r est seed miss eye off\n");
  for (int pulse = RC_PULSE_IANDD; pulse <= RC_PULSE_RRC; pulse++)
    for (size_t a = 0; a < sizeof sps_l / sizeof *sps_l; a++)
      for (size_t b = 0; b < sizeof m_l / sizeof *m_l; b++)
        for (size_t d = 0; d < sizeof tau_l / sizeof *tau_l; d++)
          {
            row_t w = { pulse, sps_l[a], m_l[b], tau_l[d] };
            /* the held rectangle is written for whole sps */
            if (pulse == RC_PULSE_IANDD && w.sps != floor (w.sps))
              continue;
            res_t r = run_row (&w);
            if (!r.ok)
              {
                printf ("%s %.3f %zu %+.2f  FAILED TO RUN\n",
                        pulse == RC_PULSE_RRC ? "rrc" : "rect", w.sps, w.m,
                        w.tau);
                fail = 1;
                continue;
              }
            printf ("%s %.3f %zu %+.2f %.3f %.3f %+.3f %+.5f %+.5f %+.5f "
                    "%.3f %.3f\n",
                    pulse == RC_PULSE_RRC ? "rrc" : "rect", w.sps, w.m, w.tau,
                    r.bank_sps, r.quality, r.r, r.est, r.seed, r.miss, r.eye,
                    r.off);
            if (r.bank_sps < BANK_SPS_MIN)
              continue;
            held++;
            double tol = pulse == RC_PULSE_RRC ? SEED_TOL_RRC : SEED_TOL_RECT;
            if (!(fabs (r.seed) <= tol) || !(r.eye >= EYE_MIN))
              {
                printf ("FAIL %s sps %.3f m %zu tau %+.2f: seed %+.5f of "
                        "%.3f, eye %.3f of %.2f\n",
                        pulse == RC_PULSE_RRC ? "rrc" : "rect", w.sps, w.m,
                        w.tau, r.seed, tol, r.eye, EYE_MIN);
                red++;
              }
          }
  printf ("# held %d rows, %d red\n", held, red);
  if (check && (red || held < 60))
    fail = 1;
  return fail;
}
