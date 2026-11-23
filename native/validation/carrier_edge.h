/**
 * @file carrier_edge.h
 * @brief The pre-despread carrier loop's mean-loop derivation at the
 *        415.1-B-1 point: its detector's S-curve, the mean loop on it, and
 *        the worst-start no-slip edge.
 *
 * One home for the derivation docs/design/async-dsss-receiver.md's
 * "The pre-despread carrier loop" row prints
 * (validate_costas_pullin --design) and the error-rate window reads (the
 * pull-in harness's budget check, "The error-rate window"): a hand-over
 * inside the edge is the one whose pull-in the analytic budget predicts.
 * Validation-only; static inline so a translation unit pays for what it
 * calls.
 *
 * The edge: the largest frequency step, in B_L = bn / T, from which no start
 * phase (worst start) or the start at 0 crosses two or more lock points on
 * the mean loop -- the loop's own recursion (costas_update()'s order) with
 * the detector's mean S-curve, by quadrature of the window sum's Gaussian
 * noise, in place of the noisy discriminator. Nothing is drawn.
 */
#ifndef DP_CARRIER_EDGE_H
#define DP_CARRIER_EDGE_H

#include "dp_s415_test.h"

#include "doppler/loop_filter/loop_filter_core.h"
#include <math.h>
#include <stdlib.h>

#define HB_NPH 361    /* S-curve points over (-90, 90] degrees, 0.5 deg  */
#define HB_NQ 481     /* quadrature points a noise axis, +/-6 sigma    */
#define HB_NSTART 144 /* start phases --design reads per step and sign  */

/* The detectors --design derives. TODAY is Costas's own, the one the draws
   run; the three I*Q forms on the window sum w = I + jQ differ only in
   what divides I*Q: the signal power A^2 (unit amplitude), the update's own
   |w|^2 (the limiter carrier_nda_disc() is), or the mean total power
   A^2 + E|n|^2. */
enum
{
  HB_DET_TODAY,
  HB_DET_IQ_UNIT,
  HB_DET_IQ_LIM,
  HB_DET_IQ_PWR,
  HB_N_DET
};
/** One configuration: a cell's update SNR and one of the two bandwidths. */
typedef struct
{
  double fc, esn0, bn, t_upd;   /* carrier, Es/N0 dB, bn, code period s    */
  int    det;                   /* HB_DET_*: the detector derived          */
  double gamma_db;              /* C/N0 T, the update's coherent SNR, dB    */
  double ph[HB_NPH], s[HB_NPH]; /* the mean S-curve E[e | phase error]    */
  double k, v;                  /* its slope at 0; var(e) at the lock point */
  double beff, sigma;           /* noise bandwidth at slope k, Hz; jitter   */
} hb_cfg_t;

static inline double
hb_fold (double a)
{
  return a - M_PI * floor (a / M_PI + 0.5);
}

/* The detector's statistics at update SNR gamma (linear): the S-curve
   E[sin(fold(arg(A e^{j phi} + n)))] over the phase error phi, its slope at
   0 and its variance at 0, by quadrature of the complex Gaussian n (unit
   variance a rail, A^2 = 2 gamma). This is Costas's own discriminator,
   sgn(Re P) Im P / |P| (costas_update()), written as the folded phase. */
static inline void
hb_scurve (hb_cfg_t *c)
{
  const double a    = sqrt (2.0 * pow (10.0, c->gamma_db / 10.0));
  const double h    = 12.0 / (HB_NQ - 1);
  double       wsum = 0.0;
  for (int i = 0; i < HB_NQ; i++)
    for (int j = 0; j < HB_NQ; j++)
      {
        const double x = -6.0 + i * h, y = -6.0 + j * h;
        wsum += exp (-0.5 * (x * x + y * y));
      }
  for (int p = 0; p < HB_NPH; p++)
    {
      c->ph[p]   = -0.5 * M_PI + M_PI * p / (HB_NPH - 1);
      double acc = 0.0, acc2 = 0.0;
      for (int i = 0; i < HB_NQ; i++)
        for (int j = 0; j < HB_NQ; j++)
          {
            const double x = -6.0 + i * h, y = -6.0 + j * h;
            const double w  = exp (-0.5 * (x * x + y * y)) / wsum;
            const double ii = a * cos (c->ph[p]) + x;
            const double qq = a * sin (c->ph[p]) + y;
            const double e
                = c->det == HB_DET_TODAY     ? sin (hb_fold (atan2 (qq, ii)))
                  : c->det == HB_DET_IQ_UNIT ? ii * qq / (a * a)
                  : c->det == HB_DET_IQ_LIM  ? ii * qq / (ii * ii + qq * qq)
                                             : ii * qq / (a * a + 2.0);
            acc += w * e;
            acc2 += w * e * e;
          }
      c->s[p] = acc;
      if (p == (HB_NPH - 1) / 2)
        c->v = acc2 - acc * acc;
    }
  const int m = (HB_NPH - 1) / 2;
  c->k        = (c->s[m + 1] - c->s[m - 1]) / (c->ph[m + 1] - c->ph[m - 1]);
  /* The loop at slope k: wn scales by sqrt(k), zeta by sqrt(k); its noise
     bandwidth, against the designed one, is sqrt(k) (zeta' + 1/(4 zeta'))
     / (zeta + 1/(4 zeta)). The jitter is var(e)/k^2 through 2 B_eff T. */
  const double z = 0.707, zk = z * sqrt (c->k);
  const double ratio = sqrt (c->k) * (zk + 0.25 / zk) / (z + 0.25 / z);
  c->beff            = ratio * c->bn / c->t_upd;
  c->sigma           = sqrt (2.0 * c->beff * c->t_upd * c->v / (c->k * c->k));
}

