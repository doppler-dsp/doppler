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
 * Asserted, the same in the sweep and --check, the ghost rule's own two
 * claims (async-dsss-receiver.md §8), on every draw: it releases a
 * receiver only when that receiver's carrier sits a whole nonzero number
 * of lattice steps off a stronger holder's (FORCED: off A's; SECOND: off
 * B's own, so a distinct B is released only after the hand-over moved its
 * receiver onto a lattice image), and it never takes one on its parent's
 * carrier (FORCED, within half a step of A at the end). The shipped pool's
 * fine hand-over carries most forced ghosts to A's own carrier, where the
 * rule leaves them (the declared near-far limit, §8.2, doppler#3263), so
 * the rule's release of a k = 2 ghost is pinned on a pool with the refine
 * off, where the seed stays on its lattice image (--check, one draw).
 * Printed, with Wilson 95% intervals: the forced outcomes by k, the
 * counterfactual release rate by B's level, and the pool's own ghosts.
 * --check: 2.8 GHz, FORCED k = 0 and 2 and SECOND at 38.8 and 50 dB-Hz,
 * one draw each, and the refine-off k = 2 draw.
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
#include "doppler/detection/detection_core.h"
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

/* The refine stage: on, the shipped pool; off only for the ghost rule's
   own k = 2 pin in --check. */
static bool gh_refine = true;

static dp_async_dsss_pool_state_t *
gh_pool (const uint8_t *code, double fc, size_t n, double lost_s)
{
  return dp_s415_pool_refine (code, fc, n, lost_s, 1, gh_refine);
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
  double t_code_first_s, t_code_last_s, df_end_hz, df_end_lat;
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
  /* The centered data-copy test: each block less its mean before the
     normalized |sum|, so a code-only run (constant in both streams)
     centers away. At blocks of 100 and 25 symbols, at the lag the
     100-symbol statistic picks; its null measured in the same draw at
     +GH_NULL_SHIFT symbols (five whole frames: the code-only windows stay
     aligned, the data are independent), best over the same +-GH_RHO_LAG;
     the threshold at 25 is that null's mean plus Q_inv(GH_PFA / lags)
     null block sd over sqrt(blocks), and t_cross the first time, s of
     collected symbols, the running mean clears the threshold at its own
     block count (NaN: never). */
  double rc100, rc25, nl100, nl25, ns25, thr25, t_cross;
  size_t nb25;
  /* SECOND, the hand-over read (--handover): receiver 1's hand-over
     source and the Doppler it handed over at, less B's and A's truth
     there, Hz, s past the seed; and at its last tracking step (the step
     before a release, or the run's end) its carrier less B's and A's
     truth in lattice steps and its code phase less theirs in chips,
     wrapped to [-code_len/2, code_len/2). NaN: never tracked. */
  int    ho_src;
  double ho_b_hz, ho_a_hz, t_ho_s, end_b_lat, end_a_lat, end_b_chips,
      end_a_chips, t_end_s;
} gh_t;

/* --handover: SECOND only, every level and carrier, the draws named by
   --seeds (all when empty), and the hand-over columns. */
static int      gh_ho_only;
static uint32_t gh_seeds[16];
static size_t   gh_n_seeds;

/* a - b on a circle of n, in [-n/2, n/2). */
static double
gh_wrap (double a, double b, double n)
{
  const double d = fmod (a - b + 1.5 * n, n);
  return (d < 0.0 ? d + n : d) - 0.5 * n;
}

/* The centered test's null shift, symbols: five frames of 1,000. */
#define GH_NULL_SHIFT 5000
/* Its false-release probability, a decision, before the lags' union. */
#define GH_PFA 1e-3

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

/* Centered blocks of B symbols at one lag: each block's normalized |sum|
   of (s1 - mean) conj (s0 - mean), into v (at most n_max); returns the
   count. s1[i] against s0[i + lag]. */
static size_t
gh_cblocks (const gh_acc_t *a, long lag, size_t B, double *v, size_t n_max)
{
  const long n0 = (long)a->ns[0], n1 = (long)a->ns[1];
  const long lo = lag < 0 ? -lag : 0;
  const long hi = n1 < n0 - lag ? n1 : n0 - lag;
  size_t     nb = 0;
  for (long b = lo; b + (long)B <= hi && nb < n_max; b += (long)B)
    {
      double _Complex m0 = 0.0, m1 = 0.0;
      for (long i = b; i < b + (long)B; i++)
        {
          m1 += a->s[1][i];
          m0 += a->s[0][i + lag];
        }
      m0 /= (double)B;
      m1 /= (double)B;
      double _Complex ab = 0.0;
      double e0 = 0.0, e1 = 0.0;
      for (long i = b; i < b + (long)B; i++)
        {
          const double _Complex x = a->s[1][i] - m1;
          const double _Complex y = a->s[0][i + lag] - m0;
          ab += x * conj (y);
          e1 += creal (x * conj (x));
          e0 += creal (y * conj (y));
        }
      v[nb++] = e0 > 0.0 && e1 > 0.0 ? cabs (ab) / sqrt (e0 * e1) : 0.0;
    }
  return nb;
}

static double
gh_vmean (const double *v, size_t n)
{
  double m = 0.0;
  for (size_t i = 0; i < n; i++)
    m += v[i];
  return n ? m / (double)n : NAN;
}

/* The centered test (gh_t's rc100 ... t_cross). */
static void
gh_rho_centered (const gh_acc_t *a, gh_t *o)
{
  o->rc100 = o->rc25 = o->nl100 = o->nl25 = o->ns25 = o->thr25 = NAN;
  o->t_cross                                                   = NAN;
  o->nb25                                                      = 0;
  const size_t n = a->ns[0] < a->ns[1] ? a->ns[0] : a->ns[1];
  if (n < 400)
    return;
  const size_t nmax = n / 25 + 1;
  double      *v    = dp_xmalloc (nmax * sizeof *v);
  long         best = 0;
  double       bm   = -1.0;
  for (long lag = -GH_RHO_LAG; lag <= GH_RHO_LAG; lag++)
    {
      const double m = gh_vmean (v, gh_cblocks (a, lag, 100, v, nmax));
      if (m > bm)
        {
          bm   = m;
          best = lag;
        }
    }
  o->rc100 = bm;
  o->nb25  = gh_cblocks (a, best, 25, v, nmax);
  o->rc25  = gh_vmean (v, o->nb25);
  /* The null, the same statistic five frames off, best over the lags. */
  double sd25 = NAN;
  if (n > GH_NULL_SHIFT + 2 * GH_RHO_LAG + 400)
    {
      double b100 = -1.0, b25 = -1.0;
      for (long d = -GH_RHO_LAG; d <= GH_RHO_LAG; d++)
        {
          const long   lag = GH_NULL_SHIFT + d;
          const double m1  = gh_vmean (v, gh_cblocks (a, lag, 100, v, nmax));
          if (m1 > b100)
            b100 = m1;
          const size_t k  = gh_cblocks (a, lag, 25, v, nmax);
          const double m2 = gh_vmean (v, k);
          if (m2 > b25)
            {
              b25       = m2;
              double ss = 0.0;
              for (size_t i = 0; i < k; i++)
                ss += (v[i] - m2) * (v[i] - m2);
              sd25 = k > 1 ? sqrt (ss / (double)(k - 1)) : NAN;
            }
        }
      o->nl100 = b100;
      o->nl25  = b25;
      o->ns25  = sd25;
    }
  /* The threshold at the tenure's blocks, and the first crossing of the
     running mean at its own count (from 8 blocks, 200 symbols). */
  const double q = dp_det_q_inv (GH_PFA / (double)(2 * GH_RHO_LAG + 1));
  if (!isnan (o->nl25) && !isnan (sd25) && o->nb25)
    {
      o->thr25         = o->nl25 + q * sd25 / sqrt ((double)o->nb25);
      const size_t k   = gh_cblocks (a, best, 25, v, nmax);
      double       sum = 0.0;
      for (size_t j = 0; j < k; j++)
        {
          sum += v[j];
          const double thr = o->nl25 + q * sd25 / sqrt ((double)(j + 1));
          if (j + 1 >= 8 && sum / (double)(j + 1) > thr)
            {
              o->t_cross = (double)((j + 1) * 25) / DP_S415_SYM;
              break;
            }
        }
    }
  free (v);
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
  gh_rho_centered (a, o);
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
  o->outcome    = p->released_ghost ? 1 : p->released_lost ? 2 : 0;
  o->code_frac  = n ? (double)code_up / (double)n : NAN;
  o->sym_frac   = n ? (double)sym_up / (double)n : NAN;
  o->df_end_lat = o->df_end_hz / p->lattice_hz;
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
  o->ho_src  = -1;
  o->ho_b_hz = o->ho_a_hz = o->t_ho_s = NAN;
  o->end_b_lat = o->end_a_lat = o->end_b_chips = o->end_a_chips = NAN;
  o->t_end_s                                                    = NAN;
  gh_acc_t acc                                                  = { 0 };
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
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          const dp_air_truth_t ta = dp_air_truth (&air, 0, air.n_out);
          const dp_air_truth_t tb = dp_air_truth (&air, 1, air.n_out);
          if (isnan (o->t_ho_s))
            {
              o->ho_src
                  = dp_async_dsss_receiver_get_handover_source (p->rx[1]);
              o->ho_b_hz = s1.doppler_hz - tb.car_hz;
              o->ho_a_hz = s1.doppler_hz - ta.car_hz;
              o->t_ho_s  = t - t_seed;
            }
          o->end_b_lat   = (s1.doppler_hz - tb.car_hz) / p->lattice_hz;
          o->end_a_lat   = (s1.doppler_hz - ta.car_hz) / p->lattice_hz;
          o->end_b_chips = gh_wrap (s1.chip_phase, tb.code_chips, 1023.0);
          o->end_a_chips = gh_wrap (s1.chip_phase, ta.code_chips, 1023.0);
          o->t_end_s     = t - t_seed;
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
      else if (strcmp (argv[i], "--handover") == 0)
        gh_ho_only = 1;
      else if (strcmp (argv[i], "--seeds") == 0 && i + 1 < argc)
        for (char *tok = strtok (argv[++i], ","); tok && gh_n_seeds < 16;
             tok       = strtok (NULL, ","))
          gh_seeds[gh_n_seeds++] = (uint32_t)strtoul (tok, NULL, 10);
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

  if (gh_ho_only)
    {
      /* The hand-over read: which estimate receiver 1 handed B over from,
         how far from B it landed, and where it was when it left. */
      printf ("# handover\nfc_ghz,b_dbhz,seed,outcome,code_frac,lag_r,"
              "ho_src,t_ho_s,ho_b_hz,ho_a_hz,t_end_s,end_b_lat,end_a_lat,"
              "end_b_chips,end_a_chips\n");
      for (int f = 0; f < 2; f++)
        for (int l = 0; l < 5; l++)
          for (uint32_t s = 1; s <= draws; s++)
            {
              int named = gh_n_seeds == 0;
              for (size_t j = 0; j < gh_n_seeds; j++)
                named |= gh_seeds[j] == s;
              if (!named || (s - 1) % shards != shard)
                continue;
              gh_t o;
              DP_REQUIRE (gh_second (code, fcs[f], b_lvls[l], s, &o) == 0);
              printf ("%.1f,%.1f,%u,%d,%.3f,%ld,%d,%.3f,%.2f,%.2f,%.3f,"
                      "%.3f,%.3f,%.3f,%.3f\n",
                      fcs[f] / 1e9, b_lvls[l], s, o.outcome, o.code_frac,
                      o.lag_r, o.ho_src, o.t_ho_s, o.ho_b_hz, o.ho_a_hz,
                      o.t_end_s, o.end_b_lat, o.end_a_lat, o.end_b_chips,
                      o.end_a_chips);
              /* A receiver that reached tracking reports the estimate it
                 was handed over from: one of the three sources. */
              if (!isnan (o.t_ho_s))
                DP_CHECK_MSG (o.ho_src >= ASYNC_DSSS_RX_HO_SEED
                                  && o.ho_src <= ASYNC_DSSS_RX_HO_FINE,
                              "a tracking receiver names its hand-over "
                              "source");
            }
      free (gh_ratios);
      DP_TEST_END ("validate_async_dsss_pool_ghosts");
    }

  if (emit)
    printf ("# forced\nfc_ghz,k,seed,outcome,code_frac,sym_frac,any_k,"
            "code_down0_s,both_down0_s,code_down1_s,both_down1_s,"
            "t_code_first_s,t_code_last_s,df_end_hz,"
            "ratio_med_db,ratio_p10_db,ratio_p90_db,dphi_start,dphi_end,"
            "dphi_spread,acf_start_db,dphi_win_med,rho_coh,rho_nc,n_rho,"
            "rc100,rc25,nl100,nl25,ns25,thr25,nb25,t_cross_s\n");
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
            /* The rule's two claims, on the ghost's carrier at its end
               (its release step, or the run's end). */
            if (o.outcome == 1)
              DP_CHECK_MSG (fabs (o.df_end_lat) >= 0.5,
                            "the ghost rule releases a ghost only off its "
                            "parent's carrier by a whole lattice step");
            if (fabs (o.df_end_lat) < 0.5)
              DP_CHECK_MSG (o.outcome != 1, "the ghost rule never takes a "
                                            "ghost on its parent's carrier");
            if (emit)
              printf ("%.1f,%d,%u,%d,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,"
                      "%.3f,%.1f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.3f,"
                      "%.4f,%.4f,%zu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%zu,%.2f\n",
                      fcs[f] / 1e9, k, s, o.outcome, o.code_frac, o.sym_frac,
                      o.any_k, o.code_down_s[0], o.both_down_s[0],
                      o.code_down_s[1], o.both_down_s[1], o.t_code_first_s,
                      o.t_code_last_s, o.df_end_hz, o.ratio_med_db,
                      o.ratio_p10_db, o.ratio_p90_db, o.dphi_start, o.dphi_end,
                      o.dphi_spread, o.acf_start_db, o.dphi_win_med, o.rho_coh,
                      o.rho_nc, o.n_rho, o.rc100, o.rc25, o.nl100, o.nl25,
                      o.ns25, o.thr25, o.nb25, o.t_cross);
          }
        if (!emit && n)
          {
            printf ("    %.1f GHz, k = %+d: %zu draws\n", fcs[f] / 1e9, k, n);
            gh_rate ("released by the ghost rule", rel, n);
            gh_rate ("freed by the loss release", lost, n);
            gh_rate ("held", held, n);
          }
      }
  if (check)
    {
      /* The ghost rule's own pin: with the refine off the seed stays on
         its lattice image, two steps off A, and the rule releases it. */
      gh_refine = false;
      gh_t o;
      DP_REQUIRE (gh_forced (code, fcs[1], 2, 1, &o) == 0);
      gh_refine = true;
      printf ("  refine off, 2.8 GHz, k = +2: outcome %d, carrier %.2f "
              "lattice steps off A at the end\n",
              o.outcome, o.df_end_lat);
      DP_CHECK_MSG (o.outcome == 1 && fabs (o.df_end_lat - 2.0) < 0.5,
                    "refine off: a k = 2 sidelobe ghost is released by the "
                    "ghost rule");
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
            "lag_r,b_ber,rc100,rc25,nl100,nl25,ns25,thr25,nb25,t_cross_s\n");
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
                      "%.3f,%.2f,%.3f,%.4f,%.4f,%zu,%ld,%.5f,%.4f,%.4f,%.4f,"
                      "%.4f,%.4f,%.4f,%zu,%.2f\n",
                      fcs[f] / 1e9, b_lvls[l], s, o.outcome, o.code_frac,
                      o.any_k, o.ratio_med_db, o.ratio_p10_db, o.ratio_p90_db,
                      o.dphi_start, o.dphi_end, o.dphi_spread, o.acf_start_db,
                      o.dphi_win_med, o.rho_coh, o.rho_nc, o.n_rho, o.lag_r,
                      o.b_ber, o.rc100, o.rc25, o.nl100, o.nl25, o.ns25,
                      o.thr25, o.nb25, o.t_cross);
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
            /* Released only when its receiver no longer sat on B's
               carrier: the hand-over had moved it onto a lattice image
               (§8.2), which the rule correctly takes. */
            if (o.outcome == 1)
              DP_CHECK_MSG (fabs (o.end_b_lat) >= 0.5,
                            "the ghost rule releases a distinct emitter's "
                            "receiver only off that emitter's carrier");
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
