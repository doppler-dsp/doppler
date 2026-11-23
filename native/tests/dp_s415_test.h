/**
 * @file dp_s415_test.h
 * @brief The receiver's specification point, CCSDS 415.1-B-1, stated once:
 *        its geometry, the receiver and pool built there, and an emitter on
 *        the air there.
 *
 * Every number is a row of docs/design/async-dsss-receiver.md §1's table,
 * which names its source; nothing here is chosen. A test, a validator or a
 * bench that measures the receiver at the point includes this header rather
 * than restating the point, so they cannot measure two points under one
 * name. Before this header the point was written out six times, under five
 * prefixes.
 *
 *   code         Gold, 1,023 chips (dp_dsss_gold_1023(), dp_dsss_test.h)
 *   chip rate    F_fwd x 31 / (221 x 96)                  table 4-1
 *   carrier      F_fwd 2.2 to 2.8 GHz                     owner
 *   symbols      3,000 sym/s, asynchronous to the code    owner
 *   capture      11 MSa/s, complex                        owner
 *   Doppler      within +/-1,500 Hz, ramping 400 Hz/s     owner
 *   floor        Es/N0 4 dB                               owner
 *   frame        1,000 symbols, the first 275 code alone  owner, §1.4
 *
 * The emitter goes through the scene of dp_air_test.h, which owns the
 * front end's model. The receiver and the pool take the binding's
 * defaults for everything the point does not set.
 */
#ifndef DP_S415_TEST_H
#define DP_S415_TEST_H

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "dp_air_test.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Chips a code period. */
#define DP_S415_SF 1023u
/** The capture rate, complex samples a second. */
#define DP_S415_FS 11e6
/** Data symbols a second. */
#define DP_S415_SYM 3000.0
/** The band's two ends, F_fwd, Hz. */
#define DP_S415_FC_LO 2.2e9
#define DP_S415_FC_HI 2.8e9
/** The Doppler uncertainty about best lock, one side, Hz. */
#define DP_S415_SPAN_HZ 1500.0
/** The Doppler rate, Hz/s. */
#define DP_S415_RATE_HZ_S 400.0
/** The sensitivity floor, Es/N0 dB. */
#define DP_S415_FLOOR_DB 4.0
/** Symbols a frame, and of them the code-only ones that open it. */
#define DP_S415_FRAME_SYM 1000u
#define DP_S415_CODE_ONLY_SYM 275u

/** The lock allocation from arrival, s: 415.1-B-1 §7.4.1's acquisition
 *  time, and the owner's "typically allocated 20 seconds to lock". A link
 *  whose flags have not all sustained by then did not lock. */
#define DP_S415_LOCK_S 20.0
/** The error rate's scored stretch after the window opens, s
 *  (docs/design/async-dsss-receiver.md, "The error-rate window"). */
#define DP_S415_SCORE_S 5.0

/** The receiver's five lock flags, in one order for every harness that
 *  reads them: the symbol, code, pre-despread carrier, demodulator carrier
 *  and demodulator sync flags. */
enum
{
  DP_S415_FLAG_SYM,
  DP_S415_FLAG_CODE,
  DP_S415_FLAG_CAR,
  DP_S415_FLAG_DEMOD,
  DP_S415_FLAG_SYNC,
  DP_S415_N_FLAGS
};

/** Read the five flags of @p rx into @p f, all down unless it is tracking
 *  (the flags of a receiver that is searching or refining are not its
 *  link's). */
static inline void
dp_s415_lock_flags (const dp_async_dsss_receiver_state_t *rx,
                    unsigned char                         f[DP_S415_N_FLAGS])
{
  const int tr = rx->state == ASYNC_DSSS_RX_TRACKING;
  f[DP_S415_FLAG_SYM]
      = (unsigned char)(tr && dp_async_dsss_receiver_get_locked (rx));
  f[DP_S415_FLAG_CODE]
      = (unsigned char)(tr && dp_async_dsss_receiver_get_code_locked (rx));
  f[DP_S415_FLAG_CAR]
      = (unsigned char)(tr && dp_carrier_nda_get_locked (&rx->car));
  f[DP_S415_FLAG_DEMOD]
      = (unsigned char)(tr && rx->rx && rx->rx->l.car_lock.locked);
  f[DP_S415_FLAG_SYNC]
      = (unsigned char)(tr && rx->rx && rx->rx->l.timing.lock.locked);
}

/** Chips a second at carrier @p fc: F_fwd x 31 / (221 x 96). */
static inline double
dp_s415_chip_rate (double fc)
{
  return fc * 31.0 / (221.0 * 96.0);
}

/** C/N0, dB-Hz, of an Es/N0 at the point's symbol rate. */
static inline double
dp_s415_cn0 (double esn0_db)
{
  return esn0_db + 10.0 * log10 (DP_S415_SYM);
}

