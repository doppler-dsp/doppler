/**
 * @file timing_est_noise.c
 * @brief Validation: SymbolTimingEstimator under noise, clock offset and
 *        Doppler, against docs/design/timing-estimator.md sections 4 and 7.
 *
 * ## The chain every row runs
 *
 *     wfm_synth (symbols, U samples a symbol, rectangle or root-raised
 *                cosine, its own carrier `f`)
 *       -> doppler_channel (the rows that dilate the time base)
 *       -> every D-th sample from starting offset `o`   (N = U / D a symbol)
 *          the rectangle as the mean of the D samples up to it (boxcar),
 *          the root-raised cosine as that one sample
 *       -> + awgn                                       (Es/N0 at N a symbol)
 *       -> the shipped matched filter (RateConverter, matched, rate 1)
 *       -> dp_timing_est_estimate(), one block or two
 *
 * The symbols are the library's PRBS (31-bit, one generator a row, so no
 * two trials share a pattern) times a starting phase drawn a trial. **A
 * carrier offset is on every row**: `f` is drawn a row, 0.01 to 0.05 of
 * the symbol rate, either sign, unless the row sets it. Nothing is
 * synthesized here.
 *
 * The rectangle is averaged over each kept sample's own interval because
 * that is the design condition RateSync's bank was given, and because a
 * rectangle merely sampled carries no timing finer than a sample: at a
 * whole N, D starting offsets give the same samples. That case is kept as
 * one sweep of the starting-offset axis, `rect_point`.
 *
 * ## The truth, and the prediction
 *
 * Both come from ONE measurement that does not involve the estimator: a
 * lone symbol through the same chain, at each of the D decimation phases,
 * which is the end-to-end pulse `g` on the generator's grid.
 *
 *  - **The eye center** is that pulse's energy centroid. The mean error is
 *    measured against it.
 *  - **The statistics** follow from `g`, the matched filter's impulse
 *    response `h` and the block's geometry, with `a_k` the symbols and `w`
 *    the filtered noise. With `c = sum |y|^2 exp(-j theta)`, `m = E c` and
 *    the fluctuation split along `m` (in phase, `i`) and across it
 *    (quadrature, `q`), the three uncorrelated parts of `|y|^2` give
 *
 *        pattern:      sum over pairs k<l of
 *                      (sum_n 2 Re(g_k g_l*) W[n])^2
 *        signal*noise: 2 sigma^2 sum_k || h * (g_k W) ||^2
 *        noise*noise:  sigma^4 sum_n sum_n' r[n-n']^2 W[n] W[n']
 *
 *    for a weight `W` (the quadrature sinusoid, the in-phase one, or 1 for
 *    the block's total power), `r = h * h` and `sigma^2` the noise a
 *    sample ahead of the filter. Then, to first order,
 *
 *        var(tau_hat) = V_q / (4 pi^2 |m|^2)
 *
 *    and section 4.1's `k_ss`, `k_sn`, `k_nn` are those three parts times
 *    `L`, `L * gamma` and `L * gamma^2`. Carried one order further,
 *    `var * (1 + 3 V_i/|m|^2 - 2 V_q/|m|^2)`, which is what a row is held
 *    to; a row where that correction passes a quarter is printed and not
 *    asserted, because the expansion is not a prediction there.
 *    `E |c|^2 = |m|^2 + V_q + V_i` is exact and is asserted on every row.
 *
 * ## What each row is held to
 *
 * The prediction above is computed on the row's own geometry: the block's
 * samples, where each symbol's pulse falls among them (on a dilated time
 * base when the channel dilates it), and the row's carrier. The design
 * page's closed forms are printed beside it and held where they apply.
 *
 * Noise (section 4.1):
 *  - The mean error is the predicted bias; the variance is the prediction
 *    one order past first; `E |c|^2` and the mean power are the pulse's.
 *  - The bias is not zero for the rectangle at a whole N: the line's phase
 *    depends on where the samples fall in the symbol, by +-0.011 symbols
 *    at N = 4. It is zero for the root-raised cosine, which is band
 *    limited, and it is small at an N that is not whole, where the
 *    sampling phase runs through every value inside the block. A block
 *    that is not a whole number of symbols adds the leak of the power's
 *    mean into the bin (N = 3.2, L = 128: 410 samples).
 *  - Rectangle, N = 4, 128 symbols, Es/N0 4 dB: 0.018 to 0.026 symbols
 *    rms across sampling phases. The design page's bank model gave 0.020
 *    to 0.027.
 *  - Root-raised cosine: the line is weak (quality 0.03 to 0.045 at
 *    roll-off 0.35), so the expansion is a prediction only for long
 *    blocks: 256 symbols and up at 4 dB, 128 and up at 10 dB.
 *
 * Clock offset (section 4.2), `x` cycles of the line a block:
 *  - Inside `|x| < 1/2` the rate is the predicted one, which for the
 *    band-limited pulse is the offset sent (`-d` for a channel dilation
 *    `d`; `eps / (1 + eps)` in the header's terms).
 *  - The line falls as the page's `sinc(x)^2` for the band-limited pulse.
 *    For the rectangle at a whole N it does not: the line's size depends
 *    on the sampling phase (0.117 to 0.159 of the power at N = 4, 10 dB),
 *    an offset sweeps that phase, and the loss measured against the line
 *    at the block's own phase is anything from none to twice the page's.
 *  - At the edge and past it nothing is held. The rows are printed: the
 *    rate reads `(x - round(x)) / L`, the wrong sign at `x = 0.75`.
 *
 * Doppler (section 4.3):
 *  - The carrier alone, a tenth of the symbol rate off: the rate reads 0
 *    and the variance is the pulse's at that carrier.
 *  - Carrier and time base together, as doppler_channel applies them: for
 *    the band-limited pulse the rate is the dilation. For the rectangle
 *    at a whole N a drift much smaller than a sample over the block is
 *    read through the sampling-phase bias, whose slope is up to +-0.28:
 *    20 ppm over 512 symbols reads as 13 to 26 ppm.
 *  - A Doppler rate, `q` the change in clock offset across a block times
 *    `L`: for the band-limited pulse the line falls as
 *    `|integral_{-1/2}^{1/2} exp(j pi q u^2) du|^2` and the timing moves
 *    by that integral's argument over `2 pi` (0.80 and 0.082 symbols at
 *    `q = 2`).
 *
 * Quality (section 4.4):
 *  - Noise alone: `c` is circular Gaussian with `E |c|^2 = V_q + V_i`,
 *    and the mean quality is the Rayleigh mean (0.0277 for the
 *    rectangle's filter at N = 4 and 512 samples; samples taken as
 *    independent would give 0.039).
 *  - The threshold for a false-alarm probability is bracketed. The
 *    Rayleigh threshold leaves out the spread of the block's power and
 *    gives the target or more. The threshold that averages over that
 *    spread as if it were independent of `|c|` gives the target or less,
 *    by up to a third at 1e-3 from 128 symbols, because the two rise and
 *    fall together.
 *  - Detection is the normal probability that `|c|` exceeds the threshold
 *    times the power, held within 0.03 beyond the interval.
 *  - No transitions: no line, and the threshold is exceeded no more often
 *    than its false-alarm probability.
 *
 * Starting offset: the mean error is the predicted bias at each of 64
 * offsets, and the 64 normalized errors have unit mean square. The
 * rectangle merely sampled passes the same test with a bias that steps
 * through +-0.117 symbols: half a sample.
 *
 * Tolerances are the run's own standard errors, four of them for the
 * --check rows and five for the full sweep's thousand assertions; a
 * probability carries its Wilson 95% interval. Every `--check` assertion is
 * evaluated again on a named wrong case and required to fail.
 *
 * Usage:  timing_est_noise [--check] [--emit] [--csv DIR]
 *         (`--emit`: the measurements as CSV blocks on stdout, the tables
 *          on stderr; `--check --emit` is the report's call)
 *         (`--csv DIR`: the same blocks as DIR/<block>.csv; the full
 *          sweep's are the report's record)
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/boxcar/boxcar_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "doppler/pn/pn_core.h"
#include "doppler/timing_est/timing_est_core.h"
#include "doppler/wfm/wfm_dsp.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_rng_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI 6.283185307179586476925
#define BETA 0.35
#define SPAN 8
/* Symbols discarded ahead of the first block: the shaper's span, the
   matched filter's, and the channel's delay. */
#define LEAD 24
#define HI_CAP ((size_t)1 << 17)
#define LO_CAP ((size_t)1 << 15)
#define NH_MAX 512

/* ── output: tables to `g_say`, CSV blocks kept until the end ─────────── */
static FILE *g_say;
#define say(...) fprintf (g_say, __VA_ARGS__)
enum
{
  BLK_CONST,
  BLK_NOISE,
  BLK_CLOCK,
  BLK_DOPPLER,
  BLK_CHIRP,
  BLK_H0,
  BLK_H1,
  BLK_OFFSET,
  BLK_OFFSUM,
  BLK_N
};
#define BLK_CAP ((size_t)1 << 17)
static char  *g_blk[BLK_N];
static size_t g_blk_len[BLK_N];
static void
emitf (int b, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  int n = vsnprintf (g_blk[b] + g_blk_len[b], BLK_CAP - g_blk_len[b], fmt, ap);
  va_end (ap);
  if (n > 0 && (size_t)n < BLK_CAP - g_blk_len[b])
    g_blk_len[b] += (size_t)n;
}

static int g_check;
/* How many standard errors a row is held within. The --check rows make
   about forty assertions and the full sweep about a thousand, so the same
   chance of one false failure in the whole run (under 1 in 400) is four
   standard errors for the first and five for the second. */
static double g_z = 4.0;

/* ── running moments ──────────────────────────────────────────────────── */
typedef struct
{
  double n, s1, s2, s3, s4;
} acc_t;

static void
acc_add (acc_t *a, double v)
{
  a->n += 1.0;
  a->s1 += v;
  a->s2 += v * v;
  a->s3 += v * v * v;
  a->s4 += v * v * v * v;
}

static double
acc_mean (const acc_t *a)
{
  return a->s1 / a->n;
}

static double
acc_var (const acc_t *a)
{
  double m = acc_mean (a);
  return a->s2 / a->n - m * m;
}

/* Standard error of the mean. */
static double
acc_se (const acc_t *a)
{
  return sqrt (acc_var (a) / a->n);
}

/* Standard error of the variance, from the run's own fourth moment. */
static double
acc_var_se (const acc_t *a)
{
  double m = acc_mean (a), v = acc_var (a);
  double m4 = a->s4 / a->n - 4.0 * m * a->s3 / a->n
              + 6.0 * m * m * a->s2 / a->n - 3.0 * m * m * m * m;
  double d  = m4 - v * v;
  return d > 0.0 ? sqrt (d / a->n) : 0.0;
}

static double
wrap_half (double d)
{
  return d - floor (d + 0.5);
}

/* integral_{-1/2}^{1/2} exp (j pi q u^2) du, Simpson's rule. */
static double _Complex fresnel (double q)
{
  enum
  {
    NS = 2000
  };
  double _Complex s = 0.0;
  for (int i = 0; i <= NS; i++)
    {
      double u = -0.5 + (double)i / NS;
      double w = (i == 0 || i == NS) ? 1.0 : (i & 1) ? 4.0 : 2.0;
      s += w * cexp (I * M_PI * q * u * u);
    }
  return s / (3.0 * NS);
}

