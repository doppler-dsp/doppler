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
 *   §19 gain_update_period: zero-order hold, and P-independent convergence
 *   §20 Settling scales with loop_bw, and is SLOWER on a quiet input
 *   §21 create and reset seed p_avg from the reference, at any ref_db
 *   §22 The block gain is a linear ramp within a chunk, not a staircase
 *   §23 decim is neutral at the steady state (and NOT mid-transient)
 *   §24 A failed attach leaves the object detached
 *   §25 agc_settling_samples obeys the physics it reports
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
#include "dp_test.h"
#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

/* The assertion counter is dp_test.h's, at file scope, which is what lets the
 * §13+ section functions assert for themselves rather than funnelling a bool
 * back through main().  Their own diagnostics print and then `return 0`, so
 * the failure is counted by main's DP_CHECK on the section — no
 * DP_RECORD_FAIL is needed here.
 *
 * dp_nearf / dp_cnearf replace this file's ALMOST_EQ / ALMOST_EQ_C at the
 * same precision they had: both compared in FLOAT, so every call site keeps
 * the tolerance it was verified against.  The one that deserves a second
 * look is §12's blob-determinism check at 1e-9 — in float that is exact
 * equality after rounding, which is weaker than the bit-identical it means.
 * Left alone deliberately: tightening a tolerance is a behaviour change, not
 * part of a mechanical migration.  Filed as gh-682. */

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

/* ── §19 — gain_update_period: a documented feature with no coverage ──────
 *
 * P > 1 amortises the transcendentals: the detector and the gain-apply run
 * every sample while the loop-filter command refreshes once per P, with the
 * integrator step scaled by P so it advances at the same per-sample rate.
 * Nothing in this file ever set it to anything but the default.
 *
 * Two things to pin, and the second is the one a wrong P-scaling breaks:
 * the converged gain must not depend on P, and the applied gain must be a
 * genuine zero-order hold -- constant for P-1 samples, then stepping. */
static int
gain_update_period_holds_and_converges (void)
{
  const float complex dir = 0.6f + 0.8f * I;
  int                 ok  = 1;
  double              ref = 0.0, want = -20.0; /* 10x input -> -20 dB gain */
  size_t              Ps[3] = { 1, 8, 32 };
  for (int k = 0; k < 3; k++)
    {
      agc_state_t *s = agc_create (ref, 0.0025, 0.05);
      if (!s)
        return 0;
      s->gain_update_period = Ps[k];
      for (int n = 0; n < 8000; n++)
        (void)agc_step (s, dir * 10.0f);
      if (!(fabs (s->gain_db - want) < 0.5))
        {
          fprintf (stderr, "  §19 P=%zu converged to %g dB, not %g\n", Ps[k],
                   s->gain_db, want);
          ok = 0;
        }
      agc_destroy (s);
    }

  /* The zero-order hold itself. With P = 8 the applied gain must be
     unchanged for 7 samples and then move -- a loop that refreshed every
     sample, or one that never refreshed, both fail this. */
  {
    agc_state_t *s = agc_create (ref, 0.0025, 0.05);
    if (!s)
      return 0;
    s->gain_update_period = 8;
    for (int n = 0; n < 64; n++) /* get off the seed so the gain is moving */
      (void)agc_step (s, dir * 10.0f);
    double held    = agc_get_applied_gain_db (s);
    int    changes = 0;
    for (int n = 0; n < 8; n++)
      {
        (void)agc_step (s, dir * 10.0f);
        double now = agc_get_applied_gain_db (s);
        if (now != held)
          {
            changes++;
            held = now;
          }
      }
    /* Vacuity precondition: the gain must actually be moving, or "held
       constant" is a statement about a converged loop and proves nothing. */
    if (changes == 0)
      {
        fprintf (stderr, "  §19 precondition — gain never moved, so the "
                         "hold is not being tested\n");
        ok = 0;
      }
    else if (changes != 1)
      {
        fprintf (stderr,
                 "  §19 P=8 applied gain changed %d times in 8 "
                 "samples, expected exactly 1 (zero-order hold)\n",
                 changes);
        ok = 0;
      }
    agc_destroy (s);
  }
  return ok;
}

/* ── §20 — settling is level-DEPENDENT, and the header now says so ────────
 *
 * The old prose claimed a loud and a quiet input settle in the same number
 * of samples. The existing §3 "tested" that by running both for a fixed
 * 4000 samples and checking both ended at the reference -- which one
 * settling in 100 samples and the other in 3900 also passes. This measures
 * the time.
 *
 * Two claims, opposite in spirit, and both are real: the FILTER's time
 * constant scales as 1/(4*loop_bw), and the OBJECT is slower on a quiet
 * input because the detector measures in power. */
static long
tau_1e (double loop_bw, double alpha, double amp, long budget)
{
  const float complex dir = 0.6f + 0.8f * I;
  agc_state_t        *s   = agc_create (0.0, loop_bw, alpha);
  if (!s)
    return -1;
  double gain_inf = -20.0 * log10 (amp); /* ref 0 dB */
  double err0     = fabs (gain_inf);
  long   n        = 0;
  for (; n < budget; n++)
    {
      (void)agc_step (s, dir * (float)amp);
      if (fabs (s->gain_db - gain_inf) <= err0 / 2.718281828459045)
        break;
    }
  agc_destroy (s);
  return n < budget ? n + 1 : -1;
}