/* ── The use case's residual rate, R_env (async-dsss-receiver.md §1.8) ─────
 * The one home of the pass model the residual's rate comes from: the
 * receiver at the specification point is configured with it
 * (dp_s415_rx(), dp_s415_pool()), and the pull-in validator's pass arm
 * draws at it. DP_S415_RATE_HZ_S (400 Hz/s) is the raw pass's peak rate:
 * it selects which passes are in scope, and is not a rate the receiver
 * meets. */
/* The use case's dynamics (docs/design/async-dsss-receiver.md, "The
   dynamics the receiver is specified for"; owner 11-17: LEO, Doppler
   pre-compensated, the residual offset uniform in +/-DP_S415_SPAN_HZ, its rate
   a conservative envelope from an ephemeris error model). A LEO pass is a
   circular orbit over a spherical Earth seen from a fixed point above a
   DP_S415_PASS_MASK_DEG elevation mask, Earth rotation ignored (its worst case
   is the factor dp_s415_pass_rotation()); the passes in scope are those whose
   RAW peak rate is the specification's DP_S415_RATE_HZ_S, one a grid
   altitude with its maximum elevation solved. Over them:
     - timing: the along-track error, residual f(t) - f(t - dt) for every
       dt up to the one whose residual reaches DP_S415_SPAN_HZ, both branches;
     - proportional: the cross-check, residual eps f(t) for every eps up to
       DP_S415_SPAN_HZ over the pass's visible peak offset.
   R_env is dp_s415_r_env_of(): the timing envelope's maximum (the ephemeris
   error model, owner 11-17), times the rotation factor, rounded up to the
   whole Hz/s. The proportional model is its cross-check, printed with the pass
   that drives it, and does not set it. */
#define DP_S415_PASS_MASK_DEG 10.0
#define DP_S415_PASS_RE_KM 6371.0
#define DP_S415_PASS_MU 398600.4418
#define DP_S415_PASS_C_KMS 299792.458
#define DP_S415_PASS_OMEGA_E 7.2921e-5
#define DP_S415_PASS_DT 0.02
#define DP_S415_PASS_TRIM 25
#define DP_S415_PASS_BINS                                                     \
  15 /* |offset| bins of DP_S415_SPAN_HZ / DP_S415_PASS_BINS Hz */

/* One pass's residual-free curve: f, f' and f'' (Hz, Hz/s, Hz/s^2) on a
   DP_S415_PASS_DT grid above the mask, the range rate in closed form and every
   end point a numerical derivative touches trimmed. Returns the point
   count (0: the pass never clears the mask); *v the orbital speed, km/s. */
static inline size_t
dp_s415_pass_curve (double h_km, double el_deg, double fc, double **f,
                    double **f1, double **f2, double *v)
{
  const double rs = DP_S415_PASS_RE_KM + h_km,
               w  = sqrt (DP_S415_PASS_MU / (rs * rs * rs));
  const double e  = el_deg * M_PI / 180.0,
               em = DP_S415_PASS_MASK_DEG * M_PI / 180.0;
  const double g0 = acos (DP_S415_PASS_RE_KM * cos (e) / rs) - e;
  const double gm = acos (DP_S415_PASS_RE_KM * cos (em) / rs) - em;
  *v              = rs * w;
  if (g0 >= gm)
    return 0;
  const double tmax = acos (cos (gm) / cos (g0)) / w;
  const size_t n    = (size_t)(2.0 * tmax / DP_S415_PASS_DT);
  if (n < 4 * DP_S415_PASS_TRIM)
    return 0;
  double *a = dp_xmallocn (n, sizeof *a), *b = dp_xmallocn (n, sizeof *b),
         *c = dp_xmallocn (n, sizeof *c);
  for (size_t i = 0; i < n; i++)
    {
      const double t  = -tmax + (double)i * DP_S415_PASS_DT;
      const double cg = cos (g0) * cos (w * t);
      const double d  = sqrt (DP_S415_PASS_RE_KM * DP_S415_PASS_RE_KM + rs * rs
                              - 2.0 * DP_S415_PASS_RE_KM * rs * cg);
      a[i] = -fc / DP_S415_PASS_C_KMS * DP_S415_PASS_RE_KM * rs * cos (g0) * w
             * sin (w * t) / d;
    }
  for (size_t i = 1; i + 1 < n; i++)
    b[i] = (a[i + 1] - a[i - 1]) / (2.0 * DP_S415_PASS_DT);
  for (size_t i = 2; i + 2 < n; i++)
    c[i] = (b[i + 1] - b[i - 1]) / (2.0 * DP_S415_PASS_DT);
  const size_t m = n - 2 * DP_S415_PASS_TRIM;
  memmove (a, a + DP_S415_PASS_TRIM, m * sizeof *a);
  memmove (b, b + DP_S415_PASS_TRIM, m * sizeof *b);
  memmove (c, c + DP_S415_PASS_TRIM, m * sizeof *c);
  *f = a, *f1 = b, *f2 = c;
  return m;
}

