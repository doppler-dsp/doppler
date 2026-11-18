#include "doppler/state_filter/state_filter_core.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── The exact loop (docs/design/loop-filter.md section 11.1) ────────────
 *
 * The loop is a linear system whose state is what the simulation of it
 * carries: the oscillator phase, the frequency estimate, the rate estimate
 * (order 3) and the detector's delay line of past errors. Written that way
 * every entry is a small combination of the gains and of 1, so a slow loop
 * (gains of order 1e-3 and below) is a matrix close to the identity and
 * nothing cancels; the polynomial form `D(z)` of the page does, and loses
 * the digits the small gains live in. The bandwidth is Parseval on the
 * impulse response, `Bn = (1/2) (d^2 + c P c^T)` with `P = A P A^T + b b^T`
 * solved exactly (a linear system in the entries of P); and since (A, b) is
 * controllable, A is stable exactly when that P is positive definite. */

/* States: phase, frequency, [rate], then the delay line. The line holds up
 * to k + 1 past errors for a delay k + f: at most 3 + 4 = 7. */
#define SF_MAXN 7

/* Build A, b and the output row c, d of the loop. Returns the number of
 * states, or 0 for an argument outside the domain. */
static int
sf_loop (int order, double kp, double ki, double kr, double delay, double gain,
         double A[SF_MAXN][SF_MAXN], double *b, double *c, double *d)
{
  if ((order != 2 && order != 3) || !isfinite (kp) || !isfinite (ki)
      || !isfinite (kr) || !(delay >= 0.0 && delay <= STATE_FILTER_DELAY_LIMIT)
      || !(gain > 0.0) || !isfinite (gain))
    return 0;
  const int    nl = order == 3 ? 3 : 2; /* phase, frequency, [rate] */
  const int    k  = (int)floor (delay);
  const double f  = delay - (double)k;
  const int    H  = f > 0.0 ? k + 1 : k; /* past errors the detector reads */
  const int    n  = nl + H;
  if (n > SF_MAXN)
    return 0;

  /* The detector's error e = gain * ((1-f) r[n-k] + f r[n-k-1]), with
   * r[n] = w[n] - phi[n]: a row over the states and a coefficient on the
   * input w. r[n-j] for j >= 1 is the delay line's entry j. */
  double ev[SF_MAXN] = { 0 };
  double ew          = 0.0;
  for (int pass = 0; pass < 2; pass++)
    {
      const int    j     = k + pass;
      const double coeff = (pass == 0 ? 1.0 - f : f) * gain;
      if (pass == 1 && f == 0.0)
        break;
      if (j == 0)
        {
          ev[0] -= coeff;
          ew += coeff;
        }
      else
        ev[nl + j - 1] += coeff;
    }

  for (int i = 0; i < SF_MAXN; i++)
    {
      b[i] = 0.0;
      for (int j = 0; j < SF_MAXN; j++)
        A[i][j] = 0.0;
    }
  double rate_row[SF_MAXN] = { 0 }, int_row[SF_MAXN] = { 0 },
         phi_row[SF_MAXN] = { 0 };
  double rate_b           = 0.0, int_b, phi_b;
  /* rate += kr*e; integ += rate + ki*e; control = integ + kp*e;
   * phi += control. The order-2 filter has no rate. */
  if (order == 3)
    {
      for (int j = 0; j < n; j++)
        rate_row[j] = kr * ev[j];
      rate_row[2] += 1.0;
      rate_b = kr * ew;
    }
  for (int j = 0; j < n; j++)
    int_row[j] = (order == 3 ? rate_row[j] : 0.0) + ki * ev[j];
  int_row[1] += 1.0;
  int_b = (order == 3 ? rate_b : 0.0) + ki * ew;
  for (int j = 0; j < n; j++)
    phi_row[j] = int_row[j] + kp * ev[j];
  phi_row[0] += 1.0;
  phi_b = int_b + kp * ew;

  for (int j = 0; j < n; j++)
    {
      A[0][j] = phi_row[j];
      A[1][j] = int_row[j];
      if (order == 3)
        A[2][j] = rate_row[j];
    }
  b[0] = phi_b;
  b[1] = int_b;
  if (order == 3)
    b[2] = rate_b;
  if (H >= 1)
    {
      A[nl][0] = -1.0; /* the newest error: w - phi */
      b[nl]    = 1.0;
      for (int j = 1; j < H; j++)
        A[nl + j][nl + j - 1] = 1.0; /* the line shifts */
    }
  for (int j = 0; j < n; j++)
    c[j] = phi_row[j];
  *d = phi_b;
  return n;
}

