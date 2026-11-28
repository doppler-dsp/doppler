/**
 * @file dp_s415_test.h
 * @brief The harness's side of the CCSDS 415.1-B-1 specification point:
 *        the pass model R_env is derived from, the five lock flags, the
 *        scoring window, and an emitter on the air there.
 *
 * The point itself is the library's, doppler/ccsds/ccsds_415.h: its
 * numbers, its chip rate and C/N0, and (through the Ccsds415Receiver and
 * Ccsds415Pool views) the receiver and pool built there. This header adds
 * what a harness needs and a user does not:
 *
 *   - the LEO pass model R_env comes from (DP_CCSDS_415_R_ENV_HZ_S is held
 *     equal to dp_s415_r_env() by test_async_dsss_receiver_core.c);
 *   - dp_s415_lock_flags(), the five flags of the receiver's status record
 *     in one order;
 *   - DP_S415_SCORE_S, the error rate's scored stretch;
 *   - dp_s415_emitter(), the scene of dp_air_test.h, which owns the front
 *     end's model;
 *   - dp_s415_rx() and dp_s415_pool(), short spellings of the views.
 *
 * Every number is a row of docs/design/async-dsss-receiver.md §1's table,
 * which names its source; nothing here is chosen.
 */
#ifndef DP_S415_TEST_H
#define DP_S415_TEST_H

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/ccsds/ccsds_415.h"
#include "dp_air_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** The raw pass's peak Doppler rate, Hz/s: it selects the passes in scope
 *  of the pass model below, and is not a rate the receiver meets (that is
 *  R_env, DP_CCSDS_415_R_ENV_HZ_S). */
#define DP_S415_RATE_HZ_S 400.0
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

/** Read the five flags of @p rx's status record into @p f, all down unless
 *  it is tracking (the flags of a receiver that is searching or refining
 *  are not its link's). The public record only: a downstream program reads
 *  the same five through dp_async_dsss_receiver_status(). */
static inline void
dp_s415_lock_flags (const dp_async_dsss_receiver_state_t *rx,
                    unsigned char                         f[DP_S415_N_FLAGS])
{
  const async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  const int                          tr = st.state == ASYNC_DSSS_RX_TRACKING;
  f[DP_S415_FLAG_SYM]                   = (unsigned char)(tr && st.locked);
  f[DP_S415_FLAG_CODE]  = (unsigned char)(tr && st.code_locked);
  f[DP_S415_FLAG_CAR]   = (unsigned char)(tr && st.carrier_locked);
  f[DP_S415_FLAG_DEMOD] = (unsigned char)(tr && st.demod_carrier_locked);
  f[DP_S415_FLAG_SYNC]  = (unsigned char)(tr && st.sync_locked);
}

/* ── The use case's residual rate, R_env (async-dsss-receiver.md §1.8) ─────
 * The one home of the pass model the residual's rate comes from: the
 * receiver at the specification point is configured with its result,
 * installed as DP_CCSDS_415_R_ENV_HZ_S (ccsds_415.h), which
 * test_async_dsss_receiver_core.c holds equal to dp_s415_r_env(); and the
 * pull-in validator's pass arm draws at it. DP_S415_RATE_HZ_S (400 Hz/s) is
 * the raw pass's peak rate: it selects which passes are in scope, and is not a
 * rate the receiver meets. */
/* The use case's dynamics (docs/design/async-dsss-receiver.md, "The
   dynamics the receiver is specified for"; owner 11-17: LEO, Doppler
   pre-compensated, the residual offset uniform in +/-DP_CCSDS_415_SPAN_HZ, its
   rate a conservative envelope from an ephemeris error model). A LEO pass is a
   circular orbit over a spherical Earth seen from a fixed point above a
   DP_S415_PASS_MASK_DEG elevation mask, Earth rotation ignored (its worst case
   is the factor dp_s415_pass_rotation()); the passes in scope are those whose
   RAW peak rate is the specification's DP_S415_RATE_HZ_S, one a grid
   altitude with its maximum elevation solved. Over them:
     - timing: the along-track error, residual f(t) - f(t - dt) for every
       dt up to the one whose residual reaches DP_CCSDS_415_SPAN_HZ, both
   branches;
     - proportional: the cross-check, residual eps f(t) for every eps up to
       DP_CCSDS_415_SPAN_HZ over the pass's visible peak offset.
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
  15 /* |offset| bins of DP_CCSDS_415_SPAN_HZ / DP_S415_PASS_BINS Hz */

