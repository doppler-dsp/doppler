/*
 * test_agc_core.c — C-level unit tests for the log-domain AGC.
 *
 * Tests cover:
 *   §1  Lifecycle: create seeds gain_db = 0 dB and p_avg = reference power
 *   §2  Convergence: a loud input is driven down to ref_db
 *   §3  Both a quiet and a loud input reach ref within one budget
 *   §4  A non-zero reference converges to ref_db, not 0 dB
 *   §5  agc_exp10_ / agc_log10_ agree with libm at sample points
 *   §6  Decimated steps() reaches the same steady state as step()
 *   §7  steps() supports in-place operation
 *   §8  decim 8 / 16 / 32 all converge the output to the reference
 *   §9  reset restores post-create state; configuration survives
 *   §10 applied_gain_db is 0 at create and equals gain_db at convergence
 *   §11 Output clipping is square, and never perturbs the loop
 *   §12 Serializable round-trip, blob determinism, telemetry attach/detach
 *   §13 One non-finite input sample cannot poison the detector
 *   §14 Silence leaves the loop finite AND recoverable, both entry points
 *   §15 agc_exp10_ is total: no input yields a negative gain
 *   §16 agc_log10_ is total: a NaN does not read as a plausible level
 *   §17 applied_gain_db stays finite when the linear gain underflows
 *   §18 saturate()'s own contract, including both NaN destinations
 *
 * Sections §13 onward were added by the validation campaign. They exist
 * because agc_core.h claimed the power floor was "never reached in normal
 * operation" and nothing ran that claim — while measurement showed the loop
 * could be destroyed permanently by a single non-finite sample, and by ~800
 * samples of silence. Each was proven by sabotage before being trusted.
 *
 * They share one shape worth naming: every one asserts a VACUITY
 * PRECONDITION first. "The state is finite" is satisfied by a loop that did
 * nothing at all, so each test first establishes that the thing it is
 * guarding against actually happened — the gain moved, the linear gain
 * underflowed, the detector saturated — and only then asserts the guard
 * held. Without that, reverting the guard would leave them green.
 *
 * §18 tests a util primitive from the agc test rather than from a util one,
 * because the util module has no C test harness at all and its per-module
 * CMakeLists is jm-generated. Filed rather than worked around here.
 */
#include "agc/agc_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

/* File-scope so the §13+ section functions can CHECK for themselves rather
   than funnelling a bool back through main(). */
static int _fails = 0;

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return _almost_eq (crealf (a), crealf (b), tol)
         && _almost_eq (cimagf (a), cimagf (b), tol);
}
#define ALMOST_EQ(a, b, tol) _almost_eq ((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol)                                                \
  _almost_eq_c ((float complex) (a), (float complex) (b), tol)

/* Feed n copies of a constant-magnitude sample; return the power of the
 * final output in dB.  Used to probe the converged loop state. */
static double
run_const (agc_state_t *agc, float complex x, size_t n)
{
  float complex y = 0.0f + 0.0f * I;
  for (size_t i = 0; i < n; i++)
    y = agc_step (agc, x);
  double p = (double)crealf (y) * crealf (y) + (double)cimagf (y) * cimagf (y);
  return 10.0 * log10 (p);
}

/* ── §13 — one non-finite input sample cannot poison the detector ─────────
 *
 * The EMA is where an input sample first becomes PERSISTENT state, so it is
 * the boundary the guard defends.  Unguarded, a single Inf or NaN sample
 * drove p_avg non-finite and it never returned: a following normal sample
 * left it NaN, and the object was dead for the rest of the run.
 *
 * The direction is asserted too, not just finiteness.  An unknown level
 * must read LOUD so the loop turns the gain DOWN; a guard that sent NaN to
 * the floor instead would be equally "finite" and would drive the gain up,
 * railing everything downstream.  That is what the gain_db < 0 check pins. */