/* Balance the loop's coordinates: a diagonal similarity x = D x' with D a
 * power of two per state (exact in floating point), chosen so each state's
 * row and column of A carry comparable weight (the LAPACK dgebal rule, with
 * b counted in the row). The gains of a slow loop span six decades (kr is
 * about 1e-9 where kp is 4e-3), so in the raw coordinates the entries of A^k
 * and of P span as many; balanced, they are comparable and squaring A keeps
 * its digits. P' = D^-1 P D^-T and c' = c D, so the stability verdict and the
 * bandwidth are those of the unbalanced loop: only the conditioning changes.
 */
static void
sf_balance (double A[SF_MAXN][SF_MAXN], double *b, double *c, int n)
{
  for (int sweep = 0; sweep < 30; sweep++)
    {
      int changed = 0;
      for (int i = 0; i < n; i++)
        {
          double r = fabs (b[i]), col = 0.0;
          for (int j = 0; j < n; j++)
            if (j != i)
              {
                r += fabs (A[i][j]);
                col += fabs (A[j][i]);
              }
          if (!(r > 0.0) || !(col > 0.0))
            continue;
          /* scaling state i by 2^g divides its row by 2^g and multiplies
           * its column by 2^g: equalize at g = round (log2 (r / col) / 2) */
          const int g = (int)lround (0.5 * log2 (r / col));
          if (g == 0)
            continue;
          const double f = ldexp (1.0, g);
          for (int j = 0; j < n; j++)
            if (j != i)
              {
                A[i][j] /= f;
                A[j][i] *= f;
              }
          b[i] /= f;
          c[i] *= f;
          changed = 1;
        }
      if (!changed)
        break;
    }
}

/* Stability and bandwidth of the loop in one computation. Returns 1 and sets
 * *bn when the loop is stable, 0 otherwise (or outside the domain).
 *
 * P = sum over k >= 0 of A^k b b^T (A^T)^k, and the loop is stable exactly
 * when A^k goes to zero. Both come from the doubling iteration
 *
 *   P <- P + A P A^T,   A <- A A,
 *
 * which after j passes holds the first 2^j terms of the sum and A^(2^j): the
 * sum's terms are positive semidefinite (it cannot lose definiteness by
 * cancellation, so P keeps its digits at every scale the gains span; single
 * entries may be negative), a loop at radius 1 - delta is decided in
 * about log2 (40/delta) passes (one at delta 1e-9 needs 36), an unstable one
 * overflows or stalls at order one and a marginal one (a gain at zero, a
 * pole at z = 1: the loop is not driven or does not decay) never goes to
 * zero, which is how the "not controllable reads as not stable" rule falls
 * out. Raising the delay line to the 2^j power by repeated squaring costs
 * only digits close to the edge: a loop within about 1e-12 of the unit
 * circle is within rounding of it, and either answer is correct there.
 *
 * A positivity test on the solved Lyapunov equation loses pivots to rounding
 * when the gains span six decades (a loop stable with margin 1e-4 reads
 * unstable); the sum does not, because its terms are positive semidefinite.
 */
