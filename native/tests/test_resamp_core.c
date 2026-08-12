/*
 * test_resamp_core.c — C-level unit tests for the polyphase resampler.
 *
 * Tests cover:
 *   §1  Lifecycle and properties (create, rate, num_phases, num_taps)
 *   §2  set_rate and reset read back (literals only — see §15, §16)
 *   §3  R == 1 is a one-arm all-pass: flat |H|, constant group delay
 *   §4  A resampled pure tone is still a pure tone, 10 rates × both paths
 *   §5  Output counts for 2× decimation, 2× interpolation, unity ctrl
 *   §6  Serializable state round-trip, decimating / interpolating / fractional
 *   §7  resamp_interp_fill == resamp_execute, and block-boundary invariance
 *   §8  execute_ctrl_push == execute_ctrl, incl. unity's neighbours
 *   §9  A single-phase bank selects arm 0; phase_inc survives rate 1.0
 *   §10 The ctrl accumulator names the arm the NEXT output reads
 *   §11 One wrap of `mu` buys one INPUT interval, not one output period
 *   §12 `mu` is steady at an exact rate, slewing at a rate error
 *   §13 resamp_dc_gain is the bank's own gain, computed, on both paths
 *   §14 resamp_destroy(NULL) is a no-op; create_custom rejects
 *   §15 set_rate preserves the accumulator and the delay line
 *   §16 reset zeroes every accumulator and every delay buffer
 *   §17 The bank's advertised 60 dB stopband and 0.4/0.6 cutoffs
 *   §17b The same bank anti-imaging: the artifact floor for R >= 1
 *   §18 execute_ctrl rides the interpolator at EVERY rate
 *   §20 interp_inputs_needed is exact at every rate, not just integer ones
 *
 * Sections numbered §10 onward were added by the validation campaign, which
 * enumerated resamp_core.h's prose claims and asked of each whether anything
 * ran it. Three public entry points had ZERO mentions in this file
 * (resamp_get_ctrl_acc, resamp_dc_gain, resamp_destroy(NULL)) and two more
 * were pinned only at their literals — the comment claimed more than the
 * assertion did, which is prose wearing a test's clothes. Each new section was
 * proven by sabotage before being trusted.
 *
 * §10-§12 measure the control accumulator against an EXTERNAL truth, never
 * against the other entry point. That is deliberate: §8 is a consistency test,
 * structurally blind to any defect both paths share, and it passed throughout
 * the period the ctrl accumulator was running the decimator's recurrence on
 * the interpolator's structure.
 */

#include "dp_test.h"
#include "resamp/resamp_impl.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* File-scope so the §10+ section functions can CHECK for themselves rather
   than funnelling a bool back through main(). */

/* ── The two resampling gates ────────────────────────────────────────────
 *
 * They are BOTH needed, and mutation testing shows why: their sensitivities
 * are disjoint.
 *
 *   tone purity      catches the ACCUMULATOR.  Blind to any fixed LTI
 *                    response -- a constant delay, a ripple, a whole filter.
 *   R == 1 all-pass  catches the boundary, the bank origin and the arm
 *                    family.  Blind to the accumulator, because at unity
 *                    1/R == R and the interpolator's and decimator's
 *                    recurrences are the same one.
 *
 * That second blindness is not a curiosity, it is the history: running the
 * decimator's accumulator on the interpolator's structure cost 55-60 dB at
 * every non-unity rate and every unity-rate test in the suite still passed.
 * Never accept a unity-rate result as evidence about the accumulator.
 */

enum
{
  GATE_N    = 2048, /* inputs per probe */
  GATE_SKIP = 64    /* filter transient discarded before projecting */
};

/* Run `rate` over a tone, through either entry point, into `y`. */
static size_t
gate_run (double rate, double f0, int use_ctrl, float _Complex *y, size_t cap)
{
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0;
  size_t t = 0;
  for (size_t i = 0; i < (size_t)GATE_N; i++)
    {
      double ph        = 2.0 * M_PI * f0 * (double)i;
      float _Complex x = CMPLXF ((float)cos (ph), (float)sin (ph));
      float _Complex b[32];
      size_t g;
      if (use_ctrl)
        g = resamp_execute_ctrl_push (r, x, 0.0, b, 32);
      else
        g = resamp_execute (r, &x, 1, b, 32);
      for (size_t j = 0; j < g && t < cap; j++)
        y[t++] = b[j];
    }
  resamp_destroy (r);
  return t;
}

/* Least-squares projection onto the tone the output must be. */
/* `m0` is the ABSOLUTE output index of y[0].  It must be carried, not
 * assumed zero: the discarded transient would otherwise appear as an extra
 * GATE_SKIP samples of delay in any phase comparison across frequency. */
static double _Complex gate_project (const float _Complex *y, size_t n,
                                     double f_out, size_t m0)
{
  double _Complex num = 0.0, den = 0.0;
  for (size_t m = 0; m < n; m++)
    {
      double _Complex ref = cexp (I * 2.0 * M_PI * f_out * (double)(m0 + m));
      num += (double _Complex)y[m] * conj (ref);
      den += ref * conj (ref);
    }
  return num / den;
}

/* A resampled pure tone must still be a pure tone.  Residual, in dB.
 * Needs no timing convention, so it cannot beg the question a comparison
 * against a reference implementation would. */
static double
gate_tone_evm_db (double rate, int use_ctrl)
{
  static float _Complex y[8 * GATE_N];
  const double f0 = 0.05;
  size_t       t  = gate_run (rate, f0, use_ctrl, y, sizeof y / sizeof y[0]);
  if (t <= (size_t)GATE_SKIP + 64)
    return 0.0; /* nothing produced: caller's count check will catch it */
  double _Complex a
      = gate_project (y + GATE_SKIP, t - GATE_SKIP, f0 / rate, GATE_SKIP);
  double err = 0.0, sig = 0.0;
  for (size_t m = GATE_SKIP; m < t; m++)
    {
      double _Complex ref
          = a * cexp (I * 2.0 * M_PI * (f0 / rate) * (double)m);
      double _Complex e = (double _Complex)y[m] - ref;
      err += creal (e * conj (e));
      sig += creal (ref * conj (ref));
    }
  return 10.0 * log10 (err / sig);
}

/* Serializable-state round-trip: split a stream at `cut`, hand the resampler's
 * state to a fresh instance (same rate), and resume — the concatenated output
 * must equal an uninterrupted run bit-for-bit.  Returns 1 on success. */
static int
rt_resamp (double rate)
{
  enum
  {
    L   = 400,
    CAP = 1024
  };
  const size_t cut = 157; /* odd → mid-fractional-phase split */
  float _Complex in[L], outA[CAP], outB[CAP];
  for (size_t i = 0; i < (size_t)L; i++)
    {
      double ph = 2.0 * M_PI * 0.031 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }

  resamp_state_t *ra = resamp_create (rate);
  size_t          nA = resamp_execute (ra, in, L, outA, CAP);
  resamp_destroy (ra);

  resamp_state_t *r1   = resamp_create (rate);
  size_t          nB   = resamp_execute (r1, in, cut, outB, CAP);
  size_t          sb   = resamp_state_bytes (r1);
  void           *blob = malloc (sb);
  resamp_get_state (r1, blob);
  resamp_destroy (r1);

  resamp_state_t *r2 = resamp_create (rate);
  int             ok = (resamp_set_state (r2, blob) == DP_OK);
  /* standard envelope: a magic-clobbered blob is rejected, r2 untouched */
  ((char *)blob)[0] ^= (char)0xFF;
  ok = ok && (resamp_set_state (r2, blob) == DP_ERR_INVALID);
  ((char *)blob)[0] ^= (char)0xFF;
  nB += resamp_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
  resamp_destroy (r2);
  free (blob);

  ok = ok && (nA == nB);
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      ok = 0;
  return ok;
}

