/**
 * @file test_state_filter_core.c
 * @brief Unit tests for the fixed-gain third-order state filter.
 *
 * Every claim of docs/design/loop-filter.md section 11 to 14 that the header
 * makes is pinned here, and the exact bandwidth and stability function is
 * held by routes that share no code with it: a Parseval sum over the REAL
 * `dp_state_filter_step` (and, for order 2, over a local copy of the
 * second-order recurrence with gains from the Rice parameterisation; the
 * shipped `ref2_step` is held against the same function by
 * native/validation/state_filter_loop.c), a Jury test written from the
 * polynomial's own coefficients, and the published closed forms.
 *
 * Sections:
 *   1. Lifecycle and the domain: create refusals, calloc-zeroed estimates,
 *      init stores bn and t and does not touch integ or rate
 *   2. The recurrence, against hand literals and a closed form
 *   3. The gains: the delivered bandwidth is the one asked for (a simulated
 *      Parseval sum on the real step), the shape constants are used
 *   4. reset, configure and a refused call that changes nothing
 *   5. steps() == repeated step(), including aliasing
 *   6. The exact function: closed forms, simulation at delays 0, 1, 2 and an
 *      interpolated fraction, a Jury oracle on random gains, order 2 through
 *      a local copy of the second-order recurrence (the shipped
 *      dp_loop_filter_step is held against the same function by
 *      native/validation/state_filter_loop.c under ctest)
 *   7. The stability rectangle: the bound derived from the exact function
 *      rather than restated, the grid, one stable interval in K, the lower
 *      edge against the analytic one
 *   8. Steady-state errors of both orders, against a simulated loop
 *   9. Serialized state: round trip, the flavor key, forged payloads
 */
#include "doppler/state_filter/state_filter_core.h"
#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The loop under test, simulated from first principles (the harness shape of
 * loop_filter_noise_bw.c): a raw error r[n] = w[n] - phi[n], read by the
 * detector `delay` updates late, the filter, an oscillator that accumulates
 * the control. A fractional delay k + f is linear interpolation of the
 * detector's error between r[n-k] and r[n-k-1]. `step` is the filter under
 * test. */
typedef double (*step_fn) (void *, double);

static double
sf_step (void *s, double e)
{
  return dp_state_filter_step ((dp_state_filter_state_t *)s, e);
}

/* The second-order recurrence, written here from the textbook form. This is
 * deliberately a local copy and not a dependency on the shipped filter:
 * native/validation/state_filter_loop.c holds the shipped dp_loop_filter_step
 * against the same exact function under ctest (--check), so the two are
 * tied together there. Do not "fix" this into a link on loop_filter. The
 * gains are Rice's parameterisation (Digital Communications: A Discrete-Time
 * Approach, App. C), not the shipped filter's expression. */
typedef struct
{
  double kp, ki, integ;
} ref2_t;

static void
ref2_init (ref2_t *s, double bn, double zeta, double t)
{
  const double theta = 4.0 * zeta * bn * t / (4.0 * zeta * zeta + 1.0);
  const double den   = 1.0 + 2.0 * zeta * theta + theta * theta;
  s->kp              = 4.0 * zeta * theta / den;
  s->ki              = 4.0 * theta * theta / den;
}

static double
ref2_step (ref2_t *s, double e)
{
  s->integ += s->ki * e;
  return s->integ + s->kp * e;
}

static double
lf_step (void *s, double e)
{
  return ref2_step ((ref2_t *)s, e);
}

#define HIST 16

/* Bn = (1/2) sum phi^2 for a unit impulse input, summed until the response
 * has died (or `n_max`). Returns the sum; `tail` receives the share of it
 * that landed in the last half, a converged answer being one near zero. */
static double
bn_parseval (step_fn step, void *s, double delay, double gain, size_t n_max,
             double *tail)
{
  const int    k       = (int)floor (delay);
  const double f       = delay - (double)k;
  double       r[HIST] = { 0 }; /* r[0] = r[n], r[1] = r[n-1], ... */
  double       phi = 0.0, sum = 0.0, half = 0.0;
  for (size_t n = 0; n < n_max; n++)
    {
      if (n == n_max / 2)
        half = sum;
      for (int i = HIST - 1; i > 0; i--)
        r[i] = r[i - 1];
      r[0]           = ((n == 0) ? 1.0 : 0.0) - phi;
      const double e = (1.0 - f) * r[k] + f * r[k + 1];
      phi += step (s, gain * e); /* phi[n+1] = phi[n] + c[n] */
      sum += phi * phi;
    }
  if (tail)
    *tail = sum > 0.0 ? (sum - half) / sum : 0.0;
  return 0.5 * sum;
}

/* The closed forms of docs/design/loop-filter.md section 11.1, typed from
 * the page and not from the implementation. */
static double
bn2_closed (double kp, double ki)
{
  return (2.0 * kp * kp + (2.0 + kp) * ki)
         / (2.0 * kp * (4.0 - 2.0 * kp - ki));
}

static double
bn3_closed (double kp, double ki, double kr)
{
  return (2 * ki * ki * kp + 4 * ki * ki + 4 * ki * kp * kp + 3 * ki * kp * kr
          + 4 * ki * kr + 4 * kp * kp * kr + kp * kr * kr - 4 * kp * kr
          + kr * kr)
         / (2.0 * (ki * kp + kp * kr - kr) * (8.0 - 2 * ki - 4 * kp - kr));
}

/* The gains solved in long double by the same bisection on the same closed
 * form, written here with no code of the object's: what a platform that
 * evaluates the closed form with other rounding (a fused multiply-add, a wider
 * register) could produce. The largest relative difference from the object's
 * double gains, over the grid below, is what a restore's gain tolerance has to
 * absorb between platforms. */
static long double
bn3_ld (long double kp, long double ki, long double kr)
{
  return (2 * ki * ki * kp + 4 * ki * ki + 4 * ki * kp * kp + 3 * ki * kp * kr
          + 4 * ki * kr + 4 * kp * kp * kr + kp * kr * kr - 4 * kp * kr
          + kr * kr)
         / (2.0L * (ki * kp + kp * kr - kr) * (8.0L - 2 * ki - 4 * kp - kr));
}

static void
gains_ld (double B, long double *kp, long double *ki, long double *kr)
{
  const long double a = 2.0L, b = 2.0L;
  const long double c  = (a * b * b + a * a - b) / (4.0L * (a * b - 1.0L));
  const long double s0 = (long double)B / c;
  long double       lo = 0.5L * s0, hi = 1.01L * s0;
  for (int i = 0; i < 200; i++)
    {
      const long double th = 0.5L * (lo + hi);
      if (bn3_ld (b * th, a * th * th, th * th * th) < (long double)B)
        lo = th;
      else
        hi = th;
    }
  const long double th = 0.5L * (lo + hi);
  *kp                  = b * th;
  *ki                  = a * th * th;
  *kr                  = th * th * th;
}

/* Jury's conditions for the delay-free third-order loop, written from the
 * page's statement of them. */
static int
jury3 (double kp, double ki, double kr)
{
  const double a1 = kp + ki + kr - 3.0, a2 = 3.0 - 2.0 * kp - ki,
               a3 = kp - 1.0;
  return kr > 0.0 && 8.0 - 4.0 * kp - 2.0 * ki - kr > 0.0 && fabs (a3) < 1.0
         && fabs (a3 * a3 - 1.0) > fabs (a3 * a1 - a2);
}

/* Stability by simulation: the impulse response of the loop at a delay and
 * gain either dies (+1), blows up (-1) or is too slow to call (0). */
static int
sim_stable (step_fn step, void *s, double delay, double gain)
{
  double tail;
  double sum = bn_parseval (step, s, delay, gain, 60000, &tail);
  if (!isfinite (sum) || sum > 1e12)
    return -1;
  return tail < 1e-9 ? +1 : 0;
}

/* The chunk-invariance harness's three hooks: a fresh filter, and `steps`
 * over a partition of the stream. */
