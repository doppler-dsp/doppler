/**
 * @file async_dsss_pool_ghosts.c
 * @brief The pool's ghosts at the 415.1-B-1 point: what the ghost rule
 *        releases, what it leaves, and what admitting k = 0 would cost.
 *
 * The pool's ghost rule (docs/design/async-dsss-receiver.md §8) releases a
 * receiver that tracks a stronger holder's Doppler-offset sidelobe: its
 * carrier still on the holder's, a whole NONZERO number k of lattice steps
 * off, its code lock low. A ghost on its parent's own carrier (k = 0) is
 * left: a true emitter sharing a stronger one's carrier looks the same.
 * The test of that choice is two populations, and a third says where k = 0
 * ghosts come from at all.
 *
 * Stimulus: the use-case scene, native/tests/dp_air_test.h, at 11 MSa/s,
 * 3,000 sym/s, the 275-in-1,000 window; C/N0 at the antenna. The pool is
 * designed at the use case's floor (4 dB Es/N0, 38.8 dB-Hz), its search
 * +/-1,500 Hz at 400 Hz/s through the window, as the specification sets
 * it. A receiver is put on a phase and Doppler by hand
 * (dp_async_dsss_receiver_seed() on the pool's receiver), and every push
 * completes the pool's group, so its control step runs at the end of
 * each.
 *
 * FORCED. A at PARENT_DBHZ; receiver 1 seeded once receiver 0 tracks A
 * with code lock, on A's sidelobe SIDE chips from A's phase (a Gold
 * sidelobe at the in-phase level, 63 of 1023) and on A's carrier plus k
 * lattice steps, k in -2..2, at the C/N0 the search would report for that
 * peak. Run FORCED_S after the seed with lost_confirm_s 2.0. Each ends
 * released by the ghost rule, freed by the loss release, or held; with
 * its code- and symbol-lock fractions while tracking.
 *
 * SECOND. A at 70 dB-Hz and a DISTINCT emitter B on A's carrier (+/-5 Hz),
 * at a drawn code offset, at B_DBHZ from the floor up: receiver 1 seeded
 * on B's own phase and Doppler once receiver 0 tracks A. Run until both
 * rings have been full for a window. Recorded: whether the rule released
 * B, and whether the rule with k = 0 admitted would have
 * (dp__async_dsss_pool_ghost_of_any_k(), the same tests on the same rings
 * -- async_dsss_pool_private.h), and B's code-lock fraction.
 *
 * ALONE. One emitter, a pool of four searching, ALONE_S; every tenure in
 * which a receiver holds a phase more than a chip from the emitter's is a
 * ghost the pool took on its own: its offset in chips and in lattice
 * steps, and how it ended.
 *
 * Asserted, the same in the sweep and --check: the rule never releases a
 * distinct emitter on its parent's carrier (SECOND, every level), and it
 * releases a k = 2 sidelobe ghost (FORCED, k = 2, every draw). Printed,
 * with Wilson 95% intervals: the forced outcomes by k, the counterfactual
 * release rate by B's level, and the pool's own ghosts. --check: 2.8 GHz,
 * FORCED k = 0 and 2 and SECOND at 38.8 and 50 dB-Hz, one draw each.
 *
 * Usage:  validate_async_dsss_pool_ghosts [--check] [--emit] [--draws N]
 *             [--shard i/n] [--forced-s S]  (FORCED's run past the seed,
 *             default GH_FORCED_S) [--second-s S]  (SECOND's, past the
 *             rings' window; default: a window after both are full, at
 *             most GH_SECOND_S)
 */
#include "async_dsss_receiver_common.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_private.h"
#include "dp_air_test.h"
#include "dp_dsss_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GH_SIDE 167u
#define GH_DRAWS 20u
#define GH_FORCED_S 10.0
#define GH_SECOND_S 4.0
#define GH_ALONE_S 4.0
#define GH_PARENT_DBHZ 60.0
#define GH_A2_DBHZ 70.0

static dp_async_dsss_pool_state_t *
gh_pool (const uint8_t *code, double fc, size_t n, double lost_s)
{
  return dp_s415_pool (code, fc, n, lost_s, 1);
}

static dp_air_emitter_t
gh_emitter (const uint8_t *code, double fc, double f0, double cn0,
            double lag_chips, uint32_t seed)
{
  return dp_s415_emitter (code, fc, f0, 0.0, cn0,
                          lag_chips / dp_s415_chip_rate (fc), seed);
}

static long
gh_autocorr (const uint8_t *code, size_t t)
{
  long r = 0;
  for (size_t i = 0; i < 1023; i++)
    r += (code[i] == code[(i + t) % 1023]) ? 1 : -1;
  return r;
}

/* One push: a whole group (the pool's carry is always empty here). */
static int
gh_push (dp_async_dsss_pool_state_t *p, dp_air_t *air, float complex *x)
{
  const size_t want = p->group - p->carry_len;
  DP_REQUIRE (dp_air_next (air, x, want));
  (void)dp_async_dsss_pool_push (p, x, want);
  DP_REQUIRE (p->carry_len == 0);
  return 0;
}