static inline double
hb_s (const hb_cfg_t *c, double phi)
{
  const double f = hb_fold (phi);
  double       x = (f - c->ph[0]) / (c->ph[1] - c->ph[0]);
  int          i = (int)x;
  if (i < 0)
    i = 0;
  if (i > HB_NPH - 2)
    i = HB_NPH - 2;
  x -= i;
  return c->s[i] * (1.0 - x) + c->s[i + 1] * x;
}

/** The mean loop's answer for one draw: the lock points it crosses on the
    way in (signed, half cycles) and the last update its phase error is
    more than sigma from where it settles. */
typedef struct
{
  long   n;
  size_t t_last;
  int    settled;
} hb_pred_t;

/* The loop's own recursion with the mean S-curve in place of the noisy
   discriminator: costas_update()'s order, integ += ki e, then the NCO
   advances by integ and is nudged by kp e. th is truth minus NCO, radians;
   dw the step in radians an update. */
static inline hb_pred_t
hb_mean_loop (const hb_cfg_t *c, double frac, double th0, size_t max_upd)
{
  dp_loop_filter_state_t lf;
  dp_loop_filter_init (&lf, c->bn, 0.707, 1.0);
  const double dw = (2.0 * M_PI) * frac * c->bn;
  double       th = th0, integ = 0.0;
  hb_pred_t    r  = { 0, 0, 0 };
  const long   n0 = lround (th0 / M_PI);
  for (size_t u = 0; u < max_upd; u++)
    {
      const double e = hb_s (c, th);
      integ += lf.ki * e;
      th += dw - integ - lf.kp * e;
      if (fabs (hb_fold (th)) > c->sigma)
        r.t_last = u + 1;
      if (u > 100 && fabs (dw - integ) < 1e-9 && fabs (hb_fold (th)) < 1e-6)
        {
          r.settled = 1;
          break;
        }
    }
  r.n = lround (th / M_PI) - n0;
  return r;
}

/* One configuration at an explicit loop bandwidth: a receiver's own bn,
   at carrier fc and Es/N0 esn0 dB, with detector det. */
static inline void
hb_cfg_bn (hb_cfg_t *c, double fc, double esn0, double bn, int det)
{
  c->det            = det;
  const double chip = dp_s415_chip_rate (fc);
  c->fc             = fc;
  c->esn0           = esn0;
  c->t_upd          = (double)DP_S415_SF / chip;
  c->bn             = bn;
  c->gamma_db       = dp_s415_cn0 (esn0) + 10.0 * log10 (c->t_upd);
  hb_scurve (c);
}

/* The same at one of the two bandwidths --half-bw runs: the receiver's
   rule at the floor (rule != 0) or ASYNC_DSSS_RX_BN_CARRIER. */
static inline void
hb_cfg_det (hb_cfg_t *c, double fc, double esn0, int rule, int det)
{
  const double chip = dp_s415_chip_rate (fc);
  hb_cfg_bn (c, fc, esn0,
             rule ? dp_async_dsss_receiver_carrier_bn (
                        dp_s415_cn0 (DP_S415_FLOOR_DB), chip, DP_S415_SF)
                  : ASYNC_DSSS_RX_BN_CARRIER,
             det);
}

/* The largest step, in B_L, from which no start phase (worst start) or the
   start at 0 crosses two or more lock points on the mean loop: bisection
   to 1/2^13 of the bracket. */
static inline int
hb_slips (const hb_cfg_t *c, double frac, int worst, size_t max_upd)
{
  if (!worst)
    return labs (hb_mean_loop (c, frac, 0.0, max_upd).n) >= 1;
  for (int sg = -1; sg <= 1; sg += 2)
    for (int k = 1; k <= HB_NSTART; k++)
      if (labs (hb_mean_loop (c, sg * frac, -0.5 * M_PI + M_PI * k / HB_NSTART,
                              max_upd)
                    .n)
          >= 2)
        return 1;
  return 0;
}

static inline double
hb_edge (const hb_cfg_t *c, int worst)
{
  double       lo = 0.05, hi = 2.0;
  const size_t mx = (size_t)(60.0 / c->bn);
  for (int i = 0; i < 13; i++)
    {
      const double m = 0.5 * (lo + hi);
      if (hb_slips (c, m, worst, mx))
        hi = m;
      else
        lo = m;
    }
  return lo;
}

/* The pre-despread loop's worst-start edge, Hz, for a receiver at carrier
   fc and Es/N0 esn0 running bn: the unit form's detector (carrier_nda's,
   the one the receiver runs) on the mean loop. A hand-over inside it is one
   whose pull-in the analytic settle budget predicts (the error-rate window's
   check). Derived once a configuration and kept: the harnesses that call
   it are single-threaded, and run one cell a process. */
static inline double
hb_edge_hz (double fc, double esn0, double bn)
{
  static double c_fc, c_es, c_bn, c_hz = -1.0;
  if (c_hz < 0.0 || fc != c_fc || esn0 != c_es || bn != c_bn)
    {
      hb_cfg_t *c = malloc (sizeof *c);
      if (!c)
        abort ();
      hb_cfg_bn (c, fc, esn0, bn, HB_DET_IQ_UNIT);
      c_hz = hb_edge (c, 1) * bn / c->t_upd;
      free (c);
      c_fc = fc;
      c_es = esn0;
      c_bn = bn;
    }
  return c_hz;
}

#endif /* DP_CARRIER_EDGE_H */
