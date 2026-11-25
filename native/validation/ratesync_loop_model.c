/**
 * @file ratesync_loop_model.c
 * @brief Two timing detectors closed around the measured cascade: the
 *        S-curve, the timing error under noise, and the pull-in.
 *
 * A MODEL, not the library's loop: it says what a derivative-bank detector
 * would do before one is built (docs/design/ratesync-timing.md, "The
 * detector that goes with that pulse"), beside Gardner on the bank as
 * built. The input need not hold a whole number of samples a symbol; the
 * output is one strobe a symbol.
 *
 * ## The signal
 *
 * Symbols are rectangles `S` input samples long, `S` any real number. An
 * input sample is the average of the waveform over its own interval, so
 * the sample an edge falls in holds the two symbols in proportion and the
 * symbol timing is continuous in the samples. Complex white noise is added
 * at the input, variance `S / gamma` a sample for an Es/N0 of `gamma`.
 * The integer stages ahead of the bank are the built converter's own,
 * measured as `g[n]` (ratesync_model.h) and applied by convolution.
 *
 * ## The arms
 *
 * With `F(y)` the running sum of `g`, linear between samples, a symbol
 * starting at input time `tau` reaches the bank sample at input time `t`
 * as `q = F(t + 1 - tau) - F(t + 1 - tau - S)`, for any real `tau`.
 *
 *   - optimum arm      h(tau)  = R^-1 q_tau, scaled to unit noise gain
 *   - its derivative   dh/dtau, a central difference in tau, per symbol
 *   - built arm        the converter's own bank row for that position
 *
 * ## The detectors
 *
 *   derivative   e = Re{ conj(y) dy },  y = h's, dy = (dh/dtau)'s, at the
 *                symbol's strobe: one output a symbol
 *   gardner      e = Re{ conj(mid) (on - prev) }, the built arm at the
 *                strobe and half a symbol before it: two outputs a symbol
 *
 * Each is divided by its own slope at lock, measured from its S-curve, so
 * `bn` names the same loop for both. The loop filter is the library's
 * (dp_loop_filter_step); the timing estimate advances `S * (1 - ctrl)`
 * input samples a symbol.
 *
 * ## What it prints, for each plan
 *
 *   1. the S-curve of each detector over one symbol, no noise;
 *   2. open loop at lock: S_e(0) / Kd^2, and so the predicted timing error;
 *   3. closed loop: the timing error measured against that prediction;
 *   4. pull-in from a quarter and from half a symbol off.
 *
 * `--check` runs one non-integer plan short and holds the model's claims:
 * each closed-loop row agrees with its own open-loop prediction, and the
 * derivative detector's figure is the lower of the two at Es/N0 4 dB.
 *
 * Usage:  ratesync_loop_model [--check]
 */

#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/resamp/resamp_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "ratesync_model.h"

#include <stdio.h>
#include <stdlib.h>

#define ZETA 0.707
#define BLK 256
#define NDET 2
enum
{
  DET_GARDNER,
  DET_DERIV
};
static const char *const DET_NAME[NDET] = { "gardner", "derivative" };

typedef struct
{
  /* the cascade */
  double g[GLEN];
  double fp[GLEN]; /* fp[n] = sum of g[m] for 0 <= m < n          */
  size_t d, first;
  long   reach;
  /* the bank */
  const resamp_state_t *rs;
  size_t                nt;
  double                rinv[MAXT * MAXT];
  double                s; /* input samples a symbol, nominal */
  /* where the arms sit for a symbol starting at I0 */
  long   j_opt, j_blt;
  size_t p_blt;
  double kd[NDET];
} model_t;

/* The running sum of g at any real y. */
static double
f_at (const model_t *m, double y)
{
  if (y <= 0.0)
    return 0.0;
  if (y >= (double)(m->reach + 1))
    return m->fp[m->reach + 1];
  long   n = (long)floor (y);
  double f = y - (double)n;
  return m->fp[n] + f * (m->fp[n + 1] - m->fp[n]);
}

