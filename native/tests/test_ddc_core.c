/*
 * test_ddc_core.c — C-level unit tests for the complex-input DDC.
 *
 * Covers the plain flavor (ddc_create) and the matched one
 * (ddc_create_matched), the two control ports, and serialization. The
 * real-input twin has its own suite in test_ddcr_core.c.
 */
#include "ddc/ddc_core.h"
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

/* ── Serializable state: mid-stream split resumes bit-for-bit ──────────────
 */
static void
test_state_roundtrip (void)
{
  const double    norm_freq = -0.1, rate = 0.25;
  const size_t    L = 4096, cut = 1201, CAP = 4096;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)cos (0.17 * (double)i) + I * (float)sin (0.013 * (double)i);

  ddc_state_t *ra = ddc_create (norm_freq, rate);
  size_t       nA = ddc_execute (ra, in, L, outA, CAP);
  ddc_destroy (ra);

  ddc_state_t *r1   = ddc_create (norm_freq, rate);
  size_t       nB   = ddc_execute (r1, in, cut, outB, CAP);
  size_t       sb   = ddc_state_bytes (r1);
  void        *blob = malloc (sb);
  ddc_get_state (r1, blob);
  ddc_destroy (r1);

  ddc_state_t *r2 = ddc_create (norm_freq, rate);
  CHECK (ddc_set_state (r2, blob) == DP_OK);
  ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
  CHECK (ddc_set_state (r2, blob) == DP_ERR_INVALID);
  ((char *)blob)[0] ^= (char)0xFF;
  nB += ddc_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
  ddc_destroy (r2);
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

/* ══════════════════════════════════════════════════════════════════════════
 * The matched flavor and the two control ports
 *
 * The pulse itself is RateConverter's (tested there); what these pin is what
 * this layer adds: that the pulse survives the composition, that the two
 * ports are exactly the LO's and the terminal stage's own accumulators, and
 * that a loop closed around either one behaves.
 * ══════════════════════════════════════════════════════════════════════════
 */

/* The two constructors are the two flavors: the matched one refuses to build
 * an unmatched object, and everything else is rejected rather than coerced. */
static void
test_flavors_and_invalid_params (void)
{
  ddc_state_t *plain = ddc_create (0.0, 0.25);
  CHECK (plain != NULL);
  CHECK (ddc_get_clipped (plain) == false);
  ddc_destroy (plain);

  CHECK (ddc_create (0.0, 0.0) == NULL);
  CHECK (ddc_create (0.0, -0.25) == NULL);
  /* RC_PULSE_NONE would hand back an object whose "matched filter" is a
     Kaiser anti-alias bank — that is what ddc_create() is for. */
  CHECK (ddc_create_matched (0.0, 0.25, RC_PULSE_NONE, 0.35, 8, 2.0, 1024)
         == NULL);
  CHECK (ddc_create_matched (0.0, -0.25, RC_PULSE_RRC, 0.35, 8, 2.0, 1024)
         == NULL);
  CHECK (ddc_create_matched (0.0, 0.25, RC_PULSE_RRC, 2.0, 8, 2.0, 1024)
         == NULL); /* beta out of range — rejected by the cascade */

  ddc_state_t *m
      = ddc_create_matched (0.0, 0.125, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (m != NULL);
  ddc_destroy (m);
  ddc_state_t *r
      = ddc_create_matched (0.0, 0.125, RC_PULSE_IANDD, 0.35, 4, 4.0, 256);
  CHECK (r != NULL);
  ddc_destroy (r);
}

/* The frequency port IS the LO's frequency axis: steering by `f` from a
 * centre of zero is bit-for-bit the same stream as tuning the LO to `f` and
 * steering by nothing. That equivalence is the whole claim — the port adds no
 * scaling, no smoothing and no state of its own. */
static void
test_freq_port_is_the_lo_axis (void)
{
  const double    f = 0.037, rate = 0.125;
  const size_t    L = 2048, CAP = 512;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  float _Complex *outC = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.11 * (double)i))
            + I * (float)(0.25 * sin (0.029 * (double)i));

  ddc_state_t *a
      = ddc_create_matched (0.0, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_state_t *b
      = ddc_create_matched (f, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_state_t *c
      = ddc_create_matched (0.0, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nA = ddc_execute_ctrl (a, in, L, 0.0, f, outA, CAP);
  size_t nB = ddc_execute_ctrl (b, in, L, 0.0, 0.0, outB, CAP);
  size_t nC = ddc_execute_ctrl (c, in, L, 0.0, 0.0, outC, CAP);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);

  /* Teeth: the same comparison against an unsteered LO must fail loudly, or
     the equality above would be vacuous. */
  int differs = 0;
  for (size_t i = 0; i < nA && i < nC; i++)
    if (cabsf (outA[i] - outC[i]) > 1e-3f)
      differs++;
  CHECK (differs > (int)(nA / 4));

  /* And the centre frequency is untouched — the deviation is per-sample and
     transient, so the object holds no loop state. */
  CHECK (ddc_get_norm_freq (a) == 0.0);

  ddc_destroy (a);
  ddc_destroy (b);
  ddc_destroy (c);
  free (in);
  free (outA);
  free (outB);
  free (outC);
}

/* Push == block, bit-for-bit, with BOTH ports held off centre. The block form
 * is the cheap open-loop path (a fixed Doppler offset, a rate trim); the push
 * form is the only one a closed loop can use. They must not drift. */
static void
test_push_equals_block (void)
{
  const double    rate = 0.125, rctrl = 0.002, fctrl = 0.01;
  const size_t    L = 2048, CAP = 512;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.11 * (double)i))
            + I * (float)(0.25 * sin (0.029 * (double)i));

  ddc_state_t *a
      = ddc_create_matched (-0.1, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_state_t *b
      = ddc_create_matched (-0.1, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nA = ddc_execute_ctrl (a, in, L, rctrl, fctrl, outA, CAP);
  size_t nB = 0;
  for (size_t i = 0; i < L; i++)
    nB += ddc_execute_ctrl_push (b, in[i], rctrl, fctrl, outB + nB, CAP - nB);
  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);
  ddc_destroy (a);
  ddc_destroy (b);
  free (in);
  free (outA);
  free (outB);
}