static inline double
dp_s415_pass_peak_rate (double h_km, double el_deg, double fc)
{
  double      *f, *f1, *f2, v, pk = 0.0;
  const size_t n = dp_s415_pass_curve (h_km, el_deg, fc, &f, &f1, &f2, &v);
  for (size_t i = 0; i < n; i++)
    pk = fmax (pk, fabs (f1[i]));
  if (n)
    free (f), free (f1), free (f2);
  return pk;
}

/* The pass at altitude h whose raw peak rate is DP_S415_RATE_HZ_S: its
   maximum elevation by bisection; NAN when even overhead stays under. */
static inline double
dp_s415_pass_el (double h_km, double fc)
{
  /* In scope only where some pass at h has exactly that peak: not when
     even overhead stays under it, nor when even the lowest pass above the
     mask already exceeds it. */
  double lo = DP_S415_PASS_MASK_DEG + 0.5, hi = 89.9;
  if (dp_s415_pass_peak_rate (h_km, hi, fc) < DP_S415_RATE_HZ_S
      || dp_s415_pass_peak_rate (h_km, lo, fc) > DP_S415_RATE_HZ_S)
    return NAN;
  for (int i = 0; i < 30; i++)
    {
      const double m = 0.5 * (lo + hi);
      if (dp_s415_pass_peak_rate (h_km, m, fc) > DP_S415_RATE_HZ_S)
        hi = m;
      else
        lo = m;
    }
  return lo;
}

typedef struct
{
  double step_km;
  int    n_pass;
  double env[DP_S415_PASS_BINS]; /* timing: largest |rate| by |offset| bin */
  double timing_max, prop_max, v_min, f2_max;
  double res_f2_max;     /* timing: the largest |residual rate'|, Hz/s^2 */
  double raw_ppm_max[2]; /* the raw |offset| / carrier, ppm, per carrier */
  double prop_h_km, prop_el_deg, prop_fc, prop_eps; /* its driving pass */
} dp_s415_pass_env_t;

/* The envelope over every in-scope pass on a step_km altitude grid from
   350 km, at both carriers. */
static inline void
dp_s415_pass_envelope (double step_km, dp_s415_pass_env_t *pe)
{
  memset (pe, 0, sizeof *pe);
  pe->step_km                = step_km;
  pe->v_min                  = INFINITY;
  static const double fcs[2] = { DP_S415_FC_LO, DP_S415_FC_HI };
  for (int k = 0; k < 2; k++)
    for (double h = 350.0; h <= 1200.0 + 1e-9; h += step_km)
      {
        const double el = dp_s415_pass_el (h, fcs[k]);
        if (isnan (el))
          continue;
        double      *f, *f1, *f2, v;
        const size_t n = dp_s415_pass_curve (h, el, fcs[k], &f, &f1, &f2, &v);
        if (!n)
          continue;
        pe->n_pass++;
        pe->v_min  = fmin (pe->v_min, v);
        double fpk = 0.0, f1pk = 0.0;
        for (size_t i = 0; i < n; i++)
          {
            fpk        = fmax (fpk, fabs (f[i]));
            f1pk       = fmax (f1pk, fabs (f1[i]));
            pe->f2_max = fmax (pe->f2_max, fabs (f2[i]));
          }
        /* timing: lag j samples, j stepping 0.05 s, until the residual
           first passes the bound */
        for (size_t j = 1; j < n; j += (size_t)(0.05 / DP_S415_PASS_DT))
          {
            double rmax = 0.0;
            for (size_t i = j; i < n; i++)
              rmax = fmax (rmax, fabs (f[i] - f[i - j]));
            if (rmax > DP_S415_SPAN_HZ)
              break;
            for (size_t i = j; i < n; i++)
              {
                const double o = fabs (f[i] - f[i - j]);
                int bi = (int)(o / (DP_S415_SPAN_HZ / DP_S415_PASS_BINS));
                if (bi >= DP_S415_PASS_BINS)
                  bi = DP_S415_PASS_BINS - 1;
                pe->env[bi] = fmax (pe->env[bi], fabs (f1[i] - f1[i - j]));
                pe->res_f2_max
                    = fmax (pe->res_f2_max, fabs (f2[i] - f2[i - j]));
              }
          }
        pe->raw_ppm_max[k] = fmax (pe->raw_ppm_max[k], fpk / fcs[k] * 1e6);
        /* proportional: the largest residual rate is eps_max f'_max */
        if (DP_S415_SPAN_HZ / fpk * f1pk > pe->prop_max)
          {
            pe->prop_max    = DP_S415_SPAN_HZ / fpk * f1pk;
            pe->prop_h_km   = h;
            pe->prop_el_deg = el;
            pe->prop_fc     = fcs[k];
            pe->prop_eps    = DP_S415_SPAN_HZ / fpk;
          }
        free (f), free (f1), free (f2);
      }
  for (int i = 0; i < DP_S415_PASS_BINS; i++)
    pe->timing_max = fmax (pe->timing_max, pe->env[i]);
}