/* resamp_interp_fill must reproduce the interpolation branch of
 * resamp_execute() bit-for-bit (both call the same per-output kernel), and a
 * single fill of M outputs must equal M single-output fills fed on demand
 * (block-boundary invariance) — the two properties the polyphase pulse shaper
 * relies on for step()==steps(). Uses an integer-rate custom bank so the
 * overflow count (inputs_needed) is exact. Returns 1 on success. */
static int
eq_interp_fill (size_t nphases, size_t ntaps)
{
  enum
  {
    BIG = 512,
    M   = 300
  };
  float *bank = malloc (nphases * ntaps * sizeof (float));
  for (size_t p = 0; p < nphases; p++)
    for (size_t t = 0; t < ntaps; t++)
      bank[p * ntaps + t]
          = (float)sin (0.3 * (double)(p + 1) * (double)(t + 1));
  float _Complex in[BIG], out_ref[M], out_a[M], out_b[M];
  for (size_t i = 0; i < (size_t)BIG; i++)
    {
      double ph = 2.0 * M_PI * 0.017 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }

  /* Reference: interpolation branch of resamp_execute with ample input. */
  resamp_state_t *rr
      = resamp_create_custom (nphases, ntaps, bank, (double)nphases);
  size_t nref = resamp_execute (rr, in, BIG, out_ref, M);
  resamp_destroy (rr);
  int ok = (nref == (size_t)M);

  /* A: one fill of M outputs. Consumes exactly inputs_needed. */
  resamp_state_t *ra
      = resamp_create_custom (nphases, ntaps, bank, (double)nphases);
  size_t need = resamp_interp_inputs_needed (ra, M);
  size_t ca   = resamp_interp_fill (ra, in, out_a, M);
  resamp_destroy (ra);
  ok = ok && (ca == need);
  for (size_t i = 0; i < (size_t)M; i++)
    ok = ok && crealf (out_a[i]) == crealf (out_ref[i])
         && cimagf (out_a[i]) == cimagf (out_ref[i]);

  /* B: M single-output fills, fed on demand (the synth's step() model). */
  resamp_state_t *rb
      = resamp_create_custom (nphases, ntaps, bank, (double)nphases);
  size_t xi = 0;
  for (size_t i = 0; i < (size_t)M; i++)
    xi += resamp_interp_fill (rb, in + xi, out_b + i, 1);
  resamp_destroy (rb);
  ok = ok && (xi == need);
  for (size_t i = 0; i < (size_t)M; i++)
    ok = ok && crealf (out_b[i]) == crealf (out_ref[i])
         && cimagf (out_b[i]) == cimagf (out_ref[i]);

  free (bank);
  return ok;
}

/* A composite rate a HAIR above unity must still consume its input.
 *
 * `1/delta` just below 1.0 rounds its fractional part up to a full period,
 * and a bare cast of 2^32 to uint32_t is undefined (C99 6.3.1.4) — on x86 it
 * yields 0, which reads as "no fraction", while `floor(1/delta)` is 0 and so
 * says "no whole interval" either. ctrl_debt came out 0, no input was ever
 * loaded, and the call emitted max_out copies of one sample off an unchanged
 * delay line. The rounded-up fraction IS one more whole interval and is
 * carried now.
 *
 * The window is `delta` in (1.0, 1.0 + ~1.16e-10], which sounds unreachable
 * and is not: a Doppler ramp through zero sweeps a receiver's composite rate
 * straight across it. It cost async_dsss_receiver_spec_demo its lock at the
 * time of closest approach — tracking cleanly to within tens of Hz, then
 * -350 Hz and lock_metric -0.109 the moment the offset crossed zero.
 *
 * Asserted on the OUTPUT COUNT and on variation, not on sample values: at
 * `delta ~ 1` one output per input is the whole contract, and a stalled load
 * shows up as both a count blow-out and a constant. Returns 1 when ok. */
static int
ctrl_near_unity_consumes_input (void)
{
  enum
  {
    L   = 512,
    CAP = 8192
  };
  /* Straddle the window: inside it, at its edges, and well outside on both
     sides. The negative side is the control — it never had the defect, so a
     fix that broke it would show here. */
  static const double dev[] = { 0.0,     2.3e-16, 1.0e-12, 1.0e-11,  5.0e-11,
                                1.0e-10, 1.0e-9,  1.0e-6,  -1.0e-10, -1.0e-6 };

  float _Complex in[L], out[CAP];
  double ctrl[L];
  for (size_t i = 0; i < (size_t)L; i++)
    {
      double ph = 2.0 * M_PI * 0.037 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }

  int ok = 1;
  for (size_t k = 0; k < sizeof dev / sizeof dev[0]; k++)
    {
      for (size_t i = 0; i < (size_t)L; i++)
        ctrl[i] = dev[k];

      resamp_state_t *r = resamp_create (1.0);
      if (!r)
        return 0;
      size_t n = resamp_execute_ctrl (r, in, ctrl, L, out, CAP);
      resamp_destroy (r);

      /* One output per input, give or take the boundary sample. A stall
         runs to CAP. */
      if (n < (size_t)L - 2 || n > (size_t)L + 2)
        {
          fprintf (stderr,
                   "  ctrl dev %+.3e: %zu outputs from %d inputs "
                   "(expected ~%d)\n",
                   dev[k], n, L, L);
          ok = 0;
          continue;
        }
      /* And it must be a signal, not one sample repeated. */
      size_t repeats = 0;
      for (size_t i = 1; i < n; i++)
        if (crealf (out[i]) == crealf (out[i - 1])
            && cimagf (out[i]) == cimagf (out[i - 1]))
          repeats++;
      if (repeats > n / 8)
        {
          fprintf (stderr, "  ctrl dev %+.3e: %zu/%zu outputs repeat\n",
                   dev[k], repeats, n);
          ok = 0;
        }
    }
  return ok;
}

/* resamp_execute_ctrl_push (one input at a time) must reproduce the block
 * resamp_execute_ctrl on the same (in, ctrl[]) bit-for-bit — the property a
 * closed timing loop relies on to steer the strobe per output. Returns 1 ok.
 */
static int
eq_ctrl_push (double rate)
{
  enum
  {
    L   = 400,
    CAP = 1024
  };
  float _Complex in[L], out_ref[CAP], out_push[CAP];
  double ctrl[L];
  for (size_t i = 0; i < (size_t)L; i++)
    {
      double ph = 2.0 * M_PI * 0.023 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
      /* a slowly-varying rate deviation, both signs */
      ctrl[i] = 0.01 * sin (0.05 * (double)i);
    }

  resamp_state_t *rb   = resamp_create (rate);
  size_t          nref = resamp_execute_ctrl (rb, in, ctrl, L, out_ref, CAP);
  resamp_destroy (rb);

  resamp_state_t *rp = resamp_create (rate);
  size_t          np = 0;
  for (size_t i = 0; i < (size_t)L && np < CAP; i++)
    np += resamp_execute_ctrl_push (rp, in[i], ctrl[i], out_push + np,
                                    CAP - np);
  resamp_destroy (rp);

  int ok = (np == nref);
  for (size_t i = 0; i < nref && i < np; i++)
    if (crealf (out_ref[i]) != crealf (out_push[i])
        || cimagf (out_ref[i]) != cimagf (out_push[i]))
      ok = 0;
  return ok;
}

