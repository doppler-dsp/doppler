#include "doppler/state_filter_kalman/state_filter_kalman_core.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── The Kalman flavor ───────────────────────────────────────────────────
 *
 * The model is the observer form of the loop: x' = F x + K e with
 * F = [[1,1,0],[0,1,1],[0,0,1]] and K = (kp+ki+kr, ki+2*kr, kr), the
 * innovation variance S = P[0][0] + r, K = F P[:,0] / S and
 * P' = F P F^T - S K K^T + Q with Q = diag(0, 0, q). The covariance is the
 * predicted state's, stored as its upper triangle p00 p01 p02 p11 p12 p22. */

typedef struct
{
  double m[3][3];
} m3_t;

static m3_t
m3_mul (const m3_t *a, const m3_t *b)
{
  m3_t c;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      c.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j]
                  + a->m[i][2] * b->m[2][j];
  return c;
}

static m3_t
m3_t_ (const m3_t *a)
{
  m3_t c;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      c.m[i][j] = a->m[j][i];
  return c;
}

static m3_t
m3_add (const m3_t *a, const m3_t *b)
{
  m3_t c;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      c.m[i][j] = a->m[i][j] + b->m[i][j];
  return c;
}

/* The inverse by cofactors; the caller's matrices are I + G X with G and X
 * positive semidefinite, which are never singular. */
static m3_t
m3_inv (const m3_t *a)
{
  const double (*m)[3] = a->m;
  m3_t c;
  c.m[0][0] = m[1][1] * m[2][2] - m[1][2] * m[2][1];
  c.m[0][1] = m[0][2] * m[2][1] - m[0][1] * m[2][2];
  c.m[0][2] = m[0][1] * m[1][2] - m[0][2] * m[1][1];
  c.m[1][0] = m[1][2] * m[2][0] - m[1][0] * m[2][2];
  c.m[1][1] = m[0][0] * m[2][2] - m[0][2] * m[2][0];
  c.m[1][2] = m[0][2] * m[1][0] - m[0][0] * m[1][2];
  c.m[2][0] = m[1][0] * m[2][1] - m[1][1] * m[2][0];
  c.m[2][1] = m[0][1] * m[2][0] - m[0][0] * m[2][1];
  c.m[2][2] = m[0][0] * m[1][1] - m[0][1] * m[1][0];
  const double det
      = m[0][0] * c.m[0][0] + m[0][1] * c.m[1][0] + m[0][2] * c.m[2][0];
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      c.m[i][j] /= det;
  return c;
}

/* The gains the step reads, from the covariance's first column. */
static void
kalman_gains (const double p[6], double r, double *kp, double *ki, double *kr)
{
  const double S  = p[0] + r;
  const double k0 = (p[0] + p[1]) / S, k1 = (p[1] + p[2]) / S, k2 = p[2] / S;
  *kp = k0 - k1 + k2;
  *ki = k1 - 2.0 * k2;
  *kr = k2;
}

int
dp_state_filter_kalman_steady (double q, double r, double p[6],
                               double gains[3])
{
  if (!(q > 0.0 && r > 0.0) || !isfinite (q) || !isfinite (r))
    {
      memset (p, 0, 6 * sizeof *p); /* defined, not left as it was found */
      memset (gains, 0, 3 * sizeof *gains);
      return 0;
    }
  /* Structure-preserving doubling for P = Q + F P (EYE + G P)^-1 F^T with
   * G = H^T H / r, written in the form X = Q + A^T X (EYE + G X)^-1 A with
   * A = F^T: A, G and X double each pass and A -> 0, X -> P. Quadratic
   * convergence, so about log2(1/(bn*t)) passes whatever the bandwidth. */
  m3_t       A   = { { { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 1 } } }; /* F^T */
  m3_t       G   = { { { 1.0 / r, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } };
  m3_t       X   = { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, q } } };
  const m3_t EYE = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
  for (int it = 0; it < 200; it++)
    {
      const m3_t GX    = m3_mul (&G, &X);
      const m3_t IGX   = m3_add (&EYE, &GX);
      const m3_t W     = m3_inv (&IGX);
      const m3_t AW    = m3_mul (&A, &W);
      const m3_t AT    = m3_t_ (&A);
      const m3_t A2    = m3_mul (&AW, &A);
      const m3_t AWG   = m3_mul (&AW, &G);
      const m3_t AWGAT = m3_mul (&AWG, &AT);
      const m3_t G2    = m3_add (&G, &AWGAT);
      const m3_t ATX   = m3_mul (&AT, &X);
      const m3_t ATXW  = m3_mul (&ATX, &W);
      const m3_t ATXWA = m3_mul (&ATXW, &A);
      const m3_t X2    = m3_add (&X, &ATXWA);
      double     dx = 0.0, xm = 0.0;
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
          {
            dx = fmax (dx, fabs (X2.m[i][j] - X.m[i][j]));
            xm = fmax (xm, fabs (X2.m[i][j]));
          }
      A = A2;
      G = G2;
      X = X2;
      if (dx <= 1e-16 * xm)
        break;
    }
  p[0] = X.m[0][0];
  p[1] = X.m[0][1];
  p[2] = X.m[0][2];
  p[3] = X.m[1][1];
  p[4] = X.m[1][2];
  p[5] = X.m[2][2];
  kalman_gains (p, r, &gains[0], &gains[1], &gains[2]);
  return 1;
}