static int
sf_response (int order, double kp, double ki, double kr, double delay,
             double gain, double *bn)
{
  double    A[SF_MAXN][SF_MAXN], b[SF_MAXN], c[SF_MAXN], d;
  const int n = sf_loop (order, kp, ki, kr, delay, gain, A, b, c, &d);
  if (n == 0)
    return 0;
  sf_balance (A, b, c, n);

  double P[SF_MAXN][SF_MAXN];
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      P[i][j] = b[i] * b[j];
  int stable = 0;
  for (int pass = 0; pass < 64; pass++)
    {
      double T[SF_MAXN][SF_MAXN], N[SF_MAXN][SF_MAXN], A2[SF_MAXN][SF_MAXN];
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
          {
            double t = 0.0, u = 0.0;
            for (int k = 0; k < n; k++)
              {
                t += A[i][k] * P[k][j];
                u += A[i][k] * A[k][j];
              }
            T[i][j]  = t;
            A2[i][j] = u;
          }
      double amax = 0.0;
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
          {
            double t = 0.0;
            for (int k = 0; k < n; k++)
              t += T[i][k] * A[j][k];
            N[i][j] = t;
          }
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
          {
            P[i][j] += 0.5 * (N[i][j] + N[j][i]); /* keep P symmetric */
            A[i][j] = A2[i][j];
            /* fmax would skip a NaN entry: test each one, so a non-finite
             * power of A is never read as small */
            if (!isfinite (A2[i][j]))
              return 0;
            amax = fmax (amax, fabs (A2[i][j]));
          }
      if (amax > 1e150)
        return 0; /* the powers of A grow without bound */
      if (amax < 1e-12)
        {
          stable = 1; /* A^(2^pass) is gone: every later term is negligible */
          break;
        }
    }
  if (!stable)
    return 0;
  double sum = d * d;
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      sum += c[i] * P[i][j] * c[j];
  if (!isfinite (sum))
    return 0;
  *bn = 0.5 * sum;
  return 1;
}

int
dp_state_filter_loop_stable (int order, double kp, double ki, double kr,
                             double delay, double gain)
{
  double bn;
  return sf_response (order, kp, ki, kr, delay, gain, &bn);
}

double
dp_state_filter_loop_bandwidth (int order, double kp, double ki, double kr,
                                double delay, double gain)
{
  double bn;
  return sf_response (order, kp, ki, kr, delay, gain, &bn) ? bn : NAN;
}

double
dp_state_filter_steady_error (int order, double ki, double kr, int input_order,
                              double x)
{
  if (order != 2 && order != 3)
    return NAN;
  if (input_order == 1)
    return 0.0;
  if (order == 2 && input_order == 2)
    return ki == 0.0 ? copysign (INFINITY, x) : x / ki;
  if (order == 2 && input_order == 3)
    return x == 0.0 ? 0.0 : copysign (INFINITY, x);
  if (order == 3 && input_order == 2)
    return 0.0;
  if (order == 3 && input_order == 3)
    return kr == 0.0 ? (x == 0.0 ? 0.0 : copysign (INFINITY, x)) : x / kr;
  return NAN;
}

/* ── The gains ───────────────────────────────────────────────────────── */

/* The exact noise bandwidth of the delay-free order-3 loop, in closed form
 * (the Lyapunov solution of docs/design/loop-filter.md section 11.1 step 5).
 * The solve below runs on it; the general function above is the independent
 * route the C test compares it to. */
static double
sf_bn3 (double kp, double ki, double kr)
{
  const double num = 2.0 * ki * ki * kp + 4.0 * ki * ki + 4.0 * ki * kp * kp
                     + 3.0 * ki * kp * kr + 4.0 * ki * kr + 4.0 * kp * kp * kr
                     + kp * kr * kr - 4.0 * kp * kr + kr * kr;
  const double den
      = 2.0 * (ki * kp + kp * kr - kr) * (8.0 - 2.0 * ki - 4.0 * kp - kr);
  return num / den;
}