/* ── the chain ────────────────────────────────────────────────────────── */
typedef struct
{
  int rrc;           /* 1: root-raised cosine; 0: rectangle                  */
  int u;             /* generator samples a symbol                           */
  int d;             /* decimation: the estimator sees u / d a symbol        */
  int avg;           /* 1: a kept sample is the mean of the d generator samples
                        up to it (a front end that averages over its own
                        interval); 0: it is that one sample */
  double f;          /* the generator's carrier, cycles a symbol             */
  int    chan;       /* 1: through doppler_channel at the generator's rate,
                        one symbol a second, so fs = u Hz             */
  double carrier_hz; /* the channel's carrier, in symbol rates        */
  double ppm, ppm_s; /* its Doppler and Doppler rate                  */
} chain_t;

typedef struct
{
  chain_t                     c;
  double                      n; /* samples a symbol at the estimator */
  dp_RateConverter_state_t   *mf;
  dp_awgn_state_t            *nz;
  dp_doppler_channel_state_t *ch;
  dp_timing_est_state_t      *te;
  float                      *taps;
  size_t                      ntaps;
} rig_t;

static float _Complex g_hi[HI_CAP], g_ch[HI_CAP];
static float _Complex g_lo[LO_CAP], g_nz[LO_CAP], g_y[LO_CAP + 8192];
static float _Complex g_sym[4096];

static void
rig_close (rig_t *r)
{
  if (r->mf)
    dp_RateConverter_destroy (r->mf);
  dp_awgn_destroy (r->nz);
  dp_doppler_channel_destroy (r->ch);
  dp_timing_est_destroy (r->te);
  free (r->taps);
  memset (r, 0, sizeof *r);
}

static int
rig_open (rig_t *r, const chain_t *c)
{
  memset (r, 0, sizeof *r);
  r->c  = *c;
  r->n  = (double)c->u / (double)c->d;
  r->mf = dp_RateConverter_create_matched (
      1.0, 1, c->rrc ? RC_PULSE_RRC : RC_PULSE_IANDD, BETA, SPAN, r->n, 1024);
  r->nz = dp_awgn_create (1u, 0.0f);
  r->te = dp_timing_est_create (r->n);
  if (c->chan)
    r->ch = dp_doppler_channel_create ((double)c->u, c->carrier_hz, c->ppm,
                                       c->ppm_s);
  if (c->rrc)
    {
      r->ntaps = wfm_rrc_ntaps (c->u, SPAN);
      r->taps  = (float *)dp_xnn (malloc (r->ntaps * sizeof *r->taps));
      dp_wfm_rrc_taps (BETA, c->u, SPAN, r->taps);
    }
  if (!r->mf || !r->nz || !r->te || (c->chan && !r->ch))
    {
      rig_close (r);
      return -1;
    }
  return 0;
}

/* The matched filter over g_lo[0..nl), into g_y. */
static size_t
matched (rig_t *r, size_t nl)
{
  dp_RateConverter_reset (r->mf);
  return dp_RateConverter_execute (r->mf, g_lo, nl, g_y,
                                   sizeof g_y / sizeof *g_y);
}

/* `nsym` symbols of g_sym through the chain at starting offset `o`, with
   noise of `amp` a rail (0: none). The matched filter's output is in g_y;
   returns its length, or 0. */
static size_t
run_chain (rig_t *r, int o, size_t nsym, float amp, uint64_t nseed)
{
  const chain_t *c  = &r->c;
  size_t         nh = nsym * (size_t)c->u;
  if (nh > HI_CAP)
    return 0;
  dp_wfm_synth_state_t *g = dp_xnn (
      dp_wfm_synth_create (WFM_SYNTH_SYMBOLS, 1.0, c->f / (double)c->u,
                           WFM_SYNTH_SNR_CLEAN, 1, 1u, c->u, 7, 0, 0, 0.0));
  int rc = dp_wfm_synth_set_symbols (g, g_sym, nsym);
  if (rc == 0 && c->rrc)
    rc = dp_wfm_synth_set_rrc (g, r->taps, r->ntaps);
  if (rc == 0)
    dp_wfm_synth_steps (g, g_hi, nh);
  dp_wfm_synth_destroy (g);
  if (rc != 0)
    return 0;

  const float _Complex *src = g_hi;
  if (r->ch)
    {
      dp_doppler_channel_reset (r->ch);
      size_t done = 0, out = 0;
      while (done < nh)
        {
          size_t blk = nh - done;
          if (blk > DOPPLER_CHANNEL_MAX_BLOCK)
            blk = DOPPLER_CHANNEL_MAX_BLOCK;
          size_t got  = dp_doppler_channel_execute (r->ch, g_hi + done, blk,
                                                    g_ch + out, HI_CAP - out);
          size_t used = dp_doppler_channel_consumed (r->ch);
          if (!used)
            break;
          done += used;
          out += got;
        }
      src = g_ch;
      nh  = out;
    }

  size_t nl = 0;
  if (c->avg)
    {
      dp_boxcar_state_t bx;
      dp_boxcar_init (&bx, (size_t)c->d, 1.0);
      for (size_t j = 0; j < nh && nl < LO_CAP; j++)
        {
          float _Complex v = dp_boxcar_step (&bx, src[j]);
          if (j >= (size_t)o && (j - (size_t)o) % (size_t)c->d == 0)
            g_lo[nl++] = v;
        }
    }
  else
    for (size_t j = (size_t)o; j < nh && nl < LO_CAP; j += (size_t)c->d)
      g_lo[nl++] = src[j];
  if (amp > 0.0f)
    {
      dp_awgn_set_amplitude (r->nz, amp);
      dp_awgn_reseed (r->nz, nseed);
      dp_awgn_generate (r->nz, nl, g_nz, nl);
      for (size_t i = 0; i < nl; i++)
        g_lo[i] += g_nz[i];
    }
  return matched (r, nl);
}

/* Noise alone, `nl` samples ahead of the filter. */
static size_t
run_noise (rig_t *r, size_t nl, float amp, uint64_t nseed)
{
  dp_awgn_set_amplitude (r->nz, amp);
  dp_awgn_reseed (r->nz, nseed);
  dp_awgn_generate (r->nz, nl, g_lo, nl);
  return matched (r, nl);
}

/* ── the pulse: one symbol through the chain, on the generator's grid ─── */
typedef struct
{
  double _Complex *g; /* g[j], j a generator-sample index          */
  long             len;
  long             ja, jb;    /* its support                               */
  double           jc;        /* its energy centroid                       */
  double           h[NH_MAX]; /* the matched filter's impulse response  */
  double           r[NH_MAX]; /* its autocorrelation, r[0..nh)          */
  int              nh;
} pulse_t;

static void
pulse_free (pulse_t *p)
{
  free (p->g);
  p->g = NULL;
}

/* A lone symbol at index `kp`, at each of the `d` decimation phases. */
static int
pulse_measure (const chain_t *c, long kp, pulse_t *p)
{
  rig_t r;
  memset (p, 0, sizeof *p);
  if (rig_open (&r, c) != 0)
    return -1;
  size_t nsym = (size_t)(kp + LEAD);
  p->len      = (long)nsym * c->u + 64;
  p->g = (double _Complex *)dp_xnn (calloc ((size_t)p->len, sizeof *p->g));
  memset (g_sym, 0, nsym * sizeof *g_sym);
  g_sym[kp]  = 1.0f;
  long   jpk = 0;
  double pk  = 0.0;
  for (int i = 0; i < c->d; i++)
    {
      size_t ny = run_chain (&r, i, nsym, 0.0f, 0);
      if (!ny)
        {
          rig_close (&r);
          return -1;
        }
      for (size_t n = 0; n < ny; n++)
        {
          long j = i + (long)n * c->d;
          if (j >= p->len)
            break;
          p->g[j]  = (double)crealf (g_y[n]) + I * (double)cimagf (g_y[n]);
          double e = creal (p->g[j] * conj (p->g[j]));
          if (e > pk)
            {
              pk  = e;
              jpk = j;
            }
        }
    }
  /* The support, inside a window that holds both filters' spans. */
  long w    = (2 * SPAN + 3) * c->u;
  long lo   = jpk - w < 0 ? 0 : jpk - w,
       hi   = jpk + w >= p->len ? p->len - 1 : jpk + w;
  p->ja     = jpk;
  p->jb     = jpk;
  double se = 0.0, sj = 0.0;
  for (long j = lo; j <= hi; j++)
    {
      double e = creal (p->g[j] * conj (p->g[j]));
      if (e > 1e-12 * pk)
        {
          if (j < p->ja)
            p->ja = j;
          if (j > p->jb)
            p->jb = j;
        }
      se += e;
      sj += e * (double)(j - jpk);
    }
  p->jc = (double)jpk + sj / se;
  /* one zero sample either side, so a read between two samples at the
     support's very end has both */
  if (p->ja > 0)
    p->ja--;
  if (p->jb < p->len - 2)
    p->jb++;

  /* The matched filter's impulse response, and that it is one filter: an
     impulse one sample later gives the same response one sample later. */
  enum
  {
    AT = 64,
    NI = 2048
  };
  static float _Complex ya[NI + 64];
  for (int pass = 0; pass < 2; pass++)
    {
      memset (g_lo, 0, NI * sizeof *g_lo);
      g_lo[AT + pass] = 1.0f;
      size_t ny       = matched (&r, NI);
      if (ny < NI - 64)
        {
          rig_close (&r);
          return -1;
        }
      if (pass == 0)
        memcpy (ya, g_y, (ny < NI ? ny : NI) * sizeof *ya);
      else
        for (size_t t = 1; t + 64 < ny; t++)
          if (cabsf (g_y[t] - ya[t - 1]) > 1e-6f)
            {
              rig_close (&r);
              return -2;
            }
    }
  double hm = 0.0;
  for (int t = 0; t < NH_MAX; t++)
    {
      p->h[t] = (double)crealf (ya[AT + t]);
      if (fabs (p->h[t]) > hm)
        hm = fabs (p->h[t]);
    }
  p->nh = 1;
  for (int t = 0; t < NH_MAX; t++)
    if (fabs (p->h[t]) > 1e-9 * hm)
      p->nh = t + 1;
  if (p->nh >= NH_MAX)
    {
      rig_close (&r);
      return -3; /* the response has not ended inside the window */
    }
  for (int m = 0; m < p->nh; m++)
    {
      double a = 0.0;
      for (int t = 0; t + m < p->nh; t++)
        a += p->h[t] * p->h[t + m];
      p->r[m] = a;
    }
  rig_close (&r);
  return 0;
}

/* The pulse between two of its samples, and the first and last block
   sample a pulse reaching from `x` (to `x`) generator samples covers. */
static double _Complex pulse_at (const pulse_t *p, double idx)
{
  long   b = (long)floor (idx);
  double a = idx - (double)b;
  return (1.0 - a) * p->g[b] + a * p->g[b + 1];
}

static long
pulse_lo (double x, int d)
{
  double v = ceil (x / d);
  return v < 0.0 ? 0 : (long)v;
}

static long
pulse_hi (double x, int d, long nb)
{
  double v = floor (x / d);
  return v > (double)(nb - 1) ? nb - 1 : (long)v;
}

/* ── the prediction, from the pulse ───────────────────────────────────── */
enum
{
  WQ, /* across the line: moves the timing            */
  WI, /* along it: moves the line's size              */
  WD  /* the block's total power                      */
};

typedef struct
{
  double m_abs;         /* |E c|                                        */
  double dtot;          /* E sum |y|^2                                  */
  double part[3][3][3]; /* [pattern, signal*noise, noise*noise][W][W'] */
  double v[3][3];       /* their sum                                    */
  double bias;          /* E (tau_hat - eye center), symbols            */
  double var1;          /* first order                                  */
  double var2;          /* one order further                            */
  double corr;          /* that correction's size, 3 V_i + 2 V_q over |m|^2 */
  double c2;            /* E |c|^2                                      */
  double k[3];          /* k_ss, k_sn, k_nn                             */
} pred_t;

