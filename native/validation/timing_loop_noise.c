/**
 * @file timing_loop_noise.c
 * @brief Validation: the timing detector's noise, and the timing error a
 *        closed loop makes of it, against docs/design/symsync.md section 5.
 *
 * The design page sizes the timing loop by
 *
 *     sigma_tau^2 = 2 * bn * S_e(0) / Kd^2          (symbols^2)
 *
 * with `bn` the closed loop's one-sided noise bandwidth as a fraction of
 * the symbol rate, `Kd` the detector's slope at lock and `S_e(0)` the
 * detector noise's spectrum at zero frequency, per symbol. For Gardner on
 * integrate-and-dump BPSK at unit amplitude, with g = Es/N0 (linear):
 *
 *     Kd     = 3/2
 *     R_e(0) = 1.5/g + 1/g^2          (per-symbol variance)
 *     R_e(1) = -0.375/g - 0.125/g^2   (lag one; zero beyond)
 *     S_e(0) = R_e(0) + 2*R_e(1) = (3/4) * (1/g) * (1 + 1/g)
 *
 * This harness measures each term on both shipped timing loops, one pass
 * each: RateSync, whose matched polyphase bank is also its fractional
 * delay, and SymbolSync, which interpolates matched-filtered samples with
 * a Farrow filter. SymbolSync is fed by the same matched bank, held at
 * rate 1, so the two see one filter and one noise.
 *
 * The gain around the loop differs, and `bn` is the delivered bandwidth
 * only when it is 1. RateSync multiplies the raw detector output by
 * `1/Kd`, so its gain is `A^2`. SymbolSync divides it by the running
 * on-time power, so its gain is `Kd * g / (1 + g)` at any level. Each
 * closed-loop row prints the loop filter's law for that gain,
 * `(4*K*zeta^2 + 1) / (4*zeta^2 + 1)`, beside the measurement.
 *
 *   - **Phase 1, the timing truth.** The timing error is the strobe's
 *     position on the input clock less where the transmitter put that
 *     symbol. The position is the resampler's own: the newest input under
 *     the taps plus dp_resamp_get_ctrl_acc(), the fraction its accumulator
 *     holds for the output about to be emitted (what
 *     dp_resamp_execute_ctrl_pos() reports); for SymbolSync it is where
 *     its timing NCO wrapped. Phase 1 has no noise and a
 *     transmit clock `eps` off, so the strobe crosses every input boundary
 *     hundreds of times; a position that were one input out anywhere would
 *     show as a step of 1/8 symbol, and the residual must stay flat.
 *   - **Phase 2, open loop.** `bn = 0`, the strobe at the eye center, noise
 *     at a stated Es/N0. The raw detector output's autocovariance gives
 *     `R_e(0)` and `R_e(1)`; `S_e(0)` is the variance of its block means
 *     times the block length, which assumes no correlation length. The
 *     strobe's own power gives the Es/N0 the detector saw, which holds the
 *     noise scaling. The rectangle is held to the closed form above. The
 *     root-raised cosine has no closed form on the page yet: its `S_e(0)`
 *     is measured here and used below.
 *   - **Phase 3, closed loop.** The same stream with the loop closed. Its
 *     timing error's variance, over phase 2's measured `S_e(0)`, is the
 *     bandwidth the loop delivered:
 *     `Bn = sigma_tau^2 * Kd^2 / (2 * S_e(0))`. The page's relation is
 *     `Bn / bn` equal to the law above. Run on the root-raised cosine.
 *   - **Phase 4, level.** The same at half and at twice the amplitude:
 *     RateSync's law is 0.5 and 3.0, SymbolSync's does not move.
 *   - **Phase 5, the rectangle closed loop.** The same as phase 3 on the
 *     integrate-and-dump pulse. The bank's taps are a sampled rectangle
 *     (RateConverter_core.c, rc_pulse_h), so an arm differs from its
 *     neighbor only in which whole inputs it sums, and in RateSync the
 *     strobe's position within one input interval changes nothing.
 *     SymbolSync's Farrow filter interpolates between the bank's outputs.
 *     This phase prints both; it gates nothing.
 *
 * `Kd` itself is not measured again here: `ratesync_scurve.c` holds each
 * pulse's slope through this cascade against `dp_symsync_ted_slope()`,
 * which is what `ted_scale` inverts.
 *
 * The stimulus is `dp_tx_make()` (the library's PRBS) and the noise is
 * `dp_cgauss()`; nothing is synthesized here.
 *
 * Usage:  timing_loop_noise [--check]
 *         timing_loop_noise --trace DIR   (one CSV a closed-loop scenario:
 *                                          the loop filter's output, its
 *                                          integrator and the timing error
 *                                          at every symbol)
 */