static int
settling_scales_with_bandwidth_and_depends_on_level (void)
{
  int ok = 1;

  /* (a) The filter's bandwidth scaling: halve loop_bw, double the time
     constant. Measured against the SAME input level, so the detector's
     contribution is common to both and cancels. A hard-coded step size --
     one that ignored loop_bw -- fails here and nowhere else. */
  long t_fast = tau_1e (0.005, 0.05, 10.0, 200000);
  long t_slow = tau_1e (0.0025, 0.05, 10.0, 200000);
  if (t_fast <= 0 || t_slow <= 0)
    {
      fprintf (stderr, "  §20 settling did not complete (%ld, %ld)\n", t_fast,
               t_slow);
      ok = 0;
    }
  else
    {
      double ratio = (double)t_slow / (double)t_fast;
      if (!(ratio > 1.6 && ratio < 2.4))
        {
          fprintf (stderr,
                   "  §20 halving loop_bw scaled settling by %.2fx "
                   "(%ld -> %ld), expected ~2x\n",
                   ratio, t_fast, t_slow);
          ok = 0;
        }
    }

  /* (b) The object is NOT level-independent, and the direction is fixed: a
     quiet input is SLOWER, never faster. This is the claim the header used
     to make in reverse, so a regression that "restored" level-independence
     would have to make the quiet case faster -- which this catches. */
  long t_loud  = tau_1e (0.0025, 0.01, 100.0, 500000);
  long t_quiet = tau_1e (0.0025, 0.01, 0.01, 500000);
  if (t_loud <= 0 || t_quiet <= 0)
    {
      fprintf (stderr, "  §20 level sweep did not complete (%ld, %ld)\n",
               t_loud, t_quiet);
      ok = 0;
    }
  else if (!(t_quiet > t_loud))
    {
      fprintf (stderr,
               "  §20 quiet input settled in %ld samples vs %ld loud — the "
               "detector's asymmetry has vanished; re-derive the header\n",
               t_quiet, t_loud);
      ok = 0;
    }
  return ok;
}

/* ── §21 — the seed is the REFERENCE power, at any reference ──────────────
 *
 * create() and reset() both seed p_avg with 10^(ref_db/10) so the first
 * block of on-target samples produces no transient. §1 and §9 checked this
 * at ref_db = 0 only -- where the seed is 1.0, and a seed hard-wired to 1.0
 * passes identically. Anything setting p_avg by hand must use the reference
 * power and not a measured input power, so this is the claim that keeps
 * that promise honest. */
static int
seed_is_the_reference_power (void)
{
  int    ok      = 1;
  double refs[5] = { -12.0, -6.0, 0.0, 6.0, 12.0 };
  for (int k = 0; k < 5; k++)
    {
      double       want = pow (10.0, refs[k] * 0.1);
      agc_state_t *s    = agc_create (refs[k], 0.0025, 0.05);
      if (!s)
        return 0;
      if (!(fabs (s->p_avg - want) < 1e-12 * (want > 1.0 ? want : 1.0)))
        {
          fprintf (stderr, "  §21 create(ref %g): p_avg %g, expected %g\n",
                   refs[k], s->p_avg, want);
          ok = 0;
        }
      if (!(s->g_last == 1.0))
        {
          fprintf (stderr, "  §21 create(ref %g): g_last %g, expected 1.0\n",
                   refs[k], s->g_last);
          ok = 0;
        }

      /* reset() re-seeds from the CURRENT ref_db, and clears g_last. Both
         need a vacuity precondition: perturb first, or a reset that did
         nothing at all would pass. */
      for (int n = 0; n < 500; n++)
        (void)agc_step (s, (0.6f + 0.8f * I) * 25.0f);
      if (!(fabs (s->p_avg - want) > 1e-6 && s->g_last != 1.0))
        {
          fprintf (stderr,
                   "  §21 precondition (ref %g) — the loop did not move "
                   "(p_avg %g, g_last %g)\n",
                   refs[k], s->p_avg, s->g_last);
          ok = 0;
        }
      agc_reset (s);
      if (!(fabs (s->p_avg - want) < 1e-12 * (want > 1.0 ? want : 1.0)))
        {
          fprintf (stderr, "  §21 reset(ref %g): p_avg %g, expected %g\n",
                   refs[k], s->p_avg, want);
          ok = 0;
        }
      if (!(s->g_last == 1.0 && s->gain_db == 0.0 && s->gain_phase == 0))
        {
          fprintf (stderr,
                   "  §21 reset(ref %g): g_last %g gain_db %g phase %zu\n",
                   refs[k], s->g_last, s->gain_db, s->gain_phase);
          ok = 0;
        }
      /* Configuration survives. */
      if (!(s->ref_db == refs[k]))
        {
          fprintf (stderr, "  §21 reset clobbered ref_db (%g)\n", s->ref_db);
          ok = 0;
        }
      agc_destroy (s);
    }
  return ok;
}

