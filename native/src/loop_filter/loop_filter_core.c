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

/* Standard 2nd-order PI loop-filter gains. bn is the loop noise bandwidth
 * (normalized, cycles/sample), zeta the damping factor, t the update period
 * in samples. wn is the natural frequency; the discrete kp/ki follow the
 * canonical bilinear-mapped form (e.g. Stephens & Thomas). The one formula:
 * init writes it, and the predicate checks what it gives, so the two cannot
 * disagree about which inputs make a loop. */
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
  if (!(bn >= 0.0 && zeta > 0.0 && t > 0.0 && isfinite (bn) && isfinite (zeta)
        && isfinite (t))) /* NaN fails every comparison */
    return 0;
  double kp, ki;
  lf_gains (bn, zeta, t, &kp, &ki);
  return isfinite (kp) && isfinite (ki);
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

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_loop_filter, dp_loop_filter_state_t,
                     LOOP_FILTER_STATE_MAGIC, LOOP_FILTER_STATE_VERSION)

void
dp_loop_filter_steps (dp_loop_filter_state_t *state, const double *x,
                      double *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = dp_loop_filter_step (state, x[i]);
}