/* The altitude grid of the use case's passes (async-dsss-receiver.md §1.8):
   350 to 1200 km in 50 km steps, for the envelope and the pass table alike. */
#define DP_S415_ALT_LO 350.0
#define DP_S415_ALT_HI 1200.0
#define DP_S415_ALT_STEP 50.0

/* The geometry of one pass: rs the orbit radius, w the angular rate, g0 the
   Earth-central angle from the peak to the satellite's rise and fall at the
   pass's maximum elevation, gm the same at the mask, tmax the time from the
   peak to the mask, and v the orbital speed. Filled by
   dp_s415_pass_geometry(), the one place these are derived. */
typedef struct
{
  double rs, w, g0, gm, tmax, v;
} dp_s415_pass_geo_t;

/* Fill g for a pass at altitude h_km with maximum elevation el_deg. Returns 0
   when the pass is out of scope (it never clears the mask: g0 >= gm); g->v is
   set either way. */
static inline int
dp_s415_pass_geometry (double h_km, double el_deg, dp_s415_pass_geo_t *g)
{
  g->rs           = DP_S415_PASS_RE_KM + h_km;
  g->w            = sqrt (DP_S415_PASS_MU / (g->rs * g->rs * g->rs));
  const double e  = el_deg * M_PI / 180.0,
               em = DP_S415_PASS_MASK_DEG * M_PI / 180.0;
  g->g0           = acos (DP_S415_PASS_RE_KM * cos (e) / g->rs) - e;
  g->gm           = acos (DP_S415_PASS_RE_KM * cos (em) / g->rs) - em;
  g->v            = g->rs * g->w;
  if (g->g0 >= g->gm)
    return 0;
  g->tmax = acos (cos (g->gm) / cos (g->g0)) / g->w;
  return 1;
}

/* The pass's samples on the DP_S415_PASS_DT grid, rise to set above the mask
   (before the derivative trim). */
static inline size_t
dp_s415_pass_samples (const dp_s415_pass_geo_t *g)
{
  return (size_t)(2.0 * g->tmax / DP_S415_PASS_DT);
}

/* The slant range at time t from the peak, km: the law dp_s415_pass_curve
   differentiates, the same expression in the same order. The curve is
   bit-identical to the inline form with gcc. With clang 21 under the
   validator's fast-math flags the contraction differs: 3.4M of 3.7M curve
   values move by at most 1.0e-7 absolute (3.2e-6 relative, worst in f''), and
   R_env is unchanged. That is a ULP-scale tolerance, not a defect. */
static inline double
dp_s415_pass_range_at (const dp_s415_pass_geo_t *g, double t)
{
  const double cg = cos (g->g0) * cos (g->w * t);
  return sqrt (DP_S415_PASS_RE_KM * DP_S415_PASS_RE_KM + g->rs * g->rs
               - 2.0 * DP_S415_PASS_RE_KM * g->rs * cg);
}

/* One pass's residual-free curve: f, f' and f'' (Hz, Hz/s, Hz/s^2) on a
   DP_S415_PASS_DT grid above the mask, the range rate in closed form and every
   end point a numerical derivative touches trimmed. Returns the point
   count (0: the pass never clears the mask); *v the orbital speed, km/s. */