/* ── §22 — the block form is a first-order hold, not a staircase ──────────
 *
 * agc_steps() interpolates the applied gain linearly across each chunk so
 * there is no inter-chunk step. Measured by reading the realised gain per
 * sample straight off the output of a constant input.
 *
 * Must be measured during the TRANSIENT: at convergence the ramp is flat
 * and a staircase would be indistinguishable, which is the vacuity trap
 * here. */
static int
block_gain_is_a_first_order_hold (void)
{
  enum
  {
    N = 64,
    D = 8
  };
  int                  ok  = 1;
  const float complex  dir = 0.6f + 0.8f * I;
  static float complex in[N], out[N];
  for (size_t i = 0; i < N; i++)
    in[i] = dir * 10.0f; /* hot, so the loop is moving */

  agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
  if (!s)
    return 0;
  s->decim = D;
  agc_steps (s, in, out, N);

  /* Chunk 1 (samples 8..15) is the first with a commanded gain: chunk 0
     runs at the seed. Its per-sample gain steps must be equal. */
  double g[D + 1];
  for (int i = 0; i <= D; i++)
    g[i] = (double)cabsf (out[D + i - 1]) / (double)cabsf (in[D + i - 1]);

  /* Compare each step against the chunk's MEAN step rather than against the
     first: the gains are read back off float32 outputs, so each carries
     ~1e-7 of rounding, and chaining that to one reference exaggerates it.
     1% of the mean is still four orders clear of a staircase, which would
     put seven steps at zero and one at the whole chunk's change. */
  double mean_step = 0.0;
  for (int i = 0; i < D; i++)
    mean_step += (g[i + 1] - g[i]) / (double)D;

  /* Vacuity precondition: the chunk must actually be ramping. */
  if (!(fabs (mean_step) > 1e-6))
    {
      fprintf (stderr,
               "  §22 precondition — chunk is flat (mean step %g); a "
               "staircase would pass this\n",
               mean_step);
      ok = 0;
    }
  for (int i = 0; i < D; i++)
    {
      double step = g[i + 1] - g[i];
      if (!(fabs (step - mean_step) < 0.01 * fabs (mean_step)))
        {
          fprintf (stderr,
                   "  §22 in-chunk step %d is %g against a mean of %g — not "
                   "a linear ramp\n",
                   i, step, mean_step);
          ok = 0;
          break;
        }
    }
  agc_destroy (s);
  return ok;
}

/* ── §23 — decim is neutral, and ONE number says how neutral ─────────────
 *
 * The per-chunk coefficients are compounded internally so a caller changing
 * decim does not retune. §8 checked only that each decim reached the
 * reference by sample 4000, one decim at a time; this compares the three
 * against each other on gain_db, which is tighter.
 *
 * This section used to record a 2.53 dB mid-transient spread as a finding
 * and assert nothing about it. The cause was the loop filter integrating
 * RECTANGULARLY: the closed-loop error decays by (1 - k1) per sample with
 * k1 = 4*loop_bw, so over d samples by (1 - k1)^d, while a chunked update
 * applying d*k1 approximates that -- and (1 - d*k1) is always the smaller,
 * so a larger decim always converged faster. Compounding it with
 * ema_alpha_decim (doppler#699) cut the spread 3.3x at the same settings:
 *
 *     n     BEFORE (d*k1)              AFTER (1-(1-k1)^d)
 *           d8       d16      d32  sp    d8       d16      d32   sp
 *     64   -10.307  -10.872 -11.826 1.52 -9.998  -10.164 -10.220 0.22
 *     128  -16.563  -17.313 -19.090 2.53 -16.197 -16.459 -16.971 0.77
 *     256  -19.657  -19.946 -20.708 1.05 -19.531 -19.681 -20.158 0.63
 *     512  -19.992  -19.998 -19.989 0.01 -19.987 -19.993 -19.998 0.01
 *
 * What is LEFT is not the loop filter and not the detector's pole: the
 * applied gain ramps across each chunk (first-order hold), so a longer
 * chunk ramps over a longer span and the detector sees a different signal.
 * That residual is governed by how far the loop moves within one chunk,
 * which is the group `4*decim*loop_bw` -- the same one the header's
 * `loop_bw << 1/(4*decim)` precondition is written in. Swept:
 *
 *     4*decim*loop_bw   gain falling   gain rising
 *     0.008             0.015 dB       0.054 dB
 *     0.032             0.059 dB       0.197 dB
 *     0.128             0.281 dB       0.592 dB
 *     0.320             1.077 dB       0.909 dB  <- these settings
 *     0.640             3.734 dB       0.970 dB
 *
 * BOTH directions, because the loop is not symmetric -- the detector is
 * inside it and measures power, so a RISING gain costs ~4x a falling one
 * at the same group and is what sets the rule. A first pass measured only
 * the falling direction and set the rule 4x too loose; the Python example
 * cold-starts into a weak signal and caught it.
 *
 * So the rule is `worst spread ~ 6 * 4*decim*loop_bw`, and "well below" in
 * the precondition means <= 0.05, not <= 0.3. Both halves are asserted
 * below: the steady state agrees regardless, and INSIDE the rule the
 * transient agrees too, in the worse direction. These settings sit at
 * 0.32 -- six times the rule -- which is why the anomaly showed up here,
 * and they are still measured rather than asserted tightly. */