/* Pulse `pl`, symbols `u` generator samples apart and carrying a carrier
   of `f` cycles a symbol, read every `d` samples for `nb` samples, in noise
   of `amp` a rail ahead of the filter. `tau` is the eye center on the
   estimator's clock at the block's mean sample. The symbols may run on a
   dilated time base: `dm` is the dilation at that sample and `ds` its
   change a symbol, so the eye's phase `y` symbols from the middle is
   `tau - dm y - ds y^2 / 2`. `sig` 0 is noise alone. */
static void
predict (const pulse_t *pl, int u, int d, double f, double tau, long nb,
         double amp, int sig, double gamma, double dm, double ds, pred_t *p)
{
  memset (p, 0, sizeof *p);
  /* the pulse's reach either side of its centroid, generator samples,
     one sample in from the support's end so a read between two samples
     always has both */
  double wl = pl->jc - (double)pl->ja, wr = (double)(pl->jb - 1) - pl->jc;
  long   last = (nb - 1) * d;
  double xm   = 0.5 * (double)last;
  double s2   = 2.0 * amp * amp;
  double sw2  = s2 * pl->r[0];

  /* Where each symbol's pulse sits on the block's generator grid: eye j is
     where the clock's phase x / u less the eye's own phase is the integer
     j. The position is not a whole sample, so the pulse is read between
     its samples (PULSE_AT). */
  long    jlo = (long)floor (-wr / u * 1.05) - 4;
  long    jhi = (long)ceil (((double)last + wl) / u * 1.05) + 4;
  double *xk
      = (double *)dp_xnn (malloc ((size_t)(jhi - jlo + 1) * sizeof *xk));
  long nk = 0;
  for (long j = jlo; j <= jhi; j++)
    {
      double c = (double)j + tau - xm / u;
      double y
          = 2.0 * c
            / ((1.0 + dm) + sqrt ((1.0 + dm) * (1.0 + dm) + 2.0 * ds * c));
      double x = xm + u * y;
      if (x + wr < 0.0 || x - wl > (double)last)
        continue;
      xk[nk++] = x;
    }
  long kmin = 0, kmax = nk - 1;

  double *pw = (double *)dp_xnn (calloc ((size_t)nb, sizeof *pw));
  double *w  = (double *)dp_xnn (malloc ((size_t)nb * 3 * sizeof *w));
#define E_OF(k) (xk[k])
#define I_LO(k) (pulse_lo (xk[k] - wl, d))
#define I_HI(k) (pulse_hi (xk[k] + wr, d, nb))
#define G_AT(k, i) (pulse_at (pl, pl->jc + (double)((i) * (long)d) - xk[k]))
  if (sig)
    for (long k = kmin; k <= kmax; k++)
      for (long i = I_LO (k); i <= I_HI (k); i++)
        pw[i] += creal (G_AT (k, i) * conj (G_AT (k, i)));

  double _Complex m = 0.0;
  for (long i = 0; i < nb; i++)
    {
      double th = TWO_PI * (double)i * d / u;
      m += (pw[i] + sw2) * cexp (-I * th);
      p->dtot += pw[i] + sw2;
    }
  p->m_abs           = cabs (m);
  double _Complex ub = (sig && p->m_abs > 0.0) ? conj (m) / p->m_abs : 1.0;
  for (long i = 0; i < nb; i++)
    {
      double _Complex e = cexp (-I * TWO_PI * (double)i * d / u) * ub;
      w[3 * i + WQ]     = cimag (e);
      w[3 * i + WI]     = creal (e);
      w[3 * i + WD]     = 1.0;
    }

  /* pattern: every pair of symbols whose pulses overlap */
  if (sig)
    for (long k = kmin; k <= kmax; k++)
      for (long l = k + 1; l <= kmax && E_OF (l) - wl <= E_OF (k) + wr; l++)
        {
          long ia = I_LO (l), ib = I_HI (k);
          double _Complex rot = cexp (I * TWO_PI * f * (double)(k - l));
          double a[3]         = { 0.0, 0.0, 0.0 };
          for (long i = ia; i <= ib; i++)
            {
              double v = 2.0 * creal (rot * G_AT (k, i) * conj (G_AT (l, i)));
              for (int j = 0; j < 3; j++)
                a[j] += v * w[3 * i + j];
            }
          for (int j = 0; j < 3; j++)
            for (int jj = 0; jj < 3; jj++)
              p->part[0][j][jj] += a[j] * a[jj];
        }

  /* signal * noise: each pulse's weighted samples through the filter */
  if (sig && amp > 0.0)
    {
      long             cap = (long)((wl + wr) / d) + 4 + pl->nh;
      double _Complex *y
          = (double _Complex *)dp_xnn (malloc ((size_t)cap * 3 * sizeof *y));
      for (long k = kmin; k <= kmax; k++)
        {
          long ia = I_LO (k), ib = I_HI (k);
          if (ib < ia)
            continue;
          long nx = ib - ia + 1, nyy = nx + pl->nh - 1;
          memset (y, 0, (size_t)nyy * 3 * sizeof *y);
          for (long x = 0; x < nx; x++)
            {
              double _Complex gv = G_AT (k, ia + x);
              for (int t = 0; t < pl->nh; t++)
                for (int j = 0; j < 3; j++)
                  y[3 * (x + t) + j] += pl->h[t] * gv * w[3 * (ia + x) + j];
            }
          for (long t = 0; t < nyy; t++)
            for (int j = 0; j < 3; j++)
              for (int jj = 0; jj < 3; jj++)
                p->part[1][j][jj]
                    += 2.0 * s2 * creal (y[3 * t + j] * conj (y[3 * t + jj]));
        }
      free (y);
    }

  /* noise * noise */
  if (amp > 0.0)
    for (long i = 0; i < nb; i++)
      {
        long a = i - (pl->nh - 1) < 0 ? 0 : i - (pl->nh - 1);
        long b = i + (pl->nh - 1) >= nb ? nb - 1 : i + (pl->nh - 1);
        for (long ii = a; ii <= b; ii++)
          {
            double rr = pl->r[labs (i - ii)];
            rr        = s2 * s2 * rr * rr;
            for (int j = 0; j < 3; j++)
              for (int jj = 0; jj < 3; jj++)
                p->part[2][j][jj] += rr * w[3 * i + j] * w[3 * ii + jj];
          }
      }
#undef E_OF
#undef I_LO
#undef I_HI
#undef G_AT

  for (int c = 0; c < 3; c++)
    for (int j = 0; j < 3; j++)
      for (int jj = 0; jj < 3; jj++)
        p->v[j][jj] += p->part[c][j][jj];
  double m2 = p->m_abs * p->m_abs;
  p->c2     = m2 + p->v[WQ][WQ] + p->v[WI][WI];
  if (sig && m2 > 0.0)
    {
      double ll = (double)nb * d / u;
      p->bias   = wrap_half (-carg (m) / TWO_PI - tau)
                  + p->v[WQ][WI] / (TWO_PI * m2);
      p->var1   = p->v[WQ][WQ] / (TWO_PI * TWO_PI * m2);
      p->corr   = (3.0 * p->v[WI][WI] + 2.0 * p->v[WQ][WQ]) / m2;
      p->var2
          = p->var1 * (1.0 + (3.0 * p->v[WI][WI] - 2.0 * p->v[WQ][WQ]) / m2);
      double s = ll / (TWO_PI * TWO_PI * m2);
      p->k[0]  = s * p->part[0][WQ][WQ];
      p->k[1]  = s * p->part[1][WQ][WQ] * gamma;
      p->k[2]  = s * p->part[2][WQ][WQ] * gamma * gamma;
    }
  free (pw);
  free (w);
  free (xk);
}

/* The quality threshold for a false-alarm probability `pfa`, from the
   noise-alone prediction `h0`. `c` is circular Gaussian, so
   P(|c|^2 > T) = exp (-T / E|c|^2); the threshold multiplies the block's
   power D, taken as normal about its mean with relative spread `s`, which
   averages that tail to  exp (-a / (1 + 2 a s^2)) / sqrt (1 + 2 a s^2)
   with a = eta^2 D^2 / E|c|^2. `spread` 0 is the Rayleigh threshold. */
static double
h0_tail (const pred_t *h0, double eta, int spread)
{
  double a  = eta * eta * h0->dtot * h0->dtot / h0->c2;
  double s2 = spread ? h0->v[WD][WD] / (h0->dtot * h0->dtot) : 0.0;
  double g  = 1.0 + 2.0 * a * s2;
  return exp (-a / g) / sqrt (g);
}

static double
h0_threshold (const pred_t *h0, double pfa, int spread)
{
  double lo = 0.0, hi = 1.0;
  for (int it = 0; it < 80; it++)
    {
      double mid = 0.5 * (lo + hi);
      if (h0_tail (h0, mid, spread) > pfa)
        lo = mid;
      else
        hi = mid;
    }
  return 0.5 * (lo + hi);
}

/* Detection probability at threshold `eta`: |c| is |m| + i + V_q / (2 |m|)
   and the comparison is against eta * D, both taken as normal. */
static double
pd_model (const pred_t *p, double eta)
{
  double mu = p->m_abs + p->v[WQ][WQ] / (2.0 * p->m_abs) - eta * p->dtot;
  double var
      = p->v[WI][WI] + eta * eta * p->v[WD][WD] - 2.0 * eta * p->v[WI][WD];
  return 0.5 * erfc (-mu / sqrt (2.0 * var));
}

/* ── one row: its geometry, its truth, its prediction, its trials ─────── */
typedef struct
{
  chain_t  c;
  int      o;       /* starting offset, generator samples; -1 draws it */
  int      draw_f;  /* 1: draw the generator's carrier                 */
  int      l;       /* symbols a block                                 */
  int      nblk;    /* 1 or 2 blocks                                   */
  double   esn0_db; /* >= 100: no noise                                */
  long     trials;
  uint32_t seed;
  int      constant; /* 1: every symbol the same (no transitions)      */
  double   f_chan;   /* carrier offset the channel adds, cycles a
                        symbol: the prediction's pulse carries it too  */
  double thr[2];     /* quality thresholds to count against            */
} rowcfg_t;

typedef struct
{
  double n;   /* samples a symbol                                */
  long   nb;  /* samples a block                                 */
  double lc;  /* its length in symbols of the clock              */
  long   n0;  /* the first block's first sample                  */
  float  amp; /* noise, a rail                                   */
  double f;   /* the generator's carrier as run                  */
  int    o;
  double eye;       /* the eye center at block 1's mean sample         */
  double d_mid;     /* the channel's dilation there                    */
  double d_junc;    /* and between the two blocks' middles             */
  pred_t p;         /* block 1, from the pulse, on the row's time base */
  pred_t pf;        /* the same with no dilation                       */
  double rate_pred; /* the rate two such blocks' predicted timings give */
  double rate_var;  /* and its variance                              */
  acc_t  err, rate, c2, pw, q;
  long   hits[2];
} row_t;

static uint32_t
seed_mix (uint32_t a, uint32_t b)
{
  uint32_t s = a * 2654435761u ^ (b + 0x9E3779B9u);
  for (int i = 0; i < 4; i++)
    dp_xs32 (&s);
  return s ? s : 1u;
}

static float
amp_for (double esn0_db, double n)
{
  if (esn0_db >= WFM_SYNTH_SNR_CLEAN)
    return 0.0f;
  return dp_awgn_amplitude_for_snr (
      (float)wfm_synth_snr_over_fs (3, 1, n, esn0_db), 1.0f);
}

