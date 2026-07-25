/*
 * test_ddcr_core.c — C-level unit tests for the real-input DDC.
 *
 * The real twin of test_ddc_core.c: same two flavors (ddcr_create /
 * ddcr_create_matched), same two control ports, plus the halfband R2C front
 * end this type adds. Also the integration gate for the whole chain's
 * serializers (hbdecim_r2c -> LO -> RateConverter), none of which have a
 * standalone C target.
 */
#include "ddcr/ddcr_core.h"
#include "dp_mf_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return fabsf (crealf (a) - crealf (b)) <= tol
         && fabsf (cimagf (a) - cimagf (b)) <= tol;
}

/* ── ddcr_run pure-transducer round-trip ──────────────────────────────────
 * The stateless `ddcr_run(state_in, state_out, …)` face: a whole-stream
 * reference run, then a split where the first half emits its state and a
 * fresh engine restores it, plus the corrupted-blob path (set_state rejects
 * -> run returns 0).
 *
 * The rate steers the RateConverter plan, so this also exercises the CIC and
 * Resampler stage serializers: ddcr's cascade runs at 2*rate, so rate 0.0625
 * -> RC 0.125 -> CIC(/8), and rate 0.375 -> RC 0.75 -> Resampler. The
 * rate=0.25 case below covers the halfband plan. */
static void
test_run_roundtrip (double norm_freq, double rate)
{
  const size_t    L = 1024, cut = 391, CAP = 2048;
  float          *in   = malloc (L * sizeof (float));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)cos (0.11 * (double)i) + 0.5f * (float)sin (0.029 * i);

  ddcr_state_t *ra = ddcr_create (norm_freq, rate);
  size_t        nA = ddcr_run (ra, NULL, NULL, in, L, outA, CAP);
  ddcr_destroy (ra);

  ddcr_state_t *r1   = ddcr_create (norm_freq, rate);
  size_t        sb   = ddcr_state_bytes (r1);
  void         *blob = malloc (sb);
  size_t        nB   = ddcr_run (r1, NULL, blob, in, cut, outB, CAP);
  ddcr_destroy (r1);

  ddcr_state_t *r2 = ddcr_create (norm_freq, rate);
  nB += ddcr_run (r2, blob, NULL, in + cut, L - cut, outB + nB, CAP - nB);
  ddcr_destroy (r2);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    /* Restored vs continuous output matches up to FMA-grouping ULPs at the
     * split boundary: the CIC / Resampler taps contract differently across
     * the cut on arm64 (the halfband plan is grouping-invariant, hence exact
     * there). A real state-restore bug would be O(1) — far above tol. */
    if (!_almost_eq_c (outA[i], outB[i], 1e-3f))
      bad++;
  CHECK (bad == 0);

  /* a corrupted state_in must make ddcr_run reject (set_state != 0) -> 0 out
   */
  ddcr_state_t *r3 = ddcr_create (norm_freq, rate);
  ddcr_get_state (r3, blob);
  ((char *)blob)[0] ^= (char)0xFF; /* clobber the header magic */
  CHECK (ddcr_run (r3, blob, NULL, in, cut, outB, CAP) == 0);
  ddcr_destroy (r3);

  free (blob);
  free (in);
  free (outA);
  free (outB);
}