static int
guard_survives_one_bad_sample (float complex bad, const char *what)
{
  int          ok = 1;
  agc_state_t *s  = agc_create (0.0, 0.0025, 0.05);
  if (!s)
    return 0;

  /* Vacuity precondition: the loop starts in a known-good state, so the
     assertions below are about the guard and not about nothing happening. */
  if (!(s->p_avg == 1.0 && s->gain_db == 0.0))
    {
      fprintf (stderr, "  §13 %s: precondition — loop did not start clean\n",
               what);
      ok = 0;
    }

  (void)agc_step (s, bad);

  if (!isfinite (s->p_avg))
    {
      fprintf (stderr, "  §13 %s: p_avg went non-finite (%g)\n", what,
               s->p_avg);
      ok = 0;
    }
  if (!isfinite (s->gain_db))
    {
      fprintf (stderr, "  §13 %s: gain_db went non-finite (%g)\n", what,
               s->gain_db);
      ok = 0;
    }
  /* The detector must have SATURATED, not ignored the sample — otherwise a
     guard that simply dropped bad samples would also pass. */
  if (!(s->p_avg > 1.0))
    {
      fprintf (stderr, "  §13 %s: detector did not saturate (p_avg %g)\n",
               what, s->p_avg);
      ok = 0;
    }
  /* ...and the saturation drove the gain DOWN. */
  if (!(s->gain_db < 0.0))
    {
      fprintf (stderr, "  §13 %s: unknown level drove gain UP (%g dB)\n", what,
               s->gain_db);
      ok = 0;
    }

  /* The loop still works afterwards: this is the half that failed before. */
  float complex y = agc_step (s, 1.0f + 0.0f * I);
  if (!isfinite (crealf (y)) || !isfinite (cimagf (y)))
    {
      fprintf (stderr, "  §13 %s: output non-finite after recovery sample\n",
               what);
      ok = 0;
    }
  if (!isfinite (s->p_avg))
    {
      fprintf (stderr, "  §13 %s: p_avg non-finite after a normal sample\n",
               what);
      ok = 0;
    }
  agc_destroy (s);
  return ok;
}

/* ── §14 — silence leaves the loop finite AND recoverable ─────────────────
 *
 * With no signal the detector reads the power floor, the filter sees a
 * constant ~+300 dB error, and the integrator climbs.  Unguarded that ended
 * in a permanently dead object after ~800 silent samples.  Guarded, the
 * wind-up is self-limiting: the gain eventually overflows, the guard reads
 * the resulting non-finite power as maximally loud, and the loop is driven
 * back.  Both entry points are checked because agc_steps() folds the
 * detector over a chunk mean and could guard only one of them. */
static int
silence_leaves_the_loop_recoverable (int use_block)
{
  enum
  {
    SETTLE = 4000,
    GAP    = 3000,
    BUDGET = 100000
  };
  const float complex dir  = 0.6f + 0.8f * I;
  const char         *what = use_block ? "steps" : "step";
  int                 ok   = 1;
  agc_state_t        *s    = agc_create (0.0, 0.0025, 0.05);
  if (!s)
    return 0;

  static float complex zeros[GAP], out[GAP];
  for (size_t i = 0; i < GAP; i++)
    zeros[i] = 0.0f + 0.0f * I;

  for (int n = 0; n < SETTLE; n++)
    (void)agc_step (s, dir * 1.0f);
  double settled = s->gain_db;

  if (use_block)
    agc_steps (s, zeros, out, GAP);
  else
    for (int n = 0; n < GAP; n++)
      (void)agc_step (s, 0.0f + 0.0f * I);

  /* Vacuity precondition: the silence must actually have moved the loop.
     "Still finite" proves nothing about a loop that never left its seed. */
  if (!(fabs (s->gain_db - settled) > 1.0))
    {
      fprintf (stderr,
               "  §14 %s: precondition — silence did not move the "
               "loop (%g -> %g)\n",
               what, settled, s->gain_db);
      ok = 0;
    }
  if (!isfinite (s->gain_db) || !isfinite (s->p_avg) || !isfinite (s->g_last))
    {
      fprintf (stderr,
               "  §14 %s: state non-finite after silence "
               "(gain %g, p_avg %g, g_last %g)\n",
               what, s->gain_db, s->p_avg, s->g_last);
      ok = 0;
    }

  /* And it comes back. This is the assertion the unguarded loop failed: it
     stayed NaN forever, so `recovered` never became true. */
  int recovered = 0;
  for (long n = 0; n < BUDGET && !recovered; n++)
    {
      (void)agc_step (s, dir * 1.0f);
      if (fabs (s->gain_db) < 1.0)
        recovered = 1;
    }
  if (!recovered)
    {
      fprintf (stderr,
               "  §14 %s: did not recover within %d samples "
               "(gain %g, p_avg %g)\n",
               what, BUDGET, s->gain_db, s->p_avg);
      ok = 0;
    }
  agc_destroy (s);
  return ok;
}