/* The pulse of a symbol starting at input time tau, at bank sample j. */
static double
q_at (const model_t *m, long j, double tau)
{
  double t = (double)m->first + (double)j * (double)m->d + 1.0 - tau;
  return f_at (m, t) - f_at (m, t - m->s);
}

/* R^-1 q for bank sample j; returns q'R^-1 q. */
static double
opt_raw (const model_t *m, long j, double tau, double *h)
{
  double q[MAXT], sig = 0.0;
  for (size_t t = 0; t < m->nt; t++)
    q[t] = q_at (m, j - (long)t, tau);
  for (size_t t = 0; t < m->nt; t++)
    {
      h[t] = 0.0;
      for (size_t u = 0; u < m->nt; u++)
        h[t] += m->rinv[t * m->nt + u] * q[u];
      sig += h[t] * q[t];
    }
  return sig;
}

/* The bank sample the optimum arm reads for a symbol starting at tau: of
   the three nearest where the arm sat for a symbol at I0, the one whose
   window keeps the most of the pulse. A short window (six taps for four
   samples a symbol) can hold the whole pulse at only one of them. */
static long
j_for (const model_t *m, double tau)
{
  long   j0   = m->j_opt + lround ((tau - (double)I0) / (double)m->d);
  long   jb   = j0;
  double best = -1.0, h[MAXT];
  for (long j = j0 - 1; j <= j0 + 1; j++)
    {
      double sig = opt_raw (m, j, tau, h);
      if (sig > best)
        {
          best = sig;
          jb   = j;
        }
    }
  return jb;
}

/* The optimum arm and its derivative, per SYMBOL of timing.
 *
 * Both are scaled to unit NOISE gain, h = R^-1 q / sqrt(q'R^-1 q), and the
 * derivative is of that scaled arm. The scale matters: the energy an arm's
 * window keeps of the pulse, q'R^-1 q, changes with where the edge falls on
 * the bank's grid, and |y|^2 peaks at the true timing only when the arm is
 * scaled by its square root. Scaled by the energy itself (unit symbol out),
 * or not at all, the peak is pulled toward the grid positions that keep
 * more of the pulse, which on a whole number of samples a symbol is a
 * standing timing error.
 */
static long
opt_arms (const model_t *m, double tau, double *h, double *hd)
{
  const double del = 0.05; /* input samples either side */
  long         j   = j_for (m, tau);
  double       hp[MAXT], hm[MAXT];
  double       sig = sqrt (opt_raw (m, j, tau, h));
  double       sp  = sqrt (opt_raw (m, j, tau + del, hp));
  double       sm  = sqrt (opt_raw (m, j, tau - del, hm));
  for (size_t t = 0; t < m->nt; t++)
    {
      hd[t] = (hp[t] / sp - hm[t] / sm) / (2.0 * del) * m->s;
      h[t] /= sig;
    }
  return j;
}

/* The built arm for a symbol starting at tau: the row the phase word would
   select, and the bank sample it reads. */
static long
blt_arm (const model_t *m, double tau, const float **row)
{
  double np    = (double)m->rs->num_phases;
  double total = (double)m->p_blt / np + (tau - (double)I0) / (double)m->d;
  double fl    = floor (total);
  size_t p     = (size_t)((total - fl) * np);
  if (p >= m->rs->num_phases)
    p = m->rs->num_phases - 1;
  *row = m->rs->bank + p * m->nt;
  return m->j_blt + (long)fl;
}