/* End to end: a carrier at 0.09375*fs carrying RRC-BPSK at 16 samples/symbol
 * comes out as symbols at two samples/symbol, matched-filtered by the same
 * dot product that decimated it. rate = 2/16 is an exact power of two, so the
 * ordinary planner would drop the fractional stage entirely — the matched
 * flavor appends it anyway, which is what makes the result steerable.
 *
 * The floor here is the plan's, not the matched filter's: this rate plans
 * CIC(8), whose alias bands fold back at about -30 dB. (The real-input twin
 * sees 2*rate, plans halfbands, and reaches -60 dB — see test_ddcr_core.c.) */
static void
test_matched_recovers_symbols (void)
{
  const double sps = 16.0, fc = 0.09375, rate = 2.0 / 16.0;
  double       best = 1e9;

  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x = mf_tx (sps, j / 16.0, fc, &n);
      float _Complex *y = calloc (n, sizeof *y);
      CHECK (x != NULL && y != NULL);
      if (!x || !y)
        {
          free (x);
          free (y);
          return;
        }
      ddc_state_t *d  = ddc_create_matched (-fc, rate, RC_PULSE_RRC, MF_BETA,
                                            MF_SPAN, 2.0, 1024);
      size_t       ny = ddc_execute (d, x, n, y, n);
      double       e  = mf_evm_db (y, ny);
      if (e < best)
        best = e;
      ddc_destroy (d);
      free (x);
      free (y);
    }

  CHECK (best < -40.0); /* measured -45.4 dB (CIC(8) alias floor) */
  if (best >= -40.0)
    fprintf (stderr, "  matched DDC EVM: %.1f dB\n", best);
}

/* The carrier port closes. A tone 0.01 cycles/sample away from where the LO
 * is tuned comes out spinning; a first-order loop reading the output phase
 * increment and writing `freq_ctrl` drives that to zero. This is the point of
 * the port being the dual of the timing one — same loop shape, other
 * accumulator — and it is the only thing here that runs closed.
 *
 * The gain is small for a reason worth stating: the loop closes AROUND the
 * matched filter, so its dead time is that filter's group delay (tens of
 * output samples for a span-8 RRC plus the cascade in front). Measured on
 * this configuration, mu = 0.01 and 0.02 converge to the mistune within 1e-9,
 * mu = 0.05 is marginal (6e-3 left after 2048 outputs) and mu = 0.1 diverges
 * outright and wanders — the same reason a real receiver's carrier loop
 * bandwidth is a small fraction of the symbol rate. */