/* ── §15 — agc_exp10_ is total ────────────────────────────────────────────
 *
 * It assembles an IEEE-754 exponent field directly.  Unguarded, past
 * |v| ~ 308 that assembly overflowed into the SIGN bit and the function
 * returned a negative number where the true answer is +inf or 0 — measured,
 * agc_exp10_(309) = -3.09e-308 and agc_exp10_(-320) = -3.23e+296.  A gain
 * that comes back negative does not lose precision, it inverts the signal.
 *
 * `>= 0.0` is also the NaN test: every comparison against NaN is false. */
static int
exp10_is_total (void)
{
  int          ok = 1;
  const double vs[]
      = { 0.0,   1.0,    -1.0,   100.0,   -100.0, 307.0, 308.0, 309.0, -309.0,
          400.0, -400.0, 1000.0, -1000.0, 1e6,    -1e6,  1e30,  -1e30 };
  for (size_t i = 0; i < sizeof vs / sizeof vs[0]; i++)
    {
      double g = agc_exp10_ (vs[i]);
      if (!(g >= 0.0))
        {
          fprintf (stderr, "  §15 agc_exp10_(%g) = %g — negative or NaN\n",
                   vs[i], g);
          ok = 0;
        }
      if (!isfinite (g))
        {
          fprintf (stderr, "  §15 agc_exp10_(%g) = %g — not finite\n", vs[i],
                   g);
          ok = 0;
        }
    }
  /* A NaN exponent attenuates: same rule the detector's guard uses, applied
     to a gain rather than a level. */
  double gn = agc_exp10_ (0.0 / 0.0);
  if (!(gn >= 0.0 && gn <= 1.0))
    {
      fprintf (stderr, "  §15 agc_exp10_(NaN) = %g — did not attenuate\n", gn);
      ok = 0;
    }
  /* Saturating must not have cost accuracy inside the working range. The
     header documents ~1e-3 relative; measured worst is 7.5e-4. */
  for (double v = -15.0; v <= 15.0; v += 0.01)
    {
      double got = agc_exp10_ (v), ref = pow (10.0, v);
      if (!(fabs (got - ref) / ref < 1e-3))
        {
          fprintf (stderr, "  §15 agc_exp10_(%g) rel err %g exceeds 1e-3\n", v,
                   fabs (got - ref) / ref);
          ok = 0;
          break;
        }
    }
  return ok;
}

/* ── §16 — agc_log10_ is total ────────────────────────────────────────────
 *
 * The bit-field split has no notion of special values.  Unguarded it read a
 * NaN's exponent field as an ordinary 1024 and returned a perfectly
 * plausible +308 where libm returns NaN — and that fabricated level is what
 * turned a stalled loop into a runaway one, because the filter believed it
 * was seeing +3084 dB and drove the gain the other way, forever.  A wrong
 * answer that looks like a right one is worse than an infinity. */