typedef struct
{
  int    outcome; /* 0 held, 1 released as a ghost, 2 freed as lost */
  double code_frac, sym_frac;
  int    any_k; /* the counterfactual took it while the rings were full */
  /* FORCED, per receiver (0 the parent's, 1 the ghost's), from the seed:
     the longest run of whole groups with the code flag down, and with both
     flags down, s; for the ghost, its first and last code-up group after
     the seed (NaN: none) and its carrier's offset from the parent's at the
     end, Hz. The release rules' two candidates read against
     lost_confirm_s. */
  double code_down_s[2], both_down_s[2];
  double t_code_first_s, t_code_last_s, df_end_hz;
  /* Both arms, every GH_RATIO_DT while both receivers track: the second
     receiver's Dll prompt amplitude over the first's, dB (its median and
     10th / 90th percentiles over the run), and their code-phase
     difference, chips in [0, 1023): at the first sample, the last, and its
     spread (largest less smallest, unwrapped about the first), with
     20 log10 |R(dphi)| / 1023 of the code's own autocorrelation at the
     first sample's nearest whole chip. NaN with no sample. */
  double ratio_med_db, ratio_p10_db, ratio_p90_db;
  double dphi_start, dphi_end, dphi_spread, acf_start_db;
  int    no_parent; /* SECOND: receiver 0 never tracked A with code lock
                       inside 3 s; the draw is counted, not scored */
  /* The phase difference's spread within each GH_WIN_S window (unwrapped
     about the window's first sample), the median over the windows, so a
     reading does not grow with the tenure. */
  double dphi_win_med;
  /* The data-copy test (visionary-architect): the second receiver's soft
     symbols against the first's over every push both track, best over a
     +-GH_RHO_LAG symbol lag. rho_coh is |sum s1 conj s0| over sqrt(sum
     |s1|^2 sum |s0|^2), one coherent sum; rho_nc the mean over blocks of
     GH_RHO_BLK symbols of each block's normalized |sum| (a relative carrier
     phase that wanders cancels the coherent sum, not this one). Null,
     independent data: rho_coh about 1/sqrt(n), rho_nc about
     sqrt(pi)/(2 sqrt(GH_RHO_BLK)) = 0.089. n_rho: the symbols it read. */
  double rho_coh, rho_nc;
  size_t n_rho;
  /* SECOND: the code's autocorrelation at B's first phase difference
     (the lag class: 63, -65 or -1 of 1023), and B's own error rate
     against its payload over the symbols it emitted while both tracked
     (best of a +-250 lag, the sign folded; NaN when not scored). */
  long   lag_r;
  double b_ber;
} gh_t;

/* The phase spread's window, s. */
#define GH_WIN_S 2.0
/* The data-copy test's lag search, symbols, and its block. */
#define GH_RHO_LAG 32
#define GH_RHO_BLK 100u

/* The sampling interval of the ratio columns, s. */
#define GH_RATIO_DT 0.1

/* Every sample, for the distribution (--emit's "# ratio" section): the
   arm (0 FORCED, 1 SECOND), k or B's level, the draw, the time past the
   seed, the prompt ratio, the autocorrelation at the sample's phase
   difference, and the difference. */
typedef struct
{
  float    fc_ghz, kl, t, ratio_db, acf_db, dphi;
  int      arm;
  unsigned seed;
} gh_ratio_t;
static gh_ratio_t *gh_ratios;
static size_t      gh_n_ratios, gh_cap_ratios;

/* One run's samples, while it lasts. */
typedef struct
{
  double r[1024];
  size_t nr;
  double next_t, d0, dlast, dmin, dmax;
  int    have;
  /* The window spreads. */
  double wd0, wmin, wmax, wt0, wsp[64];
  size_t nw;
  int    whave;
  /* Both receivers' symbols over the pushes both track. */
  float _Complex *s[2];
  size_t          ns[2], cap;
  uint64_t        first_n; /* the receive sample of the first push */
} gh_acc_t;

/* --forced-s: FORCED's run past the seed, s. */
static double gh_forced_s = GH_FORCED_S;
/* --second-s: SECOND's run past the seed, s; 0 keeps its default, until
   both rings have been full a window, at most GH_SECOND_S. */
static double gh_second_s = 0.0;

/* FORCED's every run, for the histogram (--emit's "# runs" section): one
   record per run that ended, or was still open at the run's end. */
typedef struct
{
  float    fc_ghz;
  int      k, r, both; /* receiver; 0 the code flag down, 1 both down */
  unsigned seed;
  double   len_s;
} gh_run_t;
static gh_run_t *gh_runs;
static size_t    gh_n_runs, gh_cap_runs;

