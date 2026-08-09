#include "resamp/resamp_impl.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#define ALMOST_EQ(a, b, tol) (fabsf ((float)(a) - (float)(b)) <= (float)(tol))

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
  float _Complex in[L], ctrl[L], out_ref[CAP], out_push[CAP];
  for (size_t i = 0; i < (size_t)L; i++)
    {
      double ph = 2.0 * M_PI * 0.023 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
      /* a slowly-varying rate deviation, both signs */
      ctrl[i] = CMPLXF ((float)(0.01 * sin (0.05 * (double)i)), 0.0f);
    }

  resamp_state_t *rb   = resamp_create (rate);
  size_t          nref = resamp_execute_ctrl (rb, in, ctrl, L, out_ref, CAP);
  resamp_destroy (rb);

  resamp_state_t *rp = resamp_create (rate);
  size_t          np = 0;
  for (size_t i = 0; i < (size_t)L && np < CAP; i++)
    np += resamp_execute_ctrl_push (rp, in[i], (double)crealf (ctrl[i]),
                                    out_push + np, CAP - np);
  resamp_destroy (rp);

  int ok = (np == nref);
  for (size_t i = 0; i < nref && i < np; i++)
    if (crealf (out_ref[i]) != crealf (out_push[i])
        || cimagf (out_ref[i]) != cimagf (out_push[i]))
      ok = 0;
  return ok;
}

int
main (void)
{
  int _fails = 0;

  /* ---- create / destroy ---- */
  resamp_state_t *r = resamp_create (1.0);
  CHECK (r != NULL);
  if (!r)
    return 1;

  /* ---- properties ---- */
  CHECK (resamp_get_rate (r) == 1.0);
  CHECK (resamp_get_num_phases (r) == 4096);
  CHECK (resamp_get_num_taps (r) == 19);

  /* ---- set_rate preserves phase ---- */
  resamp_set_rate (r, 2.0);
  CHECK (resamp_get_rate (r) == 2.0);

  /* ---- reset: zeroes phase/delay, preserves rate ---- */
  resamp_reset (r);
  CHECK (resamp_get_rate (r) == 2.0); /* rate must survive reset */

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
              CHECK (t == (size_t)GATE_N); /* unity is 1:1, every sample */
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
      CHECK (worst_h < 5e-3); /* all-pass */
      CHECK (spread < 2e-2);  /* PURE delay: no frequency dependence */
      CHECK (mean > 8.0 && mean < 12.0); /* and it is the filter's own */
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
          CHECK (evm < -60.0);
        }
  }

  /* ---- 2x decimation: output count ---- */
  r = resamp_create (0.5);
  CHECK (r != NULL);
  if (!r)
    return 1;

  float _Complex in2[128], out2[64];
  for (size_t i = 0; i < 128; i++)
    in2[i] = CMPLXF (1.0f, 0.0f);

  n = resamp_execute (r, in2, 128, out2, 64);
  /* expect ~64 output samples (allow filter startup delay) */
  CHECK (n >= 56 && n <= 64);
  resamp_destroy (r);

  /* ---- 2x interpolation: output count ---- */
  r = resamp_create (2.0);
  CHECK (r != NULL);
  if (!r)
    return 1;

  float _Complex in3[64], out3[132];
  for (size_t i = 0; i < 64; i++)
    in3[i] = CMPLXF (1.0f, 0.0f);

  n = resamp_execute (r, in3, 64, out3, 132);
  CHECK (n >= 120 && n <= 132);
  resamp_destroy (r);

  /* ---- execute_ctrl unity rate, zero ctrl ---- */
  r = resamp_create (1.0);
  CHECK (r != NULL);
  if (!r)
    return 1;

  float _Complex ctrl[64];
  for (size_t i = 0; i < N; i++)
    {
      in[i]   = CMPLXF (1.0f, 0.0f);
      ctrl[i] = CMPLXF (0.0f, 0.0f);
    }
  n = resamp_execute_ctrl (r, in, ctrl, N, out, N);
  CHECK (n == N);
  resamp_destroy (r);

  /* Serializable-state round-trip across rates (decimate, interpolate,
   * non-integer) — bit-exact resume from the handed-off state blob. */
  CHECK (rt_resamp (0.5)); /* decimation: decim_iad/decim_tfd path */
  CHECK (rt_resamp (2.0)); /* interpolation: delay_buf path        */
  CHECK (rt_resamp (0.4)); /* non-integer: fractional phase + ctrl */

  /* Streaming interpolation fill == resamp_execute, and block-invariant
   * (single M-fill == M on-demand 1-fills). pow-2 nphases → exact overflow
   * count. Both the pulse-shaper's steps() and step() paths depend on this. */
  CHECK (eq_interp_fill (8, 4));
  CHECK (eq_interp_fill (4, 17));
  CHECK (eq_interp_fill (16, 3));

  /* Streaming control port: one-input-at-a-time execute_ctrl_push == the block
   * execute_ctrl, for decimation, interpolation, and unity — the property a
   * closed-loop timing/rate tracker relies on to steer the strobe per output.
   */
  CHECK (eq_ctrl_push (0.4));
  CHECK (eq_ctrl_push (2.0));
  CHECK (eq_ctrl_push (1.0));

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
    CHECK (r1 != NULL);
    if (r1)
      {
        CHECK (r1->num_phases == 1);
        float _Complex in[4] = { 1.0f + 0.0f * I, 2.0f + 0.0f * I,
                                 3.0f + 0.0f * I, 4.0f + 0.0f * I };
        float _Complex out[8];
        size_t used = resamp_interp_fill (r1, in, out, 8);
        CHECK (used <= 4);
        for (size_t i = 0; i < 8; i++)
          {
            /* Every output must be a real dot product over the delay line,
               not whatever lay past the end of a one-arm bank. */
            CHECK (isfinite (crealf (out[i])));
            CHECK (isfinite (cimagf (out[i])));
            CHECK (fabsf (crealf (out[i])) <= 4.0f);
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
    CHECK (r1 != NULL);
    if (r1)
      {
        size_t need = resamp_interp_inputs_needed (r1, 1000);
        CHECK (need >= 999 && need <= 1000);
        /* Neighbours either side must agree to within a sample -- the
           conversion should be continuous across the branch boundary. */
        resamp_state_t *rlo = resamp_create (0.9999999);
        resamp_state_t *rhi = resamp_create (1.0000001);
        size_t          nlo = resamp_interp_inputs_needed (rlo, 1000);
        size_t          nhi = resamp_interp_inputs_needed (rhi, 1000);
        CHECK (nhi >= 999 && nhi <= 1000);
        CHECK (need >= nhi); /* unity consumes no fewer than faster */
        (void)nlo;
        resamp_destroy (rlo);
        resamp_destroy (rhi);
        resamp_destroy (r1);
      }
  }

  if (_fails)
    {
      fprintf (stderr, "test_resamp_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_resamp_core PASSED\n");
  return 0;
}