static void *
sf_create_obj (void *arg)
{
  (void)arg;
  return dp_state_filter_create (0.03, 1.0);
}

static void
sf_destroy_obj (void *o)
{
  dp_state_filter_destroy (o);
}

static size_t
sf_process (void *o, const void *in, size_t n, void *out, size_t cap)
{
  (void)cap;
  dp_state_filter_steps (o, in, out, n);
  return n;
}

static void
fresh (dp_state_filter_state_t *sf, double bn, double t)
{
  memset (sf, 0, sizeof *sf);
  dp_state_filter_init (sf, bn, t);
}

/* A source of gains OFF the design manifold: the gains a Kalman filter on the
 * loop's model (phase, frequency, rate, with a noise figure q on the rate)
 * holds while its covariance converges. P' = F P F^T - S K K^T + Q, with
 * K = F P[:,0] / S, S = P00 + 1, and the loop gains from K: kp = K0 - K1 + K2,
 * ki = K1 - 2 K2, kr = K2. Written here, plainly, only to supply test inputs;
 * the component that uses it is the Kalman flavor, not this object. */
static void
cov_gains (const double P[3][3], double *kp, double *ki, double *kr)
{
  const double S  = P[0][0] + 1.0;
  const double K0 = (P[0][0] + P[0][1]) / S, K1 = (P[0][1] + P[0][2]) / S,
               K2 = P[0][2] / S;
  *kp             = K0 - K1 + K2;
  *ki             = K1 - 2.0 * K2;
  *kr             = K2;
}

static void
cov_advance (double P[3][3], double q)
{
  const double S  = P[0][0] + 1.0;
  const double K0 = (P[0][0] + P[0][1]) / S, K1 = (P[0][1] + P[0][2]) / S,
               K2    = P[0][2] / S;
  const double Kv[3] = { K0, K1, K2 };
  double       FP[3][3], N[3][3];
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      FP[i][j] = P[i][j] + (i < 2 ? P[i + 1][j] : 0.0);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      N[i][j] = FP[i][j] + (j < 2 ? FP[i][j + 1] : 0.0) - S * Kv[i] * Kv[j];
  N[2][2] += q;
  memcpy (P, N, sizeof N);
}

/* The steady-state covariance for a noise figure q, by iterating to rest. */
static void
cov_steady (double P[3][3], double q)
{
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      P[i][j] = i == j ? 1.0 : 0.0;
  for (int it = 0; it < 400000; it++)
    {
      double Pn[3][3];
      memcpy (Pn, P, sizeof Pn);
      cov_advance (Pn, q);
      double dm = 0.0, pm = 0.0;
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
          {
            dm = fmax (dm, fabs (Pn[i][j] - P[i][j]));
            pm = fmax (pm, fabs (Pn[i][j]));
          }
      memcpy (P, Pn, sizeof Pn);
      if (dm <= 1e-15 * pm)
        break;
    }
}

/* The oracle for the exact stability test, sharing nothing with the Lyapunov
 * route: simulate the loop's own state recursion from a unit kick and watch
 * the state. The loop is
 *   rate += kr*m; integ += rate + ki*m; phi += integ + kp*m,  m = K*e[n-d],
 *   e[n] = -phi[n]   (the homogeneous loop: the input is zero),
 * with an integer detector delay d, a delay line of past errors, and the
 * order-3 filter. A stable loop with spectral radius 1 - delta decays like
 * (1 - delta)^N; an unstable one grows like (1 + eps)^N. After N updates the
 * verdict is stable if the state fell under 1e-6 of its peak, unstable if it
 * grew past 1e6 of the kick, and otherwise UNDECIDED: so the oracle's range
 * of validity is |radius - 1| > about 14/N, and the caller picks N for the
 * margins it wants to see (the cases below count what they could not
 * decide). */
static int
kick_stable (double kp, double ki, double kr, int d, double K, size_t N)
{
  double line[8] = { 0 };
  double rate = 0.0, integ = 0.0, phi = 1.0; /* the kick: phi starts at 1 */
  double peak = 1.0;
  for (int i = 0; i <= d; i++)
    line[i] = -1.0; /* e = -phi for the kicked past */
  for (size_t n = 0; n < N; n++)
    {
      const double m = K * line[d];
      rate += kr * m;
      integ += rate + ki * m;
      phi += integ + kp * m;
      for (int j = d; j > 0; j--)
        line[j] = line[j - 1];
      line[0]          = -phi;
      const double mag = fabs (phi) + fabs (integ) + fabs (rate);
      if (mag > peak)
        peak = mag;
      if (mag > 1e6)
        return 0;
    }
  const double fin = fabs (phi) + fabs (integ) + fabs (rate);
  return fin < 1e-6 * peak ? 1 : -1;
}