/* Full-chain serialize / restore mid-stream, bit-for-bit. */
static void
test_state_roundtrip (void)
{
  const double    norm_freq = -0.3, rate = 0.25;
  const size_t    L = 4096, cut = 1503, CAP = 2048;
  float          *in   = malloc (L * sizeof (float));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)cos (0.17 * (double)i) + 0.5f * (float)sin (0.013 * i);

  ddcr_state_t *ra = ddcr_create (norm_freq, rate);
  size_t        nA = ddcr_execute (ra, in, L, outA, CAP);
  ddcr_destroy (ra);

  ddcr_state_t *r1   = ddcr_create (norm_freq, rate);
  size_t        nB   = ddcr_execute (r1, in, cut, outB, CAP);
  size_t        sb   = ddcr_state_bytes (r1);
  void         *blob = malloc (sb);
  ddcr_get_state (r1, blob);
  ddcr_destroy (r1);

  ddcr_state_t *r2 = ddcr_create (norm_freq, rate);
  CHECK (ddcr_set_state (r2, blob) == DP_OK);
  /* a mismatched-rate engine must reject the blob */
  ddcr_state_t *rbad = ddcr_create (norm_freq, 0.2);
  CHECK (ddcr_set_state (rbad, blob) == DP_ERR_INVALID);
  ddcr_destroy (rbad);

  nB += ddcr_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
  ddcr_destroy (r2);
  free (blob);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);
  free (in);
  free (outA);
  free (outB);
}

/* The two constructors are the two flavors; the rate bound is the halfband's.
 */
static void
test_flavors_and_invalid_params (void)
{
  ddcr_state_t *plain = ddcr_create (0.0, 0.25);
  CHECK (plain != NULL);
  CHECK (ddcr_get_clipped (plain) == false);
  ddcr_destroy (plain);

  /* rate >= 0.5 — the halfband already took a factor of two */
  CHECK (ddcr_create (0.0, 0.5) == NULL);
  CHECK (ddcr_create (0.0, 0.0) == NULL);
  CHECK (ddcr_create_matched (0.0, 0.25, RC_PULSE_NONE, 0.35, 8, 2.0, 1024)
         == NULL);
  CHECK (ddcr_create_matched (0.0, 0.6, RC_PULSE_RRC, 0.35, 8, 2.0, 1024)
         == NULL);

  ddcr_state_t *m
      = ddcr_create_matched (0.0, 0.125, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (m != NULL);
  ddcr_destroy (m);
  ddcr_state_t *r
      = ddcr_create_matched (0.0, 0.125, RC_PULSE_IANDD, 0.35, 4, 4.0, 256);
  CHECK (r != NULL);
  ddcr_destroy (r);
}

/* The frequency port is the fine LO's own axis — at the INTERMEDIATE rate,
 * which is where this type's LO lives. */
static void
test_freq_port_is_the_lo_axis (void)
{
  const double    f = 0.037, rate = 0.125;
  const size_t    L = 2048, CAP = 512;
  float          *in   = malloc (L * sizeof (float));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.11 * (double)i));

  ddcr_state_t *a
      = ddcr_create_matched (0.0, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddcr_state_t *b
      = ddcr_create_matched (f, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nA = ddcr_execute_ctrl (a, in, L, 0.0, f, outA, CAP);
  size_t nB = ddcr_execute_ctrl (b, in, L, 0.0, 0.0, outB, CAP);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);
  CHECK (ddcr_get_norm_freq (a) == 0.0); /* centre untouched */

  ddcr_destroy (a);
  ddcr_destroy (b);
  free (in);
  free (outA);
  free (outB);
}

/* Push == block with both ports off centre. The halfband consumes two inputs
 * per LO step, so half the pushes emit nothing at all and the parity must
 * survive the split.
 *
 * Compared to a tolerance rather than bit-for-bit, unlike the complex chain:
 * the R2C front end's block loop contracts its multiply-adds differently from
 * its one-sample-at-a-time path, so on a machine that contracts
 * (arm64/macOS) the two groupings differ in the last ULP and the cascade
 * carries that through. A real parity or ordering bug is O(1) here — a whole
 * sample out of place, orders of magnitude above this bound. */