static void
gh_run_add (double fc, int k, unsigned seed, int r, int both, double len_s)
{
  if (len_s <= 0.0)
    return;
  if (gh_n_runs == gh_cap_runs)
    {
      gh_cap_runs = gh_cap_runs ? 2 * gh_cap_runs : 1024;
      gh_runs     = dp_xrealloc (gh_runs, gh_cap_runs * sizeof *gh_runs);
    }
  gh_runs[gh_n_runs++]
      = (gh_run_t){ (float)(fc / 1e9), k, r, both, seed, len_s };
}

static double
gh_acf_db (const uint8_t *code, double dphi)
{
  const size_t lag = (size_t)llround (dphi) % 1023u;
  return 20.0 * log10 (fabs ((double)gh_autocorr (code, lag)) / 1023.0);
}

/* A sample of the ratio columns, when both receivers track and one is
   due. */
static void
gh_sample (const uint8_t *code, const dp_async_dsss_pool_state_t *p,
           const async_dsss_receiver_status_t *s0,
           const async_dsss_receiver_status_t *s1, double fc, int arm,
           double kl, unsigned seed, double t, gh_acc_t *a)
{
  if (s0->state != ASYNC_DSSS_RX_TRACKING
      || s1->state != ASYNC_DSSS_RX_TRACKING || t < a->next_t)
    return;
  a->next_t        = t + GH_RATIO_DT;
  const double pa0 = p->rx[0]->dll->look_prompt_amp;
  const double pa1 = p->rx[1]->dll->look_prompt_amp;
  const double dphi
      = fmod (s1->chip_phase - s0->chip_phase + 2.0 * 1023.0, 1023.0);
  const double ratio = pa0 > 0.0 && pa1 > 0.0 ? 20.0 * log10 (pa1 / pa0) : NAN;
  if (!a->have)
    {
      a->have = 1;
      a->d0   = dphi;
      a->dmin = a->dmax = 0.0;
    }
  double u = dphi - a->d0; /* unwrapped about the first */
  u -= 1023.0 * floor (u / 1023.0 + 0.5);
  a->dmin  = fmin (a->dmin, u);
  a->dmax  = fmax (a->dmax, u);
  a->dlast = dphi;
  if (!a->whave || t - a->wt0 >= GH_WIN_S)
    {
      if (a->whave && a->nw < sizeof a->wsp / sizeof a->wsp[0])
        a->wsp[a->nw++] = a->wmax - a->wmin;
      a->whave = 1;
      a->wt0   = t;
      a->wd0   = dphi;
      a->wmin = a->wmax = 0.0;
    }
  double w = dphi - a->wd0;
  w -= 1023.0 * floor (w / 1023.0 + 0.5);
  a->wmin = fmin (a->wmin, w);
  a->wmax = fmax (a->wmax, w);
  if (!isnan (ratio) && a->nr < sizeof a->r / sizeof a->r[0])
    a->r[a->nr++] = ratio;
  if (gh_n_ratios == gh_cap_ratios)
    {
      gh_cap_ratios = gh_cap_ratios ? 2 * gh_cap_ratios : 4096;
      gh_ratios = dp_xrealloc (gh_ratios, gh_cap_ratios * sizeof *gh_ratios);
    }
  gh_ratios[gh_n_ratios++] = (gh_ratio_t){
    (float)(fc / 1e9),
    (float)kl,
    (float)t,
    (float)ratio,
    (float)gh_acf_db (code, dphi),
    (float)dphi,
    arm,
    seed,
  };
}

static int
gh_cmp (const void *x, const void *y)
{
  const double a = *(const double *)x, b = *(const double *)y;
  return (a > b) - (a < b);
}

/* Both receivers' symbols of the last push, when both track. */
static void
gh_collect (const dp_async_dsss_pool_state_t   *p,
            const async_dsss_receiver_status_t *s0,
            const async_dsss_receiver_status_t *s1, uint64_t n_out,
            gh_acc_t *a)
{
  if (s0->state != ASYNC_DSSS_RX_TRACKING
      || s1->state != ASYNC_DSSS_RX_TRACKING)
    return;
  const size_t m = dp_async_dsss_pool_symbols_max_out (p);
  if (!a->cap)
    a->first_n = n_out;
  for (int r = 0; r < 2; r++)
    {
      if (a->ns[r] + m > a->cap)
        {
          const size_t c = 2 * (a->ns[r] + m) + 4096;
          a->s[0]        = dp_xrealloc (a->s[0], c * sizeof *a->s[0]);
          a->s[1]        = dp_xrealloc (a->s[1], c * sizeof *a->s[1]);
          a->cap         = c;
        }
      a->ns[r]
          += dp_async_dsss_pool_symbols (p, (size_t)r, a->s[r] + a->ns[r], m);
    }
}

/* The data-copy test over the collected symbols: rho_coh and rho_nc at
   their best lag of +-GH_RHO_LAG (s1[i] against s0[i + lag]). */