double
dp_state_filter_kalman_ratio_for_bandwidth (double bnt)
{
  if (!(bnt >= STATE_FILTER_KALMAN_BNT_MIN
        && bnt <= STATE_FILTER_KALMAN_BNT_MAX))
    return 0.0;
  double lo = -130.0, hi = 10.0; /* log of q/r */
  for (int i = 0; i < 100; i++)
    {
      const double mid = 0.5 * (lo + hi);
      double       p[6], g[3];
      dp_state_filter_kalman_steady (exp (mid), 1.0, p, g);
      const double bn
          = dp_state_filter_loop_bandwidth (3, g[0], g[1], g[2], 0.0, 1.0);
      if (bn < bnt)
        lo = mid;
      else
        hi = mid;
    }
  return exp (0.5 * (lo + hi));
}

/* The steady-state bandwidth per update of the loop a (q, r) pair gives. */
static double
kalman_bandwidth (double q, double r)
{
  double p[6], g[3];
  if (!dp_state_filter_kalman_steady (q, r, p, g))
    return NAN;
  return dp_state_filter_loop_bandwidth (3, g[0], g[1], g[2], 0.0, 1.0);
}

static int start_trajectory_ok (double q, double r, double start);

int
dp_state_filter_kalman_params_ok_qr (double q, double r, double t,
                                     double start)
{
  if (!(q > 0.0 && r > 0.0 && t > 0.0) || !isfinite (q) || !isfinite (r)
      || !isfinite (t))
    return 0;
  if (!(start >= 1.0 && start <= STATE_FILTER_KALMAN_START_MAX))
    return 0;
  const double bn = kalman_bandwidth (q, r);
  if (!(bn >= STATE_FILTER_KALMAN_BNT_MIN
        && bn <= STATE_FILTER_KALMAN_BNT_MAX))
    return 0;
  /* a start above the steady state's own: the gains along the covariance's
   * whole trajectory must stay inside the stability rectangle */
  return start == 1.0 || start_trajectory_ok (q, r, start);
}

static void
kalman_set_gains (dp_state_filter_kalman_state_t *k)
{
  kalman_gains (k->p, k->r, &k->base.kp, &k->base.ki, &k->base.kr);
}

void
dp_state_filter_kalman_init_qr (dp_state_filter_kalman_state_t *k, double q,
                                double r, double t, double start)
{
  double g[3];
  k->q = q;
  k->r = r;
  if (!dp_state_filter_kalman_steady (q, r, k->p0, g))
    {
      /* Unguarded, as the base's init is, but defined: a zero covariance
       * and zero gains, which dp_state_filter_kalman_params_ok_qr() refuses.
       */
      memset (&k->base, 0, sizeof k->base);
      memset (k->p, 0, sizeof k->p);
      k->base.t = t;
      return;
    }
  k->p0[3] += (start * start - 1.0) * k->p0[3]; /* the frequency entry */
  memcpy (k->p, k->p0, sizeof k->p);
  k->base.integ = 0.0;
  k->base.rate  = 0.0;
  k->base.t     = t;
  k->base.bn    = kalman_bandwidth (q, r) / t;
  kalman_set_gains (k);
}

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_qr (double q, double r, double t, double start)
{
  if (!dp_state_filter_kalman_params_ok_qr (q, r, t, start))
    return NULL;
  dp_state_filter_kalman_state_t *k = dp_xcalloc (1, sizeof *k);
  dp_state_filter_kalman_init_qr (k, q, r, t, start);
  return k;
}