/* ── §10 — `mu` is in [0, 1), and it names the arm the NEXT output reads ──
 *
 * resamp_get_ctrl_acc() is the control port's only observable, and before the
 * validation campaign nothing ran it: zero mentions in this file. The header
 * claims the value is in [0, 1) and that it identifies a polyphase arm as
 * floor(mu * num_phases).
 *
 * The arm is made OBSERVABLE rather than inferred. A one-tap bank whose arm p
 * holds the single tap (p + 1) turns the dot product into
 * `delay[0] * (arm + 1)`, so a constant unit input makes every output the gain
 * of the arm that produced it — the index is read off the output, with no
 * reference implementation and no timing convention to beg the question.
 *
 * WHICH output mu names is the substance, and it is where the header was
 * wrong. The accumulator advances AFTER the emit (resamp_core.c: the dot
 * product reads get_branch(s, ctrl_phase), and only then does ctrl_phase +=
 * frac), so on return mu names the arm the NEXT output will read. The header
 * said "the arm the last output read", and conceded the true reading in its
 * next paragraph as though it were a peculiarity of a decimating terminal
 * stage. It holds at every rate, which is what the sweep below covers. */
static int
ctrl_acc_names_next_arm (double rate)
{
  enum
  {
    P     = 16,
    CALLS = 400
  };
  float bank[P];
  for (int p = 0; p < P; p++)
    bank[p] = (float)(p + 1);

  resamp_state_t *r = resamp_create_custom (P, 1, bank, rate);
  if (!r)
    return 0;

  int ok        = 1;
  int predicted = -1; /* the arm floor(mu * P) said would come next */
  for (int k = 0; k < CALLS; k++)
    {
      float _Complex o[8];
      size_t g = resamp_execute_ctrl_push (r, CMPLXF (1.0f, 0.0f), 0.0, o, 8);
      if (g && predicted >= 0)
        {
          int arm = (int)lrintf (crealf (o[0])) - 1;
          if (arm != predicted)
            {
              fprintf (stderr,
                       "  §10 rate=%.3f call %d: arm %d, mu predicted %d\n",
                       rate, k, arm, predicted);
              ok = 0;
            }
        }
      double mu = resamp_get_ctrl_acc (r);
      if (!(mu >= 0.0 && mu < 1.0))
        {
          fprintf (stderr, "  §10 rate=%.3f: mu %.17g outside [0,1)\n", rate,
                   mu);
          ok = 0;
        }
      predicted = (int)floor (mu * (double)P);
    }
  resamp_destroy (r);
  return ok;
}

/* ── §11 — a wrap of `mu` is one INPUT interval, not one output period ───
 *
 * The counting law, and it holds independently of the bank, the tone and the
 * filtering: every output owes floor(1/rate) whole input intervals, plus one
 * more each time the fractional accumulator wraps. The push form consumes
 * exactly one input per call, so
 *
 *     calls == outputs * floor(1/rate) + wraps - debt still outstanding
 *
 * with the outstanding debt bounded by one output's worth. That fixes what a
 * wrap COSTS, which the header stated in a unit it cannot mean: a wrap buys
 * one extra INPUT interval, and an output period is 1/rate of them, so the
 * two coincide only at unity.
 *
 * Decimating rates only. Above unity an emit is followed by at most one
 * consumption, so a call can wrap at most once and the identity degenerates
 * into `calls == wraps`, which would pass against almost anything. */
static int
ctrl_wrap_is_one_input (double rate)
{
  enum
  {
    CALLS = 4000
  };
  if (rate >= 1.0)
    return 0;
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0;

  size_t outs = 0, wraps = 0;
  double prev = resamp_get_ctrl_acc (r);
  for (int k = 0; k < CALLS; k++)
    {
      float _Complex o[8];
      outs += resamp_execute_ctrl_push (r, CMPLXF (1.0f, 0.0f), 0.0, o, 8);
      double mu = resamp_get_ctrl_acc (r);
      if (mu < prev)
        wraps++;
      prev = mu;
    }
  resamp_destroy (r);

  size_t skip    = (size_t)floor (1.0 / rate);
  long   created = (long)(outs * skip + wraps);
  long   left    = created - (long)CALLS;
  fprintf (stderr,
           "  §11 rate=%.3f: %zu outs, %zu wraps, skip %zu, debt left %ld\n",
           rate, outs, wraps, skip, left);
  return left >= 0 && left <= (long)skip + 1;
}

/* ── §12 — a steady `mu` is a settled loop, a slewing one is rate error ──
 *
 * Both halves of the header's diagnostic claim, made quantitative.
 *
 * STEADY: where the input interval is a whole number of samples the fraction
 * is exactly zero — the conversion folds into [0, 1) and truncates, so a
 * 1/rate of 2.0 becomes the phase word 0, not 2^32. mu must then be
 * bit-exactly 0.0 forever, not merely small, which is a far tighter statement
 * than "settled" and the reason this is asserted with ==.
 *
 * An assertion that a number STAYS zero is satisfied by an accessor that can
 * only ever return zero, so this one carries its vacuity precondition with it:
 * the second half steers the same rate off-exact and requires mu to move. A
 * `resamp_get_ctrl_acc` hard-wired to 0.0 takes §10, §11 and the slew case
 * below red, and without these four lines would have left this one GREEN —
 * measured, by doing exactly that.
 *
 * SLEWING: a rate a hair off one of those settles nothing. mu advances by
 * frac(1/rate) per output and wraps every 1/frac of them, so the wrap COUNT
 * is predicted rather than merely observed to be non-zero. */
static int
ctrl_acc_steady_at_exact_rate (double rate)
{
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0;
  int ok = 1;
  for (int k = 0; k < 2000; k++)
    {
      float _Complex o[8];
      resamp_execute_ctrl_push (r, CMPLXF (1.0f, 0.0f), 0.0, o, 8);
      if (resamp_get_ctrl_acc (r) != 0.0)
        {
          fprintf (stderr, "  §12 rate=%.3f: mu drifted to %.17g at %d\n",
                   rate, resamp_get_ctrl_acc (r), k);
          ok = 0;
          break;
        }
    }

  /* The precondition: mu is observable at this rate, so the zero above is
     the loop being settled and not the accessor being dead. */
  int moved = 0;
  for (int k = 0; k < 2000 && !moved; k++)
    {
      float _Complex o[8];
      resamp_execute_ctrl_push (r, CMPLXF (1.0f, 0.0f), 0.01, o, 8);
      moved = resamp_get_ctrl_acc (r) != 0.0;
    }
  if (!moved)
    {
      fprintf (stderr,
               "  §12 rate=%.3f: mu never moved under a steer — the "
               "zero above is vacuous\n",
               rate);
      ok = 0;
    }

  resamp_destroy (r);
  return ok;
}

static int
ctrl_acc_slews_at_rate_error (double rate)
{
  enum
  {
    CALLS = 8000
  };
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0;

  size_t outs = 0, wraps = 0;
  double prev = resamp_get_ctrl_acc (r);
  for (int k = 0; k < CALLS; k++)
    {
      float _Complex o[8];
      outs += resamp_execute_ctrl_push (r, CMPLXF (1.0f, 0.0f), 0.0, o, 8);
      double mu = resamp_get_ctrl_acc (r);
      if (mu < prev)
        wraps++;
      prev = mu;
    }
  resamp_destroy (r);

  double t_in     = 1.0 / rate;
  double expected = (double)outs * (t_in - floor (t_in));
  fprintf (stderr, "  §12 rate=%.4f: %zu wraps, predicted %.2f\n", rate, wraps,
           expected);
  return fabs ((double)wraps - expected) <= 1.0;
}