static int
model_init (model_t *m, dp_RateConverter_state_t *rc, double s)
{
  memset (m, 0, sizeof *m);
  m->rs = (const resamp_state_t *)rc->stage_ptrs[rc->n_stages - 1];
  m->nt = m->rs->num_taps;
  m->s  = s;
  if (m->nt > MAXT)
    return -1;
  m->d = measure_g (rc, m->g, &m->first);
  if (!m->d)
    return -1;
  for (long n = 0; n < (long)(GLEN - GOFF); n++)
    if (fabs (gat (m->g, n)) > 1e-12)
      m->reach = n;
  for (long n = 0; n <= m->reach; n++)
    m->fp[n + 1] = m->fp[n] + gat (m->g, n);
  /* R^-1, a column a solve */
  double r[MAXT];
  for (size_t k = 0; k < m->nt; k++)
    {
      r[k] = 0.0;
      for (long n = 0; n <= m->reach; n++)
        r[k] += gat (m->g, n) * gat (m->g, n + (long)(k * m->d));
    }
  static double mat[MAXT * MAXT];
  for (size_t c = 0; c < m->nt; c++)
    {
      double col[MAXT];
      for (size_t t = 0; t < m->nt; t++)
        {
          col[t] = t == c ? 1.0 : 0.0;
          for (size_t u = 0; u < m->nt; u++)
            mat[t * m->nt + u] = r[t > u ? t - u : u - t];
        }
      if (solve (mat, col, m->nt) != 0)
        return -1;
      for (size_t t = 0; t < m->nt; t++)
        m->rinv[t * m->nt + c] = col[t];
    }
  /* where each arm sits for a symbol starting at I0 */
  long   j_lo = ((long)I0 - (long)m->first) / (long)m->d - 4;
  long   j_hi = j_lo + ((long)s + m->reach) / (long)m->d + (long)m->nt + 8;
  double best = -1.0, bestb = -1.0, h[MAXT];
  for (long j = j_lo; j <= j_hi; j++)
    {
      double sig = opt_raw (m, j, (double)I0, h);
      if (sig > best)
        {
          best     = sig;
          m->j_opt = j;
        }
      double q[MAXT];
      for (size_t t = 0; t < m->nt; t++)
        q[t] = q_at (m, j - (long)t, (double)I0);
      for (size_t p = 0; p < m->rs->num_phases; p++)
        {
          const float *row = m->rs->bank + p * m->nt;
          double       sg = 0.0, nv = 0.0;
          for (size_t t = 0; t < m->nt; t++)
            {
              sg += (double)row[t] * q[t];
              for (size_t u = 0; u < m->nt; u++)
                nv += (double)row[t] * (double)row[u]
                      * r[t > u ? t - u : u - t];
            }
          double loss = nv > 0.0 ? sg * sg / nv : 0.0;
          if (loss > bestb)
            {
              bestb    = loss;
              m->j_blt = j;
              m->p_blt = p;
            }
        }
    }
  return best > 0.0 && bestb > 0.0 ? 0 : -1;
}

/* ── The stream ──────────────────────────────────────────────────────── */

typedef struct
{
  double _Complex *sj; /* bank samples                     */
  size_t           nj;
  double           t0; /* input time symbol 0 starts      */
  double           s;  /* the symbols' own period, inputs */
  size_t           nsym;
} stream_t;

static int
stream_make (const model_t *m, size_t nsym, double eps, double n0,
             uint32_t seed, stream_t *st)
{
  st->s              = m->s * (1.0 + eps);
  st->t0             = (double)I0 + 0.37; /* an edge inside a sample */
  st->nsym           = nsym;
  size_t          nx = (size_t)(st->t0 + (double)(nsym + 4) * st->s) + 8;
  float _Complex *x  = malloc (nx * sizeof *x);
  st->nj             = (nx - m->first) / m->d;
  st->sj             = malloc (st->nj * sizeof *st->sj);
  if (!x || !st->sj)
    {
      free (x);
      free (st->sj);
      return -1;
    }
  uint32_t rb = seed, rn = seed ^ 0x9E3779B9u;
  double   sg  = sqrt (n0);
  long     k   = -1; /* the symbol the sample starts in */
  float    cur = 0.0f, nxt = (float)dp_bit (&rb);
  double   edge = st->t0; /* where symbol k + 1 starts */
  for (size_t i = 0; i < nx; i++)
    {
      double lo = (double)i, v;
      while (edge <= lo)
        {
          k++;
          cur  = nxt;
          nxt  = (float)dp_bit (&rb);
          edge = st->t0 + (double)(k + 1) * st->s;
        }
      if (edge < lo + 1.0)
        v = (double)cur * (edge - lo) + (double)nxt * (lo + 1.0 - edge);
      else
        v = (double)cur;
      x[i] = (float)v + (float)sg * dp_cgauss (&rn);
    }
  for (size_t j = 0; j < st->nj; j++)
    {
      double _Complex acc = 0.0;
      long top            = (long)m->first + (long)j * (long)m->d;
      for (long n = 0; n <= m->reach && n <= top; n++)
        acc += gat (m->g, n) * (double _Complex)x[top - n];
      st->sj[j] = acc;
    }
  free (x);
  return 0;
}