static int
row_run (const rowcfg_t *cf, row_t *o)
{
  memset (o, 0, sizeof *o);
  uint32_t rng = seed_mix (cf->seed, 1u);
  chain_t  c   = cf->c;
  if (cf->draw_f)
    {
      double mag = 0.01 + 0.04 * dp_uni (&rng);
      c.f        = dp_uni (&rng) < 0.5 ? -mag : mag;
    }
  o->f   = c.f;
  o->o   = cf->o >= 0 ? cf->o : (int)(dp_uni (&rng) * c.u) % c.u;
  o->n   = (double)c.u / (double)c.d;
  o->nb  = lround (cf->l * o->n);
  o->lc  = (double)o->nb / o->n;
  o->n0  = (long)ceil (LEAD * o->n);
  o->amp = amp_for (cf->esn0_db, o->n);

  /* the channel's dilation at block 1's mean sample and between middles */
  double t_mid
      = ((double)o->o + ((double)o->n0 + 0.5 * (double)(o->nb - 1)) * c.d)
        / c.u;
  double t_junc = t_mid + 0.5 * (double)o->nb * c.d / c.u;
  if (c.chan)
    {
      o->d_mid  = (c.ppm + c.ppm_s * t_mid) * 1e-6;
      o->d_junc = (c.ppm + c.ppm_s * t_junc) * 1e-6;
    }

  /* the truth: a lone symbol near block 1's middle, through this chain */
  pulse_t pt;
  long    kp = LEAD + cf->l / 2;
  int     rc = pulse_measure (&c, kp, &pt);
  if (rc != 0)
    return rc;
  double nc  = (pt.jc - (double)o->o) / c.d;
  double nat = (double)o->n0 + 0.5 * (double)(o->nb - 1);
  double ds  = c.chan ? c.ppm_s * 1e-6 : 0.0;
  double yc  = (nc - nat) / o->n;
  o->eye     = wrap_half ((nc - (double)o->n0) / o->n + o->d_mid * yc
                          + 0.5 * ds * yc * yc);

  /* the prediction reads the same pulse: through the channel when there
     is one, whose resampler is part of the pulse, and carrying the
     carrier whichever stage applied it */
  pulse_t pp    = pt;
  double  f_pp  = c.f + cf->f_chan;
  double  gamma = pow (10.0, cf->esn0_db / 10.0);
  predict (&pp, c.u, c.d, f_pp, o->eye, o->nb, (double)o->amp, 1, gamma,
           o->d_mid, ds, &o->p);
  if (c.chan)
    predict (&pp, c.u, c.d, f_pp, o->eye, o->nb, (double)o->amp, 1, gamma, 0.0,
             0.0, &o->pf);
  else
    o->pf = o->p;
  if (cf->nblk > 1)
    {
      /* block 2 starts a block later on the clock: the eye has moved on,
         and so has the dilation */
      double eye2 = o->eye - o->d_mid * o->lc - 0.5 * ds * o->lc * o->lc;
      pred_t p2;
      predict (&pp, c.u, c.d, f_pp, wrap_half (eye2), o->nb, (double)o->amp, 1,
               gamma, o->d_mid + ds * o->lc, ds, &p2);
      o->rate_pred = wrap_half (eye2 + p2.bias - o->eye - o->p.bias) / o->lc;
      o->rate_var  = (o->p.var2 + p2.var2) / (o->lc * o->lc);
    }
  pulse_free (&pt);

  rig_t r;
  if (rig_open (&r, &c) != 0)
    return -1;
  dp_pn_state_t *pn = dp_xnn (
      dp_pn_create (wfm_synth_mls_poly (31), seed_mix (cf->seed, 2u), 31, 0));
  double dmax = fabs (c.ppm) * 1e-6
                + fabs (c.ppm_s) * 1e-6 * (2.0 * LEAD + 2.0 * cf->l + 8);
  size_t nsym
      = (size_t)ceil ((2.0 * LEAD + (double)cf->nblk * o->lc) * (1.0 + dmax))
        + 4;
  uint8_t *bits = (uint8_t *)dp_xnn (malloc (nsym));
  int      bad  = 0;
  for (long t = 0; t < cf->trials && !bad; t++)
    {
      double th         = TWO_PI * dp_uni (&rng);
      float _Complex ph = (float)cos (th) + I * (float)sin (th);
      dp_pn_generate (pn, nsym, bits, nsym);
      for (size_t k = 0; k < nsym; k++)
        g_sym[k] = (cf->constant || bits[k]) ? ph : -ph;
      size_t ny = run_chain (&r, o->o, nsym, o->amp,
                             ((uint64_t)cf->seed << 32) | (uint64_t)(t + 1));
      if (ny < (size_t)(o->n0 + cf->nblk * o->nb))
        {
          bad = 1;
          break;
        }
      dp_timing_est_reset (r.te);
      dp_timing_estimate_t e1
          = dp_timing_est_estimate (r.te, g_y + o->n0, (size_t)o->nb);
      acc_add (&o->err, wrap_half (e1.tau - o->eye));
      double cm = e1.quality * e1.power * (double)o->nb;
      acc_add (&o->c2, cm * cm);
      acc_add (&o->pw, e1.power);
      acc_add (&o->q, e1.quality);
      for (int j = 0; j < 2; j++)
        o->hits[j] += e1.quality > cf->thr[j];
      if (cf->nblk > 1)
        {
          dp_timing_estimate_t e2 = dp_timing_est_estimate (
              r.te, g_y + o->n0 + o->nb, (size_t)o->nb);
          if (!e2.has_rate)
            bad = 1;
          acc_add (&o->rate, e2.rate);
        }
    }
  free (bits);
  dp_pn_destroy (pn);
  rig_close (&r);
  return bad ? -1 : 0;
}

/* A claim and its control: the assertion on the row, and the SAME
   assertion on a named wrong case, which has to fail. */
static void
claim (int ok, int control_ok, const char *fmt, ...)
{
  char    msg[256];
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (msg, sizeof msg, fmt, ap);
  va_end (ap);
  DP_CHECK_MSG (ok, msg);
  if (control_ok >= 0)
    {
      char ctl[320];
      snprintf (ctl, sizeof ctl, "control passed, so this proves nothing: %s",
                msg);
      DP_CHECK_MSG (!control_ok, ctl);
    }
}

#define NO_CONTROL (-1)
#define WITHIN(meas, pred, tol) (fabs ((meas) - (pred)) <= (tol))

/* The three stimuli: the rectangle through a front end that averages
   each sample over its own interval, the root-raised cosine sampled, and
   the rectangle sampled, which is kept as the case that has no timing
   finer than a sample. */
enum
{
  P_RECT  = 0,
  P_RRC   = 1,
  P_POINT = 2
};

static const char *
pulse_name (int pulse)
{
  return pulse == P_RRC ? "rrc" : pulse == P_POINT ? "rect_point" : "rect";
}

static chain_t
chain_for (int pulse, int u, int d)
{
  chain_t c = { 0 };
  c.rrc     = pulse == P_RRC;
  c.u       = u;
  c.d       = d;
  c.avg     = pulse == P_RECT;
  return c;
}

/* ── axis 1: noise ────────────────────────────────────────────────────── */
static int
noise_row (int rrc, int u, int d, int l, double esn0_db, long trials,
           uint32_t seed, int with_controls, int constants)
{
  rowcfg_t cf = { 0 };
  cf.c        = chain_for (rrc, u, d);
  cf.o        = -1;
  cf.draw_f   = 1;
  cf.l        = l;
  cf.nblk     = 1;
  cf.esn0_db  = esn0_db;
  cf.trials   = trials;
  cf.seed     = seed;
  row_t r;
  if (row_run (&cf, &r) != 0)
    return -1;
  double mean = acc_mean (&r.err), se = acc_se (&r.err);
  double var = acc_var (&r.err), vse = acc_var_se (&r.err);
  double c2 = acc_mean (&r.c2), c2se = acc_se (&r.c2);
  double pw = acc_mean (&r.pw), pwse = acc_se (&r.pw);
  double pw_pred = r.p.dtot / (double)r.nb;
  int    held    = r.p.corr <= 0.25;
  say ("  %-4s %6.3f %4d %5.1f %+7.4f %2d | %+8.5f +- %7.5f (%+8.5f) | "
       "%8.6f +- %8.6f  %8.6f %8.6f %5.2f%s | %6.4f | %6.4f\n",
       pulse_name (rrc), r.n, l, esn0_db, r.f, r.o, mean, se, r.p.bias,
       sqrt (var), vse / (2.0 * sqrt (var)), sqrt (r.p.var1),
       sqrt (r.p.var2 > 0.0 ? r.p.var2 : 0.0), r.p.corr, held ? " " : "*",
       c2 / r.p.c2, acc_mean (&r.q));
  emitf (BLK_NOISE,
         "%s,%.6f,%d,%.1f,%.6f,%d,%ld,%.8f,%.8f,%.8f,%.6e,%.6e,%.6e,%.6e,"
         "%.6f,%d,%.6e,%.6e,%.6e,%.6f,%.6f,%.6f,%.6f\n",
         pulse_name (rrc), r.n, l, esn0_db, r.f, r.o, trials, mean, se,
         r.p.bias, var, vse, r.p.var1, r.p.var2, r.p.corr, held, c2, c2se,
         r.p.c2, pw, pwse, pw_pred, acc_mean (&r.q));
  if (constants)
    emitf (BLK_CONST, "%s,%.6f,%d,%.6f,%d,%.6e,%.6e,%.6e,%.6f,%.6e\n",
           pulse_name (rrc), r.n, l, r.f, r.o, r.p.k[0], r.p.k[1], r.p.k[2],
           r.p.m_abs / r.p.dtot, r.p.bias);

  /* The wrong cases: the eye center one generator sample away; the same
     row predicted with 1 dB more Es/N0 than was sent; the line a clock
     offset of a quarter cycle a block would leave. */
  pred_t wrong;
  if (with_controls)
    {
      pulse_t pl;
      chain_t c = cf.c;
      c.f       = r.f;
      if (pulse_measure (&c, LEAD, &pl) != 0)
        return -1;
      predict (&pl, u, d, r.f, r.eye, r.nb,
               (double)amp_for (esn0_db + 1.0, r.n), 1, 1.0, 0.0, 0.0, &wrong);
      pulse_free (&pl);
    }
  claim (WITHIN (mean, r.p.bias, g_z * se),
         with_controls ? WITHIN (mean + 1.0 / u, r.p.bias, g_z * se)
                       : NO_CONTROL,
         "noise %s N %.3f L %d %.0f dB: mean error %+.5f is not the "
         "predicted %+.5f within %.5f",
         pulse_name (rrc), r.n, l, esn0_db, mean, r.p.bias, g_z * se);
  if (held)
    claim (WITHIN (var, r.p.var2, g_z * vse),
           with_controls ? WITHIN (var, wrong.var2, g_z * vse) : NO_CONTROL,
           "noise %s N %.3f L %d %.0f dB: variance %.3e is not the "
           "predicted %.3e within %.3e",
           pulse_name (rrc), r.n, l, esn0_db, var, r.p.var2, g_z * vse);
  claim (WITHIN (c2, r.p.c2, g_z * c2se),
         with_controls ? WITHIN (c2,
                                 r.p.c2
                                     - (1.0 - dp_sinc (0.25) * dp_sinc (0.25))
                                           * r.p.m_abs * r.p.m_abs,
                                 g_z * c2se)
                       : NO_CONTROL,
         "noise %s N %.3f L %d %.0f dB: E|c|^2 %.4e is not the predicted "
         "%.4e within %.2e",
         pulse_name (rrc), r.n, l, esn0_db, c2, r.p.c2, g_z * c2se);
  claim (WITHIN (pw, pw_pred, g_z * pwse),
         with_controls ? WITHIN (pw, wrong.dtot / (double)r.nb, g_z * pwse)
                       : NO_CONTROL,
         "noise %s N %.3f L %d %.0f dB: mean power %.5f is not the "
         "predicted %.5f within %.5f",
         pulse_name (rrc), r.n, l, esn0_db, pw, pw_pred, g_z * pwse);
  return 0;
}