static int
decim_is_neutral_at_the_steady_state (void)
{
  enum
  {
    N = 512
  };
  int                  ok  = 1;
  const float complex  dir = 0.6f + 0.8f * I;
  static float complex in[N], out[N];
  for (size_t i = 0; i < N; i++)
    in[i] = dir * 10.0f;

  size_t ds[3] = { 8, 16, 32 };
  double first = 0.0;
  for (int k = 0; k < 3; k++)
    {
      agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
      if (!s)
        return 0;
      s->decim = ds[k];
      agc_steps (s, in, out, N);
      /* Vacuity: each must have actually converged, or "they agree" is a
         statement about three loops that all did nothing. */
      if (!(fabs (s->gain_db + 20.0) < 0.1))
        {
          fprintf (stderr,
                   "  §23 precondition — decim %zu is at %g dB after %d "
                   "samples, not converged on -20\n",
                   ds[k], s->gain_db, N);
          ok = 0;
        }
      if (k == 0)
        first = s->gain_db;
      else if (!(fabs (s->gain_db - first) < 0.05))
        {
          fprintf (stderr,
                   "  §23 decim %zu settled at %g dB against decim 8's %g — "
                   "the rescaling does not preserve the steady state\n",
                   ds[k], s->gain_db, first);
          ok = 0;
        }
      agc_destroy (s);
    }

  /* The rule, asserted. Inside `4*decim*loop_bw <= 0.05` the TRANSIENT
     agrees too, not merely the steady state — which is the claim a caller
     needs when choosing decim, and the one nothing checked before.
     loop_bw 2.5e-4 puts the worst case (decim 32) at 0.032, comfortably
     inside. Driven in the RISING-gain direction (a weak input), which is
     the worse of the two and therefore the one the rule is set by:
     measured 0.197 dB there against the 0.3 dB the rule promises, and
     0.059 dB if driven the other way. Sampled across the transient rather
     than at its end, because the end is where every decim agrees. */
  {
    enum
    {
      M = 4096
    };
    static float complex lin[M], lout[M];

    const double bw    = 2.5e-4;
    const double group = 4.0 * 32.0 * bw; /* the rule's own quantity */

    /* Both directions, and they do different jobs -- worth being explicit,
       because a bound that cannot fail is decoration. Measured at this
       group with the compounding reverted:

           direction        compounded   linear d*k1   discriminates
           falling (strong)   0.059 dB     0.146 dB    yes, 2.5x
           rising  (weak)     0.197 dB     0.232 dB    barely, 1.2x

       So the FALLING case at 0.1 dB is the regression detector: it is the
       one that goes red if the compounding is undone, verified by doing
       exactly that. The RISING case at 0.3 dB pins the user-facing promise
       in its worse direction, and is NOT sabotage-sensitive -- rising is
       dominated by the first-order hold and the loop's power-detector
       asymmetry, neither of which the coefficient touches. */
    struct
    {
      float       amp;
      double      bound;
      const char *what;
    } dirs[2] = { { 10.0f, 0.1, "falling (regression detector)" },
                  { 0.1f, 0.3, "rising (the rule's promise)" } };

    for (int di = 0; di < 2; di++)
      {
        for (size_t i = 0; i < M; i++)
          lin[i] = dir * dirs[di].amp;

        double worst = 0.0;
        size_t at    = 0;
        for (size_t n = 64; n <= M; n += 64)
          {
            double lo = 0.0, hi = 0.0;
            for (int k = 0; k < 3; k++)
              {
                agc_state_t *s = agc_create (0.0, bw, 0.05);
                if (!s)
                  return 0;
                s->decim = ds[k];
                agc_steps (s, lin, lout, n);
                double g = s->gain_db;
                agc_destroy (s);
                if (k == 0)
                  lo = hi = g;
                else
                  {
                    if (g < lo)
                      lo = g;
                    if (g > hi)
                      hi = g;
                  }
              }
            if (hi - lo > worst)
              {
                worst = hi - lo;
                at    = n;
              }
          }
        /* Vacuity: a transient that never moved would agree trivially. */
        if (!(worst > 0.0))
          {
            fprintf (stderr,
                     "  §23 %s — the three decims agree EXACTLY across the "
                     "whole transient, so this asserts nothing\n",
                     dirs[di].what);
            ok = 0;
          }
        if (!(worst < dirs[di].bound))
          {
            fprintf (stderr,
                     "  §23 %s: at 4*decim*loop_bw = %g (inside the <= 0.05 "
                     "rule) the mid-transient spread is %g dB at n = %zu, "
                     "over the %g dB bound\n",
                     dirs[di].what, group, worst, at, dirs[di].bound);
            ok = 0;
          }
      }
  }
  return ok;
}