int
main (void)
{
  /* 1 ── Lifecycle and the domain ───────────────────────────────────────── */
  {
    dp_state_filter_state_t *sf = dp_state_filter_create (0.01, 1.0);
    DP_REQUIRE (sf != NULL);
    DP_CHECK (sf->integ == 0.0 && sf->rate == 0.0); /* calloc-zeroed */
    DP_CHECK (sf->bn == 0.01 && sf->t == 1.0);
    DP_CHECK (sf->kp > 0.0 && sf->ki > 0.0 && sf->kr > 0.0);

    dp_state_filter_state_t by_value;
    memset (&by_value, 0xAB, sizeof by_value);
    dp_state_filter_init (&by_value, 0.01, 1.0);
    DP_CHECK (by_value.kp == sf->kp && by_value.ki == sf->ki
              && by_value.kr == sf->kr); /* create == init */

    /* init does NOT touch the estimates: the embedder owns them. */
    by_value.integ = 3.5;
    by_value.rate  = -0.25;
    dp_state_filter_init (&by_value, 0.02, 2.0);
    DP_CHECK (by_value.integ == 3.5 && by_value.rate == -0.25);
    DP_CHECK (by_value.bn == 0.02 && by_value.t == 2.0);
    dp_state_filter_destroy (sf);
    dp_state_filter_destroy (NULL);

    /* The domain: each refusal turns on one clause of the predicate. */
    DP_CHECK (dp_state_filter_create (-0.01, 1.0) == NULL);
    DP_CHECK (dp_state_filter_create (0.01, 0.0) == NULL);
    DP_CHECK (dp_state_filter_create (0.01, -1.0) == NULL);
    DP_CHECK (dp_state_filter_create (NAN, 1.0) == NULL);
    DP_CHECK (dp_state_filter_create (0.01, NAN) == NULL);
    DP_CHECK (dp_state_filter_create (INFINITY, 1.0) == NULL);
    DP_CHECK (dp_state_filter_create (0.01, INFINITY) == NULL);
    DP_CHECK (dp_state_filter_create (STATE_FILTER_BNT_MAX * 1.01, 1.0)
              == NULL); /* past the bound */
    DP_CHECK (dp_state_filter_create (0.01, STATE_FILTER_BNT_MAX * 110.0)
              == NULL); /* the product is what is bounded */
    DP_CHECK (dp_state_filter_create (STATE_FILTER_BNT_MIN * 0.5, 1.0)
              == NULL); /* below the floor, and not zero */
    {
      dp_state_filter_state_t *floor_edge
          = dp_state_filter_create (STATE_FILTER_BNT_MIN, 1.0);
      DP_CHECK (floor_edge != NULL); /* the floor itself is inside */
      if (floor_edge)
        DP_CHECK (dp_state_filter_state_ok (floor_edge));
      dp_state_filter_destroy (floor_edge);
    }
    dp_state_filter_state_t *edge
        = dp_state_filter_create (STATE_FILTER_BNT_MAX, 1.0);
    DP_CHECK (edge != NULL); /* the bound itself is inside */
    dp_state_filter_destroy (edge);
    dp_state_filter_state_t *frozen = dp_state_filter_create (0.0, 1.0);
    DP_REQUIRE (frozen != NULL); /* bn = 0: a frozen loop */
    DP_CHECK (frozen->kp == 0.0 && frozen->ki == 0.0 && frozen->kr == 0.0);
    DP_CHECK (dp_state_filter_step (frozen, 7.0) == 0.0);
    /* The frozen loop's exact answers are stated, not an accident of the
     * solve: its rate and frequency states are undriven (all gains zero), so
     * (A, b) is not controllable, it is not stable, and it has no bandwidth.
     * Likewise a loop with kr = 0 in order 3 (an undriven rate state) and one
     * with ki = 0 in order 2 (a pole-zero cancellation at z = 1). */
    DP_CHECK (!dp_state_filter_stable (frozen, 0.0, 1.0));
    DP_CHECK (isnan (dp_state_filter_bandwidth (frozen, 0.0, 1.0)));
    DP_CHECK (!dp_state_filter_loop_stable (3, 0.1, 0.01, 0.0, 0.0, 1.0));
    DP_CHECK (
        isnan (dp_state_filter_loop_bandwidth (3, 0.1, 0.01, 0.0, 0.0, 1.0)));
    DP_CHECK (!dp_state_filter_loop_stable (2, 0.3, 0.0, 0.0, 0.0, 1.0));
    DP_CHECK (
        isnan (dp_state_filter_loop_bandwidth (2, 0.3, 0.0, 0.0, 0.0, 1.0)));
    dp_state_filter_destroy (frozen);
  }

  /* 2 ── The recurrence ─────────────────────────────────────────────────── */
  {
    dp_state_filter_state_t s;
    fresh (&s, 0.05, 1.0);
    const double kp = s.kp, ki = s.ki, kr = s.kr;
    /* Hand literals: the first update from zero. */
    double c = dp_state_filter_step (&s, 1.0);
    DP_CHECK_NEAR (s.rate, kr, 1e-15);
    DP_CHECK_NEAR (s.integ, kr + ki, 1e-15);
    DP_CHECK_NEAR (c, kr + ki + kp, 1e-15);
    /* ...the second, with the rate already carried. */
    c = dp_state_filter_step (&s, -2.0);
    DP_CHECK_NEAR (s.rate, kr - 2.0 * kr, 1e-15);
    DP_CHECK_NEAR (s.integ, (kr + ki) + (-kr) + ki * -2.0, 1e-15);
    DP_CHECK_NEAR (c, s.integ + kp * -2.0, 1e-15);

    /* Fed a constant error the rate ramps and the frequency estimate goes
     * as the sum of that ramp: rate = n*kr*e and
     * integ = n*ki*e + kr*e*n(n+1)/2. Open loop, so unbounded -- the
     * accumulation working, as in the second-order filter. */
    fresh (&s, 0.05, 1.0);
    const double e = 0.3;
    for (int n = 1; n <= 40; n++)
      dp_state_filter_step (&s, e);
    DP_CHECK_NEAR (s.rate, 40.0 * kr * e, 1e-12);
    DP_CHECK_NEAR (s.integ, 40.0 * ki * e + kr * e * 40.0 * 41.0 / 2.0, 1e-10);
  }

  /* 3 ── The gains: the bandwidth asked for is the one delivered ────────── */
  {
    /* The shape: kp = b*th, ki = a*th^2, kr = th^3, so these two ratios
     * identify the constants without restating the scale. */
    dp_state_filter_state_t s;
    fresh (&s, 0.03, 1.0);
    const double a = STATE_FILTER_SHAPE_A, b = STATE_FILTER_SHAPE_B;
    DP_CHECK_NEAR (s.kp * s.kp / s.ki, b * b / a, 1e-12);
    DP_CHECK_NEAR (s.kp * s.kp * s.kp / s.kr, b * b * b, 1e-9);

    /* The delivered bandwidth, from a simulation of the REAL step: the
     * excess over bn*t is zero by construction, not a small fraction of
     * bn*t as the second-order filter's is. A fresh filter per cell. */
    static const double bn_t[] = { 0.002, 0.01, 0.04, 0.08, 0.12 };
    for (size_t i = 0; i < sizeof bn_t / sizeof *bn_t; i++)
      {
        fresh (&s, bn_t[i], 1.0);
        double tail;
        double bn = bn_parseval (sf_step, &s, 0.0, 1.0,
                                 (size_t)(400.0 / bn_t[i]) + 2000, &tail);
        DP_CHECK (tail < 1e-9);
        DP_CHECK_NEAR (bn / bn_t[i] - 1.0, 0.0, 2e-7);
      }
    /* bn and t enter only as the product: the same bn*t at another t. */
    dp_state_filter_state_t u;
    fresh (&s, 0.02, 2.0);
    fresh (&u, 0.04, 1.0);
    DP_CHECK_NEAR (s.kp, u.kp, 1e-15);
    DP_CHECK_NEAR (s.ki, u.ki, 1e-15);
    DP_CHECK_NEAR (s.kr, u.kr, 1e-15);
  }

  /* 3b ── The solve never saturates ────────────────────────────────────────
   * The gains are the root of a monotone equation bracketed by [0.5, 1.01]
   * times the prototype's starting scale. A bisection that ran to an end of
   * its bracket would still return a number, and a wrong one, so walk the
   * whole domain and require the root strictly inside, and the loop the
   * delivered bandwidth asked for. kr = th^3 gives th back, and
   * the starting scale is bn*t over the prototype's Bn/w0. */
  {
    const double        a = STATE_FILTER_SHAPE_A, b = STATE_FILTER_SHAPE_B;
    const double        c    = (a * b * b + a * a - b) / (4.0 * (a * b - 1.0));
    static const double ts[] = { 0.25, 1.0, 7.0 };
    int                 n_cells  = 0;
    double              worst_bw = 0.0, min_pos = 1.0, max_pos = 0.0;
    double              worst_exact = 0.0, worst_slow = 0.0, worst_gain = 0.0;
    for (size_t ti = 0; ti < sizeof ts / sizeof *ts; ti++)
      for (int i = 0; i <= 400; i++)
        {
          /* from just inside the floor to just inside the bound: bn = B/t
           * times t does not return B exactly, so the ends are nudged in */
          const double lo_b = STATE_FILTER_BNT_MIN * 1.001;
          const double hi_b = STATE_FILTER_BNT_MAX * 0.999;
          const double B    = lo_b * pow (hi_b / lo_b, (double)i / 400.0);
          dp_state_filter_state_t sf;
          memset (&sf, 0, sizeof sf);
          dp_state_filter_state_t *made
              = dp_state_filter_create (B / ts[ti], ts[ti]);
          DP_REQUIRE (made != NULL);
          sf = *made;
          dp_state_filter_destroy (made);
          DP_CHECK (dp_state_filter_state_ok (&sf));
          const double th  = cbrt (sf.kr);
          const double pos = th / (B / c); /* 0.5 .. 1.01 is the bracket */
          if (pos < min_pos)
            min_pos = pos;
          if (pos > max_pos)
            max_pos = pos;
          /* the gains against the long double solve (see gains_ld) */
          {
            long double rkp, rki, rkr;
            gains_ld (sf.bn * sf.t, &rkp, &rki, &rkr);
            const double d1 = (double)fabsl ((sf.kp - rkp) / rkp);
            const double d2 = (double)fabsl ((sf.ki - rki) / rki);
            const double d3 = (double)fabsl ((sf.kr - rkr) / rkr);
            if (d1 > worst_gain)
              worst_gain = d1;
            if (d2 > worst_gain)
              worst_gain = d2;
            if (d3 > worst_gain)
              worst_gain = d3;
          }
          /* the delivered bandwidth, from the independent closed form */
          const double bw = bn3_closed (sf.kp, sf.ki, sf.kr);
          if (fabs (bw / B - 1.0) > worst_bw)
            worst_bw = fabs (bw / B - 1.0);
          /* the exact function (a linear solve on the loop's own state
           * space) against the bandwidth asked for, down to bn*t = 1e-6;
           * below that it is not claimed */
          if (B >= 1e-6)
            {
              const double ex = dp_state_filter_loop_bandwidth (
                  3, sf.kp, sf.ki, sf.kr, 0.0, 1.0);
              const double e = fabs (ex / B - 1.0);
              if (e > worst_exact)
                worst_exact = e;
              if (B < 1e-4 && e > worst_slow)
                worst_slow = e;
            }
          n_cells++;
        }
    printf ("  exact function vs bn*t: worst %.2e over bn*t >= 1e-6, "
            "%.2e below 1e-4\n",
            worst_exact, worst_slow);
    printf ("  gains vs a long double solve: worst relative difference %.2e "
            "(long double is %d bits), the restore tolerance is 1e-9\n",
            worst_gain, LDBL_MANT_DIG);
    /* The gains match a long double solve to 1e-12 (measured 1.6e-15 on
     * x86-64, where long double is 64 bits). Where long double is wider than
     * double (x86-64 Linux 80-bit, arm64 Linux 128-bit) a pass is a bound on
     * that platform's gains against a wider reference, so two such platforms
     * agree within 2e-12 and the restore tolerance of 1e-9 is 500 times that.
     * Where it is not (macOS, Windows) the reference is the same arithmetic
     * written twice and this says nothing about the platform's spread. The
     * message names the width so a red leg says which case it was. */
    {
      char msg[128];
      snprintf (msg, sizeof msg,
                "gains within 1e-12 of a long double solve (long double is "
                "%d bits, double is %d)",
                LDBL_MANT_DIG, DBL_MANT_DIG);
      DP_CHECK_MSG (worst_gain < 1e-12, msg);
    }
    DP_CHECK (n_cells == 1203);
    DP_CHECK (worst_exact < 1e-9);      /* the exact function, down to 1e-6 */
    DP_CHECK (min_pos > 0.5 * 1.0001);  /* not at the lower end */
    DP_CHECK (max_pos < 1.01 * 0.9999); /* not at the upper end */
    DP_CHECK (worst_bw < 1e-11);        /* a bandwidth, to the last digits */
  }

  /* 4 ── reset, configure, and a refusal that changes nothing ───────────── */
  {
    dp_state_filter_state_t *sf = dp_state_filter_create (0.02, 1.0);
    DP_REQUIRE (sf != NULL);
    for (int i = 0; i < 10; i++)
      dp_state_filter_step (sf, 1.0);
    DP_CHECK (sf->integ != 0.0 && sf->rate != 0.0);
    const double kp = sf->kp, ki = sf->ki, kr = sf->kr;
    dp_state_filter_reset (sf);
    DP_CHECK (sf->integ == 0.0 && sf->rate == 0.0); /* BOTH accumulators */
    DP_CHECK (sf->kp == kp && sf->ki == ki && sf->kr == kr); /* gains kept */

    for (int i = 0; i < 10; i++)
      dp_state_filter_step (sf, 1.0);
    const double integ = sf->integ, rate = sf->rate;
    DP_CHECK (dp_state_filter_configure (sf, 0.05, 1.0) == DP_OK);
    DP_CHECK (sf->integ == integ && sf->rate == rate); /* lock preserved */
    DP_CHECK (sf->bn == 0.05 && sf->kp > kp && sf->kr > kr);

    dp_state_filter_state_t before = *sf;
    DP_CHECK (dp_state_filter_configure (sf, -1.0, 1.0) == DP_ERR_INVALID);
    DP_CHECK (dp_state_filter_configure (sf, 0.05, 0.0) == DP_ERR_INVALID);
    DP_CHECK (dp_state_filter_configure (sf, NAN, 1.0) == DP_ERR_INVALID);
    DP_CHECK (dp_state_filter_configure (sf, 1.0, 1.0) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&before, sf, sizeof before) == 0); /* nothing moved */
    dp_state_filter_destroy (sf);
  }

  /* 5 ── steps() == repeated step(), and aliasing ───────────────────────── */
  {
    enum
    {
      N = 97
    };
    double   x[N], y[N], z[N];
    uint32_t rng = 0x1234u;
    for (int i = 0; i < N; i++)
      x[i] = dp_uni (&rng) - 0.5;
    dp_state_filter_state_t a, b;
    fresh (&a, 0.04, 1.0);
    fresh (&b, 0.04, 1.0);
    for (int i = 0; i < N; i++)
      y[i] = dp_state_filter_step (&a, x[i]);
    dp_state_filter_steps (&b, x, z, 40); /* two calls: carried across */
    dp_state_filter_steps (&b, x + 40, z + 40, N - 40);
    for (int i = 0; i < N; i++)
      DP_CHECK_NEAR (y[i], z[i], 1e-15);
    DP_CHECK_NEAR (a.integ, b.integ, 1e-14);
    DP_CHECK_NEAR (a.rate, b.rate, 1e-15);
    fresh (&b, 0.04, 1.0);
    memcpy (z, x, sizeof x);
    dp_state_filter_steps (&b, z, z, N); /* out aliases x */
    for (int i = 0; i < N; i++)
      DP_CHECK_NEAR (y[i], z[i], 1e-15);
  }

  /* 5b ── Chunk invariance: the output is a function of the input stream,
   * not of how it was split into calls (every partition, bit for bit). */
  {
    enum
    {
      CN = 2000
    };
    static double x[CN];
    uint32_t      rng = 0xABCDu;
    for (int i = 0; i < CN; i++)
      x[i] = dp_uni (&rng) - 0.5;
    dp_ci_spec_t spec = {
      .name     = "state_filter steps",
      .create   = sf_create_obj,
      .destroy  = sf_destroy_obj,
      .process  = sf_process,
      .in_size  = sizeof (double),
      .out_size = sizeof (double),
      .out_cap  = CN,
    };
    DP_CHECK (dp_chunk_invariance (&spec, x, CN) == 0);
  }

  /* 6 ── The exact function ─────────────────────────────────────────────── */
  {
    /* (a) The closed forms. Order 2 at zero delay and unit gain. */
    ref2_t lf;
    memset (&lf, 0, sizeof lf);
    static const double z2[] = { 0.5, 0.707, 1.0, 2.0 };
    for (size_t i = 0; i < 4; i++)
      for (double bn = 0.002; bn <= 0.2; bn *= 3.0)
        {
          ref2_init (&lf, bn, z2[i], 1.0);
          double exact = dp_state_filter_loop_bandwidth (2, lf.kp, lf.ki, 0.0,
                                                         0.0, 1.0);
          DP_CHECK_NEAR (exact / bn2_closed (lf.kp, lf.ki) - 1.0, 0.0, 1e-7);
          /* The same loop through the local second-order step (lf_step is
           * ref2_step, a deliberate copy; see ref2_t above). */
          double tail;
          double sim = bn_parseval (lf_step, &lf, 0.0, 1.0,
                                    (size_t)(1500.0 / bn) + 4000, &tail);
          DP_CHECK_NEAR (sim / exact - 1.0, 0.0, 1e-7);
        }
    /* The closed form reduces to the first-order loop's kp/(2(2 - kp)) as
     * ki goes to 0 (at ki = 0 itself the integrator's pole sits on z = 1
     * and cancels in H, so the loop has no internal stability to report). */
    DP_CHECK_NEAR (dp_state_filter_loop_bandwidth (2, 0.3, 1e-3, 0.0, 0.0, 1.0)
                           / (0.3 / (2.0 * (2.0 - 0.3)))
                       - 1.0,
                   0.0, 2e-2);
    DP_CHECK (!dp_state_filter_loop_stable (2, 0.3, 0.0, 0.0, 0.0, 1.0));

    /* (b) Order 3 on random gains: the closed form, and a Jury oracle for
     * stability. Gains are drawn uniformly from kp in (0, 2.2),
     * ki in (0, 1.2), kr in (0, 0.4): a box that holds both stable and
     * unstable loops (about 60% stable). */
    uint32_t rng      = 0xC0FFEEu;
    int      n_stable = 0, n_agree = 0, n_total = 20000;
    double   worst = 0.0;
    for (int i = 0; i < n_total; i++)
      {
        const double u1 = dp_uni (&rng);
        const double u2 = dp_uni (&rng);
        const double u3 = dp_uni (&rng);
        const double kp = 2.2 * u1, ki = 1.2 * u2, kr = 0.4 * u3;
        const int    j = jury3 (kp, ki, kr);
        const int    g = dp_state_filter_loop_stable (3, kp, ki, kr, 0.0, 1.0);
        n_agree += (j == g);
        if (j)
          {
            n_stable++;
            double ex
                = dp_state_filter_loop_bandwidth (3, kp, ki, kr, 0.0, 1.0);
            double cf = bn3_closed (kp, ki, kr);
            if (fabs (ex / cf - 1.0) > worst)
              worst = fabs (ex / cf - 1.0);
          }
        else
          DP_CHECK (isnan (
              dp_state_filter_loop_bandwidth (3, kp, ki, kr, 0.0, 1.0)));
      }
    DP_CHECK (n_stable > n_total / 4 && n_stable < n_total * 9 / 10);
    DP_CHECK (n_agree == n_total); /* the same set of loops, every one */
    DP_CHECK (worst < 1e-7); /* the same bandwidth, on every stable one */

    /* (c) Delays and gains against simulation of the real step: integer
     * delays are real transport delays; the fraction is the interpolation
     * model of the header. */
    static const double delays[] = { 0.0, 1.0, 2.0, 0.5, 1.25, 3.0 };
    /* 0.1 is under the lower gain edge (about 0.25 of nominal) and 60 is
     * over the upper one at every delay here, so the grid holds unstable
     * loops too: without them a `stable` that always said 1 would pass. */
    static const double     gains[] = { 1.0, 0.7, 1.6, 0.1, 60.0 };
    dp_state_filter_state_t s;
    size_t                  decided = 0, decided_unstable = 0;
    for (size_t di = 0; di < sizeof delays / sizeof *delays; di++)
      for (size_t gi = 0; gi < sizeof gains / sizeof *gains; gi++)
        {
          fresh (&s, 0.03, 1.0);
          const double model
              = dp_state_filter_bandwidth (&s, delays[di], gains[gi]);
          const int stable
              = dp_state_filter_stable (&s, delays[di], gains[gi]);
          const int sim = sim_stable (sf_step, &s, delays[di], gains[gi]);
          fresh (&s, 0.03, 1.0);
          /* a simulation that cannot decide (sim == 0) says nothing, so
           * the cases it DID decide are counted below */
          DP_CHECK (stable == (sim > 0) || sim == 0);
          if (sim != 0)
            {
              decided++;
              decided_unstable += sim < 0;
            }
          if (stable)
            {
              double tail;
              double meas = bn_parseval (sf_step, &s, delays[di], gains[gi],
                                         60000, &tail);
              DP_CHECK (tail < 1e-9);
              DP_CHECK_NEAR (meas / model - 1.0, 0.0, 1e-7);
            }
          else
            DP_CHECK (isnan (model));
        }
    printf (
        "  delays x gains: %zu of 30 decided by the simulation, %zu of them "
        "unstable\n",
        decided, decided_unstable);
    /* every cell is decided, and the two out-of-range gains are unstable at
     * every one of the six delays (measured: 30 and 12) */
    DP_CHECK (decided == 30);
    DP_CHECK (decided_unstable == 12);
    /* A delay lengthens the loop and widens its bandwidth, in the order
     * of the delay (this loop at bn*t = 0.03). */
    fresh (&s, 0.03, 1.0);
    DP_CHECK (dp_state_filter_bandwidth (&s, 1.0, 1.0)
              > dp_state_filter_bandwidth (&s, 0.0, 1.0));
    DP_CHECK (dp_state_filter_bandwidth (&s, 2.0, 1.0)
              > dp_state_filter_bandwidth (&s, 1.0, 1.0));

    /* (d) The stability oracle at integer delays: a simulated impulse
     * response, on random gains, against the exact test. Cases the
     * simulation cannot call (too slow to die) are skipped, and most are
     * called. */
    rng        = 0xBEEF01u;
    int called = 0, wrong = 0, tried = 0;
    for (int i = 0; i < 400; i++)
      {
        const double u1 = dp_uni (&rng);
        const double u2 = dp_uni (&rng);
        const double u3 = dp_uni (&rng);
        const double kp = 0.02 + 0.6 * u1, ki = 0.0005 + 0.05 * u2,
                     kr = 1e-5 + 2e-3 * u3;
        const int    d  = 1 + (int)(2.0 * dp_uni (&rng)); /* 1 or 2 */
        dp_state_filter_state_t u   = { kp, ki, kr, 0.0, 0.0, 0.0, 1.0 };
        const int               sim = sim_stable (sf_step, &u, (double)d, 1.0);
        tried++;
        if (sim == 0)
          continue;
        called++;
        if ((sim > 0) != dp_state_filter_loop_stable (3, kp, ki, kr, d, 1.0))
          wrong++;
      }
    DP_CHECK (called > tried / 2);
    DP_CHECK (wrong == 0);

    /* Refusals: an argument outside its domain is not stable and has no
     * bandwidth. */
    DP_CHECK (!dp_state_filter_loop_stable (4, 0.1, 0.01, 0.001, 0.0, 1.0));
    DP_CHECK (!dp_state_filter_loop_stable (3, 0.1, 0.01, 0.001, -1.0, 1.0));
    DP_CHECK (!dp_state_filter_loop_stable (
        3, 0.1, 0.01, 0.001, STATE_FILTER_DELAY_LIMIT + 0.5, 1.0));
    DP_CHECK (!dp_state_filter_loop_stable (3, 0.1, 0.01, 0.001, 0.0, 0.0));
    DP_CHECK (!dp_state_filter_loop_stable (3, NAN, 0.01, 0.001, 0.0, 1.0));
    DP_CHECK (isnan (
        dp_state_filter_loop_bandwidth (3, 0.1, 0.01, 0.001, 0.0, -1.0)));
  }

  /* 7 ── The stability rectangle ────────────────────────────────────────── */
  {
    /* The bound, DERIVED from the exact function: the largest bn*t whose
     * loop is stable at the upper corner. STATE_FILTER_BNT_MAX is that,
     * rounded down, and tight (within 5%). */
    double lo = 0.01, hi = 0.5;
    for (int i = 0; i < 60; i++)
      {
        const double            mid = 0.5 * (lo + hi);
        dp_state_filter_state_t s;
        fresh (&s, mid, 1.0);
        if (dp_state_filter_stable (&s, STATE_FILTER_DELAY_MAX,
                                    STATE_FILTER_K_MAX))
          lo = mid;
        else
          hi = mid;
      }
    const double derived = lo;
    DP_CHECK (STATE_FILTER_BNT_MAX <= derived);
    DP_CHECK (STATE_FILTER_BNT_MAX >= 0.95 * derived);
    {
      /* ...and a loop past it is NOT stable at the corner. */
      dp_state_filter_state_t s;
      fresh (&s, 1.1 * derived, 1.0);
      DP_CHECK (!dp_state_filter_stable (&s, STATE_FILTER_DELAY_MAX,
                                         STATE_FILTER_K_MAX));
    }

    /* The grid at the bound: every delay in [0, DELAY_MAX] in steps of
     * 0.05 and the gains the rectangle names are stable, and along K the
     * stable set is ONE interval at each delay (counted, not assumed). */
    dp_state_filter_state_t s;
    fresh (&s, STATE_FILTER_BNT_MAX, 1.0);
    int unstable_in_rect = 0, max_changes = 0;
    for (int di = 0; di <= 40; di++)
      {
        const double        d = STATE_FILTER_DELAY_MAX * di / 40.0;
        static const double ks[]
            = { STATE_FILTER_K_MIN, 0.75, 1.0, 1.5, STATE_FILTER_K_MAX };
        for (size_t ki = 0; ki < 5; ki++)
          unstable_in_rect += !dp_state_filter_stable (&s, d, ks[ki]);
        int prev = -1, changes = 0;
        for (int j = 0; j <= 300; j++)
          {
            const double K  = 0.1 * pow (300.0, (double)j / 300.0);
            const int    st = dp_state_filter_stable (&s, d, K);
            if (prev >= 0 && st != prev)
              changes++;
            prev = st;
          }
        if (changes > max_changes)
          max_changes = changes;
      }
    DP_CHECK (unstable_in_rect == 0);
    DP_CHECK (max_changes <= 2); /* in at the lower edge, out at the upper */

    /* The claim K_MIN makes: at the bound, at every delay up to DELAY_MAX,
     * the measured lower edge is under K_MIN. The margin is printed. */
    {
      fresh (&s, STATE_FILTER_BNT_MAX, 1.0);
      double worst_low = 0.0;
      for (int di = 0; di <= 8; di++)
        {
          const double d  = STATE_FILTER_DELAY_MAX * di / 8.0;
          double       lo = 0.001, hi = 1.0;
          DP_REQUIRE (dp_state_filter_stable (&s, d, 1.0));
          for (int i = 0; i < 50; i++)
            {
              const double mid = sqrt (lo * hi);
              if (dp_state_filter_stable (&s, d, mid))
                hi = mid;
              else
                lo = mid;
            }
          if (hi > worst_low)
            worst_low = hi;
        }
      DP_CHECK (worst_low < STATE_FILTER_K_MIN);
      printf ("  K_low at the bound: worst %.3f over delay 0..%g, K_MIN %.2f, "
              "margin %.1f dB\n",
              worst_low, STATE_FILTER_DELAY_MAX, STATE_FILTER_K_MIN,
              20.0 * log10 (STATE_FILTER_K_MIN / worst_low));
    }

    /* The lower edge against the analytic one: for a slow loop the Routh
     * condition on the analog prototype gives K > 1/(a*b), and the
     * sampled loop's edge is within a few percent of it. */
    fresh (&s, 0.001, 1.0);
    double klo = 0.01, khi = 1.0;
    for (int i = 0; i < 50; i++)
      {
        const double mid = sqrt (klo * khi);
        if (dp_state_filter_stable (&s, 0.0, mid))
          khi = mid;
        else
          klo = mid;
      }
    DP_CHECK_NEAR (khi * STATE_FILTER_SHAPE_A * STATE_FILTER_SHAPE_B, 1.0,
                   0.03);
    /* A loop with a detector slope below that edge is unstable, and the
     * object cannot tell: it still steps. */
    DP_CHECK (!dp_state_filter_stable (&s, 0.0, 0.8 * khi));
  }

  /* 8 ── Steady-state errors ────────────────────────────────────────────── */
  {
    /* Four loops, each driven by a polynomial input until it has settled:
     * a frequency ramp (the frequency changes by r every update) into
     * order 2 and order 3, and a frequency acceleration (the ramp changes
     * by x every update) into order 3 and order 2. The phase error is read
     * at the end. */
    enum
    {
      SETTLE = 60000
    };
    const double            r = 1e-6, x = 1e-9;
    ref2_t                  lf_ramp, lf_acc;
    dp_state_filter_state_t sf_ramp, sf_acc;
    memset (&lf_ramp, 0, sizeof lf_ramp);
    memset (&lf_acc, 0, sizeof lf_acc);
    ref2_init (&lf_ramp, 0.05, 0.707, 1.0);
    ref2_init (&lf_acc, 0.05, 0.707, 1.0);
    fresh (&sf_ramp, 0.05, 1.0);
    fresh (&sf_acc, 0.05, 1.0);

    double w_r = 0, f_r = 0;          /* the ramp input             */
    double w_a = 0, f_a = 0, a_a = 0; /* the acceleration input     */
    double p_lr = 0, p_sr = 0, p_la = 0, p_sa = 0;
    double e_lr = 0, e_sr = 0, e_la = 0, e_la_prev = 0, e_sa = 0;
    for (int n = 0; n < SETTLE; n++)
      {
        f_r += r;
        w_r += f_r;
        e_lr = w_r - p_lr;
        p_lr += ref2_step (&lf_ramp, e_lr);
        e_sr = w_r - p_sr;
        p_sr += dp_state_filter_step (&sf_ramp, e_sr);

        a_a += x;
        f_a += a_a;
        w_a += f_a;
        e_la_prev = e_la;
        e_la      = w_a - p_la;
        p_la += ref2_step (&lf_acc, e_la);
        e_sa = w_a - p_sa;
        p_sa += dp_state_filter_step (&sf_acc, e_sa);
      }
    /* Order 2 under a ramp: r/ki, the exact discrete form. */
    DP_CHECK_NEAR (
        e_lr / dp_state_filter_steady_error (2, lf_ramp.ki, 0.0, 2, r) - 1.0,
        0.0, 1e-5);
    /* Order 3 under a ramp: nulled. */
    DP_CHECK (dp_state_filter_steady_error (3, 0.0, sf_ramp.kr, 2, r) == 0.0);
    DP_CHECK (fabs (e_sr) < 1e-10);
    /* Order 3 under an acceleration: x/kr. */
    DP_CHECK_NEAR (
        e_sa / dp_state_filter_steady_error (3, 0.0, sf_acc.kr, 3, x) - 1.0,
        0.0, 1e-4);
    /* Order 2 under an acceleration does not settle: its error grows by
     * x/ki every update. */
    DP_CHECK_NEAR ((e_la - e_la_prev) / (x / lf_acc.ki) - 1.0, 0.0, 1e-3);
    DP_CHECK (isinf (dp_state_filter_steady_error (2, lf_acc.ki, 0.0, 3, x)));
    /* A step in either order, and the unknowns. */
    DP_CHECK (dp_state_filter_steady_error (2, 0.1, 0.0, 1, 0.3) == 0.0);
    DP_CHECK (dp_state_filter_steady_error (3, 0.1, 0.01, 1, 0.3) == 0.0);
    DP_CHECK (isnan (dp_state_filter_steady_error (4, 0.1, 0.01, 2, 0.3)));
    DP_CHECK (isnan (dp_state_filter_steady_error (3, 0.1, 0.01, 4, 0.3)));
    /* A frozen loop does not null a ramp. */
    DP_CHECK (isinf (dp_state_filter_steady_error (2, 0.0, 0.0, 2, r)));
    DP_CHECK (isinf (dp_state_filter_steady_error (3, 0.0, 0.0, 3, x)));
  }

  /* 9 ── Serialized state ───────────────────────────────────────────────── */
  {
    dp_state_filter_state_t *a = dp_state_filter_create (0.03, 1.0);
    dp_state_filter_state_t *b = dp_state_filter_create (0.03, 1.0);
    DP_REQUIRE (a && b);
    for (int i = 0; i < 25; i++)
      dp_state_filter_step (a, 0.2 - 0.01 * i);
    DP_STATE_ROUNDTRIP_TEST (dp_state_filter, a, b);

    /* A mid-stream split resumes bit for bit: the unsplit run and the
     * restored one agree on every later control value. Both accumulators
     * are live, so a field left out of the blob shows here. */
    dp_state_filter_state_t *c = dp_state_filter_create (0.03, 1.0);
    DP_REQUIRE (c != NULL);
    void *blob = malloc (dp_state_filter_state_bytes (a));
    dp_state_filter_get_state (a, blob);
    DP_CHECK (dp_state_filter_set_state (c, blob) == DP_OK);
    DP_CHECK (c->integ == a->integ && c->rate == a->rate);
    for (int i = 0; i < 30; i++)
      {
        const double e = 0.1 * sin (0.3 * i);
        DP_CHECK (dp_state_filter_step (a, e) == dp_state_filter_step (c, e));
      }

    /* The flavor word is a reject key: a blob of another flavor is
     * refused, and the filter is left as it was. */
    dp_state_filter_get_state (a, blob);
    unsigned char *p   = blob;
    uint32_t       one = 1u;
    memcpy (p + sizeof (dp_state_hdr_t), &one, sizeof one);
    dp_state_filter_state_t keep = *c;
    DP_CHECK (dp_state_filter_set_state (c, blob) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);

    /* The pad word is written 0 and refused otherwise. */
    dp_state_filter_get_state (a, blob);
    memcpy (p + sizeof (dp_state_hdr_t) + sizeof (uint32_t), &one, sizeof one);
    keep = *c;
    DP_CHECK (dp_state_filter_set_state (c, blob) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);

    /* Forged payloads: a gain that is not the one bn and t give, a NaN
     * gain, a bn the domain refuses, a bn that disagrees with the gains.
     * Each is refused and leaves the filter unchanged. */
    const size_t off = sizeof (dp_state_hdr_t) + 2 * sizeof (uint32_t);
    for (int field = 0; field < 7; field++)
      {
        if (field == 3 || field == 4)
          continue; /* integ, rate: not checked */
        dp_state_filter_get_state (a, blob);
        double v;
        memcpy (&v, p + off + field * sizeof v, sizeof v);
        v = field == 0 ? NAN : v * 1.001;
        if (field == 5)
          v = a->bn * 3.0; /* bn the gains no longer belong to */
        if (field == 6)
          v = a->t * 2.0;
        memcpy (p + off + field * sizeof v, &v, sizeof v);
        keep = *c;
        DP_CHECK (dp_state_filter_set_state (c, blob) == DP_ERR_INVALID);
        DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);
      }
    /* The gain tolerance of dp_state_filter_state_ok (1e-9 relative, header):
     * the forged-gain test above perturbs by 1e-3, which a tolerance of 1e-4
     * also refuses. Here each gain is moved by 1e-10 (accepted) and by 1e-8
     * (refused), a decade either side of the tolerance, so a tolerance of
     * 1e-4 or of 1e-12 fails one of the two. */
    {
      dp_state_filter_state_t g;
      memset (&g, 0, sizeof g);
      dp_state_filter_init (&g, 0.02, 1.0);
      DP_CHECK (dp_state_filter_state_ok (&g));
      for (int f = 0; f < 3; f++)
        for (int sgn = -1; sgn <= 1; sgn += 2)
          {
            dp_state_filter_state_t inside = g, outside = g;
            double                 *pn = f == 0   ? &inside.kp
                                         : f == 1 ? &inside.ki
                                                  : &inside.kr;
            double                 *pf = f == 0   ? &outside.kp
                                         : f == 1 ? &outside.ki
                                                  : &outside.kr;
            *pn *= 1.0 + sgn * 1e-10;
            *pf *= 1.0 + sgn * 1e-8;
            DP_CHECK (dp_state_filter_state_ok (&inside));
            DP_CHECK (!dp_state_filter_state_ok (&outside));
          }
    }

    /* bn outside the domain, with gains consistent with nothing. */
    dp_state_filter_get_state (a, blob);
    double bad = 5.0;
    memcpy (p + off + 5 * sizeof bad, &bad, sizeof bad);
    DP_CHECK (dp_state_filter_set_state (c, blob) == DP_ERR_INVALID);
    /* ...and bn outside the domain with gains that DO match it (init is
     * unguarded, so it writes them): only the domain clause refuses it. */
    {
      dp_state_filter_state_t wide;
      memset (&wide, 0, sizeof wide);
      dp_state_filter_init (&wide, 0.3, 1.0); /* bn*t = 0.3 > the bound */
      /* the header's claim about unvalidated init: far past the bound the
       * corner (K_MAX, DELAY_MAX) is unstable, and just past the constant
       * (0.121, under the exact bound 0.1248) it is still stable, which is
       * why the header makes no claim there */
      DP_CHECK (!dp_state_filter_stable (&wide, STATE_FILTER_DELAY_MAX,
                                         STATE_FILTER_K_MAX));
      {
        dp_state_filter_state_t just;
        memset (&just, 0, sizeof just);
        dp_state_filter_init (&just, 0.121, 1.0);
        DP_CHECK (!dp_state_filter_params_ok (just.bn, just.t));
        DP_CHECK (dp_state_filter_stable (&just, STATE_FILTER_DELAY_MAX,
                                          STATE_FILTER_K_MAX));
      }
      DP_CHECK (!dp_state_filter_params_ok (wide.bn, wide.t));
      void *wblob = malloc (dp_state_filter_state_bytes (&wide));
      dp_state_filter_get_state (&wide, wblob);
      keep = *c;
      DP_CHECK (dp_state_filter_set_state (c, wblob) == DP_ERR_INVALID);
      DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);
      free (wblob);
    }
    /* The estimates are not checked: garbage in. */
    dp_state_filter_get_state (a, blob);
    bad = -123.25;
    memcpy (p + off + 3 * sizeof bad, &bad, sizeof bad);
    DP_CHECK (dp_state_filter_set_state (c, blob) == DP_OK);
    DP_CHECK (c->integ == -123.25);
    free (blob);
    dp_state_filter_destroy (a);
    dp_state_filter_destroy (b);
    dp_state_filter_destroy (c);
  }

  /* 10 ── The exact test OFF the design manifold ─────────────────────────
   * The verified range is the design gains; the Kalman flavor's gains move
   * through a region around them while it converges. The exact stability
   * answer must be right there too: every case below is judged by the kick
   * oracle (its range of validity is stated at kick_stable). */
  {
    /* The triples that first showed the defect: a slow loop's mid-transient
     * gains (bn*t 1e-3, the Kalman flavor started at three times its steady
     * frequency uncertainty), at the corner K = 2, delay 2, where the loop is
     * stable with spectral radius 0.99987 and the Lyapunov solve, in raw
     * coordinates, answered "unstable" (erratically: a nearby start did not).
     * Updates 388, 512, 515, 524, 550, 557 and 612 of that trajectory. */
    static const double flagged[7][3] = {
      { 0.0040427064495209501, 8.7237268126087663e-06,
        1.0568866752876651e-09 },
      { 0.0040151105031033963, 7.4561551963688206e-06,
        7.9370762158173476e-10 },
      { 0.0040113424237020511, 7.4229549580066918e-06, 7.88676033818813e-10 },
      { 0.0039994075526293129, 7.3233865450444674e-06,
        7.7397653903099001e-10 },
      { 0.003960124747635187, 7.0372191359912376e-06, 7.3482661762989766e-10 },
      { 0.0039484649476650528, 6.9608495723606058e-06,
        7.2512074287215385e-10 },
      { 0.0038445887355428735, 6.3779814119948971e-06,
        6.6081594377708587e-10 },
    };
    for (int i = 0; i < 7; i++)
      {
        DP_CHECK (kick_stable (flagged[i][0], flagged[i][1], flagged[i][2], 2,
                               2.0, 400000)
                  == 1);
        DP_CHECK (dp_state_filter_loop_stable (3, flagged[i][0], flagged[i][1],
                                               flagged[i][2], 2.0, 2.0));
      }

    /* A comparison OFF the manifold, over the gains of the Kalman recursion's
     * trajectory: for each bn*t (the noise figure q is bisected so the steady
     * state has that exact bandwidth) and each start (the frequency
     * variance scaled by the square of the start), the gains at twelve
     * updates spread over 8/(bn*t), at the four corners K in {0.5, 2} x delay
     * in {0, 2}. A case the oracle cannot decide in its horizon is counted,
     * not scored. */
    static const double bnts[4]   = { 1e-3, 0.01, 0.04, 0.08 };
    static const double starts[5] = { 1.0, 2.0, 3.0, 4.0, 5.0 };
    int cases = 0, undecided = 0, stable_n = 0, disagree = 0, nonpos = 0;
    for (int bi = 0; bi < 4; bi++)
      {
        double lo = -60.0, hi = 0.0; /* log q */
        for (int it = 0; it < 60; it++)
          {
            const double mid = 0.5 * (lo + hi);
            double       P[3][3];
            cov_steady (P, exp (mid));
            double kp, ki, kr;
            cov_gains (P, &kp, &ki, &kr);
            const double bn
                = dp_state_filter_loop_bandwidth (3, kp, ki, kr, 0.0, 1.0);
            if (bn < bnts[bi])
              lo = mid;
            else
              hi = mid;
          }
        const double q = exp (0.5 * (lo + hi));
        for (int si = 0; si < 5; si++)
          {
            double P[3][3];
            cov_steady (P, q);
            P[1][1] *= starts[si] * starts[si];
            const size_t n_up = (size_t)(8.0 / bnts[bi]);
            size_t       next = 1;
            for (size_t n = 0; n < n_up; n++)
              {
                if (n == next)
                  {
                    next = next + next / 3 + 1;
                    double kp, ki, kr;
                    cov_gains (P, &kp, &ki, &kr);
                    /* a transient gain at or below zero is outside the
                     * domain the oracle can judge (a double integrator with
                     * a non-positive gain is unstable by an amount no finite
                     * run sees), and the exact test answers it correctly:
                     * counted, not scored */
                    if (!(kp > 0.0 && ki > 0.0 && kr > 0.0))
                      {
                        nonpos++;
                        cov_advance (P, q);
                        continue;
                      }
                    static const double Ks[2] = { 0.5, 2.0 };
                    for (int a = 0; a < 2; a++)
                      for (int dd = 0; dd <= 2; dd += 2)
                        {
                          const int ora
                              = kick_stable (kp, ki, kr, dd, Ks[a],
                                             (size_t)(400.0 / bnts[bi]));
                          cases++;
                          if (ora < 0)
                            {
                              undecided++;
                              continue;
                            }
                          const int ex = dp_state_filter_loop_stable (
                              3, kp, ki, kr, dd, Ks[a]);
                          stable_n += ora;
                          if (ex != ora)
                            {
                              disagree++;
                              printf ("  off-manifold disagree: bn*t %g start "
                                      "%g update %zu kp %.6g ki %.6g kr %.6g "
                                      "K %g d %d: oracle %d exact %d\n",
                                      bnts[bi], starts[si], n, kp, ki, kr,
                                      Ks[a], dd, ora, ex);
                            }
                        }
                  }
                cov_advance (P, q);
              }
          }
      }
    printf ("  off-manifold: %d cases, %d stable by the oracle, %d "
            "undecided and %d samples with a non-positive gain (not scored), "
            "%d disagreements\n",
            cases, stable_n, undecided, nonpos, disagree);
    DP_CHECK (disagree == 0);
    DP_CHECK (cases - undecided >= 500);
  }

  /* 11 ── The decision rule: loops built at a known radius ────────────────
   * Gains for the delay-free order-3 loop whose characteristic polynomial is
   * (z - r)(z - 0.9)(z - 0.8), from matching coefficients with
   * z^3 + (kp+ki+kr-3) z^2 + (3-2kp-ki) z + (kp-1). The loop has a pole at
   * exactly r: just inside the unit circle reads stable, just outside reads
   * not stable, at 1e-6 and 1e-9, and a wildly unstable loop returns 0
   * without NaN. */
  {
    static const double rs[6]
        = { 1.0 - 1e-6, 1.0 - 1e-9, 1.0 + 1e-9, 1.0 + 1e-6, 1.5, 1e3 };
    static const int want[6] = { 1, 1, 0, 0, 0, 0 };
    for (int i = 0; i < 6; i++)
      {
        const double r  = rs[i];
        const double a1 = -(r + 1.7), a2 = r * 1.7 + 0.72, a3 = -r * 0.72;
        const double kp = a3 + 1.0, ki = 3.0 - 2.0 * kp - a2,
                     kr = a1 + 3.0 - kp - ki;
        const int ex = dp_state_filter_loop_stable (3, kp, ki, kr, 0.0, 1.0);
        if (ex != want[i])
          printf ("  radius %.12g: stable %d, wanted %d\n", r, ex, want[i]);
        DP_CHECK (ex == want[i]);
        if (want[i] == 0)
          DP_CHECK (isnan (
              dp_state_filter_loop_bandwidth (3, kp, ki, kr, 0.0, 1.0)));
        else
          DP_CHECK (isfinite (
              dp_state_filter_loop_bandwidth (3, kp, ki, kr, 0.0, 1.0)));
      }
    DP_CHECK (!dp_state_filter_loop_stable (3, 1e3, 1e3, 1e3, 2.0, 2.0));
    /* gains so large that the first squaring overflows: not stable, and
     * the bandwidth is NaN rather than a number made of infinities */
    DP_CHECK (!dp_state_filter_loop_stable (3, 1e200, 1e200, 1e200, 0.0, 1.0));
    DP_CHECK (isnan (
        dp_state_filter_loop_bandwidth (3, 1e200, 1e200, 1e200, 0.0, 1.0)));
    DP_CHECK (
        !dp_state_filter_loop_stable (3, 1e100, 1e100, 1e100, 4.0, 1e100));
  }

  /* 12 ── The reason a refusal gives names the argument ──────────────────
   * dp_state_filter_params_why is the predicate with a sentence: NULL exactly
   * when dp_state_filter_params_ok accepts, and otherwise a sentence that
   * names the argument that failed. Each refusal has its minimal pair (the
   * same call with that one argument made good), which gives no reason. */
  {
    static const struct
    {
      double      bn, t;
      const char *names; /* what the sentence must mention */
    } refuse[] = {
      { 0.01, 0.0, "t must" },
      { 0.01, -1.0, "t must" },
      { 0.01, INFINITY, "t must" },
      { 0.01, NAN, "t must" },
      { NAN, 1.0, "bn must not be NaN" },
      { INFINITY, 1.0, "bn must be finite" },
      { -0.01, 1.0, "bn must be >= 0" },
      { 1e-31, 1.0, "bn*t is below" },
      { STATE_FILTER_BNT_MAX * 1.01, 1.0, "bn*t is above" },
    };
    for (size_t i = 0; i < sizeof refuse / sizeof *refuse; i++)
      {
        const char *w = dp_state_filter_params_why (refuse[i].bn, refuse[i].t);
        DP_CHECK (w != NULL && strstr (w, refuse[i].names) != NULL);
        DP_CHECK (!dp_state_filter_params_ok (refuse[i].bn, refuse[i].t));
        const char *why = "unset";
        DP_CHECK (dp_state_filter_create_why (refuse[i].bn, refuse[i].t, &why)
                  == NULL);
        DP_CHECK (why == w);
      }
    /* the minimal pairs: each argument made good, the pair is accepted and
     * has no reason, and create_why builds and sets why to NULL */
    static const double good[][2]
        = { { 0.01, 1.0 }, { 0.0, 1.0 }, { STATE_FILTER_BNT_MAX, 1.0 } };
    for (size_t i = 0; i < sizeof good / sizeof *good; i++)
      {
        DP_CHECK (dp_state_filter_params_why (good[i][0], good[i][1]) == NULL);
        DP_CHECK (dp_state_filter_params_ok (good[i][0], good[i][1]));
        const char              *why = "unset";
        dp_state_filter_state_t *ok
            = dp_state_filter_create_why (good[i][0], good[i][1], &why);
        DP_CHECK (ok != NULL && why == NULL);
        dp_state_filter_destroy (ok);
      }
    /* and a NULL why is allowed */
    DP_CHECK (dp_state_filter_create_why (0.01, 0.0, NULL) == NULL);
  }

  DP_TEST_END ("test_state_filter_core");
}