/* The samples a symbol swept: generator rate and decimation. */
static const int g_sps[][2] = {
  { 60, 20 }, { 64, 20 }, { 64, 18 }, { 64, 16 }, { 64, 14 }, { 60, 12 },
  { 64, 12 }, { 60, 10 }, { 64, 10 }, { 63, 9 },  { 60, 8 },  { 64, 8 },
};
#define N_SPS ((int)(sizeof g_sps / sizeof g_sps[0]))

static int
axis_noise (void)
{
  say ("\nAxis 1. Noise. tau_hat against the eye center (a lone symbol's "
       "energy centroid through the same chain).\n"
       "  sd pred is first order, then one order further; corr is that "
       "correction's size and a row over 0.25 (*) is not held to it.\n");
  say ("  %-4s %6s %4s %5s %7s %2s | %-31s | %-49s | %6s | %6s\n", "pulse",
       "N", "L", "Es/N0", "carrier", "o", "mean error +- se (predicted)",
       "sd +- se        sd pred  2nd      corr", "E|c|^2", "qual");
  long m = g_check ? 2000 : 4000;
  if (noise_row (0, 64, 16, 128, 4.0, m, 101u, 1, 1) != 0)
    return -1;
  if (noise_row (0, 64, 20, 128, 4.0, m, 102u, 1, 1) != 0)
    return -1;
  if (noise_row (1, 64, 16, 512, 10.0, 2000, 103u, 1, 0) != 0)
    return -1;
  if (g_check)
    return 0;
  uint32_t s = 110u;
  for (int p = 0; p < 2; p++)
    for (int e = 0; e <= 14; e += 2)
      if (noise_row (p, 64, 16, 128, (double)e, m, s++, 0, 0) != 0)
        return -1;
  static const int ls[5] = { 32, 64, 128, 256, 512 };
  for (int p = 0; p < 2; p++)
    for (int g = 0; g < 2; g++)
      for (int i = 0; i < 5; i++)
        if (noise_row (p, 64, 16, ls[i], g ? 10.0 : 4.0,
                       ls[i] >= 256 ? m / 2 : m, s++, 0, 0)
            != 0)
          return -1;
  for (int p = 0; p < 2; p++)
    for (int i = 0; i < N_SPS; i++)
      {
        /* a generator rate that is no power of two shapes with the dense
           filter, which is slow: fewer trials there */
        long mm = (p && g_sps[i][0] != 64) ? m / 4 : m;
        if (noise_row (p, g_sps[i][0], g_sps[i][1], 128, 4.0, mm, s++, 0,
                       !(p == P_RECT && g_sps[i][0] == 64
                         && (g_sps[i][1] == 16 || g_sps[i][1] == 20)))
            != 0)
          return -1;
      }
  return 0;
}

/* ── axis 2: clock offset ─────────────────────────────────────────────── */
typedef struct
{
  double rate, tol;
} clock_out_t;

static int
clock_row (int rrc, int u, int d, int l, double esn0_db, double x, long trials,
           uint32_t seed, int assert_rate, int controls, clock_out_t *out)
{
  rowcfg_t cf = { 0 };
  cf.c        = chain_for (rrc, u, d);
  cf.c.chan   = 1;
  cf.o        = -1;
  cf.draw_f   = 1;
  cf.l        = l;
  cf.nblk     = 2;
  cf.esn0_db  = esn0_db;
  cf.trials   = trials;
  cf.seed     = seed;
  /* x cycles of the line a block: the eye moves -dil a symbol of the
     clock, so dil = -x / L. */
  double lc  = (double)lround (l * (double)u / d) * d / u;
  double dil = -x / lc;
  cf.c.ppm   = dil * 1e6;
  row_t r;
  if (row_run (&cf, &r) != 0)
    return -1;
  double sx   = dp_sinc (x);
  double rate = acc_mean (&r.rate), rse = acc_se (&r.rate);
  double rate_true = -dil;
  double rvar = acc_var (&r.rate), rvse = acc_var_se (&r.rate);
  double c2 = acc_mean (&r.c2), c2se = acc_se (&r.c2);
  /* the page's form: the line with no offset, times sinc^2 */
  double c2_page = r.pf.c2 - (1.0 - sx * sx) * r.pf.m_abs * r.pf.m_abs;
  double mean = acc_mean (&r.err), se = acc_se (&r.err);
  int    held = fabs (x) < 0.5 && r.p.corr <= 0.25;
  say ("  %-10s %6.3f %4d %5.1f %+6.2f | %+10.3e +- %8.1e  %+10.3e "
       "(sent %+10.3e) | %9.3e +- %7.1e  %9.3e%s | %6.4f %6.4f %6.4f | "
       "%+8.5f +- %7.5f (%+8.5f)\n",
       pulse_name (rrc), r.n, l, esn0_db, x, rate, rse, r.rate_pred, rate_true,
       rvar, rvse, r.rate_var, held ? " " : "*", c2 / r.pf.c2,
       r.p.c2 / r.pf.c2, c2_page / r.pf.c2, mean, se, r.p.bias);
  emitf (BLK_CLOCK,
         "%s,%.6f,%d,%.1f,%.4f,%ld,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%d,"
         "%.6e,%.6e,%.6e,%.6e,%.6e,%.6f,%.8f,%.8f,%.8f,%.6f\n",
         pulse_name (rrc), r.n, l, esn0_db, x, trials, rate, rse, r.rate_pred,
         rate_true, rvar, rvse, r.rate_var, held, c2, c2se, r.p.c2, r.pf.c2,
         c2_page, sx, mean, se, r.p.bias, acc_mean (&r.q));
  if (out)
    {
      out->rate = rate;
      out->tol  = g_z * rse;
    }
  /* At the edge and past it the line is weak and has left the bin: the
   row is printed and nothing is held. */
  if (fabs (x) >= 0.5)
    return 0;
  claim (WITHIN (rate, r.rate_pred, g_z * rse), NO_CONTROL,
         "clock %s L %d x %+.2f: rate %+.3e is not the predicted %+.3e "
         "within %.1e",
         pulse_name (rrc), l, x, rate, r.rate_pred, g_z * rse);
  if (!assert_rate)
    return 0;
  /* The wrong cases: for the band-limited pulse, a line the offset costs
   nothing; for the rectangle at a whole N, whose line depends on where
   the samples fall in the symbol, the page's sinc^2 applied to the line
   this block's own sampling phase would have; the eye center one
   generator sample away; a rate from blocks twice as long. */
  claim (WITHIN (c2, r.p.c2, g_z * c2se),
         controls ? WITHIN (c2, rrc == P_RRC ? r.pf.c2 : c2_page, g_z * c2se)
                  : NO_CONTROL,
         "clock %s L %d x %+.2f: E|c|^2 %.4e is not the predicted %.4e "
         "within %.2e",
         pulse_name (rrc), l, x, c2, r.p.c2, g_z * c2se);
  claim (WITHIN (mean, r.p.bias, g_z * se),
         controls ? WITHIN (mean + 1.0 / u, r.p.bias, g_z * se) : NO_CONTROL,
         "clock %s L %d x %+.2f: mean error at the block's middle %+.5f "
         "is not %+.5f within %.5f",
         pulse_name (rrc), l, x, mean, r.p.bias, g_z * se);
  /* Two blocks share the symbols at their joint; 4 / L of the variance is
     allowed for it. */
  double rtol = g_z * rvse + 4.0 / r.lc * r.rate_var;
  if (held)
    claim (WITHIN (rvar, r.rate_var, rtol),
           controls ? WITHIN (rvar, 0.25 * r.rate_var, rtol) : NO_CONTROL,
           "clock %s L %d x %+.2f: rate variance %.3e is not the "
           "predicted %.3e within %.1e",
           pulse_name (rrc), l, x, rvar, r.rate_var, rtol);
  /* The page's sinc holds where the line does not depend on where the
     samples fall in the symbol: the band-limited pulse. */
  if (rrc == P_RRC)
    claim (WITHIN (c2, c2_page, g_z * c2se), NO_CONTROL,
           "clock %s L %d x %+.2f: E|c|^2 %.4e is not the line times "
           "sinc^2, %.4e, within %.2e",
           pulse_name (rrc), l, x, c2, c2_page, g_z * c2se);
  return 0;
}

static int
axis_clock (void)
{
  say ("\nAxis 2. Clock offset through doppler_channel (no carrier of its "
       "own; the generator's is on). x is the line's cycles a block.\n"
       "  Two blocks a trial. The rate's prediction wraps past |x| = 1/2; "
       "E|c|^2 is over the line with no offset;\n  `page` is "
       "that line times sinc^2.\n");
  say ("  %-10s %6s %4s %5s %6s | %-58s | %-32s | %-20s | %s\n", "pulse", "N",
       "L", "Es/N0", "x", "rate +- se, predicted (sent)",
       "rate variance +- se, predicted", "E|c|^2 meas pred page",
       "mean error at the middle +- se (predicted)");
  long        m = g_check ? 1000 : 2000;
  clock_out_t in, past;
  if (clock_row (0, 64, 16, 128, 10.0, 0.25, m, 201u, 1, 1, &in) != 0)
    return -1;
  if (clock_row (0, 64, 16, 128, 10.0, 0.75, m, 202u, 0, 0, &past) != 0)
    return -1;
  if (clock_row (P_RRC, 64, 16, 512, 10.0, 0.25, m / 2, 203u, 1, 1, NULL) != 0)
    return -1;
  /* The control for "the rate is the clock offset": past half a cycle a
     block the same comparison, against the offset sent, has to fail. */
  claim (1, WITHIN (past.rate, 0.75 / 128.0, past.tol),
         "clock x +0.75: the rate still reads the offset sent, %+.3e",
         past.rate);
  if (g_check)
    return 0;
  static const double xs[] = { 0.0,  0.1, -0.1, -0.25, 0.4, -0.4, 0.5,
                               -0.5, 0.6, -0.6, -0.75, 1.0, -1.0, 1.25 };
  uint32_t            s    = 210u;
  for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++)
    if (clock_row (0, 64, 16, 128, 10.0, xs[i], m, s++, fabs (xs[i]) < 0.5, 0,
                   NULL)
        != 0)
      return -1;
  if (clock_row (0, 64, 20, 128, 10.0, 0.25, m, s++, 1, 0, NULL) != 0)
    return -1;
  if (clock_row (0, 64, 16, 512, 10.0, 0.25, m / 2, s++, 1, 0, NULL) != 0)
    return -1;
  static const double xr[] = { 0.0, -0.25, 0.75 };
  for (int i = 0; i < 3; i++)
    if (clock_row (1, 64, 16, 512, 10.0, xr[i], m / 4, s++, xr[i] < 0.5, 0,
                   NULL)
        != 0)
      return -1;
  return 0;
}

/* ── axis 3: Doppler ──────────────────────────────────────────────────── */
enum
{
  DOP_BOTH,    /* carrier and time base, as the channel applies both  */
  DOP_CARRIER, /* the same carrier offset from the generator, no
                  dilation                                            */
  DOP_CLOCK    /* the dilation alone: the control with no carrier     */
};
static const char *const g_dop[3] = { "both", "carrier", "clock" };

/* Carrier 5000 symbol rates up: 20 ppm is a tenth of the symbol rate. */
#define DOP_CARRIER_HZ 5000.0