static void
test_push_equals_block (void)
{
  const double    rate = 0.125, rctrl = 0.002, fctrl = 0.01;
  const size_t    L = 2048, CAP = 512;
  float          *in   = malloc (L * sizeof (float));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.11 * (double)i));

  ddcr_state_t *a
      = ddcr_create_matched (-0.7, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddcr_state_t *b
      = ddcr_create_matched (-0.7, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nA = ddcr_execute_ctrl (a, in, L, rctrl, fctrl, outA, CAP);
  size_t nB = 0;
  for (size_t i = 0; i < L; i++)
    nB += ddcr_execute_ctrl_push (b, in[i], rctrl, fctrl, outB + nB, CAP - nB);
  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (!_almost_eq_c (outA[i], outB[i], 1e-5f))
      bad++;
  CHECK (bad == 0);
  ddcr_destroy (a);
  ddcr_destroy (b);
  free (in);
  free (outA);
  free (outB);
}

/* End to end from a real ADC stream: the same RRC-BPSK the complex suite
 * measures, real-sampled. This chain reaches -60 dB where the complex one
 * reaches -45: its cascade sees 2*rate and plans halfbands instead of the
 * CIC(8) whose alias floor limits the other. */
static void
test_matched_recovers_symbols (void)
{
  const double sps = 16.0, fc = 0.09375, rate = 2.0 / 16.0;
  double       best = 1e9;

  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x  = mf_tx (sps, j / 16.0, fc, &n);
      float _Complex *y  = calloc (n, sizeof *y);
      float          *xr = malloc (n * sizeof (float));
      CHECK (x != NULL && y != NULL && xr != NULL);
      if (!x || !y || !xr)
        {
          free (x);
          free (y);
          free (xr);
          return;
        }
      for (size_t i = 0; i < n; i++)
        xr[i] = crealf (x[i]); /* the same signal, real-sampled */

      /* Tuning is at the intermediate rate: -(2*fc + 0.5). */
      ddcr_state_t *r = ddcr_create_matched (
          -(2.0 * fc + 0.5), rate, RC_PULSE_RRC, MF_BETA, MF_SPAN, 2.0, 1024);
      size_t ny = ddcr_execute (r, xr, n, y, n);
      double e  = mf_evm_db (y, ny);
      if (e < best)
        best = e;
      ddcr_destroy (r);
      free (x);
      free (y);
      free (xr);
    }

  CHECK (best < -50.0); /* measured -59.8 dB (halfband cascade) */
  if (best >= -50.0)
    fprintf (stderr, "  matched DdcR EVM: %.1f dB\n", best);
}

/* The CIC's silent input bound reaches through the real front end too. */
static void
test_clipped_forwards (void)
{
  const size_t    L   = 1024;
  float          *in  = malloc (L * sizeof (float));
  float _Complex *out = malloc (L * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(2.0 * cos (0.11 * (double)i)); /* twice full scale */

  ddcr_state_t *r = ddcr_create_matched (0.0, 2.0 / 64.0, RC_PULSE_RRC, 0.35,
                                         8, 2.0, 1024);
  CHECK (ddcr_get_clipped (r) == false);
  ddcr_execute (r, in, L, out, L);
  CHECK (ddcr_get_clipped (r) == true);
  ddcr_reset (r);
  CHECK (ddcr_get_clipped (r) == false);
  ddcr_destroy (r);

  free (in);
  free (out);
}

int
main (void)
{
  ddcr_state_t *obj = ddcr_create (0.0, 0.25);
  CHECK (obj != NULL);
  if (!obj)
    return 1;
  ddcr_reset (obj);
  ddcr_destroy (obj);

  test_state_roundtrip ();
  test_run_roundtrip (-0.1, 0.0625); /* RC 0.125 -> CIC(/8)   */
  test_run_roundtrip (0.2, 0.375);   /* RC 0.75  -> Resampler */
  test_flavors_and_invalid_params ();
  test_freq_port_is_the_lo_axis ();
  test_push_equals_block ();
  test_matched_recovers_symbols ();
  test_clipped_forwards ();

  if (_fails)
    {
      fprintf (stderr, "test_ddcr_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ddcr_core PASSED\n");
  return 0;
}