static int
log10_is_total (void)
{
  int    ok       = 1;
  double at_ceil  = agc_log10_ (AGC_POWER_CEIL);
  double at_floor = agc_log10_ (AGC_POWER_FLOOR);

  struct
  {
    double      in, want;
    const char *what;
  } cases[] = {
    { 0.0 / 0.0, at_ceil, "NaN" },    { 1.0 / 0.0, at_ceil, "+Inf" },
    { -1.0 / 0.0, at_floor, "-Inf" }, { 0.0, at_floor, "0" },
    { -1.0, at_floor, "negative" },   { 1e300, at_ceil, "above ceiling" },
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
      double got = agc_log10_ (cases[i].in);
      if (!(got == cases[i].want))
        {
          fprintf (stderr,
                   "  §16 agc_log10_(%s) = %g, expected the saturated %g\n",
                   cases[i].what, got, cases[i].want);
          ok = 0;
        }
    }
  /* The header's "silence reads about -300 dB" is now structural rather
     than something every caller has to remember to add a floor for. */
  if (!(fabs (10.0 * at_floor + 300.0) < 0.01))
    {
      fprintf (stderr, "  §16 the floor reads %g dB, not -300\n",
               10.0 * at_floor);
      ok = 0;
    }
  /* Accuracy inside the working range; header documents ~1e-3 absolute and
     the measured worst is 7.8e-4, so the old 1e-2 tolerance was ten times
     looser than the claim it was protecting. */
  for (double e = -12.0; e <= 12.0; e += 0.01)
    {
      double p = pow (10.0, e);
      if (!(fabs (agc_log10_ (p) - log10 (p)) < 1e-3))
        {
          fprintf (stderr, "  §16 agc_log10_(%g) abs err %g exceeds 1e-3\n", p,
                   fabs (agc_log10_ (p) - log10 (p)));
          ok = 0;
          break;
        }
    }
  return ok;
}

/* ── §17 — a public accessor cannot return a non-finite value ─────────────
 *
 * The state being total is not the same as everything DERIVED from it being
 * total.  agc_exp10_ correctly saturates a very negative commanded gain to
 * a linear 0, and 20*log10(0) is -INF — so this accessor handed a caller a
 * non-finite number out of a perfectly well-formed object. */
static int
applied_gain_is_finite_after_silence (void)
{
  int          ok = 1;
  agc_state_t *s  = agc_create (0.0, 0.0025, 0.05);
  if (!s)
    return 0;
  for (int n = 0; n < 3000; n++)
    (void)agc_step (s, 0.0f + 0.0f * I);

  /* Vacuity precondition: the linear gain must actually have underflowed,
     or "finite" is a statement about an ordinary gain and proves nothing. */
  if (!(s->g_last == 0.0))
    {
      fprintf (stderr,
               "  §17 precondition — g_last did not underflow (%g); the "
               "accessor is not being asked the hard question\n",
               s->g_last);
      ok = 0;
    }
  double db = agc_get_applied_gain_db (s);
  if (!isfinite (db))
    {
      fprintf (stderr, "  §17 applied_gain_db = %g, not finite\n", db);
      ok = 0;
    }
  /* Still unmistakably "off" rather than quietly plausible. */
  if (!(db < -1000.0))
    {
      fprintf (stderr, "  §17 applied_gain_db = %g does not read as off\n",
               db);
      ok = 0;
    }
  agc_destroy (s);
  return ok;
}

/* ── §18 — saturate()'s contract, including both NaN destinations ─────────
 *
 * Lives here rather than in a util test because the util module has no C
 * test harness and its per-module CMakeLists is jm-generated; filed rather
 * than worked around. The NaN destination being a PARAMETER is the part
 * worth pinning: which end is safe is domain knowledge, and this object
 * needs the ceiling for a level while a lock statistic would need the
 * floor. */