static void
gh_rho (const gh_acc_t *a, gh_t *o)
{
  o->rho_coh = o->rho_nc = NAN;
  o->n_rho               = 0;
  const size_t n0 = a->ns[0], n1 = a->ns[1];
  if (n0 < 4 * GH_RHO_BLK || n1 < 4 * GH_RHO_BLK)
    return;
  double best_c = 0.0, best_n = 0.0;
  size_t best_len = 0;
  for (long lag = -GH_RHO_LAG; lag <= GH_RHO_LAG; lag++)
    {
      const long lo = lag < 0 ? -lag : 0;
      const long hi = (long)n1 < (long)n0 - lag ? (long)n1 : (long)n0 - lag;
      if (hi - lo < (long)(4 * GH_RHO_BLK))
        continue;
      double _Complex acc = 0.0;
      double e0 = 0.0, e1 = 0.0, nc = 0.0;
      size_t nb = 0;
      for (long b = lo; b + (long)GH_RHO_BLK <= hi; b += (long)GH_RHO_BLK)
        {
          double _Complex ab = 0.0;
          double b0 = 0.0, b1 = 0.0;
          for (long i = b; i < b + (long)GH_RHO_BLK; i++)
            {
              const double _Complex x = a->s[1][i];
              const double _Complex y = a->s[0][i + lag];
              ab += x * conj (y);
              b1 += creal (x * conj (x));
              b0 += creal (y * conj (y));
            }
          acc += ab;
          e0 += b0;
          e1 += b1;
          if (b0 > 0.0 && b1 > 0.0)
            {
              nc += cabs (ab) / sqrt (b0 * b1);
              nb++;
            }
        }
      const double rc
          = e0 > 0.0 && e1 > 0.0 ? cabs (acc) / sqrt (e0 * e1) : 0.0;
      const double rn = nb ? nc / (double)nb : 0.0;
      if (rc > best_c)
        best_c = rc;
      if (rn > best_n)
        {
          best_n   = rn;
          best_len = nb * GH_RHO_BLK;
        }
    }
  o->rho_coh = best_c;
  o->rho_nc  = best_n;
  o->n_rho   = best_len;
}

/* B's error rate against its payload: the symbols receiver 1 emitted while
   both tracked, from the truth's symbol at the first such push, the best of
   a +-250 lag with the sign folded. */
static double
gh_b_ber (const dp_air_t *air, const gh_acc_t *a)
{
  if (a->ns[1] < 1000)
    return NAN;
  const long s0 = dp_air_truth (air, 1, a->first_n).symbol;
  if (s0 < 0)
    return NAN;
  double best = 0.5;
  for (int lag = -250; lag <= 250; lag++)
    {
      size_t errs = 0, cnt = 0;
      for (size_t i = 0; i < a->ns[1]; i++)
        {
          const int b = dp_air_symbol_bit (air, 1, s0 + (long)i + lag);
          if (b < 0)
            continue;
          errs += (crealf (a->s[1][i]) > 0.0f) != (b == 0);
          cnt++;
        }
      if (cnt < a->ns[1] / 2)
        continue;
      const double r = (double)errs / (double)cnt;
      best           = fmin (best, fmin (r, 1.0 - r));
    }
  return best;
}

/* The run's ratio columns from its samples. */
static void
gh_close (const uint8_t *code, gh_acc_t *a, gh_t *o)
{
  o->ratio_med_db = o->ratio_p10_db = o->ratio_p90_db = NAN;
  o->dphi_start = o->dphi_end = o->dphi_spread = o->acf_start_db = NAN;
  if (a->nr)
    {
      qsort (a->r, a->nr, sizeof a->r[0], gh_cmp);
      o->ratio_p10_db = a->r[(size_t)(0.1 * (double)(a->nr - 1))];
      o->ratio_med_db = a->r[(a->nr - 1) / 2];
      o->ratio_p90_db = a->r[(size_t)(0.9 * (double)(a->nr - 1))];
    }
  if (a->have)
    {
      o->dphi_start   = a->d0;
      o->dphi_end     = a->dlast;
      o->dphi_spread  = a->dmax - a->dmin;
      o->acf_start_db = gh_acf_db (code, a->d0);
      o->lag_r        = gh_autocorr (code, (size_t)llround (a->d0) % 1023u);
    }
  o->dphi_win_med = NAN;
  if (a->nw)
    {
      double w[64];
      memcpy (w, a->wsp, a->nw * sizeof w[0]);
      qsort (w, a->nw, sizeof w[0], gh_cmp);
      o->dphi_win_med = w[(a->nw - 1) / 2];
    }
  gh_rho (a, o);
}

static void
gh_acc_free (gh_acc_t *a)
{
  free (a->s[0]);
  free (a->s[1]);
}