/* One compiled body for the gains, out of line, so every caller (create, a
 * restore's check) gets the same bits under -ffast-math; the reason is the
 * loop filter's own (loop_filter_core.c). */
#if defined(_MSC_VER)
#define SF_GAINS_OUT_OF_LINE __declspec (noinline)
#elif defined(__GNUC__) && !defined(__clang__)
#define SF_GAINS_OUT_OF_LINE __attribute__ ((noinline, noclone))
#else
#define SF_GAINS_OUT_OF_LINE __attribute__ ((noinline))
#endif

static SF_GAINS_OUT_OF_LINE void
sf_gains (double bn, double t, double *kp, double *ki, double *kr)
{
  const double B = bn * t;
  if (!(B > 0.0))
    {
      *kp = *ki = *kr = 0.0;
      return;
    }
  const double a = STATE_FILTER_SHAPE_A;
  const double b = STATE_FILTER_SHAPE_B;
  /* The analog prototype's noise bandwidth over w0 gives the starting
   * scale; the exact discrete bandwidth is slightly wider at that scale, so
   * the solution lies in [0.5, 1] of it. Bisect the exact expression. */
  const double c  = (a * b * b + a * a - b) / (4.0 * (a * b - 1.0));
  const double s0 = B / c;
  double       lo = 0.5 * s0, hi = 1.01 * s0;
  for (int i = 0; i < 100; i++)
    {
      const double th = 0.5 * (lo + hi);
      if (sf_bn3 (b * th, a * th * th, th * th * th) < B)
        lo = th;
      else
        hi = th;
    }
  const double th = 0.5 * (lo + hi);
  *kp             = b * th;
  *ki             = a * th * th;
  *kr             = th * th * th;
}

int
dp_state_filter_params_ok (double bn, double t)
{
  /* A NaN fails every comparison; a negative bn gives a negative product,
   which is under the floor; an infinite product is over the bound. */
  if (!(t > 0.0))
    return 0;
  const double B = bn * t; /* negative for a negative bn; NaN fails below */
  return B == 0.0 || (B >= STATE_FILTER_BNT_MIN && B <= STATE_FILTER_BNT_MAX);
}

void
dp_state_filter_init (dp_state_filter_state_t *state, double bn, double t)
{
  state->bn = bn;
  state->t  = t;
  sf_gains (bn, t, &state->kp, &state->ki, &state->kr);
}

int
dp_state_filter_state_ok (const dp_state_filter_state_t *s)
{
  if (!dp_state_filter_params_ok (s->bn, s->t))
    return 0;
  double kp, ki, kr;
  sf_gains (s->bn, s->t, &kp, &ki, &kr);
  return fabs (s->kp - kp) <= 1e-9 * fabs (kp) + DBL_MIN
         && fabs (s->ki - ki) <= 1e-9 * fabs (ki) + DBL_MIN
         && fabs (s->kr - kr) <= 1e-9 * fabs (kr) + DBL_MIN;
}

const char *
dp_state_filter_params_why (double bn, double t)
{
  if (!(t > 0.0) || !isfinite (t))
    return "t must be a positive, finite update period";
  if (isnan (bn))
    return "bn must not be NaN";
  if (isinf (bn))
    return "bn must be finite";
  if (bn < 0.0)
    return "bn must be >= 0";
  const double B = bn * t;
  if (B != 0.0 && B < STATE_FILTER_BNT_MIN)
    return "bn*t is below the smallest the filter accepts";
  if (B > STATE_FILTER_BNT_MAX)
    return "bn*t is above the largest the filter accepts, the bound the "
           "third-order loop is stable to";
  return NULL;
}

dp_state_filter_state_t *
dp_state_filter_create_why (double bn, double t, const char **why)
{
  const char *w = dp_state_filter_params_why (bn, t);
  if (why)
    *why = w;
  return w ? NULL : dp_state_filter_create (bn, t);
}