/* ── §24 — a failed attach leaves the object DETACHED ─────────────────────
 *
 * agc_set_telemetry documents DP_ERR_INVALID "when the probe table cannot
 * take both probes ... the attach fails whole; the object stays detached".
 * Nothing ran it.
 *
 * The table is filled to one free slot, so the FIRST probe registers and
 * the second cannot -- which is the interesting case, because a half-armed
 * object would still have a valid id_gain and a live ctx.
 *
 * NB the other documented reject, "a prefixed name is invalid", is NOT
 * exercised here because it does not happen: an over-long prefix is
 * silently TRUNCATED by snprintf into DP_TLM_NAME_MAX, so
 * "<prefix>.gain_db" and "<prefix>.level_db" collapse to the same name, the
 * second lookup returns the first's id, and the attach reports DP_OK with
 * id_gain == id_level -- both series then interleave on one probe with no
 * way to separate them. Measured. Filed rather than pinned, because the
 * naming is shared by every object's set_telemetry and the fix is not the
 * AGC's to make. */
static int
failed_attach_leaves_it_detached (void)
{
  int       ok  = 1;
  dp_tlm_t *tlm = dp_tlm_create (256);
  if (!tlm)
    return 0;
  agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
  if (!s)
    {
      dp_tlm_destroy (tlm);
      return 0;
    }

  char nm[DP_TLM_NAME_MAX];

  /* Vacuity precondition, proved on a throwaway context because proving it
     CONSUMES the slot: after DP_TLM_MAX_PROBES-1 fillers exactly one slot
     must remain, so the AGC's first probe fits and its second cannot. That
     split is the interesting case — a half-armed object would still hold a
     valid id_gain and a live ctx. */
  {
    dp_tlm_t *probe_t = dp_tlm_create (256);
    if (!probe_t)
      {
        agc_destroy (s);
        dp_tlm_destroy (tlm);
        return 0;
      }
    for (int i = 0; i < DP_TLM_MAX_PROBES - 1; i++)
      {
        (void)snprintf (nm, sizeof nm, "filler.%d", i);
        if (dp_tlm_probe (probe_t, nm, 1) < 0)
          {
            fprintf (stderr, "  §24 could not fill the probe table (at %d)\n",
                     i);
            ok = 0;
            break;
          }
      }
    if (!(dp_tlm_probe (probe_t, "spare.one", 1) >= 0))
      {
        fprintf (stderr,
                 "  §24 precondition — no slot left after %d "
                 "fillers; the table is smaller than assumed\n",
                 DP_TLM_MAX_PROBES - 1);
        ok = 0;
      }
    if (!(dp_tlm_probe (probe_t, "spare.two", 1) < 0))
      {
        fprintf (stderr, "  §24 precondition — more than one slot was free; "
                         "the AGC's second probe would fit and the split "
                         "case is untested\n");
        ok = 0;
      }
    dp_tlm_destroy (probe_t);
  }

  for (int i = 0; i < DP_TLM_MAX_PROBES - 1; i++)
    {
      (void)snprintf (nm, sizeof nm, "filler.%d", i);
      (void)dp_tlm_probe (tlm, nm, 1);
    }

  int rc = agc_set_telemetry (s, tlm, "agc", 1);
  if (!(rc == DP_ERR_INVALID))
    {
      fprintf (stderr,
               "  §24 attach into a nearly-full table returned %d, "
               "expected DP_ERR_INVALID\n",
               rc);
      ok = 0;
    }
  if (!(s->tlm.ctx == NULL))
    {
      fprintf (stderr, "  §24 a failed attach left the object ATTACHED\n");
      ok = 0;
    }
  /* And it still runs, emitting nothing — the half that "fails whole"
     actually promises. */
  (void)agc_step (s, 1.0f + 0.0f * I);
  dp_tlm_rec_t recs[8];
  if (!(dp_tlm_read (tlm, 8, recs, 8) == 0))
    {
      fprintf (stderr, "  §24 a half-attached object emitted records\n");
      ok = 0;
    }
  agc_destroy (s);
  dp_tlm_destroy (tlm);
  return ok;
}

/* ── §25 — agc_settling_samples answers the design query ──────────────────
 *
 * It simulates the real loop rather than evaluating a fitted curve, so the
 * thing to pin is not a set of literals -- those would just restate the
 * simulation -- but the PHYSICS the answer has to obey, and the refusals.
 *
 * The physics is the same asymmetry section §20 measures directly: a quiet
 * start is slower than a loud one, settling scales as 1/loop_bw, and a
 * looser tolerance is cheaper. A helper that returned a constant, or that
 * ignored an argument, passes none of them. */