/* FORCED: receiver 1 on A's sidelobe at k lattice steps. */
static int
gh_forced (const uint8_t *code, double fc, int k, uint32_t seed, gh_t *o)
{
  const double f0 = 700.0;
  DP_REQUIRE_MSG (gh_autocorr (code, GH_SIDE) == 63,
                  "the forced sidelobe is an in-phase Gold sidelobe");
  dp_async_dsss_pool_state_t *p = gh_pool (code, fc, 2, 2.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t e
      = gh_emitter (code, fc, f0, GH_PARENT_DBHZ, 0.0, 100u + seed);
  dp_air_t       air;
  float complex *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, GH_PARENT_DBHZ,
                           7919u + seed, p->group));
  const double level  = 20.0 * log10 (63.0 / 1023.0);
  double       t_seed = -1.0, t = 0.0;
  long         n = 0, code_up = 0, sym_up = 0;
  const double dt       = (double)p->group / DP_S415_FS; /* one group, s */
  double       run_c[2] = { 0, 0 }, run_b[2] = { 0, 0 };
  memset (o, 0, sizeof *o);
  o->t_code_first_s = o->t_code_last_s = o->df_end_hz = NAN;
  gh_acc_t acc                                        = { 0 };
  for (;;)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      t                                     = (double)air.n_out / DP_S415_FS;
      const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
      const async_dsss_receiver_status_t s1 = dp_async_dsss_pool_status (p, 1);
      if (t_seed < 0.0)
        {
          if (t >= 0.5 && s0.state == ASYNC_DSSS_RX_TRACKING && s0.code_locked
              && s1.state == ASYNC_DSSS_RX_IDLE)
            {
              DP_REQUIRE (dp_async_dsss_receiver_seed (
                              p->rx[1], fmod (s0.chip_phase + GH_SIDE, 1023.0),
                              f0 + k * p->lattice_hz, GH_PARENT_DBHZ + level)
                          == DP_OK);
              t_seed = t;
            }
          DP_REQUIRE_MSG (t < 3.0, "receiver 0 tracks A with code lock");
          continue;
        }
      gh_sample (code, p, &s0, &s1, fc, 0, (double)k, seed, t - t_seed, &acc);
      gh_collect (p, &s0, &s1, air.n_out, &acc);
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          n++;
          code_up += s1.code_locked != 0;
          sym_up += s1.locked != 0;
          o->any_k |= dp__async_dsss_pool_ghost_of_any_k (p, 0, 1);
          if (s1.code_locked)
            {
              if (isnan (o->t_code_first_s))
                o->t_code_first_s = t - t_seed;
              o->t_code_last_s = t - t_seed;
            }
          o->df_end_hz = s1.doppler_hz - s0.doppler_hz;
        }
      /* The runs, both receivers, while each tracks. */
      const async_dsss_receiver_status_t sr[2] = { s0, s1 };
      for (int r = 0; r < 2; r++)
        {
          const int tr = sr[r].state == ASYNC_DSSS_RX_TRACKING;
          const int cd = tr && !sr[r].code_locked;
          const int bd = cd && !sr[r].locked;
          if (!cd)
            gh_run_add (fc, k, seed, r, 0, run_c[r]);
          if (!bd)
            gh_run_add (fc, k, seed, r, 1, run_b[r]);
          run_c[r]          = cd ? run_c[r] + dt : 0.0;
          run_b[r]          = bd ? run_b[r] + dt : 0.0;
          o->code_down_s[r] = fmax (o->code_down_s[r], run_c[r]);
          o->both_down_s[r] = fmax (o->both_down_s[r], run_b[r]);
        }
      if (s1.state == ASYNC_DSSS_RX_IDLE || t - t_seed > gh_forced_s)
        break;
    }
  for (int r = 0; r < 2; r++) /* the runs still open at the end */
    {
      gh_run_add (fc, k, seed, r, 0, run_c[r]);
      gh_run_add (fc, k, seed, r, 1, run_b[r]);
    }
  o->outcome   = p->released_ghost ? 1 : p->released_lost ? 2 : 0;
  o->code_frac = n ? (double)code_up / (double)n : NAN;
  o->sym_frac  = n ? (double)sym_up / (double)n : NAN;
  gh_close (code, &acc, o);
  o->b_ber = NAN;
  gh_acc_free (&acc);
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* SECOND: a distinct emitter B on A's carrier at `b_dbhz`. */
static int
gh_second (const uint8_t *code, double fc, double b_dbhz, uint32_t seed,
           gh_t *o)
{
  uint32_t     st   = trial_rng (seed);
  const double f0   = 700.0;
  const double df   = -5.0 + 10.0 * dp_uni (&st);
  const double lagc = 100.0 + 800.0 * dp_uni (&st); /* B's code offset */
  dp_async_dsss_pool_state_t *p = gh_pool (code, fc, 2, 0.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t em[2] = {
    gh_emitter (code, fc, f0, GH_A2_DBHZ, 0.0, 100u + seed),
    gh_emitter (code, fc, f0 + df, b_dbhz, lagc, 200u + seed),
  };
  /* Each carries its own payload, from its own stream, so their data are
     independent (the data-copy test's null), and B's error rate is read
     against its own. */
  const size_t n_bits = (size_t)(DP_S415_SYM * 40.0);
  uint8_t     *pay[2] = { dp_xmalloc (n_bits), dp_xmalloc (n_bits) };
  for (int e = 0; e < 2; e++)
    {
      uint32_t ps = 0x9a1u + 7919u * (uint32_t)e + 104729u * seed;
      for (size_t i = 0; i < n_bits; i++)
        pay[e][i] = (uint8_t)(dp_xs32 (&ps) & 1u);
      em[e].bits   = pay[e];
      em[e].n_bits = n_bits;
    }
  dp_air_t       air;
  float complex *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, em, 2, DP_S415_FS, GH_A2_DBHZ, 7919u + seed,
                           p->group));
  double t_seed = -1.0, t = 0.0, t_full = -1.0;
  long   n = 0, code_up = 0;
  memset (o, 0, sizeof *o);
  gh_acc_t acc = { 0 };
  for (;;)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      t                                     = (double)air.n_out / DP_S415_FS;
      const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
      const async_dsss_receiver_status_t s1 = dp_async_dsss_pool_status (p, 1);
      if (t_seed < 0.0)
        {
          if (t >= 0.5 && s0.state == ASYNC_DSSS_RX_TRACKING && s0.code_locked
              && s1.state == ASYNC_DSSS_RX_IDLE)
            {
              /* B's own phase at the pool's next sample, as a code NCO
                 reads it (half a sample before the continuous phase). */
              const dp_air_truth_t tb = dp_air_truth (&air, 1, air.n_out);
              DP_REQUIRE (!isnan (tb.code_chips));
              DP_REQUIRE (
                  dp_async_dsss_receiver_seed (
                      p->rx[1],
                      fmod (tb.code_chips - 0.5 * em[1].chip_rate / DP_S415_FS
                                + 1023.0,
                            1023.0),
                      tb.car_hz, b_dbhz)
                  == DP_OK);
              t_seed = t;
            }
          /* A draw whose parent never tracks with code lock is counted,
             not scored, so one does not end the sweep's population. */
          if (t >= 3.0)
            {
              o->no_parent = 1;
              break;
            }
          continue;
        }
      gh_sample (code, p, &s0, &s1, fc, 1, b_dbhz, seed, t - t_seed, &acc);
      gh_collect (p, &s0, &s1, air.n_out, &acc);
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          n++;
          code_up += s1.code_locked != 0;
          o->any_k |= dp__async_dsss_pool_ghost_of_any_k (p, 0, 1);
          if (t_full < 0.0 && p->hist_len[0] == p->ghost_window
              && p->hist_len[1] == p->ghost_window)
            t_full = t;
        }
      if (s1.state == ASYNC_DSSS_RX_IDLE
          || (gh_second_s <= 0.0 && t_full >= 0.0 && t - t_full > 1.0)
          || t - t_seed > (gh_second_s > 0.0 ? gh_second_s : GH_SECOND_S))
        break;
    }
  o->outcome   = o->no_parent        ? -1
                 : p->released_ghost ? 1
                 : p->released_lost  ? 2
                                     : 0;
  o->code_frac = n ? (double)code_up / (double)n : NAN;
  gh_close (code, &acc, o);
  o->b_ber = gh_b_ber (&air, &acc);
  gh_acc_free (&acc);
  free (pay[0]);
  free (pay[1]);
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* ALONE: one emitter at the parent's level, a pool of four searching. A
   tenure is a receiver's run in tracking; one whose code phase is ever
   more than a chip from the emitter's is a ghost the pool took on its own.
   Counts them, and how each ended: by the release counters' change at the
   step it left tracking. */