void
dp_state_filter_kalman_destroy (dp_state_filter_kalman_state_t *k)
{
  free (k);
}

void
dp_state_filter_kalman_reset (dp_state_filter_kalman_state_t *k)
{
  k->base.integ = 0.0;
  k->base.rate  = 0.0;
  memcpy (k->p, k->p0, sizeof k->p);
  kalman_set_gains (k);
}

/* P' = F P F^T - S K K^T + Q, on the upper triangle. */
static void
kalman_advance (dp_state_filter_kalman_state_t *k)
{
  const double *p = k->p;
  const double  S = p[0] + k->r;
  /* F P: rows r0 = P0 + P1, r1 = P1 + P2, r2 = P2 (P symmetric) */
  const double a[3] = { p[0] + p[1], p[1] + p[3], p[2] + p[4] };
  const double b[3] = { p[1] + p[2], p[3] + p[4], p[4] + p[5] };
  const double c[3] = { p[2], p[4], p[5] };
  /* (F P) F^T: each row r becomes (r0 + r1, r1 + r2, r2) */
  const double f00 = a[0] + a[1], f01 = a[1] + a[2], f02 = a[2];
  const double f11 = b[1] + b[2], f12 = b[2];
  const double f22 = c[2];
  const double K0 = (p[0] + p[1]) / S, K1 = (p[1] + p[2]) / S, K2 = p[2] / S;
  k->p[0] = f00 - S * K0 * K0;
  k->p[1] = f01 - S * K0 * K1;
  k->p[2] = f02 - S * K0 * K2;
  k->p[3] = f11 - S * K1 * K1;
  k->p[4] = f12 - S * K1 * K2;
  k->p[5] = f22 - S * K2 * K2 + k->q;
}

/* The rule that bounds a start (docs/design/loop-filter.md Part III): the
 * gains in force at every update of the covariance's trajectory are inside
 * the stability rectangle, K from STATE_FILTER_K_MIN to STATE_FILTER_K_MAX at
 * detector delays 0 and STATE_FILTER_DELAY_MAX: the four corners. The loop is
 * stable on one interval of K, and at the gains this flavor holds the edges
 * move monotonically with the delay from 0 to 2 (the report's edge table,
 * section 2.2: K_low rises and K_high falls); that is a measurement, not a
 * theorem, so the test checks the corner rule against a dense walk over delays
 * 0, 1 and 2 and five loop gains, not the corners alone.
 *
 * Checking every update is 8/(bn*t) walks of four exact tests, so the check
 * samples: each next test is 1% of the elapsed updates after the last (and at
 * least one update after it, so the first 100 updates are every one tested: a
 * 1% step is under one update there, which is why there is no separate head),
 * out to 8/(bn*t) updates. The two constants are measured, not assumed, by the
 * harness's sampling table (the report, section 2.5): the cap this walk gives
 * equals the dense cap to the bisection at every bn*t of the grid, a 10% step
 * moves it by up to 0.4% and a 50% step by up to 5%, and the first violation
 * at the cap comes before 0.62/(bn*t) updates. No accept on that grid
 * depends on the step while the ceiling is 5 (the rule binds only where the
 * walk is under 130 updates); the step bounds the cost of a create. The
 * horizon is measured, not
 * assumed: the gains are within 1% of their steady values after at most about
 * 4.5/(bn*t) updates (the harness's settle table, for starts to the ceiling),
 * so 8/(bn*t) covers the settling with a margin of 1.8. The gains vary on
 * the time scale 1/(bn*t) updates (the closed-loop poles), and the exact test
 * they are judged by is reliable off the design manifold
 * (docs/design/loop-filter.md section 11.1), so a sample does not step over an
 * excursion: the test pins the sampled check against the dense one (every
 * update) at, below and past the cap at three bandwidths. */
