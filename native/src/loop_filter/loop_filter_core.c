#include "doppler/loop_filter/loop_filter_core.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>

double
dp_loop_filter_wn (double bn, double zeta)
{
  /* The one definition. See the header for why it is public: every closed
     form about this loop is written in wn, and a measurement harness was
     re-deriving it rather than asking.

     Deliberately UNGUARDED, so extracting it changes dp_loop_filter_init()'s
     behaviour by exactly nothing — including the non-finite case that
     function's own docstring documents ("a non-finite argument yields NaN
     gains that never recover"). dp_loop_filter_create() is the boundary that
     rejects that domain, and test_loop_filter_core.c pins it doing so;
     a second guard here would make the header's statement false and buy
     nothing. */
  return 8.0 * zeta * bn / (4.0 * zeta * zeta + 1.0);
}

/* How close to 1 a damping counts as critical: inside this the two forms
   either side divide by a difference that has lost its digits. */
#define LF_CRITICAL_TOL 1e-6

/* The error of a unit phase step at time t, for the continuous second-order
   loop (the header's three forms). */
static double
lf_step_error (double t, double zeta, double wn)
{
  const double d = 1.0 - zeta * zeta;
  if (fabs (d) < LF_CRITICAL_TOL)
    return (1.0 - wn * t) * exp (-wn * t);
  if (d > 0.0)
    {
      const double wd = wn * sqrt (d);
      return exp (-zeta * wn * t) * cos (wd * t + asin (zeta)) / sqrt (d);
    }
  const double r  = wn * sqrt (-d);
  const double p1 = zeta * wn + r, p2 = zeta * wn - r;
  return (p1 * exp (-p1 * t) - p2 * exp (-p2 * t)) / (p1 - p2);
}

double
dp_loop_filter_settle_time (double bn, double zeta, double eps)
{
  if (!(bn > 0.0) || !(zeta > 0.0) || !(eps > 0.0) || !isfinite (bn)
      || !isfinite (zeta))
    return INFINITY;
  if (eps >= 1.0)
    return 0.0;
  const double wn = dp_loop_filter_wn (bn, zeta);
  const double d  = 1.0 - zeta * zeta;

  /* Bracket the last crossing: the error is over eps at lo and stays at or
     under it from hi on. */
  double lo = 0.0, hi;
  if (d >= LF_CRITICAL_TOL)
    {
      /* Under-damped: the error's extrema are at (k*pi - 2*asin(zeta)) / wd
         and their size is exp(-zeta*wn*t) there, so the first extremum at
         or under eps ends the search. */
      const double wd = wn * sqrt (d), pi = acos (-1.0);
      for (double k = 1.0;; k += 1.0)
        {
          hi = (k * pi - 2.0 * asin (zeta)) / wd;
          if (exp (-zeta * wn * hi) <= eps)
            break;
          lo = hi;
        }
    }
  else
    {
      /* Critical and over: one extremum, the undershoot, at twice the zero
         crossing; past it the error only decays. */
      const double r = wn * sqrt (d < 0.0 ? -d : 0.0);
      hi = d > -LF_CRITICAL_TOL
               ? 2.0 / wn
               : 2.0 * log ((zeta * wn + r) / (zeta * wn - r)) / (2.0 * r);
      while (fabs (lf_step_error (hi, zeta, wn)) > eps)
        {
          lo = hi;
          hi *= 2.0;
        }
    }
  for (int i = 0; i < 64; i++)
    {
      const double mid = 0.5 * (lo + hi);
      if (fabs (lf_step_error (mid, zeta, wn)) > eps)
        lo = mid;
      else
        hi = mid;
    }
  return hi;
}

/* One compiled body for the gains, out of line. Under -ffast-math a static
 * helper inlined into one caller and called from another compiles to two
 * copies with different bits (clang-cl did), so a gain computed by create
 * and one computed by a restore disagreed on the last bit. Forbidding the
 * inline and the clone keeps every caller on the one body (doppler#2103). */
#if defined(_MSC_VER)
#define LF_GAINS_OUT_OF_LINE __declspec (noinline)
#elif defined(__GNUC__) && !defined(__clang__)
#define LF_GAINS_OUT_OF_LINE __attribute__ ((noinline, noclone))
#else
#define LF_GAINS_OUT_OF_LINE __attribute__ ((noinline))
#endif

/* Standard 2nd-order PI loop-filter gains. bn is the loop noise bandwidth
 * (normalized, cycles/sample), zeta the damping factor, t the update period
 * in samples. wn is the natural frequency; the discrete kp/ki follow the
 * canonical bilinear-mapped form (e.g. Stephens & Thomas). The one formula:
 * init writes it, and the predicate checks what it gives, so the two cannot
 * disagree about which inputs make a loop. */
static LF_GAINS_OUT_OF_LINE void
lf_gains (double bn, double zeta, double t, double *kp, double *ki)
{
  double wn  = dp_loop_filter_wn (bn, zeta);
  double th  = wn * t;
  double den = 4.0 + 4.0 * zeta * th + th * th;
  *kp        = (8.0 * zeta * th) / den;
  *ki        = (4.0 * th * th) / den;
}