static double _Complex dot_d (const double *h, const double _Complex *sj,
                              long j, size_t nt)
{
  double _Complex a = 0.0;
  for (size_t t = 0; t < nt; t++)
    a += h[t] * sj[j - (long)t];
  return a;
}

static double _Complex dot_f (const float *h, const double _Complex *sj,
                              long j, size_t nt)
{
  double _Complex a = 0.0;
  for (size_t t = 0; t < nt; t++)
    a += (double)h[t] * sj[j - (long)t];
  return a;
}

/* One detector output for a symbol the receiver believes starts at tau.
   `prev` is Gardner's previous strobe, updated. */
static double
detect (const model_t *m, const stream_t *st, int det, double tau,
        double _Complex *prev)
{
  if (det == DET_DERIV)
    {
      double h[MAXT], hd[MAXT];
      long   j = opt_arms (m, tau, h, hd);
      return creal (conj (dot_d (h, st->sj, j, m->nt))
                    * dot_d (hd, st->sj, j, m->nt));
    }
  const float *row;
  long         j      = blt_arm (m, tau, &row);
  double _Complex on  = dot_f (row, st->sj, j, m->nt);
  long jm             = blt_arm (m, tau - 0.5 * m->s, &row);
  double _Complex mid = dot_f (row, st->sj, jm, m->nt);
  double e            = creal (conj (mid) * (on - *prev));
  *prev               = on;
  return e;
}

/* Open loop at a fixed timing error `off` symbols: the mean output and
   its spectral density at zero. */
static void
open_loop (const model_t *m, const stream_t *st, int det, double off,
           double *mean, double *s0)
{
  double sum = 0.0, bs = 0.0, bm = 0.0, bm2 = 0.0;
  size_t cnt = 0, nb = 0, inb = 0;
  double _Complex prev = 0.0;
  for (size_t k = 8; k + 8 < st->nsym; k++)
    {
      double tau = st->t0 + (double)k * st->s + off * m->s;
      double e   = detect (m, st, det, tau, &prev);
      if (k == 8)
        continue;
      sum += e;
      bs += e;
      cnt++;
      if (++inb == BLK)
        {
          double v = bs / BLK;
          bm += v;
          bm2 += v * v;
          bs  = 0.0;
          inb = 0;
          nb++;
        }
    }
  *mean = sum / (double)cnt;
  if (s0)
    *s0 = nb > 1 ? BLK
                       * (bm2 / (double)nb
                          - (bm / (double)nb) * (bm / (double)nb))
                 : 0.0;
}

typedef struct
{
  double var;     /* timing error variance, symbols^2, wrapped */
  double max_dev; /* largest excursion from where it settled   */
  long   slips;   /* whole symbols gained or lost              */
  long   settled; /* last symbol outside 0.25 of where it ends */
} loop_out_t;

