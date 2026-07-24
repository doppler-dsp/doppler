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

  /* ---- unity-rate pass-through ---- */
  r = resamp_create (1.0);
  CHECK (r != NULL);
  if (!r)
    return 1;

  static const size_t N = 64;
  float _Complex in[64], out[64];
  for (size_t i = 0; i < N; i++)
    in[i] = CMPLXF ((float)i, -(float)i);

  size_t n = resamp_execute (r, in, N, out, N);
  CHECK (n == N);
  for (size_t i = 0; i < N; i++)
    {
      CHECK (ALMOST_EQ (crealf (out[i]), (float)i, 1e-4f));
      CHECK (ALMOST_EQ (cimagf (out[i]), -(float)i, 1e-4f));
    }
  resamp_destroy (r);

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

  if (_fails)
    {
      fprintf (stderr, "test_resamp_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_resamp_core PASSED\n");
  return 0;
}