static int
settling_samples_is_the_loop_it_describes (void)
{
  int ok = 1;

  /* Refusals first: an answer here would be a plausible-looking guess. */
  struct
  {
    double      bw, alpha, err, tol;
    const char *what;
  } bad[] = {
    { 0.0, 0.05, 40.0, 0.5, "loop_bw 0" },
    { -0.01, 0.05, 40.0, 0.5, "negative loop_bw" },
    { 0.0025, 0.0, 40.0, 0.5, "alpha 0" },
    { 0.0025, 1.5, 40.0, 0.5, "alpha above 1" },
    { 0.0025, 0.05, 40.0, 0.0, "tol_db 0" },
    { 0.0025, 0.05, 0.0 / 0.0, 0.5, "NaN error" },
  };
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
      size_t got = agc_settling_samples (bad[i].bw, bad[i].alpha, bad[i].err,
                                         bad[i].tol);
      if (got != 0)
        {
          fprintf (stderr, "  §25 %s: returned %zu, expected a refusal\n",
                   bad[i].what, got);
          ok = 0;
        }
    }

  /* Already inside the tolerance is settled, and the contract says >= 1. */
  if (agc_settling_samples (0.0025, 0.05, 0.1, 0.5) != 1)
    {
      fprintf (stderr, "  §25 an already-settled loop did not report 1\n");
      ok = 0;
    }

  size_t quiet = agc_settling_samples (0.0025, 0.05, 40.0, 0.5);
  size_t loud  = agc_settling_samples (0.0025, 0.05, -40.0, 0.5);
  size_t loose = agc_settling_samples (0.0025, 0.05, 40.0, 3.0);
  size_t fast  = agc_settling_samples (0.01, 0.05, 40.0, 0.5);

  /* Vacuity precondition: every one must be a real answer, or the
     comparisons below are between refusals. */
  if (!(quiet && loud && loose && fast))
    {
      fprintf (stderr,
               "  §25 precondition — a valid query was refused "
               "(%zu, %zu, %zu, %zu)\n",
               quiet, loud, loose, fast);
      return 0;
    }

  /* A quiet start is the slow direction — the detector's concave log. */
  if (!(quiet > loud))
    {
      fprintf (stderr,
               "  §25 a +40 dB start took %zu against a -40 dB start's %zu; "
               "the asymmetry has vanished or reversed\n",
               quiet, loud);
      ok = 0;
    }
  /* A looser bar is cheaper. Catches a helper ignoring tol_db. */
  if (!(loose < quiet))
    {
      fprintf (stderr, "  §25 tol_db 3.0 cost %zu against 0.5's %zu\n", loose,
               quiet);
      ok = 0;
    }
  /* Settling scales as 1/loop_bw: 4x the bandwidth, roughly a quarter the
     samples. Loose bounds because the detector's share does not scale with
     the filter's -- that non-scaling IS the object's character, so this
     asserts the law without pretending it is exact. */
  double scale = (double)quiet / (double)fast;
  if (!(scale > 2.0 && scale < 6.0))
    {
      fprintf (stderr,
               "  §25 4x the bandwidth changed settling by %.2fx (%zu -> "
               "%zu), expected roughly 4x\n",
               scale, quiet, fast);
      ok = 0;
    }

  /* And the answer is the LOOP's, not a formula's: run the real thing to
     the reported sample count and it must genuinely be inside tol_db.
     This is what makes the helper trustworthy rather than plausible. */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    if (!s)
      return 0;
    float complex x = (float)pow (10.0, -40.0 / 20.0) * (0.6f + 0.8f * I);
    for (size_t n = 0; n < quiet; n++)
      (void)agc_step (s, x);
    if (!(fabs (s->gain_db - 40.0) <= 0.5))
      {
        fprintf (stderr,
                 "  §25 after the reported %zu samples the loop is at %g dB, "
                 "not within 0.5 of 40 — the helper does not describe the "
                 "loop it claims to\n",
                 quiet, s->gain_db);
        ok = 0;
      }
    agc_destroy (s);
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
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* create seeds gain_db = 0 dB and p_avg = reference power = 10^0 = 1 */
  DP_CHECK (dp_nearf (obj->gain_db, 0.0, 1e-6));
  DP_CHECK (dp_nearf (obj->p_avg, 1.0, 1e-6));

  /* ---- convergence: a loud input is driven down to ref_db (0 dB) ---- */
  double out_db = run_const (obj, dir * 10.0f, 4000);
  DP_CHECK (dp_nearf (out_db, 0.0, 0.5));
  /* gain settles to -20 dB: 20*log10(10) of input attenuation */
  DP_CHECK (dp_nearf (obj->gain_db, -20.0, 0.5));
  agc_destroy (obj);

  /* ---- linear-in-dB: a quiet and a loud input settle to the same
          level within the same sample budget.  A level-dependent loop
          would leave one of them far from ref. ---- */
  agc_state_t *lo    = agc_create (0.0, 0.0025, 0.05);
  agc_state_t *hi    = agc_create (0.0, 0.0025, 0.05);
  double       lo_db = run_const (lo, dir * 0.01f, 4000);  /* -40 dB input */
  double       hi_db = run_const (hi, dir * 100.0f, 4000); /* +40 dB input */
  DP_CHECK (dp_nearf (lo_db, 0.0, 0.5));
  DP_CHECK (dp_nearf (hi_db, 0.0, 0.5));
  DP_CHECK (dp_nearf (lo_db, hi_db, 0.5));
  agc_destroy (lo);
  agc_destroy (hi);

  /* ---- non-zero reference: output converges to ref_db, not 0 dB ---- */
  agc_state_t *r    = agc_create (-6.0, 0.0025, 0.05);
  double       r_db = run_const (r, dir * 3.0f, 4000);
  DP_CHECK (dp_nearf (r_db, -6.0, 0.5));
  agc_destroy (r);

  /* ---- fast-math approximations agree with the exact functions ---- */
  DP_CHECK (dp_nearf (agc_exp10_ (0.0), 1.0, 1e-3));
  DP_CHECK (dp_nearf (agc_exp10_ (2.0), 100.0, 0.2));
  DP_CHECK (dp_nearf (agc_exp10_ (-1.5), 0.0316227766, 1e-3));
  DP_CHECK (dp_nearf (agc_log10_ (1.0), 0.0, 1e-3));
  DP_CHECK (dp_nearf (agc_log10_ (100.0), 2.0, 1e-2));
  DP_CHECK (dp_nearf (agc_log10_ (0.001), -3.0, 1e-2));

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
    DP_CHECK (dp_nearf (a->gain_db, b->gain_db, 0.3));
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
      DP_CHECK (dp_cnearf (buf[i], ref[i], 1e-6f));
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
        DP_CHECK (a->decim == AGC_DECIM_DEFAULT); /* create() default */
        a->decim = decims[di];
        agc_steps (a, in, blk, 4000);
        double pw = (double)crealf (blk[3999]) * crealf (blk[3999])
                    + (double)cimagf (blk[3999]) * cimagf (blk[3999]);
        DP_CHECK (dp_nearf (10.0 * log10 (pw), 0.0, 0.5));
        agc_destroy (a);
      }
  }

  /* ---- reset restores post-create state; config is preserved ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    run_const (s, dir * 50.0f, 2000);   /* perturb the loop */
    DP_CHECK (fabs (s->gain_db) > 1.0); /* loop has clearly moved */
    agc_reset (s);
    DP_CHECK (dp_nearf (s->gain_db, 0.0, 1e-6));
    DP_CHECK (dp_nearf (s->p_avg, 1.0, 1e-6));
    DP_CHECK (dp_nearf (s->ref_db, 0.0, 1e-6));
    DP_CHECK (dp_nearf (s->loop_bw, 0.0025, 1e-6));
    DP_CHECK (dp_nearf (s->alpha, 0.05, 1e-6));
    DP_CHECK (dp_nearf (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6));
    agc_destroy (s);
  }

  /* ---- applied-gain telemetry: agc_get_applied_gain_db reports the gain
          the signal last saw.  At create it is unity (0 dB); at
          convergence it equals the commanded gain_db. ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (dp_nearf (agc_get_applied_gain_db (s), 0.0, 1e-6));
    run_const (s, dir * 10.0f, 4000);
    DP_CHECK (dp_nearf (agc_get_applied_gain_db (s), s->gain_db, 0.5));
    DP_CHECK (dp_nearf (agc_get_applied_gain_db (s), -20.0, 0.5));
    agc_destroy (s);
  }

  /* ---- output clip: square clip (I and Q independent), applied to the
          output only — it does not feed the detector ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (dp_nearf (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6)); /* default */
    s->clip_db = 6.0; /* L = 10^(6/20) ~ 1.995 */
    double L   = pow (10.0, 6.0 / 20.0);
    /* first step: gain is exactly unity, so output = clip(x).  re (5)
       exceeds L and clamps; im (1) is below L and is kept unchanged —
       proving the clip is square, not a circular magnitude limit. */
    float complex y = agc_step (s, 5.0f + 1.0f * I);
    DP_CHECK (dp_nearf (crealf (y), L, 0.02));
    DP_CHECK (dp_nearf (cimagf (y), 1.0, 1e-6));
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
    DP_CHECK (dp_nearf (a->gain_db, b->gain_db, 1e-9));
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
        DP_CHECK (fabsf (crealf (out[i])) <= 1.0f + 1e-3f);
        DP_CHECK (fabsf (cimagf (out[i])) <= 1.0f + 1e-3f);
      }
    agc_destroy (s);
  }

  agc_destroy (NULL); /* must be a no-op */

  /* serializable state — POD snapshot round-trips + rejects a bad envelope.
   * (Moved above the final DP_TEST_END: this block used to sit after the
   * epilogue, so its own failures could never fail the test.) */
  {
    agc_state_t *a = agc_create (0.0, 0.0025, 0.05);
    agc_state_t *b = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 50; i++)
      (void)agc_step (a, 4.0f + 0.0f * I);
    DP_STATE_ROUNDTRIP_TEST (agc, a, b);
    DP_CHECK (agc_get_applied_gain_db (b) == agc_get_applied_gain_db (a));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* telemetry attach — records track the gain trajectory; blobs stay
   * deterministic (attachment zeroed); a live attachment survives
   * set_state; detach reverts to the no-op path. */
  {
    dp_tlm_t    *tlm = dp_tlm_create (256);
    agc_state_t *a   = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (tlm != NULL && a != NULL);
    DP_CHECK (agc_set_telemetry (a, tlm, "agc", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "agc.gain_db") == a->tlm.id_gain);
    DP_CHECK (dp_tlm_probe_id (tlm, "agc.level_db") == a->tlm.id_level);
    DP_CHECK (a->tlm.id_gain != a->tlm.id_level);

    /* TWO records per gain update (default period 1 -> per sample), and the
     * pair is (command, the level that command was answering): the last two
     * records are the current integrator value and the detector's measured
     * level, in that order. */
    for (int i = 0; i < 32; i++)
      (void)agc_step (a, 0.5f + 0.0f * I);
    dp_tlm_rec_t recs[128];
    size_t       n = dp_tlm_read (tlm, 128, recs, 128);
    DP_CHECK (n == 64);
    DP_CHECK (recs[n - 2].probe == a->tlm.id_gain);
    DP_CHECK (recs[n - 2].value == (float)a->gain_db);
    DP_CHECK (recs[n - 1].probe == a->tlm.id_level);
    DP_CHECK (recs[n - 1].value
              == (float)(10.0 * agc_log10_ (a->p_avg + AGC_POWER_FLOOR)));

    /* level_db is the loop's INPUT and is zero-referenced against ref_db:
     * driving a -6 dB signal (0.5 amplitude) toward a 0 dB reference, the
     * measured level must close on ref_db while the gain climbs away from 0.
     * This is the property that makes settling readable without knowing the
     * input level -- gain_db alone settles to an unknown offset. */
    {
      agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
      dp_tlm_t    *t = dp_tlm_create (1 << 13);
      DP_CHECK (s != NULL && t != NULL);
      DP_CHECK (agc_set_telemetry (s, t, "agc", 1) == DP_OK);
      for (int i = 0; i < 4096; i++)
        (void)agc_step (s, 0.5f + 0.0f * I);
      double lvl = 10.0 * agc_log10_ (s->p_avg + AGC_POWER_FLOOR);
      DP_CHECK (fabs (lvl - s->ref_db) < 0.1);  /* level -> reference   */
      DP_CHECK (fabs (s->gain_db - 6.0) < 0.1); /* gain -> +6 dB        */
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
      DP_CHECK (have_first);
      DP_CHECK (last_lvl < first_lvl); /* |level - 0 dB| shrank */
      dp_tlm_destroy (t);
      agc_destroy (s);
    }

    /* Blob determinism: an attached and a detached instance with the
     * same running state serialize byte-identically. */
    agc_state_t *d = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (d != NULL);
    *d              = *a;
    d->tlm.ctx      = NULL;
    d->tlm.id_gain  = 0;
    d->tlm.id_level = 0;
    uint8_t blob_a[sizeof (dp_state_hdr_t) + sizeof (agc_state_t)];
    uint8_t blob_d[sizeof (blob_a)];
    DP_CHECK (agc_state_bytes (a) == sizeof (blob_a));
    agc_get_state (a, blob_a);
    agc_get_state (d, blob_d);
    DP_CHECK (memcmp (blob_a, blob_d, sizeof (blob_a)) == 0);

    /* Restore into an attached instance: running state comes from the
     * blob, the receiver's own live attachment survives. */
    dp_tlm_t    *tlm2 = dp_tlm_create (256);
    agc_state_t *b    = agc_create (0.0, 0.0025, 0.05);
    DP_CHECK (tlm2 != NULL && b != NULL);
    DP_CHECK (agc_set_telemetry (b, tlm2, "rx.agc", 1) == DP_OK);
    DP_CHECK (agc_set_state (b, blob_a) == DP_OK);
    DP_CHECK (b->gain_db == a->gain_db);
    DP_CHECK (b->tlm.ctx == tlm2);

    /* Detach: emit sites revert to the single-branch no-op. */
    DP_CHECK (agc_set_telemetry (a, NULL, "agc", 1) == DP_OK);
    DP_CHECK (a->tlm.ctx == NULL);
    (void)agc_step (a, 0.5f + 0.0f * I);
    DP_CHECK (dp_tlm_read (tlm, 128, recs, 128) == 0);

    agc_destroy (d);
    agc_destroy (b);
    agc_destroy (a);
    dp_tlm_destroy (tlm2);
    dp_tlm_destroy (tlm);
  }

  /* ── §13-§18: the safety pass. Every one of these was proven by
     sabotage — reverting the guard it names turns it red. ───────────────── */
  DP_CHECK (guard_survives_one_bad_sample ((float)(1.0 / 0.0) + 0.0f * I,
                                           "+Inf real"));
  DP_CHECK (guard_survives_one_bad_sample (0.0f + (float)(1.0 / 0.0) * I,
                                           "+Inf imag"));
  DP_CHECK (guard_survives_one_bad_sample ((float)(0.0 / 0.0) + 0.0f * I,
                                           "NaN real"));
  DP_CHECK (guard_survives_one_bad_sample (1.0f + (float)(0.0 / 0.0) * I,
                                           "NaN imag"));

  DP_CHECK (silence_leaves_the_loop_recoverable (0));
  DP_CHECK (silence_leaves_the_loop_recoverable (1));

  DP_CHECK (exp10_is_total ());
  DP_CHECK (log10_is_total ());
  DP_CHECK (applied_gain_is_finite_after_silence ());
  DP_CHECK (saturate_contract ());

  /* ── §19-§24: the rest of the claim inventory — the header's prose that
     nothing ran, and the tests whose comments claimed more than their
     assertions did. ──────────────────────────────────────────────────── */
  DP_CHECK (gain_update_period_holds_and_converges ());
  DP_CHECK (settling_scales_with_bandwidth_and_depends_on_level ());
  DP_CHECK (seed_is_the_reference_power ());
  DP_CHECK (block_gain_is_a_first_order_hold ());
  DP_CHECK (decim_is_neutral_at_the_steady_state ());
  DP_CHECK (failed_attach_leaves_it_detached ());
  DP_CHECK (settling_samples_is_the_loop_it_describes ());

  DP_TEST_END ("test_agc_core");
}