/* The closed loop from symbol k0, starting `off` symbols wrong. */
static void
closed_loop (const model_t *m, const stream_t *st, int det, double bn,
             double off, size_t k0, size_t nrun, size_t skip, loop_out_t *o)
{
  /* Zeroed first: dp_loop_filter_init() sets the gains and leaves the
     integrator alone, and this loop must start with no rate estimate. */
  dp_loop_filter_state_t lf;
  memset (&lf, 0, sizeof lf);
  dp_loop_filter_init (&lf, bn, ZETA, 1.0);
  double tau           = st->t0 + (double)k0 * st->s + off * m->s;
  double _Complex prev = 0.0;
  double  sum = 0.0, sum2 = 0.0, first_err = 0.0;
  size_t  cnt = 0;
  double *err = malloc (nrun * sizeof *err);
  memset (o, 0, sizeof *o);
  if (!err)
    return;
  for (size_t i = 0; i < nrun; i++)
    {
      size_t k = k0 + i;
      /* which symbol the estimate is nearest, and how far from it */
      double x = (tau - st->t0) / st->s - (double)k;
      err[i]   = x;
      /* a loop that has run away would read past the stream */
      if (fabs (x) > 4.0)
        {
          o->slips = 999;
          free (err);
          return;
        }
      double e = detect (m, st, det, tau, &prev) / m->kd[det];
      double c = i ? dp_loop_filter_step (&lf, e) : 0.0;
      tau += m->s * (1.0 - c);
      if (i == skip)
        first_err = x;
      if (i >= skip)
        {
          double w = x - round (x);
          sum += w;
          sum2 += w * w;
          cnt++;
        }
    }
  double end = round (err[nrun - 1]);
  o->slips   = lround (round (err[nrun - 1]) - round (first_err));
  for (size_t i = 0; i < nrun; i++)
    {
      if (fabs (err[i] - end) >= 0.25)
        o->settled = (long)i + 1;
      if (i >= skip && fabs (err[i] - end) > o->max_dev)
        o->max_dev = fabs (err[i] - end);
    }
  if (cnt)
    o->var = sum2 / (double)cnt - (sum / (double)cnt) * (sum / (double)cnt);
  free (err);
}

/* A feedforward estimate of the timing from `len` symbols, with no loop:
 * the likelihood |y|^2 from the unit-noise-gain arm at FF_K hypotheses a
 * symbol, summed over the symbols, and the phase of its first Fourier
 * coefficient across the hypotheses,
 *
 *     c = sum_k sum_i |y_k(tau0 + i*S/FF_K)|^2 exp(-j 2 pi i / FF_K)
 *     tau_hat = tau0 - (S / 2 pi) arg c
 *
 * which is the Oerder and Meyr estimator read off the bank instead of off
 * a squared sample stream. It takes `len` symbols whatever the starting
 * offset, and it has no half-symbol point to hang at. Returns the
 * estimate's error in symbols, wrapped to [-0.5, 0.5). */
#define FF_K 4
static double
feedforward (const model_t *m, const stream_t *st, size_t k0, size_t len,
             double off)
{
  double _Complex c = 0.0;
  for (size_t i = 0; i < len; i++)
    {
      double tau0 = st->t0 + (double)(k0 + i) * st->s + off * m->s;
      for (int q = 0; q < FF_K; q++)
        {
          double h[MAXT], hd[MAXT];
          double tau        = tau0 + (double)q * m->s / FF_K;
          long   j          = opt_arms (m, tau, h, hd);
          double _Complex y = dot_d (h, st->sj, j, m->nt);
          double p          = creal (y) * creal (y) + cimag (y) * cimag (y);
          c += p * cexp (-I * 2.0 * M_PI * (double)q / FF_K);
        }
    }
  /* the likelihood peaks where the hypothesis meets the symbol, which is
     -off symbols from tau0 */
  double est = -carg (c) / (2.0 * M_PI); /* symbols past tau0, mod 1 */
  double err = off + est;
  return err - round (err);
}

typedef struct
{
  double sps;
  size_t m;
} plan_t;