static int
saturate_contract (void)
{
  int    ok    = 1;
  double nan_v = 0.0 / 0.0, inf_v = 1.0 / 0.0;
  struct
  {
    double      v, lo, hi, nan_to, want;
    const char *what;
  } cases[] = {
    { 0.5, 0.0, 1.0, 1.0, 0.5, "inside is passed through" },
    { 0.0, 0.0, 1.0, 1.0, 0.0, "the lower bound is inclusive" },
    { 1.0, 0.0, 1.0, 1.0, 1.0, "the upper bound is inclusive" },
    { 2.0, 0.0, 1.0, 1.0, 1.0, "above saturates high" },
    { -3.0, 0.0, 1.0, 1.0, 0.0, "below saturates low" },
    { inf_v, 0.0, 1.0, 1.0, 1.0, "+Inf is just above" },
    { -inf_v, 0.0, 1.0, 1.0, 0.0, "-Inf is just below" },
    { nan_v, 0.0, 1.0, 1.0, 1.0, "NaN takes the caller's end (high)" },
    { nan_v, 0.0, 1.0, 0.0, 0.0, "NaN takes the caller's end (low)" },
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
      double got
          = saturate (cases[i].v, cases[i].lo, cases[i].hi, cases[i].nan_to);
      if (!(got == cases[i].want))
        {
          fprintf (stderr, "  §18 %s: got %g, expected %g\n", cases[i].what,
                   got, cases[i].want);
          ok = 0;
        }
    }
  return ok;
}