static int
gains_in_rectangle (const dp_state_filter_kalman_state_t *k)
{
  static const double ks[2] = { STATE_FILTER_K_MIN, STATE_FILTER_K_MAX };
  static const double ds[2] = { 0.0, STATE_FILTER_DELAY_MAX };
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
      if (!dp_state_filter_loop_stable (3, k->base.kp, k->base.ki, k->base.kr,
                                        ds[j], ks[i]))
        return 0;
  return 1;
}

static int
start_trajectory_ok (double q, double r, double start)
{
  dp_state_filter_kalman_state_t k;
  dp_state_filter_kalman_init_qr (&k, q, r, 1.0, start);
  const size_t n    = (size_t)(8.0 / kalman_bandwidth (q, r));
  size_t       next = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (i == next)
        {
          if (!gains_in_rectangle (&k))
            return 0;
          next = next + next / 100 + 1;
        }
      kalman_advance (&k);
      kalman_set_gains (&k);
    }
  return 1;
}

double
dp_state_filter_kalman_update (dp_state_filter_kalman_state_t *k, double e)
{
  /* The gains in force are always the ones P gives (init, reset and the
   * end of this function keep that), so the blob can be checked for it. */
  const double c = dp_state_filter_step (&k->base, e);
  kalman_advance (k);
  kalman_set_gains (k);
  return c;
}

void
dp_state_filter_kalman_steps (dp_state_filter_kalman_state_t *k,
                              const double *x, double *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = dp_state_filter_kalman_step (k, x[i]);
}

static int
configure_qr (dp_state_filter_kalman_state_t *k, double q, double r, double t,
              double start)
{
  if (!dp_state_filter_kalman_params_ok_qr (q, r, t, start))
    return DP_ERR_INVALID;
  const double integ = k->base.integ, rate = k->base.rate;
  dp_state_filter_kalman_init_qr (k, q, r, t, start);
  k->base.integ = integ; /* keep lock: a reconfigure, not a restart */
  k->base.rate  = rate;
  return DP_OK;
}

/* ── The Kalman blob ─────────────────────────────────────────────────────
 * Its own magic, so a fixed-gain blob and this one refuse each other at the
 * envelope. [envelope][kp ki kr integ rate bn t][p x6]
 * [p0 x6][q r]. */

#define SFK_PAYLOAD_BYTES (21 * sizeof (double))

/* A symmetric 3x3 (upper triangle) is positive definite when its leading
 * principal minors are positive, by Cholesky without the square roots. */
static int
sfk_pd (const double p[6])
{
  const double d0 = p[0];
  if (!(d0 > 0.0))
    return 0;
  const double l10 = p[1] / d0, l20 = p[2] / d0;
  const double d1 = p[3] - l10 * p[1];
  if (!(d1 > 0.0))
    return 0;
  const double l21 = (p[4] - l20 * p[1]) / d1;
  const double d2  = p[5] - l20 * p[2] - l21 * l21 * d1;
  return d2 > 0.0;
}

size_t
dp_state_filter_kalman_state_bytes (const dp_state_filter_kalman_state_t *k)
{
  (void)k;
  return sizeof (dp_state_hdr_t) + SFK_PAYLOAD_BYTES;
}

void
dp_state_filter_kalman_get_state (const dp_state_filter_kalman_state_t *k,
                                  void                                 *blob)
{
  const size_t total = dp_state_filter_kalman_state_bytes (k);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, STATE_FILTER_KALMAN_STATE_MAGIC,
            STATE_FILTER_KALMAN_STATE_VERSION, total);
  dp_w_f64 (&w, k->base.kp);
  dp_w_f64 (&w, k->base.ki);
  dp_w_f64 (&w, k->base.kr);
  dp_w_f64 (&w, k->base.integ);
  dp_w_f64 (&w, k->base.rate);
  dp_w_f64 (&w, k->base.bn);
  dp_w_f64 (&w, k->base.t);
  for (int i = 0; i < 6; i++)
    dp_w_f64 (&w, k->p[i]);
  for (int i = 0; i < 6; i++)
    dp_w_f64 (&w, k->p0[i]);
  dp_w_f64 (&w, k->q);
  dp_w_f64 (&w, k->r);
}