typedef struct
{
  size_t tenures, ghosts, by_ghost, by_lost, by_dup, open;
} gh_alone_t;

static int
gh_alone (const uint8_t *code, double fc, uint32_t seed, gh_alone_t *o,
          int emit)
{
  uint32_t                    st = trial_rng (seed);
  dp_async_dsss_pool_state_t *p  = gh_pool (code, fc, 4, 2.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t e = gh_emitter (code, fc, -1200.0 + 2400.0 * dp_uni (&st),
                                   GH_PARENT_DBHZ, 0.0, 300u + seed);
  dp_air_t         air;
  float complex   *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, GH_PARENT_DBHZ,
                           6007u + seed, p->group));
  int      in[4] = { 0 }, off[4] = { 0 };
  double   worst[4] = { 0 };
  uint64_t g0 = 0, l0 = 0, d0 = 0;
  memset (o, 0, sizeof *o);
  while ((double)air.n_out / DP_S415_FS < GH_ALONE_S)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      const dp_air_truth_t t = dp_air_truth (&air, 0, air.n_out);
      for (size_t r = 0; r < 4; r++)
        {
          const async_dsss_receiver_status_t s
              = dp_async_dsss_pool_status (p, r);
          const int tracking = s.state == ASYNC_DSSS_RX_TRACKING;
          if (tracking && !isnan (t.code_chips))
            {
              double d
                  = fmod (s.chip_phase - t.code_chips + 1023.0 * 2.5, 1023.0)
                    - 511.5;
              d = fabs (d);
              if (!in[r])
                {
                  in[r]    = 1;
                  off[r]   = 0;
                  worst[r] = 0.0;
                  o->tenures++;
                }
              if (d > 1.0)
                off[r] = 1;
              if (d > worst[r])
                worst[r] = d;
            }
          else if (in[r] && !tracking)
            {
              in[r] = 0;
              if (off[r])
                {
                  o->ghosts++;
                  o->by_ghost += p->released_ghost > g0;
                  o->by_lost += p->released_lost > l0;
                  o->by_dup += p->released_duplicate > d0;
                  if (emit)
                    printf ("%.1f,%u,%zu,%.1f,%llu\n", fc / 1e9, seed, r,
                            worst[r], (unsigned long long)air.n_out);
                }
            }
        }
      g0 = p->released_ghost;
      l0 = p->released_lost;
      d0 = p->released_duplicate;
    }
  for (size_t r = 0; r < 4; r++)
    if (in[r] && off[r])
      {
        o->ghosts++;
        o->open++;
      }
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