void
dp_loop_filter_init (dp_loop_filter_state_t *state, double bn, double zeta,
                     double t)
{
  /* The gains from lf_gains(); integ is left untouched so a reconfigure
     preserves lock. */
  state->bn   = bn;
  state->zeta = zeta;
  state->t    = t;
  lf_gains (bn, zeta, t, &state->kp, &state->ki);
}

int
dp_loop_filter_params_ok (double bn, double zeta, double t)
{
  /* Finite inputs are not enough: a finite bn past ~7e153 at t = 1 squares
     th to inf, and ki = inf/inf is NaN (doppler#2103). The gains themselves
     must be finite, which refuses no loop anyone runs. */
  if (!(bn >= 0.0 && zeta > 0.0 && t > 0.0)) /* NaN fails every comparison */
    return 0;
  double kp, ki;
  lf_gains (bn, zeta, t, &kp, &ki);
  return isfinite (kp) && isfinite (ki);
}

int
dp_loop_filter_fll_bn_ok (double bn_fll)
{
  /* NaN fails the comparison; the gain itself must be finite, which refuses
     +inf and the finite values whose 4x overflows. */
  return bn_fll >= 0.0 && isfinite (4.0 * bn_fll);
}

/* Whether a stored filter (a restored blob's) is one a live filter could
 * hold. Its gains are restored verbatim, so they are not recomputed; they are
 * checked against the one gains body at the filter's own bandwidth, zeta and
 * interval, to a tight relative tolerance. A tolerance rather than equality,
 * so a blob from another compiler or platform still restores, while a
 * forged NaN, infinite or absurd gain is refused. */
int
dp_loop_filter_state_ok (const dp_loop_filter_state_t *s)
{
  if (!dp_loop_filter_params_ok (s->bn, s->zeta, s->t))
    return 0;
  double kp, ki;
  lf_gains (s->bn, s->zeta, s->t, &kp, &ki);
  const double tol_kp = 1e-9 * fabs (kp) + DBL_MIN;
  const double tol_ki = 1e-9 * fabs (ki) + DBL_MIN;
  return fabs (s->kp - kp) <= tol_kp && fabs (s->ki - ki) <= tol_ki;
}

/* An embedded filter before its owner's init has run is all-zero, and its
 * owner's blob carries it that way; see the header. */
int
dp_loop_filter_embedded_ok (const dp_loop_filter_state_t *s)
{
  const int zero = s->kp == 0.0 && s->ki == 0.0 && s->integ == 0.0
                   && s->bn == 0.0 && s->zeta == 0.0 && s->t == 0.0;
  return zero || dp_loop_filter_state_ok (s);
}

dp_loop_filter_state_t *
dp_loop_filter_create (double bn, double zeta, double t)
{
  /* This is the untrusted boundary for a standalone loop: `LoopFilter(...)`
     hands a Python caller's arbitrary doubles straight here, where t = 0
     used to yield kp = ki = 0 — a dead loop indistinguishable from the
     legitimate frozen bn = 0 — and t = inf or a NaN argument yielded NaN
     gains, which poison every subsequent update permanently.
     dp_loop_filter_init() is deliberately NOT guarded: it is the by-value
     path, and an embedder that takes these numbers from a caller checks them
     with dp_loop_filter_params_ok() first. Not every embedder does yet: the
     Dll did not, which is how a NaN reached its gains (doppler#2103), and
     the ones still to do are doppler#2112.

     Validating here also makes the arithmetic TOTAL. With bn >= 0 and
     zeta > 0 the intermediate th is non-negative, so
     den = 4 + 4*zeta*th + th^2 >= 4 and can no longer pass through zero —
     which it can for zeta >= 1 with a sufficiently negative bn. */
  if (!dp_loop_filter_params_ok (bn, zeta, t))
    return NULL;

  dp_loop_filter_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  dp_loop_filter_init (obj, bn, zeta, t); /* integ already zeroed by calloc */
  return obj;
}

void
dp_loop_filter_destroy (dp_loop_filter_state_t *state)
{
  free (state);
}

int
dp_loop_filter_configure (dp_loop_filter_state_t *state, double bn,
                          double zeta, double t)
{
  /* A retune is as untrusted as create(): `LoopFilter.configure` hands a
     Python caller's doubles straight here (doppler#2103). */
  if (!dp_loop_filter_params_ok (bn, zeta, t))
    return DP_ERR_INVALID;
  dp_loop_filter_init (state, bn, zeta, t); /* recompute gains, keep integ */
  return DP_OK;
}

void
dp_loop_filter_reset (dp_loop_filter_state_t *state)
{
  state->integ = 0.0;
}

/* Serializable state — pointer-free POD whole-struct snapshot, restored
 * only when dp_loop_filter_state_ok() accepts it: the gains are taken
 * verbatim, so a forged NaN or infinite gain would otherwise reach every
 * embedder that restores a loop filter -- LoopFilter itself, the ratesync
 * loop and the MPSK carrier filter (doppler#2641). See
 * DP_DEFINE_POD_STATE_CHECKED in dp_state.h. */
DP_DEFINE_POD_STATE_CHECKED (dp_loop_filter, dp_loop_filter_state_t,
                             LOOP_FILTER_STATE_MAGIC,
                             LOOP_FILTER_STATE_VERSION,
                             dp_loop_filter_state_ok)

void
dp_loop_filter_steps (dp_loop_filter_state_t *state, const double *x,
                      double *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = dp_loop_filter_step (state, x[i]);
}