dp_state_filter_state_t *
dp_state_filter_create (double bn, double t)
{
  if (!dp_state_filter_params_ok (bn, t))
    return NULL;
  dp_state_filter_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  dp_state_filter_init (obj, bn, t); /* integ and rate zeroed by calloc */
  return obj;
}

void
dp_state_filter_destroy (dp_state_filter_state_t *state)
{
  free (state);
}

int
dp_state_filter_configure (dp_state_filter_state_t *state, double bn, double t)
{
  if (!dp_state_filter_params_ok (bn, t))
    return DP_ERR_INVALID;
  dp_state_filter_init (state, bn, t);
  return DP_OK;
}

void
dp_state_filter_reset (dp_state_filter_state_t *state)
{
  state->integ = 0.0;
  state->rate  = 0.0;
}

bool
dp_state_filter_stable (const dp_state_filter_state_t *state, double delay,
                        double gain)
{
  return dp_state_filter_loop_stable (3, state->kp, state->ki, state->kr,
                                      delay, gain);
}

double
dp_state_filter_bandwidth (const dp_state_filter_state_t *state, double delay,
                           double gain)
{
  return dp_state_filter_loop_bandwidth (3, state->kp, state->ki, state->kr,
                                         delay, gain);
}

void
dp_state_filter_steps (dp_state_filter_state_t *state, const double *x,
                       double *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = dp_state_filter_step (state, x[i]);
}

/* ── Serializable state ──────────────────────────────────────────────────
 * [envelope][flavor u32 = 0][pad u32][kp ki kr integ rate bn t]. */

#define SF_PAYLOAD_BYTES (2 * sizeof (uint32_t) + 7 * sizeof (double))

size_t
dp_state_filter_state_bytes (const dp_state_filter_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + SF_PAYLOAD_BYTES;
}

void
dp_state_filter_get_state (const dp_state_filter_state_t *state, void *blob)
{
  const size_t total = dp_state_filter_state_bytes (state);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, STATE_FILTER_STATE_MAGIC, STATE_FILTER_STATE_VERSION, total);
  dp_w_u32 (&w, 0u); /* flavor: fixed-gain */
  dp_w_u32 (&w, 0u);
  dp_w_f64 (&w, state->kp);
  dp_w_f64 (&w, state->ki);
  dp_w_f64 (&w, state->kr);
  dp_w_f64 (&w, state->integ);
  dp_w_f64 (&w, state->rate);
  dp_w_f64 (&w, state->bn);
  dp_w_f64 (&w, state->t);
}

int
dp_state_filter_set_state (dp_state_filter_state_t *state, const void *blob)
{
  const size_t total = dp_state_filter_state_bytes (state);
  int          rc = dp_state_validate (blob, total, STATE_FILTER_STATE_MAGIC,
                                       STATE_FILTER_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r   = dp_reader_init (blob, total);
  r.off           = sizeof (dp_state_hdr_t);
  uint32_t flavor = 0xFFFFFFFFu, pad = 0u; /* a failed read refuses */
  dp_r_bytes (&r, &flavor, sizeof flavor);
  dp_r_bytes (&r, &pad, sizeof pad);
  if (flavor != 0u || pad != 0u)
    return DP_ERR_INVALID; /* another flavor's blob, or a pad that is not 0 */
  dp_state_filter_state_t c;
  dp_r_bytes (&r, &c.kp, sizeof c.kp);
  dp_r_bytes (&r, &c.ki, sizeof c.ki);
  dp_r_bytes (&r, &c.kr, sizeof c.kr);
  dp_r_bytes (&r, &c.integ, sizeof c.integ);
  dp_r_bytes (&r, &c.rate, sizeof c.rate);
  dp_r_bytes (&r, &c.bn, sizeof c.bn);
  dp_r_bytes (&r, &c.t, sizeof c.t);
  if (!dp_state_filter_state_ok (&c))
    return DP_ERR_INVALID;
  *state = c;
  return DP_OK;
}