#include "doppler/dp_complex.h"
#include "doppler/ratesync/ratesync_core.h"
#include "doppler/symsync/symsync_core.h"
#include "dp_rng_test.h"
#include "dp_tx_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The receivers' geometry: 8 terminal outputs a symbol. At 8 samples a
   symbol the cascade is the matched resampler alone at rate 1, so every
   cascade input is a terminal input and the strobe's position needs no
   decimation factor. main() refuses to run if the planner builds anything
   else. */
#define SPS 8.0
#define M 8
#define ZETA 0.707
#define BETA 0.35
#define SPAN 8
/* Block length for S_e(0): long against any correlation the pulse's span
   can put in the detector output, short enough for thousands of blocks. */
#define BLOCK 256

/* The object under measurement, set once per pass by main(). RateSync
   fuses the matched filter and the fractional delay in one polyphase bank.
   SymbolSync takes matched-filtered samples and interpolates them with a
   Farrow filter steered by an integer NCO; it is fed here by the same
   matched bank RateSync builds, held at rate 1 with no control, so the
   two see the same filter and the same noise. */
enum
{
  OBJ_RATESYNC = 0,
  OBJ_SYMSYNC  = 1
};
static int g_obj = OBJ_RATESYNC;

/* `--trace` only: when set, every scored strobe writes one row here, the
   symbol index, the loop filter's output and its integrator (both in
   symbols a symbol) and the timing error in symbols. */
static FILE *g_trace = NULL;

typedef struct
{
  int      rrc;  /* 1: root-raised cosine both ends; 0: rectangle       */
  double   amp;  /* symbol amplitude                                    */
  double   esn0; /* Es/N0, linear; <= 0 means no noise                  */
  double   bn;   /* loop bandwidth; 0 opens the loop                    */
  double   rate; /* transmit symbol-spacing scale (1 + eps)             */
  double   tau;  /* transmit timing offset, symbols                     */
  size_t   nsym; /* symbols sent                                        */
  size_t   skip; /* strobes discarded before any statistic              */
  uint32_t seed;
} run_cfg_t;

typedef struct
{
  long   n;         /* strobes scored                                 */
  double on_pwr;    /* mean |on-time|^2                               */
  double on_mag;    /* mean |Re on-time|                              */
  double e_mean;    /* mean raw detector output                       */
  double r0, r1;    /* raw detector autocovariance, lags 0 and 1      */
  double s0;        /* BLOCK * variance of the block means            */
  double tau_var;   /* variance of the wrapped timing error           */
  double tau_max;   /* largest |timing error - its mean|, unwrapped   */
  double ctrl_mean; /* mean control at the strobes                    */
  double ctrl_var;  /* its variance                                   */
  double term_rate; /* the terminal stage's rate the planner chose    */
  double ted_scale; /* 1 / Kd as installed                            */
} run_out_t;

/* Every statistic of one run, from the scored strobes' raw detector
   outputs `e`, timing errors `t` and the sums the loop kept. Frees both. */
static int
finish (run_out_t *o, double *e, double *t, long k, double cs, double cs2)
{
  if (k < 4 * BLOCK)
    {
      free (e);
      free (t);
      return -1;
    }
  o->n = k;
  o->on_pwr /= (double)k;
  o->on_mag /= (double)k;
  o->ctrl_mean = cs / (double)k;
  o->ctrl_var  = cs2 / (double)k - o->ctrl_mean * o->ctrl_mean;

  double em = 0.0;
  for (long j = 0; j < k; j++)
    em += e[j];
  em /= (double)k;
  o->e_mean = em;
  double a0 = 0.0, a1 = 0.0;
  for (long j = 1; j < k; j++)
    {
      a0 += (e[j] - em) * (e[j] - em);
      a1 += (e[j] - em) * (e[j - 1] - em);
    }
  o->r0 = a0 / (double)(k - 1);
  o->r1 = a1 / (double)(k - 1);

  /* S_e(0) without choosing a lag count: the mean of BLOCK outputs has
     variance S_e(0) / BLOCK once BLOCK is long against the correlation. */
  long   nb = k / BLOCK;
  double bs = 0.0;
  for (long b = 0; b < nb; b++)
    {
      double m = 0.0;
      for (long j = 0; j < BLOCK; j++)
        m += e[b * (long)BLOCK + j] - em;
      m /= (double)BLOCK;
      bs += m * m;
    }
  o->s0 = (double)BLOCK * bs / (double)nb;

  /* The timing error about its own mean. A whole-symbol slip moves the
     loop to the next stable zero, one symbol away, so the variance is
     taken on the error wrapped to +-0.5 about where it started;
     `tau_max` keeps the unwrapped excursion, which is what shows a slip. */
  double c0   = 0.0;
  long   head = k < 2000 ? k : 2000;
  for (long j = 0; j < head; j++)
    c0 += t[j];
  c0 /= (double)head;
  double s1 = 0.0, s2 = 0.0, mx = 0.0;
  for (long j = 0; j < k; j++)
    {
      double w = t[j] - c0;
      if (fabs (w) > mx)
        mx = fabs (w);
      w -= floor (w + 0.5);
      s1 += w;
      s2 += w * w;
    }
  s1 /= (double)k;
  o->tau_var = s2 / (double)k - s1 * s1;
  o->tau_max = mx;
  free (e);
  free (t);
  return 0;
}