/* Earth rotation, ignored by the pass model: at most the equatorial
   surface speed against the slowest in-scope orbit. */
static inline double
dp_s415_pass_rotation (double v_min_kms)
{
  return 1.0 + DP_S415_PASS_OMEGA_E * DP_S415_PASS_RE_KM / v_min_kms;
}

/* R_env, the rate a pass draw holds, Hz/s: the timing envelope's maximum
   times the rotation factor, rounded up to the whole Hz/s. */
static inline double
dp_s415_r_env_of (double timing_max, double rotation)
{
  return ceil (timing_max * rotation);
}

/** R_env, Hz/s: dp_s415_r_env_of() over the 50 km envelope, the rate the
 *  receiver at the specification point meets (async-dsss-receiver.md §1.8:
 *  the timing envelope's 10.10 Hz/s in its 1,000-1,100 Hz row, times the
 *  rotation factor 1.063, rounded up: 11). Computed once a translation
 *  unit. */
static inline double
dp_s415_r_env (void)
{
  static double r = 0.0;
  if (r == 0.0)
    {
      dp_s415_pass_env_t pe;
      dp_s415_pass_envelope (50.0, &pe);
      r = dp_s415_r_env_of (pe.timing_max, dp_s415_pass_rotation (pe.v_min));
    }
  return r;
}

/** The receiver at the point and carrier @p fc, designed for the floor:
 *  its `cn0_dbhz` is the floor's C/N0 (Es/N0 DP_S415_FLOOR_DB) whatever
 *  level the scene runs at, since a receiver is built once, for its weakest
 *  signal (owner, 2026-11-21). The binding's defaults, the point's search
 *  span, its gap (the code-only symbols, in seconds) and its Doppler rate.
 *  @p refine is the argument the callers vary. */
static inline dp_async_dsss_receiver_state_t *
dp_s415_rx (const uint8_t *code, double fc, bool refine)
{
  return dp_async_dsss_receiver_create (
      code, DP_S415_SF, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      dp_s415_cn0 (DP_S415_FLOOR_DB), 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, false, 100000, fc, 0.0, false,
      (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM, dp_s415_r_env (), refine);
}

/** A pool of @p n receivers at the point, as dp_s415_rx() builds each (at
 *  the floor's C/N0), with its release rule (@p lost_s) and @p threads. */
static inline dp_async_dsss_pool_state_t *
dp_s415_pool (const uint8_t *code, double fc, size_t n, double lost_s,
              int threads)
{
  return dp_async_dsss_pool_create (
      code, DP_S415_SF, n, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      dp_s415_cn0 (DP_S415_FLOOR_DB), 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, false, 100000, fc, lost_s, false,
      (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM, dp_s415_r_env (), threads,
      true);
}

/** An emitter on the air at the point: carrier @p fc, offset @p f0 Hz at
 *  receive time 0, ramping @p rate Hz/s, at @p cn0, its frames as the point
 *  sets them, its first chip @p lag_s after time 0, its data PRBS @p seed.
 *  The fields the point does not set (frame_off, bits, spc) stay 0: the
 *  frame starts at its first symbol, the data is the PRBS, and the scene
 *  renders at DP_AIR_SPC. */
static inline dp_air_emitter_t
dp_s415_emitter (const uint8_t *code, double fc, double f0, double rate,
                 double cn0, double lag_s, uint32_t seed)
{
  dp_air_emitter_t e = { 0 };
  e.code             = code;
  e.code_len         = DP_S415_SF;
  e.chip_rate        = dp_s415_chip_rate (fc);
  e.sym_rate         = DP_S415_SYM;
  e.carrier_hz       = fc;
  e.f0_hz            = f0;
  e.rate_hz_s        = rate;
  e.cn0_dbhz         = cn0;
  e.code_only_sym    = DP_S415_CODE_ONLY_SYM;
  e.frame_sym        = DP_S415_FRAME_SYM;
  e.lag_s            = lag_s;
  e.seed             = seed;
  return e;
}

#endif /* DP_S415_TEST_H */