int
dp_state_filter_kalman_set_state (dp_state_filter_kalman_state_t *k,
                                  const void                     *blob)
{
  const size_t total = dp_state_filter_kalman_state_bytes (k);
  int rc = dp_state_validate (blob, total, STATE_FILTER_KALMAN_STATE_MAGIC,
                              STATE_FILTER_KALMAN_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, total);
  r.off         = sizeof (dp_state_hdr_t);
  dp_state_filter_kalman_state_t c;
  memset (&c, 0, sizeof c);
  dp_r_bytes (&r, &c.base.kp, sizeof c.base.kp);
  dp_r_bytes (&r, &c.base.ki, sizeof c.base.ki);
  dp_r_bytes (&r, &c.base.kr, sizeof c.base.kr);
  dp_r_bytes (&r, &c.base.integ, sizeof c.base.integ);
  dp_r_bytes (&r, &c.base.rate, sizeof c.base.rate);
  dp_r_bytes (&r, &c.base.bn, sizeof c.base.bn);
  dp_r_bytes (&r, &c.base.t, sizeof c.base.t);
  dp_r_bytes (&r, c.p, sizeof c.p);
  dp_r_bytes (&r, c.p0, sizeof c.p0);
  dp_r_bytes (&r, &c.q, sizeof c.q);
  dp_r_bytes (&r, &c.r, sizeof c.r);
  /* The figures are the reject key: they size the steady state. P and P0
   * must be positive definite (and finite: NaN fails every comparison),
   * and the gains in force must be the ones P and r give. */
  if (!dp_state_filter_kalman_params_ok_qr (c.q, c.r, c.base.t, 1.0))
    return DP_ERR_INVALID;
  for (int i = 0; i < 6; i++)
    if (!isfinite (c.p[i]) || !isfinite (c.p0[i]))
      return DP_ERR_INVALID;
  if (!sfk_pd (c.p) || !sfk_pd (c.p0))
    return DP_ERR_INVALID;
  /* P0 is the start a live object holds: the steady state for (q, r) with
   * the frequency entry scaled by start^2, start from 1 to the ceiling. The
   * trajectory rule is not rerun (about 1 ms at bn*t 1e-2 and 6 ms at 1e-4,
   * measured; longer slower): a restored start is checked for the ceiling and
   * for being a start at all, not for the rule, which the validated
   * constructors enforce. */
  {
    double pss[6], gss[3];
    if (!dp_state_filter_kalman_steady (c.q, c.r, pss, gss))
      return DP_ERR_INVALID;
    for (int i = 0; i < 6; i++)
      if (i != 3 && fabs (c.p0[i] - pss[i]) > 1e-9 * fabs (pss[i]) + DBL_MIN)
        return DP_ERR_INVALID;
    const double start = sqrt (c.p0[3] / pss[3]);
    if (!(start >= 1.0 - 1e-9
          && start <= STATE_FILTER_KALMAN_START_MAX * (1.0 + 1e-9)))
      return DP_ERR_INVALID;
  }
  double kp, ki, kr;
  kalman_gains (c.p, c.r, &kp, &ki, &kr);
  if (fabs (c.base.kp - kp) > 1e-9 * fabs (kp) + DBL_MIN
      || fabs (c.base.ki - ki) > 1e-9 * fabs (ki) + DBL_MIN
      || fabs (c.base.kr - kr) > 1e-9 * fabs (kr) + DBL_MIN)
    return DP_ERR_INVALID;
  if (fabs (c.base.bn * c.base.t - kalman_bandwidth (c.q, c.r))
      > 1e-9 * kalman_bandwidth (c.q, c.r))
    return DP_ERR_INVALID;
  *k = c;
  return DP_OK;
}

/* ── The caller-facing figures ───────────────────────────────────────────
 * What a caller knows: the rms of the detector error when locked (sigma_e)
 * and the rms change of the rate per update (sigma_a), both in the loop's
 * own units; the (q, r) form above is these squared. A frequency
 * uncertainty is in the same units and is taken against the steady state's
 * own. */

double
dp_state_filter_kalman_start_for (double sigma_e, double sigma_a,
                                  double sigma_f0)
{
  double p[6], g[3];
  /* each figure must be positive itself: squaring first would accept a
   * negative one whose square is in range */
  if (!(sigma_e > 0.0 && sigma_a > 0.0)
      || !dp_state_filter_kalman_steady (sigma_a * sigma_a, sigma_e * sigma_e,
                                         p, g))
    return 0.0;
  if (sigma_f0 == 0.0)
    return 1.0; /* absent: the steady state */
  const double start = sigma_f0 / sqrt (p[3]);
  return (start >= 1.0 && start <= STATE_FILTER_KALMAN_START_MAX) ? start
                                                                  : 0.0;
}

