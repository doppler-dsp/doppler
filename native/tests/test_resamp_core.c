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
  int             ok = (resamp_set_state (r2, blob) == 0);
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

  if (_fails)
    {
      fprintf (stderr, "test_resamp_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_resamp_core PASSED\n");
  return 0;
}