static int
doppler_row (int rrc, int kind, double ppm, long trials, uint32_t seed,
             int controls, double *rate_out, double *tol_out, double *pred_out)
{
  rowcfg_t cf = { 0 };
  cf.c        = chain_for (rrc, 64, 16);
  cf.o        = -1;
  cf.l        = 512;
  cf.nblk     = 2;
  cf.esn0_db  = 10.0;
  cf.trials   = trials;
  cf.seed     = seed;
  double foff = DOP_CARRIER_HZ * ppm * 1e-6; /* cycles a symbol */
  if (kind == DOP_CARRIER)
    cf.c.f = foff;
  else
    {
      cf.c.chan       = 1;
      cf.c.ppm        = ppm;
      cf.c.carrier_hz = kind == DOP_BOTH ? DOP_CARRIER_HZ : 0.0;
      cf.f_chan       = kind == DOP_BOTH ? foff : 0.0;
    }
  row_t r;
  if (row_run (&cf, &r) != 0)
    return -1;
  double sent = kind == DOP_CARRIER ? 0.0 : -ppm * 1e-6;
  double rate = acc_mean (&r.rate), rse = acc_se (&r.rate);
  double mean = acc_mean (&r.err), se = acc_se (&r.err);
  double var = acc_var (&r.err), vse = acc_var_se (&r.err);
  double c2 = acc_mean (&r.c2), c2se = acc_se (&r.c2);
  int    held = r.p.corr <= 0.25;
  say ("  %-4s %-7s %+6.1f %+7.4f | %+8.2f +- %5.2f  %+8.2f (sent %+6.2f) | "
       "%+8.5f +- %7.5f | %8.6f +- %8.6f  %8.6f%s | %6.4f | %6.4f\n",
       pulse_name (rrc), g_dop[kind], ppm, kind == DOP_CLOCK ? 0.0 : foff,
       rate * 1e6, rse * 1e6, r.rate_pred * 1e6, sent * 1e6, mean, se,
       sqrt (var), vse / (2.0 * sqrt (var)),
       sqrt (r.p.var2 > 0.0 ? r.p.var2 : 0.0), held ? " " : "*", c2 / r.p.c2,
       acc_mean (&r.q));
  emitf (BLK_DOPPLER,
         "%s,%s,%.2f,%.6f,%ld,%.6e,%.6e,%.6e,%.6e,%.8f,%.8f,%.8f,%.6e,%.6e,"
         "%.6e,%d,%.6e,%.6e,%.6e,%.6f\n",
         pulse_name (rrc), g_dop[kind], ppm, kind == DOP_CLOCK ? 0.0 : foff,
         trials, rate, rse, r.rate_pred, sent, mean, se, r.p.bias, var, vse,
         r.p.var2, held, c2, c2se, r.p.c2, acc_mean (&r.q));
  if (rate_out)
    {
      *rate_out = rate;
      *tol_out  = g_z * rse;
      *pred_out = r.rate_pred;
    }
  claim (WITHIN (rate, r.rate_pred, g_z * rse), NO_CONTROL,
         "doppler %s %s %+.0f ppm: rate %+.2f ppm is not the predicted "
         "%+.2f ppm within %.2f",
         pulse_name (rrc), g_dop[kind], ppm, rate * 1e6, r.rate_pred * 1e6,
         1e6 * g_z * rse);
  /* The band-limited pulse's timing does not depend on where the samples
     fall in the symbol, so there the rate is the dilation itself. */
  if (rrc == P_RRC)
    claim (WITHIN (rate, sent, g_z * rse), NO_CONTROL,
           "doppler %s %s %+.0f ppm: rate %+.2f ppm is not the %+.2f ppm "
           "sent within %.2f",
           pulse_name (rrc), g_dop[kind], ppm, rate * 1e6, sent * 1e6,
           1e6 * g_z * rse);
  claim (WITHIN (mean, r.p.bias, g_z * se),
         controls ? WITHIN (mean + 1.0 / 64.0, r.p.bias, g_z * se)
                  : NO_CONTROL,
         "doppler %s %s %+.0f ppm: mean error %+.5f is not %+.5f within "
         "%.5f",
         pulse_name (rrc), g_dop[kind], ppm, mean, r.p.bias, g_z * se);
  if (held)
    claim (WITHIN (var, r.p.var2, g_z * vse),
           controls ? WITHIN (var, 0.5 * r.p.var2, g_z * vse) : NO_CONTROL,
           "doppler %s %s %+.0f ppm: variance %.3e is not the pulse's "
           "%.3e at that carrier within %.1e",
           pulse_name (rrc), g_dop[kind], ppm, var, r.p.var2, g_z * vse);
  claim (WITHIN (c2, r.p.c2, g_z * c2se), NO_CONTROL,
         "doppler %s %s %+.0f ppm: E|c|^2 %.4e is not the pulse's %.4e "
         "at that carrier within %.1e",
         pulse_name (rrc), g_dop[kind], ppm, c2, r.p.c2, g_z * c2se);
  return 0;
}

/* A Doppler rate: `q` is the change in clock offset across a block times
   the block's length. The channel is set so the offset is zero at block
   1's middle, and carries a carrier 5 symbol rates up beside the
   generator's drawn one. */
static int
chirp_row (int rrc, int l, double q, long trials, uint32_t seed, int controls)
{
  rowcfg_t cf     = { 0 };
  cf.c            = chain_for (rrc, 64, 16);
  cf.c.chan       = 1;
  cf.c.carrier_hz = 5.0;
  cf.o            = 0;
  cf.draw_f       = 1;
  cf.l            = l;
  cf.nblk         = 2;
  cf.esn0_db      = 10.0;
  cf.trials       = trials;
  cf.seed         = seed;
  double n = 4.0, lc = (double)l;
  long   nb = 4 * (long)l, n0 = (long)ceil (LEAD * n);
  double t_mid = (((double)n0 + 0.5 * (double)(nb - 1)) * cf.c.d) / cf.c.u;
  cf.c.ppm_s   = q / (lc * lc) * 1e6;
  cf.c.ppm     = -cf.c.ppm_s * t_mid;
  row_t r;
  if (row_run (&cf, &r) != 0)
    return -1;
  /* The page's form: the eye's phase on the clock curves as
     -q/2 (t/L)^2 about the middle, which costs the line this integral. */
  double _Complex fz = fresnel (-q);
  double loss = cabs (fz) * cabs (fz), shift = carg (fz) / TWO_PI;
  double c2 = acc_mean (&r.c2), c2se = acc_se (&r.c2);
  double c2_page = r.pf.c2 - (1.0 - loss) * r.pf.m_abs * r.pf.m_abs;
  double mean = acc_mean (&r.err), se = acc_se (&r.err);
  double rate = acc_mean (&r.rate), rse = acc_se (&r.rate);
  say ("  %-4s %4d %+6.2f %+9.3f | %6.4f +- %6.4f  %6.4f %6.4f | %+8.5f +- "
       "%7.5f  %+8.5f %+8.5f | %+9.2f +- %5.2f  %+9.2f\n",
       pulse_name (rrc), l, q, cf.c.ppm_s, c2 / r.pf.c2, c2se / r.pf.c2,
       r.p.c2 / r.pf.c2, c2_page / r.pf.c2, mean, se, r.p.bias,
       r.pf.bias + shift, rate * 1e6, rse * 1e6, r.rate_pred * 1e6);
  emitf (BLK_CHIRP,
         "%s,%d,%.4f,%.6f,%ld,%.6e,%.6e,%.6e,%.6e,%.6e,%.6f,%.8f,%.8f,%.8f,"
         "%.8f,%.6e,%.6e,%.6e\n",
         pulse_name (rrc), l, q, cf.c.ppm_s, trials, c2, c2se, r.p.c2, r.pf.c2,
         c2_page, loss, mean, se, r.p.bias, r.pf.bias + shift, rate, rse,
         r.rate_pred);
  /* The wrong case for both: a block the Doppler rate does not touch. */
  claim (WITHIN (c2, r.p.c2, g_z * c2se),
         controls ? WITHIN (c2, r.pf.c2, g_z * c2se) : NO_CONTROL,
         "doppler rate q %+.2f: E|c|^2 %.4e is not the predicted %.4e "
         "within %.1e",
         q, c2, r.p.c2, g_z * c2se);
  claim (WITHIN (mean, r.p.bias, g_z * se),
         controls ? WITHIN (mean, r.pf.bias, g_z * se) : NO_CONTROL,
         "doppler rate q %+.2f: mean error %+.5f is not the predicted "
         "%+.5f within %.5f",
         q, mean, r.p.bias, g_z * se);
  if (fabs (r.d_junc * r.lc) < 0.45)
    claim (WITHIN (rate, r.rate_pred, g_z * rse), NO_CONTROL,
           "doppler rate q %+.2f: rate %+.2f ppm is not the predicted "
           "%+.2f ppm within %.2f",
           q, rate * 1e6, r.rate_pred * 1e6, 1e6 * g_z * rse);
  /* The page's integral, where the line does not depend on the sampling
     phase. */
  if (rrc == P_RRC)
    {
      claim (WITHIN (c2, c2_page, g_z * c2se), NO_CONTROL,
             "doppler rate rrc q %+.2f: E|c|^2 %.4e is not the line times "
             "the integral's square, %.4e, within %.1e",
             q, c2, c2_page, g_z * c2se);
      claim (WITHIN (mean, r.pf.bias + shift, g_z * se), NO_CONTROL,
             "doppler rate rrc q %+.2f: mean error %+.5f is not the "
             "integral's argument, %+.5f, within %.5f",
             q, mean, r.pf.bias + shift, g_z * se);
    }
  return 0;
}

static int
axis_doppler (void)
{
  say ("\nAxis 3. Doppler through doppler_channel: carrier %.0f symbol "
       "rates up, rectangle unless said, N 4, L 512, Es/N0 10 dB.\n"
       "  `carrier` is the same offset from the generator with no "
       "dilation; `clock` is the dilation with no carrier (the control).\n",
       DOP_CARRIER_HZ);
  say ("  %-4s %-7s %6s %7s | %-27s | %-19s | %-28s | %6s | %6s\n", "pulse",
       "kind", "ppm", "carrier", "rate ppm +- se, predicted (sent)",
       "mean error +- se", "sd +- se, pulse's at carrier", "E|c|^2", "qual");
  long   m = g_check ? 400 : 1000;
  double rate_c, tol_c, rate_b, tol_b, pred_b, unused;
  if (doppler_row (0, DOP_BOTH, 20.0, m, 301u, 1, &rate_b, &tol_b, &pred_b)
      != 0)
    return -1;
  if (doppler_row (0, DOP_CARRIER, 20.0, m, 302u, 0, &rate_c, &tol_c, &unused)
      != 0)
    return -1;
  /* The control for "the dilation is read as rate": the carrier alone,
     held to the rate the dilation would have given, has to fail. */
  (void)rate_b;
  (void)tol_b;
  claim (1, WITHIN (rate_c, pred_b, tol_c),
         "doppler carrier alone: the rate reads the dilation's %+.2f ppm "
         "with no dilation (%+.2f ppm)",
         pred_b * 1e6, rate_c * 1e6);
  say ("\n  Doppler rate: q is the change in clock offset across a block "
       "times L. L 256, carrier 5 symbol rates up.\n");
  say ("  %-4s %4s %6s %9s | %-31s | %-38s | %s\n", "pulse", "L", "q", "ppm/s",
       "E|c|^2 +- se, predicted, page", "mean error +- se, predicted, page",
       "rate ppm +- se, predicted");
  if (chirp_row (P_RECT, 256, 2.0, m, 303u, 1) != 0)
    return -1;
  if (g_check)
    return 0;
  static const double ppms[] = { -20.0, -10.0, -5.0, 5.0, 10.0 };
  uint32_t            s      = 310u;
  for (int k = 0; k < 3; k++)
    for (size_t i = 0; i < sizeof ppms / sizeof ppms[0]; i++)
      if (doppler_row (0, k, ppms[i], m, s++, 0, NULL, NULL, NULL) != 0)
        return -1;
  if (doppler_row (0, DOP_CLOCK, 20.0, m, s++, 0, NULL, NULL, NULL) != 0)
    return -1;
  for (int k = 0; k < 3; k++)
    if (doppler_row (1, k, 20.0, m / 2, s++, 0, NULL, NULL, NULL) != 0)
      return -1;
  static const double qs[] = { 0.01, 0.5, -0.5, 1.0, -1.0, -2.0, 4.0 };
  for (size_t i = 0; i < sizeof qs / sizeof qs[0]; i++)
    if (chirp_row (P_RECT, 256, qs[i], m, s++, 0) != 0)
      return -1;
  if (chirp_row (P_RRC, 512, 2.0, m / 2, s++, 0) != 0)
    return -1;
  if (chirp_row (P_RRC, 512, -1.0, m / 2, s++, 0) != 0)
    return -1;
  return 0;
}