int
dp_state_filter_kalman_params_ok (double sigma_e, double sigma_a, double t,
                                  double sigma_f0)
{
  const double start
      = dp_state_filter_kalman_start_for (sigma_e, sigma_a, sigma_f0);
  return start > 0.0
         && dp_state_filter_kalman_params_ok_qr (sigma_a * sigma_a,
                                                 sigma_e * sigma_e, t, start);
}

void
dp_state_filter_kalman_init (dp_state_filter_kalman_state_t *k, double sigma_e,
                             double sigma_a, double t, double sigma_f0)
{
  /* Unguarded, as the base's init is, but defined at every input: a figure
   * that is not positive gives the inert filter (the steady solve refuses
   * q = r = 0); an unaccepted sigma_f0 is taken as the steady state; and any
   * other input, including a bandwidth outside the flavor's range, builds the
   * live filter those figures describe. dp_state_filter_kalman_params_ok() is
   * the check. */
  if (!(sigma_e > 0.0 && sigma_a > 0.0))
    {
      dp_state_filter_kalman_init_qr (k, 0.0, 0.0, t, 1.0);
      return;
    }
  double start = dp_state_filter_kalman_start_for (sigma_e, sigma_a, sigma_f0);
  if (start == 0.0)
    start = 1.0;
  dp_state_filter_kalman_init_qr (k, sigma_a * sigma_a, sigma_e * sigma_e, t,
                                  start);
}

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create (double sigma_e, double sigma_a, double t,
                               double sigma_f0)
{
  if (!dp_state_filter_kalman_params_ok (sigma_e, sigma_a, t, sigma_f0))
    return NULL;
  dp_state_filter_kalman_state_t *k = dp_xcalloc (1, sizeof *k);
  dp_state_filter_kalman_init (k, sigma_e, sigma_a, t, sigma_f0);
  return k;
}

/* The automatic case: a bandwidth and a period alone, as the fixed-gain
 * flavor takes them. The unit measurement figure leaves the gains alone (they
 * are scale-free) and the covariance starts at its steady state. */

int
dp_state_filter_kalman_params_ok_bn (double bn, double t)
{
  if (!(t > 0.0) || !isfinite (t) || !isfinite (bn))
    return 0;
  return dp_state_filter_kalman_ratio_for_bandwidth (bn * t) > 0.0;
}

void
dp_state_filter_kalman_init_bn (dp_state_filter_kalman_state_t *k, double bn,
                                double t)
{
  dp_state_filter_kalman_init_qr (
      k, dp_state_filter_kalman_ratio_for_bandwidth (bn * t), 1.0, t, 1.0);
}

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_bn (double bn, double t)
{
  if (!dp_state_filter_kalman_params_ok_bn (bn, t))
    return NULL;
  dp_state_filter_kalman_state_t *k = dp_xcalloc (1, sizeof *k);
  dp_state_filter_kalman_init_bn (k, bn, t);
  return k;
}

/* ── For the binding ─────────────────────────────────────────────────────
 * The figures form or `bn` alone: both or neither is refused. */

static int
args_form (double sigma_e, double sigma_a, double sigma_f0, double bn)
{
  const int figures = (sigma_e != 0.0 || sigma_a != 0.0 || sigma_f0 != 0.0);
  const int auto_   = (bn != 0.0);
  if (figures == auto_)
    return 0; /* both given, or neither */
  return figures ? 1 : 2;
}