int
main (void)
{
  /* unit-magnitude direction: (0.6 + 0.8j) has |.| == 1, so scaling it
   * by A gives an input of magnitude A exercising both re and im. */
  const float complex dir = 0.6f + 0.8f * I;

  /* ---- lifecycle ---- */
  agc_state_t *obj = agc_create (0.0, 0.0025, 0.05);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* create seeds gain_db = 0 dB and p_avg = reference power = 10^0 = 1 */
  CHECK (ALMOST_EQ (obj->gain_db, 0.0, 1e-6));
  CHECK (ALMOST_EQ (obj->p_avg, 1.0, 1e-6));

  /* ---- convergence: a loud input is driven down to ref_db (0 dB) ---- */
  double out_db = run_const (obj, dir * 10.0f, 4000);
  CHECK (ALMOST_EQ (out_db, 0.0, 0.5));
  /* gain settles to -20 dB: 20*log10(10) of input attenuation */
  CHECK (ALMOST_EQ (obj->gain_db, -20.0, 0.5));
  agc_destroy (obj);

  /* ---- linear-in-dB: a quiet and a loud input settle to the same
          level within the same sample budget.  A level-dependent loop
          would leave one of them far from ref. ---- */
  agc_state_t *lo    = agc_create (0.0, 0.0025, 0.05);
  agc_state_t *hi    = agc_create (0.0, 0.0025, 0.05);
  double       lo_db = run_const (lo, dir * 0.01f, 4000);  /* -40 dB input */
  double       hi_db = run_const (hi, dir * 100.0f, 4000); /* +40 dB input */
  CHECK (ALMOST_EQ (lo_db, 0.0, 0.5));
  CHECK (ALMOST_EQ (hi_db, 0.0, 0.5));
  CHECK (ALMOST_EQ (lo_db, hi_db, 0.5));
  agc_destroy (lo);
  agc_destroy (hi);

  /* ---- non-zero reference: output converges to ref_db, not 0 dB ---- */
  agc_state_t *r    = agc_create (-6.0, 0.0025, 0.05);
  double       r_db = run_const (r, dir * 3.0f, 4000);
  CHECK (ALMOST_EQ (r_db, -6.0, 0.5));
  agc_destroy (r);

  /* ---- fast-math approximations agree with the exact functions ---- */
  CHECK (ALMOST_EQ (agc_exp10_ (0.0), 1.0, 1e-3));
  CHECK (ALMOST_EQ (agc_exp10_ (2.0), 100.0, 0.2));
  CHECK (ALMOST_EQ (agc_exp10_ (-1.5), 0.0316227766, 1e-3));
  CHECK (ALMOST_EQ (agc_log10_ (1.0), 0.0, 1e-3));
  CHECK (ALMOST_EQ (agc_log10_ (100.0), 2.0, 1e-2));
  CHECK (ALMOST_EQ (agc_log10_ (0.001), -3.0, 1e-2));

  /* ---- decimated steps() converges to the same gain as a per-sample
          step() loop: the block-rate control loop reaches the same
          steady state, just by a coarser path. ---- */
  {
    static float complex in[3000];
    static float complex blk[3000];
    agc_state_t         *a = agc_create (0.0, 0.005, 0.1);
    agc_state_t         *b = agc_create (0.0, 0.005, 0.1);
    for (size_t i = 0; i < 3000; i++)
      in[i] = dir * 4.0f;
    agc_steps (a, in, blk, 3000);
    for (size_t i = 0; i < 3000; i++)
      (void)agc_step (b, in[i]);
    CHECK (ALMOST_EQ (a->gain_db, b->gain_db, 0.3));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- steps() supports in-place operation (output aliases input) ---- */
  {
    float complex buf[64], ref[64];
    agc_state_t  *a = agc_create (0.0, 0.005, 0.1);
    agc_state_t  *b = agc_create (0.0, 0.005, 0.1);
    for (size_t i = 0; i < 64; i++)
      buf[i] = dir * 5.0f;
    agc_steps (b, buf, ref, 64);
    agc_steps (a, buf, buf, 64);
    for (size_t i = 0; i < 64; i++)
      CHECK (ALMOST_EQ_C (buf[i], ref[i], 1e-6f));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- decimation factor is configurable (8 / 16 / 32); every setting
          still converges the output to the reference ---- */
  {
    static float complex in[4000];
    static float complex blk[4000];
    size_t               decims[3] = { 8, 16, 32 };
    for (size_t i = 0; i < 4000; i++)
      in[i] = dir * 8.0f;
    for (int di = 0; di < 3; di++)
      {
        agc_state_t *a = agc_create (0.0, 0.002, 0.05);
        CHECK (a->decim == AGC_DECIM_DEFAULT); /* create() default */
        a->decim = decims[di];
        agc_steps (a, in, blk, 4000);
        double pw = (double)crealf (blk[3999]) * crealf (blk[3999])
                    + (double)cimagf (blk[3999]) * cimagf (blk[3999]);
        CHECK (ALMOST_EQ (10.0 * log10 (pw), 0.0, 0.5));
        agc_destroy (a);
      }
  }

  /* ---- reset restores post-create state; config is preserved ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    run_const (s, dir * 50.0f, 2000); /* perturb the loop */
    CHECK (fabs (s->gain_db) > 1.0);  /* loop has clearly moved */
    agc_reset (s);
    CHECK (ALMOST_EQ (s->gain_db, 0.0, 1e-6));
    CHECK (ALMOST_EQ (s->p_avg, 1.0, 1e-6));
    CHECK (ALMOST_EQ (s->ref_db, 0.0, 1e-6));
    CHECK (ALMOST_EQ (s->loop_bw, 0.0025, 1e-6));
    CHECK (ALMOST_EQ (s->alpha, 0.05, 1e-6));
    CHECK (ALMOST_EQ (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6));
    agc_destroy (s);
  }

  /* ---- applied-gain telemetry: agc_get_applied_gain_db reports the gain
          the signal last saw.  At create it is unity (0 dB); at
          convergence it equals the commanded gain_db. ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), 0.0, 1e-6));
    run_const (s, dir * 10.0f, 4000);
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), s->gain_db, 0.5));
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), -20.0, 0.5));
    agc_destroy (s);
  }

  /* ---- output clip: square clip (I and Q independent), applied to the
          output only — it does not feed the detector ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    CHECK (ALMOST_EQ (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6)); /* default */
    s->clip_db = 6.0; /* L = 10^(6/20) ~ 1.995 */
    double L   = pow (10.0, 6.0 / 20.0);
    /* first step: gain is exactly unity, so output = clip(x).  re (5)
       exceeds L and clamps; im (1) is below L and is kept unchanged —
       proving the clip is square, not a circular magnitude limit. */
    float complex y = agc_step (s, 5.0f + 1.0f * I);
    CHECK (ALMOST_EQ (crealf (y), L, 0.02));
    CHECK (ALMOST_EQ (cimagf (y), 1.0, 1e-6));
    agc_destroy (s);
  }

  /* ---- clipping never perturbs the loop: the detector measures the
          unclipped signal, so gain_db evolves identically whether or
          not a clip is engaged ---- */
  {
    agc_state_t *a = agc_create (0.0, 0.0025, 0.05);
    agc_state_t *b = agc_create (0.0, 0.0025, 0.05);
    b->clip_db     = -3.0; /* aggressive clip on b only */
    for (size_t i = 0; i < 4000; i++)
      {
        (void)agc_step (a, dir * 10.0f);
        (void)agc_step (b, dir * 10.0f);
      }
    CHECK (ALMOST_EQ (a->gain_db, b->gain_db, 1e-9));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- agc_steps() square-clips its block output too ---- */
  {
    static float complex in[256], out[256];
    for (size_t i = 0; i < 256; i++)
      in[i] = dir * 50.0f;
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    s->clip_db     = 0.0; /* L = 10^0 = 1.0 */
    agc_steps (s, in, out, 256);
    for (size_t i = 0; i < 256; i++)
      {
        CHECK (fabsf (crealf (out[i])) <= 1.0f + 1e-3f);
        CHECK (fabsf (cimagf (out[i])) <= 1.0f + 1e-3f);
      }
    agc_destroy (s);
  }

  agc_destroy (NULL); /* must be a no-op */

  /* serializable state — POD snapshot round-trips + rejects a bad envelope.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    agc_state_t *a = agc_create (0.0, 0.0025, 0.05);
    agc_state_t *b = agc_create (0.0, 0.0025, 0.05);
    CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 50; i++)
      (void)agc_step (a, 4.0f + 0.0f * I);
    DP_STATE_ROUNDTRIP_TEST (agc, a, b);
    CHECK (agc_get_applied_gain_db (b) == agc_get_applied_gain_db (a));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* telemetry attach — records track the gain trajectory; blobs stay
   * deterministic (attachment zeroed); a live attachment survives
   * set_state; detach reverts to the no-op path. */
  {
    dp_tlm_t    *tlm = dp_tlm_create (256);
    agc_state_t *a   = agc_create (0.0, 0.0025, 0.05);
    CHECK (tlm != NULL && a != NULL);
    CHECK (agc_set_telemetry (a, tlm, "agc", 1) == DP_OK);
    CHECK (dp_tlm_probe_id (tlm, "agc.gain_db") == a->tlm.id_gain);
    CHECK (dp_tlm_probe_id (tlm, "agc.level_db") == a->tlm.id_level);
    CHECK (a->tlm.id_gain != a->tlm.id_level);

    /* TWO records per gain update (default period 1 -> per sample), and the
     * pair is (command, the level that command was answering): the last two
     * records are the current integrator value and the detector's measured
     * level, in that order. */
    for (int i = 0; i < 32; i++)
      (void)agc_step (a, 0.5f + 0.0f * I);
    dp_tlm_rec_t recs[128];
    size_t       n = dp_tlm_read (tlm, 128, recs, 128);
    CHECK (n == 64);
    CHECK (recs[n - 2].probe == a->tlm.id_gain);
    CHECK (recs[n - 2].value == (float)a->gain_db);
    CHECK (recs[n - 1].probe == a->tlm.id_level);
    CHECK (recs[n - 1].value
           == (float)(10.0 * agc_log10_ (a->p_avg + AGC_POWER_FLOOR)));

    /* level_db is the loop's INPUT and is zero-referenced against ref_db:
     * driving a -6 dB signal (0.5 amplitude) toward a 0 dB reference, the
     * measured level must close on ref_db while the gain climbs away from 0.
     * This is the property that makes settling readable without knowing the
     * input level -- gain_db alone settles to an unknown offset. */
    {
      agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
      dp_tlm_t    *t = dp_tlm_create (1 << 13);
      CHECK (s != NULL && t != NULL);
      CHECK (agc_set_telemetry (s, t, "agc", 1) == DP_OK);
      for (int i = 0; i < 4096; i++)
        (void)agc_step (s, 0.5f + 0.0f * I);
      double lvl = 10.0 * agc_log10_ (s->p_avg + AGC_POWER_FLOOR);
      CHECK (fabs (lvl - s->ref_db) < 0.1);  /* level -> reference   */
      CHECK (fabs (s->gain_db - 6.0) < 0.1); /* gain -> +6 dB        */
      /* And the probe carried it: the level stream must END nearer the
         reference than it STARTED, which a mis-wired probe (emitting the
         gain twice, say) would not satisfy -- the gain moves the other way. */
      dp_tlm_rec_t r[4];
      size_t       got = 0, first_lvl = 0, last_lvl = 0;
      int          have_first = 0;
      while ((got = dp_tlm_read (t, 4, r, 4)) > 0)
        for (size_t i = 0; i < got; i++)
          if (r[i].probe == s->tlm.id_level)
            {
              if (!have_first)
                {
                  first_lvl  = (size_t)(fabs ((double)r[i].value) * 1000.0);
                  have_first = 1;
                }
              last_lvl = (size_t)(fabs ((double)r[i].value) * 1000.0);
            }
      CHECK (have_first);
      CHECK (last_lvl < first_lvl); /* |level - 0 dB| shrank */
      dp_tlm_destroy (t);
      agc_destroy (s);
    }

    /* Blob determinism: an attached and a detached instance with the
     * same running state serialize byte-identically. */
    agc_state_t *d = agc_create (0.0, 0.0025, 0.05);
    CHECK (d != NULL);
    *d              = *a;
    d->tlm.ctx      = NULL;
    d->tlm.id_gain  = 0;
    d->tlm.id_level = 0;
    uint8_t blob_a[sizeof (dp_state_hdr_t) + sizeof (agc_state_t)];
    uint8_t blob_d[sizeof (blob_a)];
    CHECK (agc_state_bytes (a) == sizeof (blob_a));
    agc_get_state (a, blob_a);
    agc_get_state (d, blob_d);
    CHECK (memcmp (blob_a, blob_d, sizeof (blob_a)) == 0);

    /* Restore into an attached instance: running state comes from the
     * blob, the receiver's own live attachment survives. */
    dp_tlm_t    *tlm2 = dp_tlm_create (256);
    agc_state_t *b    = agc_create (0.0, 0.0025, 0.05);
    CHECK (tlm2 != NULL && b != NULL);
    CHECK (agc_set_telemetry (b, tlm2, "rx.agc", 1) == DP_OK);
    CHECK (agc_set_state (b, blob_a) == DP_OK);
    CHECK (b->gain_db == a->gain_db);
    CHECK (b->tlm.ctx == tlm2);

    /* Detach: emit sites revert to the single-branch no-op. */
    CHECK (agc_set_telemetry (a, NULL, "agc", 1) == DP_OK);
    CHECK (a->tlm.ctx == NULL);
    (void)agc_step (a, 0.5f + 0.0f * I);
    CHECK (dp_tlm_read (tlm, 128, recs, 128) == 0);

    agc_destroy (d);
    agc_destroy (b);
    agc_destroy (a);
    dp_tlm_destroy (tlm2);
    dp_tlm_destroy (tlm);
  }

  /* ── §13-§18: the safety pass. Every one of these was proven by
     sabotage — reverting the guard it names turns it red. ───────────────── */
  CHECK (guard_survives_one_bad_sample ((float)(1.0 / 0.0) + 0.0f * I,
                                        "+Inf real"));
  CHECK (guard_survives_one_bad_sample (0.0f + (float)(1.0 / 0.0) * I,
                                        "+Inf imag"));
  CHECK (guard_survives_one_bad_sample ((float)(0.0 / 0.0) + 0.0f * I,
                                        "NaN real"));
  CHECK (guard_survives_one_bad_sample (1.0f + (float)(0.0 / 0.0) * I,
                                        "NaN imag"));

  CHECK (silence_leaves_the_loop_recoverable (0));
  CHECK (silence_leaves_the_loop_recoverable (1));

  CHECK (exp10_is_total ());
  CHECK (log10_is_total ());
  CHECK (applied_gain_is_finite_after_silence ());
  CHECK (saturate_contract ());

  if (_fails)
    {
      fprintf (stderr, "test_agc_core FAILED (%d)\n", _fails);
      return 1;
    }

  printf ("test_agc_core PASSED\n");
  return 0;
}