/* ── axis 4: the quality figure ───────────────────────────────────────── */
/* Noise alone: the false-alarm probability at the threshold computed for
   `pfa`, and at the threshold white samples would call for. Fills `eta`. */
static int
h0_row (int rrc, int u, int d, int l, double pfa, long trials, uint32_t seed,
        int controls, double *eta_out, double *hi_out)
{
  chain_t c = chain_for (rrc, u, d);
  pulse_t pl;
  if (pulse_measure (&c, LEAD, &pl) != 0)
    return -1;
  double n  = (double)u / d;
  long   nb = lround (l * n);
  pred_t h0;
  predict (&pl, u, d, 0.0, 0.0, nb, 1.0, 0, 1.0, 0.0, 0.0, &h0);
  /* Four thresholds. Two bracket the target: the one that also averages
     over the spread of the block's power, which overstates that spread's
     effect because |c| and the power rise and fall together, and the
     Rayleigh one, which leaves it out. Two are the wrong cases: samples
     taken as independent (E|c|^2 = n sigma_w^4), and the threshold for
     ten times the target. */
  enum
  {
    T_SPREAD,
    T_RAYLEIGH,
    T_WHITE,
    T_TENFOLD,
    T_N
  };
  pred_t white = h0;
  white.c2     = (double)nb * (h0.dtot / nb) * (h0.dtot / nb);
  double eta[T_N];
  eta[T_SPREAD]   = h0_threshold (&h0, pfa, 1);
  eta[T_RAYLEIGH] = h0_threshold (&h0, pfa, 0);
  eta[T_WHITE]    = h0_threshold (&white, pfa, 0);
  eta[T_TENFOLD]  = h0_threshold (&h0, 10.0 * pfa, 1);
  pulse_free (&pl);

  rig_t r;
  if (rig_open (&r, &c) != 0)
    return -1;
  long  hits[T_N] = { 0 };
  acc_t q         = { 0 };
  long  n0        = (long)ceil (LEAD * n);
  for (long t = 0; t < trials; t++)
    {
      size_t ny = run_noise (&r, (size_t)(n0 + nb), 1.0f,
                             ((uint64_t)seed << 32) | (uint64_t)(t + 1));
      if (ny < (size_t)(n0 + nb))
        {
          rig_close (&r);
          return -1;
        }
      dp_timing_est_reset (r.te);
      dp_timing_estimate_t e
          = dp_timing_est_estimate (r.te, g_y + n0, (size_t)nb);
      acc_add (&q, e.quality);
      for (int j = 0; j < T_N; j++)
        hits[j] += e.quality > eta[j];
    }
  rig_close (&r);
  double lo[T_N], hi[T_N];
  for (int j = 0; j < T_N; j++)
    dp_wilson ((double)hits[j], (double)trials, &lo[j], &hi[j]);
  /* Rayleigh mean of |c| over the mean power. */
  double q_pred = sqrt (M_PI * h0.c2 / 4.0) / h0.dtot;
  say ("  %-4s %6.3f %4d %7.0e | %7.5f  %7.5f [%7.5f, %7.5f] | %7.5f  "
       "%7.5f [%7.5f, %7.5f] | %7.5f  %7.5f | %7.5f +- %7.5f  %7.5f\n",
       pulse_name (rrc), n, l, pfa, eta[T_SPREAD],
       (double)hits[T_SPREAD] / trials, lo[T_SPREAD], hi[T_SPREAD],
       eta[T_RAYLEIGH], (double)hits[T_RAYLEIGH] / trials, lo[T_RAYLEIGH],
       hi[T_RAYLEIGH], eta[T_WHITE], (double)hits[T_WHITE] / trials,
       acc_mean (&q), acc_se (&q), q_pred);
  emitf (BLK_H0,
         "%s,%.6f,%d,%.0e,%ld,%.6f,%ld,%.6f,%.6f,%.6f,%.6f,%ld,%.6f,%.6f,"
         "%.6f,%.6f,%ld,%.6f,%.6f,%.6f,%.6f,%.6f\n",
         pulse_name (rrc), n, l, pfa, trials, eta[T_SPREAD], hits[T_SPREAD],
         (double)hits[T_SPREAD] / trials, lo[T_SPREAD], hi[T_SPREAD],
         eta[T_RAYLEIGH], hits[T_RAYLEIGH], (double)hits[T_RAYLEIGH] / trials,
         lo[T_RAYLEIGH], hi[T_RAYLEIGH], eta[T_WHITE], hits[T_WHITE],
         lo[T_WHITE], hi[T_WHITE], acc_mean (&q), acc_se (&q), q_pred);
  claim (lo[T_SPREAD] <= pfa, controls ? lo[T_TENFOLD] <= pfa : NO_CONTROL,
         "quality %s N %.3f L %d: at the threshold that allows for the "
         "power's spread, false alarms %.5f [%.5f, %.5f] are over the "
         "target %.0e",
         pulse_name (rrc), n, l, (double)hits[T_SPREAD] / trials, lo[T_SPREAD],
         hi[T_SPREAD], pfa);
  claim (hi[T_RAYLEIGH] >= pfa, controls ? hi[T_WHITE] >= pfa : NO_CONTROL,
         "quality %s N %.3f L %d: at the Rayleigh threshold, false alarms "
         "%.5f [%.5f, %.5f] are under the target %.0e",
         pulse_name (rrc), n, l, (double)hits[T_RAYLEIGH] / trials,
         lo[T_RAYLEIGH], hi[T_RAYLEIGH], pfa);
  claim (WITHIN (acc_mean (&q), q_pred, g_z * acc_se (&q)),
         controls
             ? WITHIN (acc_mean (&q), sqrt (M_PI * white.c2 / 4.0) / h0.dtot,
                       g_z * acc_se (&q))
             : NO_CONTROL,
         "quality %s N %.3f L %d: noise-alone mean %.5f is not the "
         "predicted %.5f",
         pulse_name (rrc), n, l, acc_mean (&q), q_pred);
  if (eta_out)
    *eta_out = eta[T_SPREAD];
  if (hi_out)
    *hi_out = hi[T_SPREAD];
  return 0;
}

/* Signal at the threshold `eta`. Returns the hits' interval. */
static int
h1_row (int rrc, int l, double esn0_db, int constant, double pfa, double eta,
        long trials, uint32_t seed, double *lo_out, double *hi_out,
        double *pred_out)
{
  rowcfg_t cf = { 0 };
  cf.c        = chain_for (rrc, 64, 16);
  cf.o        = -1;
  cf.draw_f   = 1;
  cf.l        = l;
  cf.nblk     = 1;
  cf.esn0_db  = esn0_db;
  cf.trials   = trials;
  cf.seed     = seed;
  cf.constant = constant;
  cf.thr[0]   = eta;
  row_t r;
  if (row_run (&cf, &r) != 0)
    return -1;
  double lo, hi, pd = (double)r.hits[0] / trials;
  dp_wilson ((double)r.hits[0], (double)trials, &lo, &hi);
  double pred = constant ? NAN : pd_model (&r.p, eta);
  say ("  %-4s %4d %5.1f %-8s | %7.5f | %6.4f [%6.4f, %6.4f]  %6.4f | "
       "%6.4f\n",
       pulse_name (rrc), l, esn0_db, constant ? "constant" : "data", eta, pd,
       lo, hi, pred, acc_mean (&r.q));
  emitf (BLK_H1, "%s,%d,%.1f,%s,%.0e,%.6f,%ld,%ld,%.6f,%.6f,%.6f,%.6f,%.6f\n",
         pulse_name (rrc), l, esn0_db, constant ? "constant" : "data", pfa,
         eta, trials, r.hits[0], pd, lo, hi, pred, acc_mean (&r.q));
  *lo_out   = lo;
  *hi_out   = hi;
  *pred_out = pred;
  return 0;
}

/* The Gaussian form's allowance, beyond the interval. */
#define PD_ALLOW 0.03

static int
axis_quality (void)
{
  say ("\nAxis 4. The quality figure. Noise alone: P(quality > threshold) "
       "with its Wilson 95%% interval, at the threshold that allows for\n"
       "  the spread of the block's power and at the Rayleigh one, which "
       "bracket the target; at the one independent samples would call\n"
       "  for; then the mean quality against the Rayleigh mean.\n");
  say ("  %-4s %6s %4s %7s | %-37s | %-37s | %-16s | %s\n", "pulse", "N", "L",
       "target", "with spread: threshold  P [interval]",
       "Rayleigh: threshold  P [interval]", "white: thr  P",
       "mean quality +- se, predicted");
  long   m0 = g_check ? 20000 : 200000;
  double eta, h0_hi;
  if (h0_row (0, 64, 16, 128, 1e-2, m0, 401u, 1, &eta, &h0_hi) != 0)
    return -1;
  double eta_r = 0.0, hi_r;
  if (!g_check)
    {
      static const int g[][3]
          = { { 0, 16, 512 }, { 0, 20, 128 }, { 0, 8, 128 }, { 1, 16, 128 } };
      uint32_t s = 410u;
      for (int i = 0; i < 4; i++)
        if (h0_row (g[i][0], 64, g[i][1], g[i][2], 1e-2, m0, s++, 0, NULL,
                    NULL)
            != 0)
          return -1;
      if (h0_row (1, 64, 16, 512, 1e-2, m0, s++, 0, &eta_r, &hi_r) != 0)
        return -1;
      if (h0_row (0, 64, 16, 128, 1e-3, m0, s++, 0, NULL, NULL) != 0)
        return -1;
    }

  say ("\n  Signal at the 1e-2 threshold: P(quality > threshold), Wilson "
       "95%%, and the normal form's figure.\n");
  say ("  %-4s %4s %5s %-8s | %7s | %-31s | %6s\n", "pulse", "L", "Es/N0",
       "symbols", "thresh", "P [interval]  predicted", "qual");
  long   m1 = g_check ? 2000 : 4000;
  double lo, hi, pred;
  if (h1_row (0, 128, 0.0, 0, 1e-2, eta, m1, 420u, &lo, &hi, &pred) != 0)
    return -1;
  /* The wrong case: noise alone at the same threshold must not read as
     this detection probability. */
  claim (lo - PD_ALLOW <= pred && pred <= hi + PD_ALLOW,
         -PD_ALLOW <= pred && pred <= h0_hi + PD_ALLOW,
         "quality rect L 128 0 dB: detection [%.4f, %.4f] does not hold "
         "the predicted %.4f within %.2f",
         lo, hi, pred, PD_ALLOW);
  double lo_d, hi_d, lo_c, hi_c, unused;
  if (h1_row (0, 128, 10.0, 0, 1e-2, eta, m1 / 2, 421u, &lo_d, &hi_d, &unused)
      != 0)
    return -1;
  if (h1_row (0, 128, 10.0, 1, 1e-2, eta, m1, 422u, &lo_c, &hi_c, &unused)
      != 0)
    return -1;
  /* No transitions: refused no less often than noise. The wrong case is
     the same stream with its transitions, which is not refused. */
  claim (lo_c <= 1e-2, lo_d <= 1e-2,
         "quality rect L 128 10 dB, no transitions: exceeds the threshold "
         "%.4f of the time or more, over the false-alarm target",
         lo_c);
  if (g_check)
    return 0;
  uint32_t s = 430u;
  for (int e = 2; e <= 14; e += 2)
    {
      if (e == 10)
        continue;
      if (h1_row (0, 128, (double)e, 0, 1e-2, eta, m1, s++, &lo, &hi, &pred)
          != 0)
        return -1;
      claim (lo - PD_ALLOW <= pred && pred <= hi + PD_ALLOW, NO_CONTROL,
             "quality rect L 128 %d dB: detection [%.4f, %.4f] does not "
             "hold the predicted %.4f",
             e, lo, hi, pred);
    }
  for (int e = 0; e <= 14; e += 2)
    if (h1_row (1, 512, (double)e, 0, 1e-2, eta_r, m1 / 4, s++, &lo, &hi,
                &pred)
        != 0)
      return -1;
  if (h1_row (0, 128, 4.0, 1, 1e-2, eta, m1, s++, &lo_c, &hi_c, &unused) != 0)
    return -1;
  claim (lo_c <= 1e-2, NO_CONTROL,
         "quality rect L 128 4 dB, no transitions: exceeds the threshold "
         "%.4f of the time or more",
         lo_c);
  if (h1_row (1, 512, 10.0, 1, 1e-2, eta_r, m1 / 4, s++, &lo_c, &hi_c, &unused)
      != 0)
    return -1;
  claim (lo_c <= 1e-2, NO_CONTROL,
         "quality rrc L 512 10 dB, no transitions: exceeds the threshold "
         "%.4f of the time or more",
         lo_c);
  return 0;
}