const char *
dp_state_filter_kalman_args_why (double sigma_e, double sigma_a, double t,
                                 double sigma_f0, double bn)
{
  const int form = args_form (sigma_e, sigma_a, sigma_f0, bn);
  if (form == 0)
    return (bn != 0.0) ? "give sigma_e and sigma_a, or bn alone, not both"
                       : "give sigma_e and sigma_a, or bn alone";
  if (!(t > 0.0) || !isfinite (t))
    return "t must be a positive, finite update period";
  if (form == 2)
    {
      if (!isfinite (bn) || bn < 0.0)
        return "bn must be finite and positive";
      if (!dp_state_filter_kalman_params_ok_bn (bn, t))
        return "bn*t is outside the range the filter accepts";
      return NULL;
    }
  if (!(sigma_e > 0.0 && sigma_a > 0.0))
    return "sigma_e and sigma_a must both be > 0";
  double p[6], g[3];
  if (!dp_state_filter_kalman_steady (sigma_a * sigma_a, sigma_e * sigma_e, p,
                                      g))
    return "sigma_e and sigma_a must be finite figures with a steady state";
  const double b = kalman_bandwidth (sigma_a * sigma_a, sigma_e * sigma_e);
  if (!(b >= STATE_FILTER_KALMAN_BNT_MIN))
    return "the bandwidth sigma_e and sigma_a give is below the smallest "
           "the filter accepts";
  if (!(b <= STATE_FILTER_KALMAN_BNT_MAX))
    return "the bandwidth sigma_e and sigma_a give is above the largest "
           "the filter accepts";
  if (dp_state_filter_kalman_start_for (sigma_e, sigma_a, sigma_f0) <= 0.0)
    return "sigma_f0 must be 0, or from the steady state's frequency "
           "deviation up to a bounded multiple of it";
  if (!dp_state_filter_kalman_params_ok (sigma_e, sigma_a, t, sigma_f0))
    return "sigma_f0 is a start whose gains leave the stability region "
           "before they settle";
  return NULL;
}

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args_why (double sigma_e, double sigma_a,
                                        double t, double sigma_f0, double bn,
                                        const char **why)
{
  const char *w
      = dp_state_filter_kalman_args_why (sigma_e, sigma_a, t, sigma_f0, bn);
  if (why)
    *why = w;
  return w ? NULL
           : dp_state_filter_kalman_create_args (sigma_e, sigma_a, t, sigma_f0,
                                                 bn);
}

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args (double sigma_e, double sigma_a, double t,
                                    double sigma_f0, double bn)
{
  switch (args_form (sigma_e, sigma_a, sigma_f0, bn))
    {
    case 1:
      return dp_state_filter_kalman_create (sigma_e, sigma_a, t, sigma_f0);
    case 2:
      return dp_state_filter_kalman_create_bn (bn, t);
    default:
      return NULL;
    }
}

int
dp_state_filter_kalman_configure (dp_state_filter_kalman_state_t *k,
                                  double sigma_e, double sigma_a, double t,
                                  double sigma_f0, double bn)
{
  /* An argument of 0 keeps the current value (sigma_f0 0 is the steady
   * state), so a retune can name only what changes; `bn` alone is the
   * automatic case and may not be mixed with the figures. */
  const double tt = t != 0.0 ? t : k->base.t;
  if (bn != 0.0)
    {
      if (sigma_e != 0.0 || sigma_a != 0.0 || sigma_f0 != 0.0)
        return DP_ERR_INVALID;
      if (!dp_state_filter_kalman_params_ok_bn (bn, tt))
        return DP_ERR_INVALID;
      return configure_qr (
          k, dp_state_filter_kalman_ratio_for_bandwidth (bn * tt), 1.0, tt,
          1.0);
    }
  const double se = sigma_e != 0.0 ? sigma_e : sqrt (k->r);
  const double sa = sigma_a != 0.0 ? sigma_a : sqrt (k->q);
  if (!dp_state_filter_kalman_params_ok (se, sa, tt, sigma_f0))
    return DP_ERR_INVALID;
  return configure_qr (k, sa * sa, se * se, tt,
                       dp_state_filter_kalman_start_for (se, sa, sigma_f0));
}

double
dp_state_filter_kalman_bandwidth (const dp_state_filter_kalman_state_t *k,
                                  double delay, double gain)
{
  return dp_state_filter_bandwidth (&k->base, delay, gain);
}

bool
dp_state_filter_kalman_stable (const dp_state_filter_kalman_state_t *k,
                               double delay, double gain)
{
  return dp_state_filter_stable (&k->base, delay, gain);
}

double
dp_state_filter_kalman_sigma_f (const dp_state_filter_kalman_state_t *k)
{
  double p[6], g[3];
  if (!dp_state_filter_kalman_steady (k->q, k->r, p, g))
    return 0.0;
  return sqrt (p[3]);
}