/* SymbolSync's pass of run_one(). The strobe's position: the on-time
   interpolant is taken when the timing NCO wraps, `phase / phase_inc` of an
   input interval before the sample just pushed (symsync_core.h,
   symsync_step_ted: `mu = 1 - phase / inc`), with the increment as it stood
   before the step. The raw detector output is the error times the running
   power the step divided it by. Takes ownership of `x`. */
static int
run_symsync (const run_cfg_t *c, run_out_t *o, float _Complex *x, size_t n)
{
  dp_RateConverter_state_t *mf = dp_RateConverter_create_matched (
      1.0, 1, c->rrc ? RC_PULSE_RRC : RC_PULSE_IANDD, BETA, SPAN, (double)M,
      1024);
  dp_symsync_state_t *ss = dp_symsync_create (
      (size_t)SPS, c->bn, ZETA, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
  size_t  cap = c->nsym + 8;
  double *e   = (double *)malloc (cap * sizeof *e);
  double *t   = (double *)malloc (cap * sizeof *t);
  if (!x || !mf || !ss || !e || !t)
    {
      free (x);
      free (e);
      free (t);
      if (mf)
        dp_RateConverter_destroy (mf);
      if (ss)
        dp_symsync_destroy (ss);
      return -1;
    }
  o->term_rate = 1.0;
  o->ted_scale = 0.0; /* SymbolSync has none: it divides by a power */

  double   sigma = c->esn0 > 0.0 ? c->amp * sqrt (SPS / c->esn0) : 0.0;
  uint32_t rng   = c->seed * 2654435761u + 12345u;

  long          cnt = 0, k = 0, q = 0;
  double        cs = 0.0, cs2 = 0.0;
  float complex sym, ys[4];
  for (size_t i = 0; i < n; i++)
    {
      float complex xi = x[i];
      if (sigma > 0.0)
        xi += (float)sigma * dp_cgauss (&rng);
      size_t ny = dp_RateConverter_execute_ctrl_push (mf, xi, 0.0, ys, 4);
      for (size_t j = 0; j < ny; j++, q++)
        {
          double inc0 = (double)ss->timing.phase_inc;
          int    got = symsync_step_ted (ss, ys[j], &sym, SYMSYNC_TED_GARDNER);
          if (!got || ++cnt <= (long)c->skip || (size_t)k >= cap)
            continue;
          double pos = (double)q - (double)ss->timing.phase / inc0;
          double ctrl
              = (double)ss->timing.phase_inc / (double)ss->base_inc - 1.0;
          e[k] = ss->last_error * (ss->pwr_avg + 1e-6);
          t[k] = pos / SPS - (double)(cnt - 1) * c->rate;
          if (g_trace)
            fprintf (g_trace, "%ld,%.9e,%.9e,%.6f\n", cnt, ctrl, ss->lf.integ,
                     t[k]);
          cs += ctrl;
          cs2 += ctrl * ctrl;
          o->on_pwr += (double)(crealf (sym) * crealf (sym)
                                + cimagf (sym) * cimagf (sym));
          o->on_mag += fabs ((double)crealf (sym));
          k++;
        }
    }
  dp_RateConverter_destroy (mf);
  dp_symsync_destroy (ss);
  free (x);
  return finish (o, e, t, k, cs, cs2);
}

/* One stream through one RateSync, a sample at a time.

   The strobe's position. The control port emits an output at the
   accumulator's phase and only then loads the offered input
   (resamp_core.c, ctrl_push_impl), so an output emitted while input `i` is
   offered sits at `i - 1 + phase`, with `phase` the accumulator as it stood
   before the call. A call can emit two outputs when the step is under one
   input interval; both steps are the same size, so the second sits halfway
   to the phase the call leaves behind, one wrap on. Which output is the
   strobe is the loop's own count, `out_count % m == 0`. */
static int
run_one (const run_cfg_t *c, run_out_t *o)
{
  memset (o, 0, sizeof *o);
  dp_tx_cfg_t cfg   = dp_tx_defaults ();
  cfg.pulse         = c->rrc ? DP_TX_RRC : DP_TX_NRZ;
  cfg.sps           = SPS;
  cfg.beta          = BETA;
  cfg.span          = SPAN;
  cfg.amp           = c->amp;
  cfg.rate          = c->rate;
  cfg.tau           = c->tau;
  cfg.nsym          = c->nsym;
  cfg.seed          = c->seed;
  size_t          n = 0;
  float _Complex *x = dp_tx_make (&cfg, NULL, &n);
  if (g_obj == OBJ_SYMSYNC)
    return run_symsync (c, o, x, n);
  dp_ratesync_state_t *rs = dp_ratesync_create (
      SPS, c->rrc ? RATESYNC_PULSE_RRC : RATESYNC_PULSE_IANDD, BETA, SPAN, M,
      1024, c->bn, ZETA, RATESYNC_TED_GARDNER);
  size_t  cap = c->nsym + 8;
  double *e   = (double *)malloc (cap * sizeof *e);
  double *t   = (double *)malloc (cap * sizeof *t);
  if (!x || !rs || !e || !t || !rs->loop.term)
    {
      free (x);
      free (e);
      free (t);
      if (rs)
        dp_ratesync_destroy (rs);
      return -1;
    }
  o->term_rate = rs->loop.term_rate;
  o->ted_scale = rs->loop.ted_scale;

  /* Complex noise of variance N0 a sample: a symbol of SPS samples at
     amplitude A has Es = A^2 * SPS, so N0 = A^2 * SPS / (Es/N0). */
  double   sigma = c->esn0 > 0.0 ? c->amp * sqrt (SPS / c->esn0) : 0.0;
  uint32_t rng   = c->seed * 2654435761u + 12345u;

  long          cnt = 0, k = 0;
  double        cs = 0.0, cs2 = 0.0;
  float complex sym;
  for (size_t i = 0; i < n; i++)
    {
      float complex xi = x[i];
      if (sigma > 0.0)
        xi += (float)sigma * dp_cgauss (&rng);
      double p0     = dp_resamp_get_ctrl_acc (rs->loop.term);
      size_t c0     = rs->loop.out_count;
      int    primed = rs->loop.prime_left == 0;
      int    got    = ratesync_step (rs, xi, &sym);
      if (!got || !primed || ++cnt <= (long)c->skip || (size_t)k >= cap)
        continue;
      size_t nout = rs->loop.out_count - c0;
      double pos  = (double)i - 1.0 + p0;
      if (nout == 2 && c0 % M != 0)
        {
          /* The strobe is the second of two: one step past the first. */
          double p2 = dp_resamp_get_ctrl_acc (rs->loop.term);
          pos += (p2 - p0 + 1.0) / 2.0;
        }
      e[k] = rs->loop.last_error / rs->loop.ted_scale;
      /* Timing error in symbols: the strobe against symbol k's place on
         the transmit clock, to a constant. */
      t[k] = pos / SPS - (double)(cnt - 1) * c->rate;
      if (g_trace)
        fprintf (g_trace, "%ld,%.9e,%.9e,%.6f\n", cnt,
                 rs->loop.ctrl / rs->loop.term_rate, rs->loop.lf.integ, t[k]);
      cs += rs->loop.ctrl;
      cs2 += rs->loop.ctrl * rs->loop.ctrl;
      o->on_pwr += (double)(crealf (sym) * crealf (sym)
                            + cimagf (sym) * cimagf (sym));
      o->on_mag += fabs ((double)crealf (sym));
      k++;
    }
  dp_ratesync_destroy (rs);
  free (x);
  return finish (o, e, t, k, cs, cs2);
}

/* Mean and standard error of v[0..n). */
static void
mean_se (const double *v, int n, double *mean, double *se)
{
  double s = 0.0, a = 0.0;
  for (int i = 0; i < n; i++)
    s += v[i];
  s /= (double)n;
  for (int i = 0; i < n; i++)
    a += (v[i] - s) * (v[i] - s);
  *mean = s;
  *se   = n > 1 ? sqrt (a / (double)(n - 1) / (double)n) : 0.0;
}

static uint32_t
seed_of (int i)
{
  return (uint32_t)(7u + 1000u * (unsigned)i);
}

#define MAXSEED 8

/* Phase 2 at one point: the detector noise's parts, each over A^4. */
typedef struct
{
  double r0, r0_se, r1, r1_se, s0, s0_se, esn0_seen;
} open_out_t;

static int
open_loop (int rrc, double amp, double esn0, double tau, size_t nsym,
           int nseed, open_out_t *o)
{
  double r0[MAXSEED], r1[MAXSEED], s0[MAXSEED];
  double pw = 0.0, a4 = amp * amp * amp * amp;
  for (int i = 0; i < nseed; i++)
    {
      run_cfg_t c = { rrc, amp, esn0, 0.0, 1.0, tau, nsym, 64, seed_of (i) };
      run_out_t r;
      if (run_one (&c, &r) != 0)
        return -1;
      r0[i] = r.r0 / a4;
      r1[i] = r.r1 / a4;
      s0[i] = r.s0 / a4;
      pw += r.on_pwr / (amp * amp);
    }
  mean_se (r0, nseed, &o->r0, &o->r0_se);
  mean_se (r1, nseed, &o->r1, &o->r1_se);
  mean_se (s0, nseed, &o->s0, &o->s0_se);
  /* E|on|^2 / A^2 = 1 + 1/(Es/N0) at the eye center. */
  o->esn0_seen = 1.0 / (pw / (double)nseed - 1.0);
  return 0;
}

/* Phases 3 to 5 at one point. */
typedef struct
{
  double var, var_se, max_dev, ctrl_mean, ctrl_var;
} closed_out_t;

static int
closed_loop (int rrc, double amp, double esn0, double bn, double tau,
             size_t nsym, int nseed, closed_out_t *o)
{
  double v[MAXSEED];
  memset (o, 0, sizeof *o);
  /* Twenty loop time constants of settling before anything is scored. */
  size_t skip = (size_t)(20.0 / bn);
  for (int i = 0; i < nseed; i++)
    {
      run_cfg_t c
          = { rrc, amp, esn0, bn, 1.0, tau, nsym + skip, skip, seed_of (i) };
      run_out_t r;
      if (run_one (&c, &r) != 0)
        return -1;
      v[i] = r.tau_var;
      o->ctrl_mean += r.ctrl_mean / (double)nseed;
      o->ctrl_var += r.ctrl_var / (double)nseed;
      if (r.tau_max > o->max_dev)
        o->max_dev = r.tau_max;
    }
  mean_se (v, nseed, &o->var, &o->var_se);
  return 0;
}

/* The transmit offset that puts the open loop's strobe at the eye center:
   the widest eye on the input grid, then, for the smooth pulse, the zero of
   the mean detector output inside that grid cell. The rectangle stays on
   the grid, where its transitions fall on input samples. */
static int
find_lock (int rrc, double *tau, double *eye)
{
  double best = -1.0, t0 = 0.0;
  for (int j = 0; j < (int)SPS; j++)
    {
      run_cfg_t c = { rrc, 1.0, 0.0, 0.0, 1.0, (double)j / SPS, 4000, 64, 7u };
      run_out_t r;
      if (run_one (&c, &r) != 0)
        return -1;
      if (r.on_mag > best)
        {
          best = r.on_mag;
          t0   = (double)j / SPS;
        }
    }
  if (rrc)
    {
      double    lo = t0 - 0.5 / SPS, hi = t0 + 0.5 / SPS;
      run_cfg_t c = { 1, 1.0, 0.0, 0.0, 1.0, lo, 4000, 64, 7u };
      run_out_t r;
      if (run_one (&c, &r) != 0)
        return -1;
      double e_lo = r.e_mean;
      for (int it = 0; it < 24; it++)
        {
          c.tau = 0.5 * (lo + hi);
          if (run_one (&c, &r) != 0)
            return -1;
          if ((r.e_mean > 0.0) == (e_lo > 0.0))
            lo = c.tau;
          else
            hi = c.tau;
        }
      t0    = 0.5 * (lo + hi);
      c.tau = t0;
      if (run_one (&c, &r) != 0)
        return -1;
      best = r.on_mag;
    }
  *tau = t0;
  *eye = best;
  return 0;
}

static double
db (double x)
{
  return 10.0 * log10 (x);
}

/* The gain around the loop, which the loop filter's `bn` assumes is 1.
   RateSync multiplies the raw detector output by a constant, 1/Kd at unit
   amplitude, so its gain is A^2. SymbolSync divides it by the running
   on-time power, A^2 * (1 + 1/g), so its gain is Kd * g / (1 + g) at any
   level. */
static double
loop_gain (double kd, double amp, double esn0)
{
  if (g_obj == OBJ_RATESYNC)
    return amp * amp;
  return esn0 > 0.0 ? kd * esn0 / (1.0 + esn0) : kd;
}

/* The loop filter's law for the bandwidth a loop of gain K delivers. */
static double
bn_law (double kk)
{
  return (4.0 * kk * ZETA * ZETA + 1.0) / (4.0 * ZETA * ZETA + 1.0);
}

/* Every phase, on the object `g_obj` names. Returns the failure count, or
   -1 when a run could not be made. */
static int
measure (int check)
{
  int          fails      = 0;
  const char  *obj        = g_obj == OBJ_RATESYNC ? "RateSync" : "SymbolSync";
  size_t       nsym       = check ? 200000 : 400000;
  int          nseed      = check ? 2 : 6;
  const char  *name[2]    = { "iandd", "rrc" };
  const double esn0_db[3] = { 4.0, 5.0, 10.0 };
  double       tau_lock[2], kd[2], s0_meas[2][3];

  /* ── the eye center, and the geometry everything below assumes ───── */
  for (int p = 0; p < 2; p++)
    {
      double eye;
      if (find_lock (p, &tau_lock[p], &eye) != 0)
        return -1;
      run_cfg_t c = { p, 1.0, 0.0, 0.0, 1.0, tau_lock[p], 4000, 64, 7u };
      run_out_t r;
      if (run_one (&c, &r) != 0)
        return -1;
      kd[p] = dp_symsync_ted_slope (
          SYMSYNC_TED_GARDNER, p ? SYMSYNC_PULSE_RRC : SYMSYNC_PULSE_IANDD,
          BETA, SPAN);
      printf ("%s(sps %.0f, %s, gardner): dp_symsync_ted_slope %.6f\n"
              "  eye center at transmit tau %.5f symbols, mean |symbol| "
              "%.6f, mean detector output %+.2e\n",
              obj, SPS, name[p], kd[p], tau_lock[p], eye, r.e_mean);
      if (g_obj == OBJ_RATESYNC
          && (fabs (r.term_rate - 1.0) > 1e-12
              || fabs (1.0 / r.ted_scale - kd[p]) > 1e-9))
        {
          printf ("FAIL: terminal rate %.6f, 1/ted_scale %.6f: the "
                  "geometry is not the one this harness assumes\n",
                  r.term_rate, 1.0 / r.ted_scale);
          return 1;
        }
    }

  /* ── phase 1: the timing truth, against a known clock offset ─────── */
  printf ("\nPhase 1. Timing truth: no noise, transmit clock off by eps, "
          "bn 0.005, rrc.\n  The strobe's position less the symbol's place "
          "must stay flat while the strobe drifts\n  196 symbols across "
          "the input grid. A position one input out is a step of "
          "0.125.\n");
  printf ("  %10s  %14s  %14s  %12s\n", "eps", "rms residual", "max residual",
          "mean ctrl");
  {
    const double eps[3] = { 1e-3, -1e-3, 0.0 };
    for (int i = 0; i < (check ? 1 : 3); i++)
      {
        run_cfg_t c = { 1,           1.0,    0.0,  0.005, 1.0 + eps[i],
                        tau_lock[1], 200000, 4000, 7u };
        run_out_t r;
        if (run_one (&c, &r) != 0)
          return -1;
        printf ("  %+10.1e  %14.3e  %14.3e  %+12.4e\n", eps[i],
                sqrt (r.tau_var), r.tau_max, r.ctrl_mean);
        /* Half an input: under the 0.125 step a wrong position makes,
           over the pulse's own pattern jitter (0.022 at most, measured). */
        if (r.tau_max > 0.0625)
          {
            printf ("  FAIL: residual %.4f symbols; the position is not "
                    "the strobe's\n",
                    r.tau_max);
            fails++;
          }
      }
  }

  /* ── phase 2: the detector's noise, open loop ────────────────────── */
  printf ("\nPhase 2. Detector noise, open loop at the eye center, unit "
          "amplitude.\n  %zu symbols x %d seeds a row; +- is one standard "
          "error across seeds.\n  S_e(0) is %d times the variance of "
          "%d-symbol block means.\n",
          nsym, nseed, BLOCK, BLOCK);
  printf ("  %-5s  %6s  %9s  %19s  %8s  %19s  %8s  %19s  %8s\n", "pulse",
          "Es/N0", "at strobe", "R(0) measured", "theory", "R(1) measured",
          "theory", "S_e(0) measured", "theory");
  for (int p = 0; p < 2; p++)
    for (int g = 0; g < 3; g++)
      {
        open_out_t o;
        if (check && (g == 1 || (p == 1 && g == 0)))
          continue;
        if (open_loop (p, 1.0, pow (10.0, esn0_db[g] / 10.0), tau_lock[p],
                       nsym, nseed, &o)
            != 0)
          return -1;
        s0_meas[p][g] = o.s0;
        double ig     = pow (10.0, -esn0_db[g] / 10.0);
        double r0_t   = 1.5 * ig + ig * ig;
        double r1_t   = -0.375 * ig - 0.125 * ig * ig;
        double s0_t   = 0.75 * ig * (1.0 + ig);
        if (p == 0)
          printf ("  %-5s  %3.0f dB  %6.2f dB  %9.5f +- %7.5f  %8.5f  "
                  "%9.5f +- %7.5f  %8.5f  %9.5f +- %7.5f  %8.5f\n",
                  name[p], esn0_db[g], db (o.esn0_seen), o.r0, o.r0_se, r0_t,
                  o.r1, o.r1_se, r1_t, o.s0, o.s0_se, s0_t);
        else
          printf ("  %-5s  %3.0f dB  %6.2f dB  %9.5f +- %7.5f  %8s  "
                  "%9.5f +- %7.5f  %8s  %9.5f +- %7.5f  %8s\n",
                  name[p], esn0_db[g], db (o.esn0_seen), o.r0, o.r0_se, "",
                  o.r1, o.r1_se, "", o.s0, o.s0_se, "");
        /* The rectangle: 4% or four standard errors, whichever is wider,
           and the noise scaling to 0.1 dB on either pulse. */
        double tol = fmax (0.04 * s0_t, 4.0 * o.s0_se);
        if ((p == 0 && fabs (o.s0 - s0_t) > tol)
            || fabs (db (o.esn0_seen) - esn0_db[g]) > 0.1)
          {
            printf ("  FAIL: %s at Es/N0 %.0f dB: S_e(0) %.5f against "
                    "%.5f +- %.5f, or the strobe's Es/N0 is off\n",
                    name[p], esn0_db[g], o.s0, s0_t, tol);
            fails++;
          }
      }
  if (!check)
    {
      printf ("  The rectangle at Es/N0 4 dB off the contracted level, "
              "over A^4:\n");
      const double amps[2] = { 0.5, 2.0 };
      for (int a = 0; a < 2; a++)
        {
          open_out_t o;
          if (open_loop (0, amps[a], pow (10.0, 0.4), tau_lock[0], nsym, nseed,
                         &o)
              != 0)
            return -1;
          printf ("    A %.1f: S_e(0)/A^4 %9.5f +- %7.5f, strobe Es/N0 "
                  "%.2f dB\n",
                  amps[a], o.s0, o.s0_se, db (o.esn0_seen));
        }
    }

  /* ── phases 3 and 5: the closed loop's timing error ──────────────── */
  const double bns[3] = { 0.005, 0.002, 0.00068 };
  for (int p = 1; p >= 0; p--)
    {
      if (check && p == 0)
        continue;
      printf ("\nPhase %d. Timing error, closed loop, unit amplitude, "
              "%s.\n  Bn = sigma_tau^2 * Kd^2 / (2 * S_e(0) measured in "
              "phase 2); law is the loop filter's for this object's loop "
              "gain.\n",
              p ? 3 : 5, name[p]);
      printf ("  %6s  %8s  %21s  %9s  %15s  %6s  %8s  %10s  %10s\n", "Es/N0",
              "bn", "sigma_tau^2 measured", "sigma_tau", "Bn/bn", "law",
              "max dev", "mean ctrl", "var ctrl");
      if (!check)
        {
          closed_out_t o;
          if (closed_loop (p, 1.0, 0.0, 0.005, tau_lock[p], nsym, 1, &o) != 0)
            return -1;
          printf ("  %6s  %8.5f  %10.3e  %9s  %9.5f  %15s  %6.3f  %8.3f  "
                  "%+10.3e  %10.3e\n",
                  "none", 0.005, o.var, "", sqrt (o.var), "",
                  bn_law (loop_gain (kd[p], 1.0, 0.0)), o.max_dev, o.ctrl_mean,
                  o.ctrl_var);
        }
      for (int g = 0; g < 3; g += 2)
        for (int b = 0; b < 3; b++)
          {
            if (check && !(g == 2 && b == 0))
              continue;
            closed_out_t o;
            if (closed_loop (p, 1.0, pow (10.0, esn0_db[g] / 10.0), bns[b],
                             tau_lock[p], nsym, nseed, &o)
                != 0)
              return -1;
            double sc    = kd[p] * kd[p] / (2.0 * s0_meas[p][g]) / bns[b];
            double ratio = o.var * sc, rse = o.var_se * sc;
            double law = bn_law (
                loop_gain (kd[p], 1.0, pow (10.0, esn0_db[g] / 10.0)));
            printf ("  %3.0f dB  %8.5f  %10.3e +- %8.1e  %9.5f  %6.3f +- "
                    "%5.3f  %6.3f  %8.3f  %+10.3e  %10.3e\n",
                    esn0_db[g], bns[b], o.var, o.var_se, sqrt (o.var), ratio,
                    rse, law, o.max_dev, o.ctrl_mean, o.ctrl_var);
            if (check)
              {
                double tol = fmax (0.08 * law, 4.0 * rse);
                if (fabs (ratio - law) > tol || o.max_dev > 0.5)
                  {
                    printf ("  FAIL: Bn/bn %.3f is outside %.3f +- %.3f, or "
                            "the loop slipped\n",
                            ratio, law, tol);
                    fails++;
                  }
              }
          }

      /* ── phase 4: the level moves the bandwidth ──────────────────── */
      if (p == 1 && !check)
        {
          printf ("\nPhase 4. Level, closed loop at Es/N0 10 dB, bn "
                  "0.002, rrc.\n  The loop filter's law is Bn/bn = "
                  "(4*K*zeta^2 + 1)/(4*zeta^2 + 1), K this object's loop "
                  "gain.\n");
          printf ("  %5s  %21s  %15s  %8s  %8s\n", "A", "sigma_tau^2 measured",
                  "Bn/bn", "law", "max dev");
          const double amps[3] = { 0.5, 1.0, 2.0 };
          for (int a = 0; a < 3; a++)
            {
              closed_out_t o;
              if (closed_loop (1, amps[a], 10.0, 0.002, tau_lock[1], nsym,
                               nseed, &o)
                  != 0)
                return -1;
              double law = bn_law (loop_gain (kd[1], amps[a], 10.0));
              double sc  = kd[1] * kd[1] / (2.0 * s0_meas[1][2]) / 0.002;
              printf ("  %5.1f  %10.3e +- %8.1e  %6.3f +- %5.3f  %8.3f  "
                      "%8.3f\n",
                      amps[a], o.var, o.var_se, o.var * sc, o.var_se * sc, law,
                      o.max_dev);
            }
        }
    }

  return fails;
}

/* `--trace DIR`: one CSV a scenario, from the first strobe, so the loop
   filter's output can be plotted against time. The scenarios are the
   closed-loop rows of phases 1, 3, 4 and 5, one seed each. */
#define TRACE_NSYM 60000

static int
trace_one (const char *dir, const char *label, const run_cfg_t *c)
{
  char path[1024];
  snprintf (path, sizeof path, "%s/%s_%s_%s.csv", dir,
            g_obj == OBJ_RATESYNC ? "ratesync" : "symsync",
            c->rrc ? "rrc" : "iandd", label);
  g_trace = fopen (path, "w");
  if (!g_trace)
    return -1;
  fprintf (g_trace, "symbol,lf_out,lf_integ,tau\n");
  run_out_t r;
  int       rc = run_one (c, &r);
  fclose (g_trace);
  g_trace = NULL;
  printf ("%s  sigma_tau %.5f  max dev %.3f\n", path, sqrt (r.tau_var),
          r.tau_max);
  return rc;
}

static int
trace_all (const char *dir)
{
  const double esn0_db[2] = { 4.0, 10.0 };
  const double bns[3]     = { 0.005, 0.002, 0.00068 };
  char         label[64];
  for (g_obj = OBJ_RATESYNC; g_obj <= OBJ_SYMSYNC; g_obj++)
    for (int p = 0; p < 2; p++)
      {
        double tau, eye;
        if (find_lock (p, &tau, &eye) != 0)
          return -1;
        run_cfg_t c = { p, 1.0, 0.0, 0.005, 1.0, tau, TRACE_NSYM, 0, 7u };
        if (trace_one (dir, "none_bn0.00500_a1.0_eps0", &c) != 0)
          return -1;
        for (int g = 0; g < 2; g++)
          for (int b = 0; b < 3; b++)
            {
              c.esn0 = pow (10.0, esn0_db[g] / 10.0);
              c.bn   = bns[b];
              snprintf (label, sizeof label, "%.0fdB_bn%.5f_a1.0_eps0",
                        esn0_db[g], bns[b]);
              if (trace_one (dir, label, &c) != 0)
                return -1;
            }
        if (!p)
          continue;
        const double amps[2] = { 0.5, 2.0 };
        for (int a = 0; a < 2; a++)
          {
            run_cfg_t l
                = { 1, amps[a], 10.0, 0.002, 1.0, tau, TRACE_NSYM, 0, 7u };
            snprintf (label, sizeof label, "10dB_bn0.00200_a%.1f_eps0",
                      amps[a]);
            if (trace_one (dir, label, &l) != 0)
              return -1;
          }
        const double eps[2] = { 1e-3, -1e-3 };
        for (int i = 0; i < 2; i++)
          {
            run_cfg_t d
                = { 1, 1.0, 0.0, 0.005, 1.0 + eps[i], tau, TRACE_NSYM, 0, 7u };
            snprintf (label, sizeof label, "none_bn0.00500_a1.0_eps%+.0e",
                      eps[i]);
            if (trace_one (dir, label, &d) != 0)
              return -1;
          }
      }
  return 0;
}

int
main (int argc, char **argv)
{
  if (argc > 2 && strcmp (argv[1], "--trace") == 0)
    return trace_all (argv[2]) == 0 ? 0 : 2;
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fails = 0;
  for (g_obj = OBJ_RATESYNC; g_obj <= OBJ_SYMSYNC; g_obj++)
    {
      printf ("%s==== %s ====\n", g_obj ? "\n" : "",
              g_obj == OBJ_RATESYNC ? "RateSync" : "SymbolSync");
      int f = measure (check);
      if (f < 0)
        return 2;
      fails += f;
    }
  printf ("\n%s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