/* ── axis 5: starting offset ──────────────────────────────────────────── */
static int
offset_sweep (int rrc, int d, int l, double esn0_db, long trials,
              uint32_t seed, int controls)
{
  enum
  {
    U = 64
  };
  double chi = 0.0, chi_fixed = 0.0, zmax = 0.0, eye0 = 0.0, vr = 0.0;
  double bias0 = 0.0, b_lo = 1.0, b_hi = -1.0, l_lo = 1e300, l_hi = 0.0;
  for (int o = 0; o < U; o++)
    {
      rowcfg_t cf = { 0 };
      cf.c        = chain_for (rrc, U, d);
      /* one carrier for the whole sweep, drawn from the sweep's seed */
      uint32_t rng = seed_mix (seed, 77u);
      double   mag = 0.01 + 0.04 * dp_uni (&rng);
      cf.c.f       = dp_uni (&rng) < 0.5 ? -mag : mag;
      cf.o         = o;
      cf.l         = l;
      cf.nblk      = 1;
      cf.esn0_db   = esn0_db;
      cf.trials    = trials;
      cf.seed      = seed_mix (seed, (uint32_t)o + 100u);
      row_t r;
      if (row_run (&cf, &r) != 0)
        return -1;
      double mean = acc_mean (&r.err), se = acc_se (&r.err);
      double z = (mean - r.p.bias) / se;
      if (o == 0)
        {
          eye0  = r.eye;
          bias0 = r.p.bias;
        }
      /* The wrong case: the eye center taken as offset 0's at every
         offset, as if the starting offset did not move it. */
      double zf = (wrap_half (mean + r.eye - eye0) - bias0) / se;
      chi += z * z / U;
      chi_fixed += zf * zf / U;
      if (fabs (z) > zmax)
        zmax = fabs (z);
      vr += acc_var (&r.err) / r.p.var2 / U;
      double line = r.p.m_abs / r.p.dtot;
      b_lo        = fmin (b_lo, r.p.bias);
      b_hi        = fmax (b_hi, r.p.bias);
      l_lo        = fmin (l_lo, line);
      l_hi        = fmax (l_hi, line);
      emitf (BLK_OFFSET,
             "%s,%.6f,%d,%.6f,%.8f,%.8f,%.8f,%.6e,%.6e,%.6f,%.6f\n",
             pulse_name (rrc), r.n, o, r.eye, mean, se, r.p.bias,
             acc_var (&r.err), r.p.var2, line, acc_mean (&r.q));
    }
  double bound = 1.0 + g_z * sqrt (2.0 / U);
  say ("  %-10s %6.3f %4d %5.1f %5ld | %5.2f | %5.3f  %5.3f | %9.1f | "
       "%5.3f | %+8.5f to %+8.5f | %6.4f to %6.4f\n",
       pulse_name (rrc), (double)U / d, l, esn0_db, trials, zmax, chi, bound,
       chi_fixed, vr, b_lo, b_hi, l_lo, l_hi);
  emitf (BLK_OFFSUM,
         "%s,%.6f,%d,%.1f,%ld,%.4f,%.4f,%.4f,%.4f,%.4f,%.8f,%.8f,%.6f,%.6f\n",
         pulse_name (rrc), (double)U / d, l, esn0_db, trials, zmax, chi, bound,
         chi_fixed, vr, b_lo, b_hi, l_lo, l_hi);
  claim (zmax <= g_z && chi <= bound, controls ? chi_fixed <= bound : -1,
         "offset %s N %.3f: over 64 starting offsets the largest "
         "normalized mean error is %.2f and their mean square %.3f "
         "(bound %.3f)",
         pulse_name (rrc), (double)U / d, zmax, chi, bound);
  return 0;
}

static int
axis_offset (void)
{
  say ("\nAxis 5. Starting offset: 64 offsets across one symbol. z is the "
       "mean error less its prediction over its standard error.\n");
  say ("  %-10s %6s %4s %5s %5s | %5s | %-12s | %9s | %5s | %-20s | %s\n",
       "pulse", "N", "L", "Es/N0", "each", "max z", "mean z^2 bound",
       "fixed eye", "var", "predicted bias", "predicted line");
  if (offset_sweep (0, 16, 128, 10.0, g_check ? 100 : 500, 501u, 1) != 0)
    return -1;
  if (g_check)
    return 0;
  if (offset_sweep (0, 20, 128, 10.0, 500, 502u, 0) != 0)
    return -1;
  if (offset_sweep (1, 16, 512, 10.0, 200, 503u, 0) != 0)
    return -1;
  /* The rectangle merely sampled: 16 generator offsets give the same
     samples, so the timing it carries moves in steps of a sample. */
  if (offset_sweep (P_POINT, 16, 128, 10.0, 500, 504u, 0) != 0)
    return -1;
  return 0;
}

/* The chain's level: the generator's stream has unit power for both
   pulses, so a symbol's energy is one a symbol period and the noise a
   sample at the estimator's rate is N / (Es/N0), whatever the front end
   does to the samples. */
static int
level_check (int rrc)
{
  chain_t c = chain_for (rrc, 64, 16);
  rig_t   r;
  if (rig_open (&r, &c) != 0)
    return -1;
  enum
  {
    NS = 1500
  };
  dp_pn_state_t *pn
      = dp_xnn (dp_pn_create (wfm_synth_mls_poly (31), 5u, 31, 0));
  static uint8_t bits[NS];
  dp_pn_generate (pn, NS, bits, NS);
  for (int k = 0; k < NS; k++)
    g_sym[k] = bits[k] ? 1.0f : -1.0f;
  dp_pn_destroy (pn);
  size_t ny = run_chain (&r, 0, NS, 0.0f, 0);
  rig_close (&r);
  if (!ny)
    return -1;
  /* g_hi still holds the generator's stream */
  double p = 0.0;
  long   a = LEAD * 64, b = (NS - LEAD) * 64;
  for (long i = a; i < b; i++)
    p += (double)(crealf (g_hi[i]) * crealf (g_hi[i])
                  + cimagf (g_hi[i]) * cimagf (g_hi[i]));
  p /= (double)(b - a);
  say ("  %s: the generator's mean power %.5f\n", pulse_name (rrc), p);
  DP_CHECK_NEAR (p, 1.0, 0.01);
  return 0;
}

int
main (int argc, char **argv)
{
  int         emit = 0;
  const char *csv  = NULL;
  for (int i = 1; i < argc; i++)
    {
      g_check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--csv") == 0 && i + 1 < argc)
        csv = argv[++i];
    }
  g_say = emit ? stderr : stdout;
  g_z   = g_check ? 4.0 : 5.0;
  for (int b = 0; b < BLK_N; b++)
    g_blk[b] = (char *)dp_xnn (calloc (BLK_CAP, 1));

  say ("SymbolTimingEstimator: %s. A row is held within %.0f standard "
       "errors of its own trials.\n",
       g_check ? "the --check rows" : "the full sweep", g_z);
  int rc = level_check (0) || level_check (1) || axis_noise () || axis_clock ()
           || axis_doppler () || axis_quality () || axis_offset ();
  DP_CHECK_MSG (rc == 0, "a row could not be run");

  static const char *const name[BLK_N]
      = { "constants",  "noise",        "clock",
          "doppler",    "doppler_rate", "quality_h0",
          "quality_h1", "offset",       "offset_summary" };
  static const char *const head[BLK_N] = {
    "pulse,sps,L,carrier,offset,k_ss,k_sn,k_nn,line,bias",
    "pulse,sps,L,esn0_db,carrier,offset,trials,err_mean,err_se,"
    "bias_pred,var,var_se,var1,var2,corr,held,c2,c2_se,c2_pred,"
    "power,power_se,power_pred,quality",
    "pulse,sps,L,esn0_db,x,trials,rate,rate_se,rate_pred,"
    "rate_true,rate_var,rate_var_se,rate_var_pred,held,c2,c2_se,"
    "c2_pred,c2_flat,c2_page,sinc,err_mean,err_se,bias_pred,"
    "quality",
    "pulse,kind,ppm,carrier,trials,rate,rate_se,rate_pred,"
    "rate_sent,err_mean,err_se,bias_pred,var,var_se,var2,held,c2,"
    "c2_se,c2_pred,quality",
    "pulse,L,q,ppm_s,trials,c2,c2_se,c2_pred,c2_flat,c2_page,"
    "loss,err_mean,err_se,err_pred,err_page,rate,rate_se,"
    "rate_pred",
    "pulse,sps,L,pfa,trials,threshold,hits,p,lo,hi,ray_threshold,"
    "ray_hits,ray_p,ray_lo,ray_hi,white_threshold,white_hits,"
    "white_lo,white_hi,q_mean,q_se,q_pred",
    "pulse,L,esn0_db,symbols,pfa,threshold,trials,hits,pd,lo,hi,"
    "pd_pred,quality",
    "pulse,sps,offset,eye,err_mean,err_se,bias_pred,var,var_pred,"
    "line_pred,quality",
    "pulse,sps,L,esn0_db,trials,z_max,chi2,bound,chi2_fixed,"
    "var_ratio,bias_lo,bias_hi,line_lo,line_hi",
  };
  /* `--csv DIR`: the same blocks, one file each, which is how the full
     sweep is kept as the report's record. */
  if (csv)
    for (int b = 0; b < BLK_N; b++)
      {
        char path[1024];
        snprintf (path, sizeof path, "%s/%s.csv", csv, name[b]);
        FILE *fh = fopen (path, "w");
        DP_CHECK_MSG (fh != NULL, "a --csv file could not be opened");
        if (fh)
          {
            fprintf (fh, "%s\n%s", head[b], g_blk[b]);
            fclose (fh);
          }
      }
  if (emit)
    {
      for (int b = 0; b < BLK_N; b++)
        printf ("# %s\n%s\n%s\n", name[b], head[b], g_blk[b]);
      DP_TEST_EMIT_END ("validate_timing_est_noise");
    }
  DP_TEST_END ("validate_timing_est_noise");
}