static int
run_plan (plan_t pl, int check)
{
  dp_RateConverter_state_t *rc = dp_RateConverter_create_matched (
      (double)pl.m / pl.sps, 1, RC_PULSE_IANDD, 0.0, 1, (double)pl.m, 1024);
  DP_REQUIRE (rc != NULL);
  static model_t m;
  DP_REQUIRE (model_init (&m, rc, pl.sps) == 0);
  char label[256] = "";
  for (int i = 0; i < rc->n_stages; i++)
    {
      char one[64];
      (void)dp_RateConverter_stage_label (rc, i, one, sizeof one);
      strncat (label, i ? " + " : "", sizeof label - strlen (label) - 1);
      strncat (label, one, sizeof label - strlen (label) - 1);
    }
  printf ("\n%.4g input samples a symbol: %s; the bank's grid is %.4g a "
          "symbol, %zu taps an arm\n",
          pl.sps, label, pl.sps / (double)m.d, m.nt);

  /* 1. the S-curves, and each detector's slope at lock */
  stream_t quiet;
  DP_REQUIRE (stream_make (&m, check ? 3000 : 6000, 0.0, 0.0, 7u, &quiet)
              == 0);
  for (int det = 0; det < NDET; det++)
    {
      double lo, hi;
      open_loop (&m, &quiet, det, -1.0 / 64.0, &lo, NULL);
      open_loop (&m, &quiet, det, +1.0 / 64.0, &hi, NULL);
      m.kd[det] = (hi - lo) / (2.0 / 64.0);
    }
  printf ("  slope at lock: gardner %+.4f, derivative %+.4f\n",
          m.kd[DET_GARDNER], m.kd[DET_DERIV]);
  DP_CHECK (m.kd[DET_GARDNER] != 0.0 && m.kd[DET_DERIV] != 0.0);
  if (!check)
    {
      printf ("  S-curve, mean output over its slope at lock, against the "
              "timing error in symbols:\n  %10s",
              "error");
      for (int i = -8; i <= 8; i++)
        printf (" %+6.3f", (double)i / 16.0);
      for (int det = 0; det < NDET; det++)
        {
          printf ("\n  %10s", DET_NAME[det]);
          for (int i = -8; i <= 8; i++)
            {
              double mu;
              open_loop (&m, &quiet, det, (double)i / 16.0, &mu, NULL);
              printf (" %+6.3f", mu / m.kd[det]);
            }
        }
      printf ("\n");
    }
  free (quiet.sj);

  /* 2 and 3. open-loop figure, then the closed loop against it */
  static const double esn0_db[3] = { 4.0, 7.0, 10.0 };
  static const double bns[4]     = { 0.02, 0.01, 0.005, 0.002 };
  size_t              nsym       = check ? 30000 : 120000;
  printf ("  %-6s  %-10s  %11s  %7s  %21s  %9s  %6s  %s\n", "Es/N0",
          "detector", "S_e(0)/Kd^2", "bn", "timing rms meas / pred",
          "meas/pred", "slips", "max dev");
  double fig[3][NDET] = { { 0 } };
  for (int gi = 0; gi < (check ? 1 : 3); gi++)
    {
      double   n0 = pl.sps / pow (10.0, esn0_db[gi] / 10.0);
      stream_t st;
      DP_REQUIRE (stream_make (&m, nsym, 0.0, n0, 11u + (uint32_t)gi, &st)
                  == 0);
      for (int det = 0; det < NDET; det++)
        {
          double mu, s0;
          open_loop (&m, &st, det, 0.0, &mu, &s0);
          fig[gi][det] = s0 / (m.kd[det] * m.kd[det]);
          for (int b = 0; b < 4; b++)
            {
              if (check && b != 2)
                continue;
              loop_out_t o;
              size_t     skip = (size_t)(20.0 / bns[b]);
              closed_loop (&m, &st, det, bns[b], 0.0, 16, nsym - 40, skip, &o);
              double pred = sqrt (2.0 * bns[b] * fig[gi][det]);
              if (o.slips == 999)
                {
                  printf ("  %3.0f dB  %-10s  %11.4e  %7.4f  left by more "
                          "than 4 symbols\n",
                          esn0_db[gi], DET_NAME[det], fig[gi][det], bns[b]);
                  continue;
                }
              printf ("  %3.0f dB  %-10s  %11.4e  %7.4f  %9.5f / %-9.5f  "
                      "%9.3f  %6ld  %.3f\n",
                      esn0_db[gi], DET_NAME[det], fig[gi][det], bns[b],
                      sqrt (o.var), pred, sqrt (o.var) / pred, o.slips,
                      o.max_dev);
              /* The closed loop is this open-loop figure through a loop
                 of noise bandwidth bn. The band is wide: a quarter either
                 way catches a wrong slope, a wrong sign or a factor of
                 two in bn, which are the errors this row can make; the
                 full sweep prints how close it really is. */
              if (check)
                DP_CHECK (o.slips == 0 && sqrt (o.var) > 0.75 * pred
                          && sqrt (o.var) < 1.25 * pred);
            }
        }
      /* 4. pull-in, at Es/N0 4 dB */
      if (gi == 0 && !check)
        {
          static const double offs[2] = { 0.25, 0.5 };
          printf ("  pull-in at Es/N0 4 dB, 8 starts a row: symbols until "
                  "the error stays inside 0.25 (median, worst)\n");
          for (int det = 0; det < NDET; det++)
            for (int b = 1; b < 4; b++)
              for (int oi = 0; oi < 2; oi++)
                {
                  enum
                  {
                    NST = 8
                  };
                  long   t[NST];
                  size_t nrun = (size_t)(20.0 / bns[b]);
                  size_t room = (nsym - 64) / NST - 64;
                  if (nrun > room)
                    nrun = room;
                  for (int r = 0; r < NST; r++)
                    {
                      loop_out_t o;
                      closed_loop (&m, &st, det, bns[b], offs[oi],
                                   16 + (size_t)r * (nrun + 64), nrun, 0, &o);
                      t[r] = o.settled;
                    }
                  for (int a = 1; a < NST; a++) /* insertion sort */
                    for (int c = a; c > 0 && t[c] < t[c - 1]; c--)
                      {
                        long sw  = t[c];
                        t[c]     = t[c - 1];
                        t[c - 1] = sw;
                      }
                  printf ("    %-10s  bn %.4f  from %.2f symbol: %5ld, "
                          "%5ld\n",
                          DET_NAME[det], bns[b], offs[oi], t[NST / 2],
                          t[NST - 1]);
                }
          /* The feedforward estimate, from 64 starting offsets spread
             over one symbol. Its error is what the loop is then handed. */
          printf ("  feedforward estimate at Es/N0 4 dB, %d hypotheses a "
                  "symbol, 64 starts across one symbol:\n  %8s  %12s  "
                  "%12s  %s\n",
                  FF_K, "symbols", "error rms", "worst error",
                  "sqrt(figure / symbols)");
          static const size_t lens[4] = { 32, 64, 128, 256 };
          for (int li = 0; li < 4; li++)
            {
              double s2 = 0.0, worst = 0.0;
              for (int r = 0; r < 64; r++)
                {
                  double e = feedforward (&m, &st, 16 + (size_t)r * 300,
                                          lens[li], (double)r / 64.0 - 0.5);
                  s2 += e * e;
                  worst = fmax (worst, fabs (e));
                }
              printf ("  %8zu  %12.4f  %12.4f  %.4f\n", lens[li],
                      sqrt (s2 / 64.0), worst,
                      sqrt (fig[0][DET_DERIV] / (double)lens[li]));
            }
        }
      free (st.sj);
    }
  /* The claim the detector is proposed on. */
  printf ("  derivative against gardner at Es/N0 4 dB: %+.2f dB\n",
          10.0 * log10 (fig[0][DET_DERIV] / fig[0][DET_GARDNER]));
  DP_CHECK (fig[0][DET_DERIV] < fig[0][DET_GARDNER]);
  dp_RateConverter_destroy (rc);
  return 0;
}

int
main (int argc, char **argv)
{
  int                 check = argc > 1 && strcmp (argv[1], "--check") == 0;
  static const plan_t all[] = {
    { 64.0, 4 }, { 104.0 / 3.0, 4 }, { 9.3, 4 }, { 4.3, 4 }, { 4.0, 4 },
  };
  static const plan_t few[] = { { 4.3, 4 } };
  const plan_t       *pl    = check ? few : all;
  size_t n = check ? sizeof few / sizeof *few : sizeof all / sizeof *all;
  printf ("Two timing detectors closed around each planned cascade, one "
          "strobe a symbol out.\n");
  for (size_t i = 0; i < n; i++)
    DP_CHECK (run_plan (pl[i], check) == 0);
  DP_TEST_END ("validate_ratesync_loop_model");
}