static void
test_carrier_loop_pulls_in (void)
{
  const double f0 = 0.05, tuned = -0.04, rate = 0.25, mu = 0.01;
  const size_t L = 8192;
  float _Complex o[8];

  ddc_state_t *s
      = ddc_create_matched (tuned, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (s != NULL);

  double freq_ctrl = 0.0, e_last = 0.0;
  float _Complex prev = 0.0f;
  int have_prev       = 0;
  for (size_t i = 0; i < L; i++)
    {
      double ph        = 2.0 * M_PI * f0 * (double)i;
      float _Complex x = (float _Complex) (0.25 * (cos (ph) + I * sin (ph)));
      size_t n         = ddc_execute_ctrl_push (s, x, 0.0, freq_ctrl, o, 8);
      for (size_t j = 0; j < n; j++)
        {
          if (have_prev && i > 1024) /* let the cascade's delay lines fill */
            {
              /* Residual frequency in cycles per OUTPUT sample; the port
                 wants cycles per INPUT sample, hence the rate factor. */
              e_last = carg (o[j] * conjf (prev)) / (2.0 * M_PI);
              freq_ctrl -= mu * e_last * rate;
            }
          prev      = o[j];
          have_prev = 1;
        }
    }
  CHECK (fabs (f0 + tuned + freq_ctrl) < 1e-6); /* measured 6e-11 */
  CHECK (fabs (e_last) < 1e-5);
  CHECK (ddc_get_norm_freq (s) == tuned); /* the centre never moved */
  ddc_destroy (s);

  /* Teeth: hold the port at zero and the same stream keeps spinning at
     0.01/rate cycles per output sample. */
  ddc_state_t *t
      = ddc_create_matched (tuned, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  double open_e = 0.0;
  prev          = 0.0f;
  have_prev     = 0;
  for (size_t i = 0; i < L; i++)
    {
      double ph        = 2.0 * M_PI * f0 * (double)i;
      float _Complex x = (float _Complex) (0.25 * (cos (ph) + I * sin (ph)));
      size_t n         = ddc_execute_ctrl_push (t, x, 0.0, 0.0, o, 8);
      for (size_t j = 0; j < n; j++)
        {
          if (have_prev && i > 1024)
            open_e = carg (o[j] * conjf (prev)) / (2.0 * M_PI);
          prev      = o[j];
          have_prev = 1;
        }
    }
  CHECK (fabs (open_e - (f0 + tuned) / rate) < 1e-3);
  ddc_destroy (t);
}

/* The CIC's silent input bound is observable through the composition: a DDC
 * that plans a CIC inherits it, and nothing in the samples says so. */
static void
test_clipped_forwards (void)
{
  const size_t    L   = 1024;
  float _Complex *in  = malloc (L * sizeof (float _Complex));
  float _Complex *out = malloc (L * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(2.0 * cos (0.11 * (double)i)); /* twice full scale */

  /* rate 2/64 plans a CIC(32) + terminal stage. */
  ddc_state_t *d
      = ddc_create_matched (0.0, 2.0 / 64.0, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (ddc_get_clipped (d) == false);
  ddc_execute (d, in, L, out, L);
  CHECK (ddc_get_clipped (d) == true);
  ddc_reset (d);
  CHECK (ddc_get_clipped (d) == false); /* sticky until reset, not forever */
  ddc_destroy (d);

  /* A halfband plan has no CIC, so the honest answer is false however hard it
     is driven — those plans are scale-free. */
  ddc_state_t *h
      = ddc_create_matched (0.0, 0.5, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_execute (h, in, L, out, L);
  CHECK (ddc_get_clipped (h) == false);
  ddc_destroy (h);

  free (in);
  free (out);
}

/* A matched DDC serializes like any other: same descriptor in, bit-exact
 * resume. And a blob from a matched cascade must not restore into a plain one
 * at the same rate — the stage plans differ, so the envelope rejects it
 * rather than reinterpreting the bytes. */
static void
test_matched_state_roundtrip (void)
{
  const double    rate = 0.125, nf = -0.1;
  const size_t    L = 4096, cut = 1201, CAP = 1024;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.17 * (double)i))
            + I * (float)(0.25 * sin (0.013 * (double)i));

  ddc_state_t *a
      = ddc_create_matched (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nA = ddc_execute (a, in, L, outA, CAP);
  ddc_destroy (a);

  ddc_state_t *b
      = ddc_create_matched (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t nB   = ddc_execute (b, in, cut, outB, CAP);
  void  *blob = malloc (ddc_state_bytes (b));
  ddc_get_state (b, blob);
  ddc_destroy (b);

  ddc_state_t *c
      = ddc_create_matched (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (ddc_set_state (c, blob) == DP_OK);
  nB += ddc_execute (c, in + cut, L - cut, outB + nB, CAP - nB);
  ddc_destroy (c);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);

  ddc_state_t *plain = ddc_create (nf, rate);
  CHECK (ddc_set_state (plain, blob) == DP_ERR_INVALID);
  ddc_destroy (plain);

  free (blob);
  free (in);
  free (outA);
  free (outB);
}

int
main (void)
{
  ddc_state_t *obj = ddc_create (0.0, 0.25);
  CHECK (obj != NULL);
  if (!obj)
    return 1;
  ddc_reset (obj);
  ddc_destroy (obj);

  test_state_roundtrip ();
  test_flavors_and_invalid_params ();
  test_freq_port_is_the_lo_axis ();
  test_push_equals_block ();
  test_matched_recovers_symbols ();
  test_carrier_loop_pulls_in ();
  test_clipped_forwards ();
  test_matched_state_roundtrip ();

  if (_fails)
    {
      fprintf (stderr, "test_ddc_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ddc_core PASSED\n");
  return 0;
}