/* ── §13 — `resamp_dc_gain` is the bank's own gain, on BOTH paths ────────
 *
 * Three claims in one docblock, none of them run before now:
 *   (a) the default Kaiser bank's DC gain is 1.0 — the header's own @code
 *       block prints "1.000", so that doctest is a claim about this function;
 *   (b) arm 0 answers for every arm, because a polyphase bank's arms are one
 *       filter at different fractional delays and so share a DC gain;
 *   (c) it is COMPUTED, not measured — so a measurement has to agree with it,
 *       on the decimating path too, where the `rate` pre-scale and the
 *       integrate-and-dump over the whole bank are claimed to cancel.
 *
 * (b) is what justifies reading only arm 0, and it is precisely the claim a
 * measurement of arm 0 can never catch. */
static double
dc_gain_worst_arm_deviation (void)
{
  resamp_state_t *r = resamp_create (0.5);
  if (!r)
    return -1.0;
  double g0    = resamp_dc_gain (r);
  double worst = 0.0;
  for (size_t p = 0; p < r->num_phases; p++)
    {
      double s = 0.0;
      for (size_t t = 0; t < r->num_taps; t++)
        s += (double)r->bank[p * r->num_taps + t];
      if (fabs (s - g0) > worst)
        worst = fabs (s - g0);
    }
  resamp_destroy (r);
  return worst;
}

/* Measured DC response: a constant input through resamp_execute, settled. */
static double
dc_gain_measured (double rate)
{
  enum
  {
    NIN = 4096,
    CAP = 16384
  };
  static float _Complex in[NIN], out[CAP];
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0.0;
  for (size_t i = 0; i < NIN; i++)
    in[i] = CMPLXF (1.0f, 0.0f);
  size_t n = resamp_execute (r, in, NIN, out, CAP);
  resamp_destroy (r);
  if (n < 128)
    return 0.0;
  double acc = 0.0;
  size_t cnt = 0;
  for (size_t k = n / 2; k < n; k++, cnt++) /* skip the startup transient */
    acc += (double)crealf (out[k]);
  return acc / (double)cnt;
}

/* ── §14 — resamp_destroy(NULL) is a no-op, and create_custom rejects ────
 *
 * "NULL is a no-op" is a header sentence nothing executed. The custom
 * constructor's guards are in the same position: each is a documented
 * rejection with no test behind it. */