static inline size_t
dp_s415_pass_curve (double h_km, double el_deg, double fc, double **f,
                    double **f1, double **f2, double *v)
{
  dp_s415_pass_geo_t g;
  const int          in = dp_s415_pass_geometry (h_km, el_deg, &g);
  *v                    = g.v;
  if (!in)
    return 0;
  const size_t n = dp_s415_pass_samples (&g);
  if (n < 4 * DP_S415_PASS_TRIM)
    return 0;
  const double rs = g.rs, w = g.w, g0 = g.g0, tmax = g.tmax;
  double      *a = dp_xmallocn (n, sizeof *a), *b = dp_xmallocn (n, sizeof *b),
         *c = dp_xmallocn (n, sizeof *c);
  for (size_t i = 0; i < n; i++)
    {
      const double t = -tmax + (double)i * DP_S415_PASS_DT;
      const double d = dp_s415_pass_range_at (&g, t);
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
  static const double fcs[2] = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  for (int k = 0; k < 2; k++)
    for (double h = DP_S415_ALT_LO; h <= DP_S415_ALT_HI + 1e-9; h += step_km)
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
            if (rmax > DP_CCSDS_415_SPAN_HZ)
              break;
            for (size_t i = j; i < n; i++)
              {
                const double o = fabs (f[i] - f[i - j]);
                int bi = (int)(o / (DP_CCSDS_415_SPAN_HZ / DP_S415_PASS_BINS));
                if (bi >= DP_S415_PASS_BINS)
                  bi = DP_S415_PASS_BINS - 1;
                pe->env[bi] = fmax (pe->env[bi], fabs (f1[i] - f1[i - j]));
                pe->res_f2_max
                    = fmax (pe->res_f2_max, fabs (f2[i] - f2[i - j]));
              }
          }
        pe->raw_ppm_max[k] = fmax (pe->raw_ppm_max[k], fpk / fcs[k] * 1e6);
        /* proportional: the largest residual rate is eps_max f'_max */
        if (DP_CCSDS_415_SPAN_HZ / fpk * f1pk > pe->prop_max)
          {
            pe->prop_max    = DP_CCSDS_415_SPAN_HZ / fpk * f1pk;
            pe->prop_h_km   = h;
            pe->prop_el_deg = el;
            pe->prop_fc     = fcs[k];
            pe->prop_eps    = DP_CCSDS_415_SPAN_HZ / fpk;
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

/* ── The per-pass table (docs/design/pool-constellation-scene.md §5) ────────
 * One row per in-scope pass: the carrier, its altitude and maximum elevation
 * (dp_s415_pass_el), its full length 2 t_max, its slant range at the peak and
 * at the mask, and the largest |20 log10 (d(t+30)/d(t))| inside it. The range
 * d(t) is the header's own geometry (dp_s415_pass_curve's cos (w t) form, with
 * the same g0 and mask). The pool scene's pass table and its spreads are
 * computed here, once, and printed by pi_design(). */
#define DP_S415_TABLE_WINDOW_S 30.0
#define DP_S415_TABLE_STEP_S 0.5
typedef struct
{
  int    carrier; /* 0: DP_CCSDS_415_FC_LO, 1: DP_CCSDS_415_FC_HI */
  double fc_hz, alt_km, el_deg, dur_s, peak_km, mask_km, worst30_db;
} dp_s415_pass_row_t;

/* Fill rows for every in-scope pass over DP_S415_ALT_LO..DP_S415_ALT_HI km in
   DP_S415_ALT_STEP steps, both carriers: the passes dp_s415_pass_envelope()
   takes, by the same geometry and scope rule. Returns the count (25 on the
   header as it stands); at most `max` rows are written. */
static inline size_t
dp_s415_pass_table (dp_s415_pass_row_t *rows, size_t max)
{
  static const double fcs[2] = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  size_t              n      = 0;
  for (int k = 0; k < 2; k++)
    for (double h = DP_S415_ALT_LO; h <= DP_S415_ALT_HI + 1e-9;
         h += DP_S415_ALT_STEP)
      {
        const double el = dp_s415_pass_el (h, fcs[k]);
        if (isnan (el))
          continue;
        dp_s415_pass_geo_t g;
        if (!dp_s415_pass_geometry (h, el, &g)
            || dp_s415_pass_samples (&g) < 4 * DP_S415_PASS_TRIM)
          continue;
        double worst = 0.0;
        for (double t = -g.tmax; t + DP_S415_TABLE_WINDOW_S <= g.tmax;
             t += DP_S415_TABLE_STEP_S)
          {
            const double d0 = dp_s415_pass_range_at (&g, t);
            const double d1
                = dp_s415_pass_range_at (&g, t + DP_S415_TABLE_WINDOW_S);
            worst = fmax (worst, fabs (20.0 * log10 (d1 / d0)));
          }
        if (n < max)
          rows[n] = (dp_s415_pass_row_t){
            .carrier    = k,
            .fc_hz      = fcs[k],
            .alt_km     = h,
            .el_deg     = el,
            .dur_s      = 2.0 * g.tmax,
            .peak_km    = dp_s415_pass_range_at (&g, 0.0),
            .mask_km    = dp_s415_pass_range_at (&g, g.tmax),
            .worst30_db = worst,
          };
        n++;
      }
  return n;
}

/** The receiver at the point and carrier @p fc: the Ccsds415Receiver view
 *  (dp_async_dsss_receiver_create_ccsds415()), designed for the floor
 *  whatever level the scene runs at. @p refine is the argument the callers
 *  vary. */
static inline dp_async_dsss_receiver_state_t *
dp_s415_rx_release (const uint8_t *code, double fc, bool refine,
                    double lost_s);

static inline dp_async_dsss_receiver_state_t *
dp_s415_rx (const uint8_t *code, double fc, bool refine)
{
  return dp_s415_rx_release (code, fc, refine, 0.0);
}

/** As dp_s415_rx(), with the release armed: both flags down for longer
 *  than @p lost_s enters lost (0: never, dp_s415_rx()'s). */
static inline dp_async_dsss_receiver_state_t *
dp_s415_rx_release (const uint8_t *code, double fc, bool refine, double lost_s)
{
  return dp_async_dsss_receiver_create_ccsds415 (code, DP_CCSDS_415_SF, fc,
                                                 lost_s, refine, NULL);
}

/** As dp_s415_pool(), with the refine stage on or off (@p refine). */
static inline dp_async_dsss_pool_state_t *
dp_s415_pool_refine (const uint8_t *code, double fc, size_t n, double lost_s,
                     int threads, bool refine)
{
  return dp_async_dsss_pool_create_ccsds415 (code, DP_CCSDS_415_SF, n, fc,
                                             lost_s, threads, refine, NULL);
}

/** A pool of @p n receivers at the point, as dp_s415_rx() builds each (at
 *  the floor's C/N0), with its release rule (@p lost_s) and @p threads. */
static inline dp_async_dsss_pool_state_t *
dp_s415_pool (const uint8_t *code, double fc, size_t n, double lost_s,
              int threads)
{
  return dp_s415_pool_refine (code, fc, n, lost_s, threads, true);
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
  e.code_len         = DP_CCSDS_415_SF;
  e.chip_rate        = dp_ccsds_415_chip_rate (fc);
  e.sym_rate         = DP_CCSDS_415_SYM;
  e.carrier_hz       = fc;
  e.f0_hz            = f0;
  e.rate_hz_s        = rate;
  e.cn0_dbhz         = cn0;
  e.code_only_sym    = DP_CCSDS_415_CODE_ONLY_SYM;
  e.frame_sym        = DP_CCSDS_415_FRAME_SYM;
  e.lag_s            = lag_s;
  e.seed             = seed;
  return e;
}

/** The test's own expectation of dp_ccsds_415_rx()'s table, typed here
 *  once: the point's numbers from ccsds_415.h, and for the rest the
 *  receiver binding's defaults (objects/async_dsss_receiver.toml), which
 *  the table claims to equal. A wrong value written inside the table
 *  itself fails here, where a view-equals-table check cannot see it. */
static inline void
dp_s415_table_is_expected (const dp_ccsds_415_rx_t *a, double fc,
                           double lost_s, bool refine)
{
#define S415_EXPECT(f, v) DP_CHECK_MSG (a->f == (v), "table " #f " == " #v)
  S415_EXPECT (chip_rate, dp_ccsds_415_chip_rate (fc));
  S415_EXPECT (symbol_rate, 3000.0);
  S415_EXPECT (sample_rate, 11e6);
  S415_EXPECT (m, 2);
  S415_EXPECT (cn0_dbhz, dp_ccsds_415_cn0 (4.0));
  S415_EXPECT (pfa, 1e-3);
  S415_EXPECT (pd, 0.9);
  S415_EXPECT (doppler_uncertainty, 1500.0);
  S415_EXPECT (segments, 4u);
  S415_EXPECT (sps, 8u);
  S415_EXPECT (differential, 0);
  S415_EXPECT (refine_max_error_db, 0.5);
  S415_EXPECT (refine_samples_per_symbol, 4u);
  S415_EXPECT (refine_design_margin_db, 14.0);
  S415_EXPECT (refine_n_fft, 64u);
  S415_EXPECT (refine_zero_pad, 8u);
  S415_EXPECT (refine_sequential, false);
  S415_EXPECT (refine_max_n_blocks, 100000u);
  S415_EXPECT (carrier_freq_hz, fc);
  S415_EXPECT (lost_confirm_s, lost_s);
  S415_EXPECT (reacquire, false);
  S415_EXPECT (code_only_s, 275.0 / 3000.0);
  S415_EXPECT (doppler_rate, 11.0);
  S415_EXPECT (refine, refine);
#undef S415_EXPECT
}

/** Every argument dp_async_dsss_receiver_create() took, read back from the
 *  built receiver @p v, equals the table @p a: the 22 it stores, the
 *  sample rate through spc, and doppler_uncertainty through its search. A
 *  state blob cannot show this: pd, differential, the refine margin and
 *  sequential flag, lost_confirm_s, reacquire and doppler_rate are config
 *  no blob carries. The receiver view's test and the pool view's (over
 *  each receiver and the search) both call it. */
static inline void
dp_s415_rx_is_the_table (const dp_async_dsss_receiver_state_t *v,
                         const dp_ccsds_415_rx_t              *a)
{
#define S415_SAME(f) DP_CHECK_MSG (v->f == a->f, #f " is the table's")
  S415_SAME (chip_rate);
  S415_SAME (symbol_rate);
  S415_SAME (m);
  S415_SAME (cn0_dbhz);
  S415_SAME (pfa);
  S415_SAME (pd);
  S415_SAME (segments);
  S415_SAME (sps);
  S415_SAME (differential);
  S415_SAME (refine_max_error_db);
  S415_SAME (refine_samples_per_symbol);
  S415_SAME (refine_design_margin_db);
  S415_SAME (refine_n_fft);
  S415_SAME (refine_zero_pad);
  S415_SAME (refine_sequential);
  S415_SAME (refine_max_n_blocks);
  S415_SAME (carrier_freq_hz);
  S415_SAME (lost_confirm_s);
  S415_SAME (reacquire);
  S415_SAME (code_only_s);
  S415_SAME (doppler_rate);
  S415_SAME (refine);
#undef S415_SAME
  DP_CHECK_MSG (v->spc == a->sample_rate / a->chip_rate,
                "sample_rate is the table's");
  DP_CHECK_MSG (
      v->acq && v->acq->search->doppler_uncertainty == a->doppler_uncertainty,
      "doppler_uncertainty is the table's");
}

#endif /* DP_S415_TEST_H */