static void
gh_rate (const char *what, size_t k, size_t n)
{
  double lo, hi;
  dp_wilson ((double)k, (double)n, &lo, &hi);
  printf ("      %-30s %3zu of %zu [%.2f, %.2f]\n", what, k, n, lo, hi);
}

int
main (int argc, char **argv)
{
  int      check = 0, emit = 0;
  unsigned draws = GH_DRAWS, shard = 0, shards = 1;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--draws") == 0 && i + 1 < argc)
        draws = (unsigned)strtoul (argv[++i], NULL, 10);
      else if (strcmp (argv[i], "--forced-s") == 0 && i + 1 < argc)
        gh_forced_s = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--second-s") == 0 && i + 1 < argc)
        gh_second_s = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--shard") == 0 && i + 1 < argc)
        DP_REQUIRE (sscanf (argv[++i], "%u/%u", &shard, &shards) == 2
                    && shards > 0 && shard < shards);
    }
  if (check)
    draws = 1;
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  static const double fcs[2]    = { DP_S415_FC_LO, DP_S415_FC_HI };
  static const double b_lvls[5] = { 38.8, 42.0, 44.0, 46.0, 50.0 };

  if (emit)
    printf ("# forced\nfc_ghz,k,seed,outcome,code_frac,sym_frac,any_k,"
            "code_down0_s,both_down0_s,code_down1_s,both_down1_s,"
            "t_code_first_s,t_code_last_s,df_end_hz,"
            "ratio_med_db,ratio_p10_db,ratio_p90_db,dphi_start,dphi_end,"
            "dphi_spread,acf_start_db,dphi_win_med,rho_coh,rho_nc,n_rho\n");
  else
    printf ("ghosts: 415.1-B-1, 11 MSa/s, a pool designed at 38.8 dB-Hz; "
            "FORCED on a %.0f dB-Hz parent's %u-chip sidelobe, %.0f s\n",
            GH_PARENT_DBHZ, GH_SIDE, gh_forced_s);
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int k = -2; k <= 2; k++)
      {
        if (check && k != 0 && k != 2)
          continue;
        size_t n = 0, rel = 0, lost = 0, held = 0;
        for (uint32_t s = 1; s <= draws; s++)
          {
            if ((s - 1) % shards != shard)
              continue;
            gh_t o;
            DP_REQUIRE (gh_forced (code, fcs[f], k, s, &o) == 0);
            n++;
            rel += o.outcome == 1;
            lost += o.outcome == 2;
            held += o.outcome == 0;
            if (k == 2)
              DP_CHECK_MSG (o.outcome == 1, "a k = 2 sidelobe ghost is "
                                            "released by the ghost rule");
            if (k == 0)
              DP_CHECK_MSG (o.outcome != 1, "the ghost rule never takes a "
                                            "k = 0 ghost");
            if (emit)
              printf ("%.1f,%d,%u,%d,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,"
                      "%.3f,%.1f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.3f,"
                      "%.4f,%.4f,%zu\n",
                      fcs[f] / 1e9, k, s, o.outcome, o.code_frac, o.sym_frac,
                      o.any_k, o.code_down_s[0], o.both_down_s[0],
                      o.code_down_s[1], o.both_down_s[1], o.t_code_first_s,
                      o.t_code_last_s, o.df_end_hz, o.ratio_med_db,
                      o.ratio_p10_db, o.ratio_p90_db, o.dphi_start, o.dphi_end,
                      o.dphi_spread, o.acf_start_db, o.dphi_win_med, o.rho_coh,
                      o.rho_nc, o.n_rho);
          }
        if (!emit && n)
          {
            printf ("    %.1f GHz, k = %+d: %zu draws\n", fcs[f] / 1e9, k, n);
            gh_rate ("released by the ghost rule", rel, n);
            gh_rate ("freed by the loss release", lost, n);
            gh_rate ("held", held, n);
          }
      }
  if (emit)
    {
      printf ("# runs\nfc_ghz,k,seed,receiver,both,len_s\n");
      for (size_t i = 0; i < gh_n_runs; i++)
        printf ("%.1f,%d,%u,%d,%d,%.4f\n", gh_runs[i].fc_ghz, gh_runs[i].k,
                gh_runs[i].seed, gh_runs[i].r, gh_runs[i].both,
                gh_runs[i].len_s);
    }
  free (gh_runs);

  if (emit)
    printf ("# second\nfc_ghz,b_dbhz,seed,outcome,code_frac,any_k,"
            "ratio_med_db,ratio_p10_db,ratio_p90_db,dphi_start,dphi_end,"
            "dphi_spread,acf_start_db,dphi_win_med,rho_coh,rho_nc,n_rho,"
            "lag_r,b_ber\n");
  else
    printf ("  SECOND: a distinct emitter on a %.0f dB-Hz one's carrier\n",
            GH_A2_DBHZ);
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int l = 0; l < 5; l++)
      {
        if (check && l != 0 && l != 4)
          continue;
        size_t n = 0, rel = 0, would = 0, orphan = 0;
        for (uint32_t s = 1; s <= draws; s++)
          {
            if ((s - 1) % shards != shard)
              continue;
            gh_t o;
            DP_REQUIRE (gh_second (code, fcs[f], b_lvls[l], s, &o) == 0);
            if (emit)
              printf ("%.1f,%.1f,%u,%d,%.3f,%d,%.2f,%.2f,%.2f,%.3f,%.3f,"
                      "%.3f,%.2f,%.3f,%.4f,%.4f,%zu,%ld,%.5f\n",
                      fcs[f] / 1e9, b_lvls[l], s, o.outcome, o.code_frac,
                      o.any_k, o.ratio_med_db, o.ratio_p10_db, o.ratio_p90_db,
                      o.dphi_start, o.dphi_end, o.dphi_spread, o.acf_start_db,
                      o.dphi_win_med, o.rho_coh, o.rho_nc, o.n_rho, o.lag_r,
                      o.b_ber);
            /* Counted, not scored: the parent never tracked A. */
            DP_CHECK_MSG (!o.no_parent, "receiver 0 tracks A with code lock");
            if (o.no_parent)
              {
                orphan++;
                continue;
              }
            n++;
            rel += o.outcome == 1;
            would += (size_t)o.any_k;
            DP_CHECK_MSG (o.outcome != 1, "the ghost rule never releases a "
                                          "distinct emitter on its "
                                          "parent's carrier");
          }
        if (!emit && n)
          {
            printf ("    %.1f GHz, B at %.1f dB-Hz: %zu draws\n", fcs[f] / 1e9,
                    b_lvls[l], n);
            gh_rate ("released by the rule", rel, n);
            gh_rate ("released if k = 0 were admitted", would, n);
          }
        if (!emit && orphan)
          printf ("    %.1f GHz, B at %.1f dB-Hz: %zu draws whose parent "
                  "never tracked A with code lock (counted, not scored)\n",
                  fcs[f] / 1e9, b_lvls[l], orphan);
      }
  if (emit)
    {
      printf ("# ratio\nfc_ghz,arm,k_or_b_dbhz,seed,t_s,ratio_db,acf_db,"
              "dphi\n");
      for (size_t i = 0; i < gh_n_ratios; i++)
        printf ("%.1f,%d,%.1f,%u,%.2f,%.2f,%.2f,%.3f\n", gh_ratios[i].fc_ghz,
                gh_ratios[i].arm, gh_ratios[i].kl, gh_ratios[i].seed,
                gh_ratios[i].t, gh_ratios[i].ratio_db, gh_ratios[i].acf_db,
                gh_ratios[i].dphi);
    }
  free (gh_ratios);
  if (emit)
    printf ("# alone\nfc_ghz,seed,receiver,worst_chips,ended_at\n");
  else
    printf ("  ALONE: one %.0f dB-Hz emitter, a pool of four, %.0f s\n",
            GH_PARENT_DBHZ, GH_ALONE_S);
  for (int f = check ? 1 : 0; f < 2; f++)
    {
      gh_alone_t a = { 0 };
      size_t     n = 0;
      for (uint32_t s = 1; s <= draws; s++)
        {
          if ((s - 1) % shards != shard)
            continue;
          gh_alone_t o;
          DP_REQUIRE (gh_alone (code, fcs[f], s, &o, emit) == 0);
          n++;
          a.tenures += o.tenures;
          a.ghosts += o.ghosts;
          a.by_ghost += o.by_ghost;
          a.by_lost += o.by_lost;
          a.by_dup += o.by_dup;
          a.open += o.open;
        }
      if (!emit && n)
        printf ("    %.1f GHz: %zu draws, %zu tenures, %zu off the emitter's "
                "phase: ended by the ghost rule %zu, the loss release %zu, "
                "the duplicate rule %zu, still held %zu\n",
                fcs[f] / 1e9, n, a.tenures, a.ghosts, a.by_ghost, a.by_lost,
                a.by_dup, a.open);
    }
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_pool_ghosts");
  DP_TEST_END ("validate_async_dsss_pool_ghosts");
}