static int
lifecycle_rejects (void)
{
  const float ok_bank[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
  int         ok         = 1;

  resamp_destroy (NULL); /* must simply return */

  if (resamp_create_custom (0, 2, ok_bank, 1.0) != NULL)
    ok = 0; /* num_phases == 0 */
  if (resamp_create_custom (2, 0, ok_bank, 1.0) != NULL)
    ok = 0; /* num_taps == 0   */
  if (resamp_create_custom (2, 2, NULL, 1.0) != NULL)
    ok = 0; /* no bank         */
  if (resamp_create_custom (2, 2, ok_bank, 0.0) != NULL)
    ok = 0; /* rate == 0       */
  if (resamp_create_custom (2, 2, ok_bank, -1.0) != NULL)
    ok = 0; /* rate < 0        */
  return ok;
}

/* ── §17 — the bank's own claim: 60 dB, 0.4/0.6 pass/stop ───────────────
 *
 * `resamp_core.h` advertises the built-in bank twice — "Built-in 4096x19
 * Kaiser bank (60 dB, 0.4/0.6 pass/stop)" — and nothing ran either number.
 * It is the claim a decimating caller leans on hardest: below unity every
 * input above the output Nyquist folds back into the band, and the stopband
 * is the only thing keeping it out.
 *
 * The cutoffs are normalised to **fs_out**, not fs_in, and for R < 1 that
 * is the whole content of the claim: the filter's job is to protect the
 * LOWER of the two rates, which when decimating is the output. So a
 * passband edge quoted as 0.4 sits at 0.4 * fs_out — i.e. at 0.4 * R in
 * input-normalised terms, which is the frequency this test has to sweep
 * because the input grid is what a caller feeds.
 *
 * Getting that backwards produces the same two numbers at one rate and the
 * wrong ones everywhere else, which is why both rates below are swept: 0.4
 * and 0.6 of fs_out land at 0.20/0.30 of fs_in at R = 0.5, and at
 * 0.10/0.15 at R = 0.25. A rule read as "the bank's own grid, scaled by
 * rate" happens to agree numerically and explains nothing.
 *
 * Measured by driving a pure tone through and reading the settled output
 * magnitude — a response measurement, needing no reference filter. */
static double
band_response_db (double rate, double f0)
{
  enum
  {
    NIN = 4096,
    CAP = 16384
  };
  static float _Complex in[NIN], out[CAP];
  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 1.0; /* impossible value: caller's check will fail */
  for (size_t i = 0; i < NIN; i++)
    {
      double ph = 2.0 * M_PI * f0 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }
  size_t n = resamp_execute (r, in, NIN, out, CAP);
  resamp_destroy (r);
  if (n < 128)
    return 1.0;
  /* Settled half only, so the filter's startup is not read as response. */
  double acc = 0.0;
  size_t cnt = 0;
  for (size_t k = n / 2; k < n; k++, cnt++)
    acc += (double)cabsf (out[k]);
  double mag = acc / (double)cnt;
  return 20.0 * log10 (mag + 1e-20);
}

/* ── §17b — the same bank's OTHER face: anti-imaging, R >= 1 ─────────────
 *
 * Interpolating, the lower of the two rates is the INPUT, so the same 60 dB
 * stopband is normalised to fs_in and does an anti-imaging job. That face
 * looked unprobeable at first — its stopband lives above fs_in/2, where no
 * input tone can reach — but it does not need a swept response at all,
 * because the structure MANUFACTURES its own probe: interpolation replicates
 * the input spectrum at every multiple of fs_in, and the stopband's whole
 * purpose is to suppress those replicas. So measure the artifact floor.
 *
 * The measurement is the SPECTRAL FLOOR across the whole rejection band,
 * not the level at a few predicted image frequencies. Naming the images
 * works only at integer R: at a fractional rate they are not stationary in
 * the output grid, so a fixed-frequency projection reads their drift rather
 * than the rejection. A resampler exists to do fractional rates, so a test
 * that cannot reach them certifies the easy half. **Any artifact anywhere
 * in the band, above -60 dBc, fails** — which is a floor scan, and does not
 * care what produced the artifact or whether it sits still.
 *
 * The band follows from §17's fs_in normalisation: interpolating, the bank
 * passes to 0.4 and stops from 0.6 of fs_in, so in fs_out units everything
 * from 0.6/R up to Nyquist is rejection band.
 *
 * Two mechanics the number depends on, both chosen rather than inherited:
 *
 *   WINDOW. The fundamental sits at 0 dBc in the same record, and a
 *   rectangular window's sidelobes decay far too slowly to read a -60 dB
 *   floor beside it — the leakage would BE the measurement. A 4-term
 *   Blackman-Harris (-92 dB sidelobes) leaves the headroom.
 *
 *   GRID. Stepped at 2 DFT bins. BH's main lobe is ~8 bins wide, so a tone
 *   anywhere in the band lands within 1 bin of a grid point and reads back
 *   under a dB low; a sparser grid would let an image hide between samples,
 *   which is the one way a floor scan can lie.
 *
 * Evaluated by a recursive phasor rather than calling cexp per sample per
 * bin: same arithmetic, and it keeps the scan inside a unit test's budget.
 * File-scope rather than nested — a nested function is a GNU extension and
 * this suite also builds under clang for `make coverage`. */
static double
dft_bin_mag (const double _Complex *yw, size_t m, double f)
{
  double _Complex rot = cexp (-I * 2.0 * M_PI * f);
  double _Complex ph = 1.0, acc = 0.0;
  for (size_t i = 0; i < m; i++)
    {
      acc += yw[i] * ph;
      ph *= rot;
    }
  return cabs (acc);
}

static double
image_floor_db (double rate, double f0)
{
  enum
  {
    NIN  = 4096,
    CAP  = 65536,
    NUSE = 4096 /* windowed record actually scanned */
  };
  static float _Complex in[NIN], out[CAP];
  static double _Complex yw[NUSE];

  /* Below 1.2 the rejection band 0.6/R .. 0.5 is empty; 1.5 keeps a
     usable width. */
  if (rate < 1.5)
    return 1.0; /* impossible value: caller's check will fail */

  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 1.0;
  for (size_t i = 0; i < NIN; i++)
    {
      double ph = 2.0 * M_PI * f0 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }
  size_t n = resamp_execute (r, in, NIN, out, CAP);
  resamp_destroy (r);
  if (n < (size_t)GATE_SKIP + 1024)
    return 1.0;

  size_t m = n - GATE_SKIP;
  if (m > (size_t)NUSE)
    m = NUSE;

  /* 4-term Blackman-Harris, applied once. */
  {
    static const double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
    for (size_t i = 0; i < m; i++)
      {
        double t = 2.0 * M_PI * (double)i / (double)(m - 1);
        double w = a0 - a1 * cos (t) + a2 * cos (2.0 * t) - a3 * cos (3.0 * t);
        yw[i]    = (double _Complex)out[GATE_SKIP + i] * w;
      }
  }

  double fund = dft_bin_mag (yw, m, f0 / rate);
  if (fund <= 0.0)
    return 1.0;

  const double lo    = 0.6 / rate;
  const double step  = 2.0 / (double)m; /* 2 DFT bins */
  double       worst = -999.0;
  for (double f = lo; f <= 0.5; f += step)
    for (int sign = 0; sign < 2; sign++)
      {
        double db
            = 20.0 * log10 (dft_bin_mag (yw, m, sign ? -f : f) / fund + 1e-30);
        if (db > worst)
          worst = db;
      }
  return worst;
}

/* ── §18 — execute_ctrl rides the INTERPOLATOR at every rate ─────────────
 *
 * The header's most load-bearing structural sentence, and the one whose
 * violation cost 55-60 dB at every non-unity rate: "resamp_execute_ctrl —
 * unified, and it rides the INTERPOLATOR at every rate."
 *
 * `execute` dispatches on rate and uses the TRANSPOSED decimator below
 * unity. So the sentence has an observable consequence in both directions,
 * and asserting only one of them proves nothing:
 *
 *   rate >= 1  both are the interpolator -> bit-for-bit identical
 *   rate <  1  different structures      -> must DIFFER
 *
 * The inequality alone would be satisfied by any corruption, so it is
 * paired with a tone-purity floor: execute_ctrl must differ from `execute`
 * AND still be a correct resampler. That pairing is the test — a defect
 * that made it agree below unity, and a defect that made it garbage, are
 * caught by different halves. */
static int
ctrl_rides_interpolator (double rate)
{
  enum
  {
    NIN = 2048,
    CAP = 8192
  };
  static float _Complex in[NIN], a[CAP], b[CAP];
  static double zero[NIN];
  const double  f0 = 0.05;
  for (size_t i = 0; i < NIN; i++)
    {
      double ph = 2.0 * M_PI * f0 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
      zero[i]   = 0.0;
    }

  resamp_state_t *r1 = resamp_create (rate);
  resamp_state_t *r2 = resamp_create (rate);
  if (!r1 || !r2)
    return 0;
  size_t na = resamp_execute (r1, in, NIN, a, CAP);
  size_t nb = resamp_execute_ctrl (r2, in, zero, NIN, b, CAP);
  resamp_destroy (r1);
  resamp_destroy (r2);

  int same = (na == nb);
  for (size_t i = 0; i < na && i < nb && same; i++)
    if (a[i] != b[i])
      same = 0;

  if (rate >= 1.0)
    {
      if (!same)
        fprintf (stderr,
                 "  §18 rate=%.3f: ctrl path differs from execute at or "
                 "above unity (na %zu, nb %zu)\n",
                 rate, na, nb);
      return same;
    }

  /* Below unity the two structures must NOT coincide — and the ctrl path
     must still be a resampler, not merely different. */
  double resid = gate_tone_evm_db (rate, 1);
  fprintf (stderr, "  §18 rate=%.3f: differs=%d, ctrl residual %.1f dB\n",
           rate, !same, resid);
  return !same && resid < -60.0;
}

/* ── §20 — `interp_inputs_needed` is exact, and more broadly than claimed ─
 *
 * The streaming contract: a caller asks how many inputs a fill of `max_out`
 * outputs will consume, generates exactly that many, and calls
 * resamp_interp_fill. Over- or under-production is a desync, so "exact" is
 * the whole value of the function.
 *
 * §7 already pins it at ONE point — a custom power-of-two bank at
 * `rate == num_phases`, which is the integer interpolation factor the header
 * scopes the guarantee to:
 *
 *   "For an integer interpolation factor ... this is exact, so a caller can
 *    generate precisely this many inputs — no over- or under-production."
 *
 * Measured, it is exact far outside that scope: every rate tried, fractional
 * included, mid-stream, for arbitrary `max_out`. That follows from the
 * construction — the prediction and the fill run the SAME recurrence on the
 * same phase_inc, so they cannot disagree — and it means a caller may rely
 * on the streaming contract at any rate, which the header currently tells
 * them not to.
 *
 * Which is also why the prediction-vs-fill comparison alone is worth little:
 * it is a consistency test between two users of one recurrence, structurally
 * blind to any defect they share (a wrong phase_inc moves both). So the
 * external truth is asserted alongside it — the inputs consumed across the
 * whole run must satisfy the counting law, `inputs ~= outputs / rate`,
 * which depends on phase_inc being RIGHT rather than merely used twice. */
static int
inputs_needed_is_exact (double rate)
{
  enum
  {
    CALLS = 200,
    NIN   = 1 << 16,
    CAP   = 4096
  };
  static float _Complex in[NIN], out[CAP];
  for (size_t i = 0; i < NIN; i++)
    in[i] = CMPLXF (1.0f, 0.0f);

  resamp_state_t *r = resamp_create (rate);
  if (!r)
    return 0;

  unsigned seed   = 20250811u;
  size_t   tot_in = 0, tot_out = 0;
  int      ok = 1;
  for (int k = 0; k < CALLS && ok; k++)
    {
      /* Varied, and never the same twice: a fixed max_out would only ever
         exercise one phase alignment. */
      size_t m    = 1 + (size_t)(rand_r (&seed) % (CAP - 1));
      size_t need = resamp_interp_inputs_needed (r, m);
      if (need > NIN)
        break;
      size_t got = resamp_interp_fill (r, in, out, m);
      if (got != need)
        {
          fprintf (stderr,
                   "  §20 rate=%.3f call %d: predicted %zu inputs, "
                   "consumed %zu\n",
                   rate, k, need, got);
          ok = 0;
        }
      tot_in += got;
      tot_out += m;
    }
  resamp_destroy (r);
  if (!ok)
    return 0;

  /* The external truth: phase_inc has to be the right number, not just the
     same number in two places. One output period is 1/rate inputs, and the
     accumulated total may lag by at most the final partial period. */
  double expect = (double)tot_out / rate;
  double err    = fabs ((double)tot_in - expect);
  fprintf (stderr,
           "  §20 rate=%.3f: %zu outputs consumed %zu inputs, law says "
           "%.1f (err %.1f)\n",
           rate, tot_out, tot_in, expect, err);
  return err <= 2.0;
}

/* ── §15 — set_rate preserves the accumulator and the delay line ─────────
 *
 * The header promises "Accumulator phase and delay line are preserved"; the
 * test carrying that sentence asserted only that get_rate() read back. A
 * retune mid-stream is exactly when the promise matters — a cleared
 * accumulator is a timing jump and a cleared delay line is a transient,
 * neither of which get_rate() can see. */
static int
set_rate_preserves_state (void)
{
  resamp_state_t *r = resamp_create (2.0);
  if (!r)
    return 0;

  /* Drive it until every preserved field is non-trivial. */
  for (int k = 0; k < 37; k++)
    {
      float _Complex o[8];
      resamp_execute_ctrl_push (r, CMPLXF ((float)(k + 1), -1.0f), 0.03, o, 8);
    }

  uint32_t phase = r->phase, cph = r->ctrl_phase, cdebt = r->ctrl_debt;
  uint32_t cahead = r->ctrl_ahead, inc0 = r->phase_inc;
  size_t   head = r->delay_head, ncopy = 2 * r->delay_cap;
  float   *bank = r->bank;
  float _Complex snap[128];
  if (ncopy > 128)
    ncopy = 128;
  for (size_t i = 0; i < ncopy; i++)
    snap[i] = r->delay_buf[i];

  resamp_set_rate (r, 3.0);

  int ok = resamp_get_rate (r) == 3.0 && r->phase_inc != inc0
           && r->phase == phase && r->ctrl_phase == cph
           && r->ctrl_debt == cdebt && r->ctrl_ahead == cahead
           && r->delay_head == head && r->bank == bank;
  for (size_t i = 0; i < ncopy; i++)
    if (r->delay_buf[i] != snap[i])
      ok = 0;
  resamp_destroy (r);
  return ok;
}

/* ── §16 — reset zeroes every accumulator and every delay buffer ─────────
 *
 * Same shape as §15 and the same gap: the header promises "Zero phase
 * accumulator, ctrl accumulator, and delay line. Rate and bank are
 * preserved", and the test asserted only that the rate survived — so a reset
 * that zeroed nothing at all passed it. Both halves are checked here, and so
 * is the vacuity precondition: a reset test proves nothing if the state was
 * already zero when it ran. */
static int
reset_zeroes_state (void)
{
  resamp_state_t *r = resamp_create (0.7);
  if (!r)
    return 0;

  for (int k = 0; k < 53; k++)
    {
      float _Complex o[8];
      resamp_execute_ctrl_push (r, CMPLXF ((float)(k + 1), 0.5f), -0.02, o, 8);
      float _Complex x = CMPLXF (1.0f, (float)k);
      resamp_execute (r, &x, 1, o, 8);
    }

  /* The vacuity precondition: there is something here to zero. */
  int dirty = r->phase || r->ctrl_phase || r->ctrl_debt || r->delay_head;
  for (size_t i = 0; i < 2 * r->delay_cap; i++)
    if (r->delay_buf[i] != 0.0f)
      dirty = 1;
  double gain0 = resamp_dc_gain (r);

  resamp_reset (r);

  int ok = dirty && r->phase == 0 && r->ctrl_phase == 0 && r->ctrl_debt == 0
           && r->ctrl_ahead == 0 && r->delay_head == 0
           && resamp_get_rate (r) == 0.7 && resamp_dc_gain (r) == gain0;
  for (size_t i = 0; i < 2 * r->delay_cap; i++)
    if (r->delay_buf[i] != 0.0f)
      ok = 0;
  for (size_t i = 0; i < r->num_taps; i++)
    if (r->decim_iad[i] != 0.0f)
      ok = 0;
  for (size_t i = 0; i + 1 < r->num_taps; i++)
    if (r->decim_tfd[i] != 0.0f)
      ok = 0;
  resamp_destroy (r);
  return ok;
}

int
main (void)
{
  /* ---- create / destroy ---- */
  resamp_state_t *r = resamp_create (1.0);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;

  /* ---- properties ---- */
  DP_CHECK (resamp_get_rate (r) == 1.0);
  DP_CHECK (resamp_get_num_phases (r) == 4096);
  DP_CHECK (resamp_get_num_taps (r) == 19);

  /* ---- set_rate preserves phase ---- */
  resamp_set_rate (r, 2.0);
  DP_CHECK (resamp_get_rate (r) == 2.0);

  /* ---- reset: zeroes phase/delay, preserves rate ---- */
  resamp_reset (r);
  DP_CHECK (resamp_get_rate (r) == 2.0); /* rate must survive reset */

  resamp_destroy (r);

  /* Buffers shared by the count checks and the execute_ctrl check below. */
  static const size_t N = 64;
  size_t              n;
  float _Complex in[64], out[64];

  /* ---- R == 1 is a ONE-ARM ALL-PASS, not a pass-through ----
     Every sample gets filtered; there is no short-circuit.  At unity the
     step is one whole period, which is 0 in a phase word, so the
     accumulator never advances, ONE arm is used forever, and the path is
     that arm: a pure delay.

     The invariant is flat |H| and CONSTANT group delay -- not out == in,
     which is what the deleted memcpy used to provide and what this test
     used to assert.  Constancy is asserted against the MEAN rather than a
     literal, because the nominal legitimately differs by one sample
     between the two entry points: the block form emits before it loads,
     the push form is handed its input first.  Both are self-consistent;
     only a frequency-DEPENDENT delay would be a defect.

     Group delay by adjacent-frequency phase differencing, so there is
     nothing to unwrap.  Thresholds come from the bank's own design (a 60 dB
     Kaiser ripples ~1e-3), not an arbitrary epsilon; a wrong accumulator
     gives a 0.1-0.5 sample sawtooth, two orders clear. */
  for (int use_ctrl = 0; use_ctrl < 2; use_ctrl++)
    {
      static float _Complex y[8 * GATE_N];
      const double df = 0.005;
      double       dly[8], worst_h = 0.0, mean = 0.0;
      for (int k = 0; k < 8; k++)
        {
          double f = 0.04 * (k + 1);
          double _Complex h[2];
          for (int s = 0; s < 2; s++)
            {
              double fs = f + (s ? df / 2 : -df / 2);
              size_t t
                  = gate_run (1.0, fs, use_ctrl, y, sizeof y / sizeof y[0]);
              DP_CHECK (t == (size_t)GATE_N); /* unity is 1:1, every sample */
              h[s]
                  = gate_project (y + GATE_SKIP, t - GATE_SKIP, fs, GATE_SKIP);
            }
          double m = 0.5 * (cabs (h[0]) + cabs (h[1]));
          if (fabs (m - 1.0) > worst_h)
            worst_h = fabs (m - 1.0);
          dly[k] = -carg (h[1] * conj (h[0])) / (2.0 * M_PI * df);
          mean += dly[k] / 8.0;
        }
      double spread = 0.0;
      for (int k = 0; k < 8; k++)
        if (fabs (dly[k] - mean) > spread)
          spread = fabs (dly[k] - mean);
      fprintf (stderr,
               "  R==1 all-pass %s: |H| dev %.3e, delay %.4f +/- %.3e\n",
               use_ctrl ? "ctrl" : "free", worst_h, mean, spread);
      DP_CHECK (worst_h < 5e-3); /* all-pass */
      DP_CHECK (spread < 2e-2);  /* PURE delay: no frequency dependence */
      DP_CHECK (mean > 8.0 && mean < 12.0); /* and it is the filter's own */
    }

  /* ---- a resampled pure tone must still be a pure tone ----
     THE gate for the accumulator.  Running the decimator's accumulator on
     the interpolator's structure reads -12 to -17 dB here; the correct rule
     reads -70 to -75.  The bound sits between them with an order of margin
     on both sides.  Exact rates (0.5, 1.0) need no interpolation at all and
     land near -145, so they are covered by the same bound. */
  {
    static const double gate_rates[]
        = { 0.5, 0.6, 0.8, 0.95, 1.0, 1.05, 1.25, 1.5, 2.0, 3.0 };
    for (size_t k = 0; k < sizeof gate_rates / sizeof gate_rates[0]; k++)
      for (int use_ctrl = 0; use_ctrl < 2; use_ctrl++)
        {
          double evm = gate_tone_evm_db (gate_rates[k], use_ctrl);
          if (!(evm < -60.0))
            fprintf (stderr, "  tone purity rate=%.2f %s: %.1f dB\n",
                     gate_rates[k], use_ctrl ? "ctrl" : "free", evm);
          DP_CHECK (evm < -60.0);
        }
  }

  /* ---- 2x decimation: output count ---- */
  r = resamp_create (0.5);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;

  float _Complex in2[128], out2[64];
  for (size_t i = 0; i < 128; i++)
    in2[i] = CMPLXF (1.0f, 0.0f);

  n = resamp_execute (r, in2, 128, out2, 64);
  /* expect ~64 output samples (allow filter startup delay) */
  DP_CHECK (n >= 56 && n <= 64);
  resamp_destroy (r);

  /* ---- 2x interpolation: output count ---- */
  r = resamp_create (2.0);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;

  float _Complex in3[64], out3[132];
  for (size_t i = 0; i < 64; i++)
    in3[i] = CMPLXF (1.0f, 0.0f);

  n = resamp_execute (r, in3, 64, out3, 132);
  DP_CHECK (n >= 120 && n <= 132);
  resamp_destroy (r);

  /* ---- execute_ctrl unity rate, zero ctrl ---- */
  r = resamp_create (1.0);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;

  double ctrl[64];
  for (size_t i = 0; i < N; i++)
    {
      in[i]   = CMPLXF (1.0f, 0.0f);
      ctrl[i] = 0.0;
    }
  n = resamp_execute_ctrl (r, in, ctrl, N, out, N);
  DP_CHECK (n == N);
  resamp_destroy (r);

  /* Serializable-state round-trip across rates (decimate, interpolate,
   * non-integer) — bit-exact resume from the handed-off state blob. */
  DP_CHECK (rt_resamp (0.5)); /* decimation: decim_iad/decim_tfd path */
  DP_CHECK (rt_resamp (2.0)); /* interpolation: delay_buf path        */
  DP_CHECK (rt_resamp (0.4)); /* non-integer: fractional phase + ctrl */

  /* Streaming interpolation fill == resamp_execute, and block-invariant
   * (single M-fill == M on-demand 1-fills). pow-2 nphases → exact overflow
   * count. Both the pulse-shaper's steps() and step() paths depend on this. */
  DP_CHECK (eq_interp_fill (8, 4));
  DP_CHECK (eq_interp_fill (4, 17));
  DP_CHECK (eq_interp_fill (16, 3));

  /* Streaming control port: one-input-at-a-time execute_ctrl_push == the block
   * execute_ctrl, for decimation, interpolation, and unity — the property a
   * closed-loop timing/rate tracker relies on to steer the strobe per output.
   */
  /* A composite rate a hair above unity must not stall the load — the
   * boundary a Doppler ramp crosses at closest approach. */
  DP_CHECK (ctrl_near_unity_consumes_input ());

  /* Decimating, unity NEIGHBOURHOOD, and interpolating. The neighbourhood
   * is the part worth spelling out: 1.0 alone is the one rate where the
   * interpolator's and decimator's recurrences coincide, so it cannot
   * distinguish them, and the rates a hair either side of it are where the
   * ctrl accumulator's boundary handling actually lives. 0.923 is the
   * terminal rate a CIC(8) cascade plans at sps = 17.333 -- a real
   * operating point rather than a round number. */
  DP_CHECK (eq_ctrl_push (0.4));
  DP_CHECK (eq_ctrl_push (0.923));
  DP_CHECK (eq_ctrl_push (0.999));
  DP_CHECK (eq_ctrl_push (1.0));
  DP_CHECK (eq_ctrl_push (1.001));
  DP_CHECK (eq_ctrl_push (2.0));
  DP_CHECK (eq_ctrl_push (3.0));

  /* A SINGLE-PHASE bank must select arm 0, not shift by 32.
   *
   * num_phases == 1 gives log2_phases == 0, so get_branch()'s
   * `ph >> (32 - log2_phases)` is a 32-bit shift of a uint32_t -- undefined
   * (C99 6.5.7p3). x86 masks the count to 5 bits, so it returns `ph` itself
   * and indexes bank[ph * num_taps]: a wild read gigabytes past a one-arm
   * bank. Any non-zero phase reaches it, so rate 2.0 (phase_inc = 2^31) is
   * enough -- no other defect required.
   *
   * The real user is wfm_synth's polyphase RRC shaper at sps == 1. There the
   * rate is exactly 1.0, whose own phase_inc conversion was also undefined
   * and yielded 0, pinning `ph` at 0 and hiding this. Fixing either alone
   * turns a silently dead waveform into a segfault, which is why this is
   * pinned independently of that conversion.
   *
   * With one arm and taps {1, 0} the output is just the newest sample. */
  {
    const float     bank1[2] = { 1.0f, 0.0f };
    resamp_state_t *r1       = resamp_create_custom (1, 2, bank1, 2.0);
    DP_CHECK (r1 != NULL);
    if (r1)
      {
        DP_CHECK (r1->num_phases == 1);
        float _Complex in[4] = { 1.0f + 0.0f * I, 2.0f + 0.0f * I,
                                 3.0f + 0.0f * I, 4.0f + 0.0f * I };
        float _Complex out[8];
        size_t used = resamp_interp_fill (r1, in, out, 8);
        DP_CHECK (used <= 4);
        for (size_t i = 0; i < 8; i++)
          {
            /* Every output must be a real dot product over the delay line,
               not whatever lay past the end of a one-arm bank. */
            DP_CHECK (isfinite (crealf (out[i])));
            DP_CHECK (isfinite (cimagf (out[i])));
            DP_CHECK (fabsf (crealf (out[i])) <= 4.0f);
          }
        resamp_destroy (r1);
      }
  }

  /* The phase_inc conversion must survive rate == 1.0.
   *
   * `upsample` is `rate >= 1.0`, so rate == 1.0 takes the DIVIDE branch and
   * computes (uint32_t)(2^32 / 1.0) -- the out-of-range float->unsigned
   * conversion (C99 6.3.1.4). x86 yields 0, which is a phase_inc that never
   * advances. resamp_interp_inputs_needed() reads phase_inc directly and is
   * the observable: at unity rate, N outputs must consume ~N inputs, and a
   * zero increment reports 0 inputs for any N.
   *
   * rate == 1.0 is not a corner: it is the rate ratesync's terminal stage
   * runs at. */
  {
    resamp_state_t *r1 = resamp_create (1.0);
    DP_CHECK (r1 != NULL);
    if (r1)
      {
        size_t need = resamp_interp_inputs_needed (r1, 1000);
        DP_CHECK (need >= 999 && need <= 1000);
        /* Neighbours either side must agree to within a sample -- the
           conversion should be continuous across the branch boundary. */
        resamp_state_t *rlo = resamp_create (0.9999999);
        resamp_state_t *rhi = resamp_create (1.0000001);
        size_t          nlo = resamp_interp_inputs_needed (rlo, 1000);
        size_t          nhi = resamp_interp_inputs_needed (rhi, 1000);
        DP_CHECK (nhi >= 999 && nhi <= 1000);
        DP_CHECK (need >= nhi); /* unity consumes no fewer than faster */
        (void)nlo;
        resamp_destroy (rlo);
        resamp_destroy (rhi);
        resamp_destroy (r1);
      }
  }

  /* ── §10 — the control accumulator names the NEXT output's arm ──────── */
  DP_CHECK (ctrl_acc_names_next_arm (0.7));
  DP_CHECK (ctrl_acc_names_next_arm (0.923));
  DP_CHECK (ctrl_acc_names_next_arm (1.3));
  DP_CHECK (ctrl_acc_names_next_arm (2.5));

  /* ── §11 — one wrap buys one INPUT interval ─────────────────────────── */
  DP_CHECK (ctrl_wrap_is_one_input (0.7));
  DP_CHECK (ctrl_wrap_is_one_input (0.3));

  /* ── §12 — steady where the interval is whole, slewing where it is not ─
     0.5, 1.0 and 0.25 all give an integer 1/rate, so the fraction is exactly
     zero. 0.499 is 0.5 with a residual rate error a loop would have to
     absorb, and its wrap count is predicted rather than merely non-zero. */
  DP_CHECK (ctrl_acc_steady_at_exact_rate (0.5));
  DP_CHECK (ctrl_acc_steady_at_exact_rate (1.0));
  DP_CHECK (ctrl_acc_steady_at_exact_rate (0.25));
  DP_CHECK (ctrl_acc_slews_at_rate_error (0.499));

  /* ── §13 — dc_gain is computed, and a measurement must agree ────────── */
  {
    resamp_state_t *rg = resamp_create (0.5);
    DP_CHECK (rg != NULL);
    if (rg)
      {
        double g = resamp_dc_gain (rg);
        fprintf (stderr, "  §13 default Kaiser dc_gain %.6f\n", g);
        DP_CHECK (fabs (g - 1.0) < 1e-3); /* the header's @code says 1.000 */
        resamp_destroy (rg);
      }

    double worst = dc_gain_worst_arm_deviation ();
    fprintf (stderr, "  §13 worst arm deviation from arm 0: %.3e\n", worst);
    DP_CHECK (worst >= 0.0
              && worst < 1e-3); /* arm 0 answers for all of them */

    /* Computed vs measured, on the interpolating and the decimating path —
       the latter is where the `rate` pre-scale has to cancel. */
    for (int i = 0; i < 2; i++)
      {
        double          rate = i ? 0.5 : 2.0;
        resamp_state_t *rr   = resamp_create (rate);
        DP_CHECK (rr != NULL);
        if (!rr)
          continue;
        double want = resamp_dc_gain (rr);
        resamp_destroy (rr);
        double got = dc_gain_measured (rate);
        fprintf (stderr, "  §13 rate %.2f: computed %.6f, measured %.6f\n",
                 rate, want, got);
        DP_CHECK (fabs (got - want) < 5e-3);
      }

    /* A custom bank answers with its own tap sum, not with 1.0. */
    const float b2[8] = { 0.75f, 1.25f, 0.5f, 1.5f, 0.25f, 1.75f, 1.0f, 1.0f };
    resamp_state_t *rc = resamp_create_custom (4, 2, b2, 1.0);
    DP_CHECK (rc != NULL);
    if (rc)
      {
        DP_CHECK (resamp_dc_gain (rc) == 2.0);
        resamp_destroy (rc);
      }
  }

  /* ── §14 — destroy(NULL) is a no-op; create_custom rejects ──────────── */
  DP_CHECK (lifecycle_rejects ());

  /* ── §15 / §16 — the two promises the old literals could not see ────── */
  DP_CHECK (set_rate_preserves_state ());
  DP_CHECK (reset_zeroes_state ());

  /* ── §17 — the bank's advertised 60 dB / 0.4-0.6 pass-stop ──────────
     The cutoffs are normalised to fs_out, so a frequency quoted as `f` of
     fs_out is sought at `f * rate` of fs_in — which is the grid a caller
     feeds. Swept at TWO rates: one rate cannot tell the rule from a pair
     of literals that happen to hold there. */
  {
    const double rates[2] = { 0.5, 0.25 };
    for (int i = 0; i < 2; i++)
      {
        double rate = rates[i];
        /* `of_out(f)` — f as a fraction of fs_out, expressed on fs_in. */
#define of_out(f) ((f) * rate)
        double pb_in   = band_response_db (rate, of_out (0.2));
        double pb_edge = band_response_db (rate, of_out (0.4));
        double sb_edge = band_response_db (rate, of_out (0.6));
        double sb_deep = band_response_db (rate, of_out (0.9));
#undef of_out
        fprintf (stderr,
                 "  §17 rate=%.2f: fs_out 0.2/0.4 -> %.2f/%.2f dB, "
                 "0.6/0.9 -> %.1f/%.1f dB\n",
                 rate, pb_in, pb_edge, sb_edge, sb_deep);
        DP_CHECK (fabs (pb_in) < 0.5);   /* well inside the passband  */
        DP_CHECK (fabs (pb_edge) < 1.0); /* at the advertised edge    */
        DP_CHECK (sb_edge < -60.0);      /* the advertised rejection  */
        DP_CHECK (sb_deep < -60.0);      /* and it stays down         */
      }
  }

  /* ── §17b — the same 60 dB, on the interpolating face ───────────────
     The structure makes its own probe: interpolation replicates the input
     spectrum at every multiple of fs_in, so the rejection band is full of
     samples of the stopband. Scanned, not sampled at named frequencies --
     ANY artifact above -60 dBc anywhere in the band fails.

     FRACTIONAL rates are the point of the scan: at 2.5 and 3.7 the images
     do not sit still in the output grid, so nothing here could name them.
     Two tones each -- one mid-band, one near the passband edge, where the
     images crowd the transition and the measurement is tightest. */
  {
    const double rates[5] = { 1.5, 2.0, 2.5, 3.7, 8.0 };
    for (int i = 0; i < 5; i++)
      {
        double lo = image_floor_db (rates[i], 0.05);
        double hi = image_floor_db (rates[i], 0.35);
        fprintf (stderr,
                 "  §17b rate=%.1f: image floor %.1f dBc (f0 .05), "
                 "%.1f dBc (f0 .35)\n",
                 rates[i], lo, hi);
        DP_CHECK (lo < -60.0);
        DP_CHECK (hi < -60.0);
      }
  }

  /* ── §18 — execute_ctrl rides the interpolator at EVERY rate ────────
     Both directions: identical to `execute` at or above unity, and
     necessarily different below it, where `execute` is the transposed
     decimator. */
  DP_CHECK (ctrl_rides_interpolator (2.0));
  DP_CHECK (ctrl_rides_interpolator (1.0));
  DP_CHECK (ctrl_rides_interpolator (0.7));
  DP_CHECK (ctrl_rides_interpolator (0.5));

  /* §19 retired: it pinned "only the real part of `ctrl` is used", and the
     port is a `double` now, so there is no imaginary half to discard. The
     claim it guarded no longer exists rather than having gone untested. */

  /* ── §20 — the streaming contract holds at every rate ───────────────
     Fractional rates included, which is where the header's "integer
     interpolation factor" scoping says not to rely on it. */
  DP_CHECK (inputs_needed_is_exact (1.0));
  DP_CHECK (inputs_needed_is_exact (1.5));
  DP_CHECK (inputs_needed_is_exact (2.0));
  DP_CHECK (inputs_needed_is_exact (2.5));
  DP_CHECK (inputs_needed_is_exact (3.7));
  DP_CHECK (inputs_needed_is_exact (7.3));

  DP_TEST_END ("test_resamp_core");
}
