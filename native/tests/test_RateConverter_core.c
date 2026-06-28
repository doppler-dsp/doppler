/*
 * test_RateConverter_core.c — C-level unit tests for RateConverter.
 *
 * Tests cover:
 *   - Invalid rate returns NULL
 *   - Stage plan and labels for every selection regime
 *   - Output length from execute()
 *   - DC gain approximately 1.0 across all rate regimes
 *   - set_rate() rebuilds cascade and changes output length
 *   - reset() yields reproducible output
 *   - execute_max_out() sanity bound
 *   - compensate flag: CIC+FIR compound stage label
 */

#include "RateConverter/RateConverter_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
_near (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
}

static float
_mean_amp (const float complex *v, size_t n)
{
  double s = 0.0;
  for (size_t i = 0; i < n; i++)
    s += cabsf (v[i]);
  return (n > 0) ? (float)(s / n) : 0.0f;
}

static float complex *
_dc_block (size_t n)
{
  float complex *b = malloc (n * sizeof (float complex));
  if (!b)
    return NULL;
  for (size_t i = 0; i < n; i++)
    b[i] = 1.0f + 0.0f * I;
  return b;
}

/* ------------------------------------------------------------------ */

static void
test_invalid_rate (void)
{
  CHECK (RateConverter_create (0.0, 0) == NULL);
  CHECK (RateConverter_create (-1.0, 0) == NULL);
}

/* ------------------------------------------------------------------ */

static void
test_stage_labels (void)
{
  char buf[64];

  /* rate >= 1: Resampler */
  {
    RateConverter_state_t *rc = RateConverter_create (2.0, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strncmp (buf, "Resampler", 9) == 0);
    CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)) == 0);
    RateConverter_destroy (rc);
  }

  /* rate = 1.0: Resampler(1) */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strncmp (buf, "Resampler", 9) == 0);
    RateConverter_destroy (rc);
  }

  /* D = 2 (rate = 0.5): single HalfbandDecimator */
  {
    RateConverter_state_t *rc = RateConverter_create (0.5, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 4 (rate = 0.25): two HalfbandDecimator stages */
  {
    RateConverter_state_t *rc = RateConverter_create (0.25, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 2);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)));
    CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 8 (rate = 0.125): CIC(8), no comp */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strcmp (buf, "CIC(8)") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 8, compensate=1: CIC(8)+FIR */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 1);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strcmp (buf, "CIC(8)+FIR") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 16 (rate = 1/16): CIC(16), exact power-of-2, n>=3 */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 16.0, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strcmp (buf, "CIC(16)") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 12 (rate = 1/12): non-power-of-2, D >= 8.
   * Nearest power-of-2 to 12 is 16; plan: CIC(16) + Resampler. */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 12.0, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 2);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strncmp (buf, "CIC", 3) == 0);
    CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)));
    CHECK (strncmp (buf, "Resampler", 9) == 0);
    RateConverter_destroy (rc);
  }

  /* D = 3 (2 <= D < 8, non-integer): Resampler */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 3.0, 0);
    CHECK (rc != NULL);
    CHECK (rc->n_stages == 1);
    CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    CHECK (strncmp (buf, "Resampler", 9) == 0);
    RateConverter_destroy (rc);
  }
}

/* ------------------------------------------------------------------ */

static void
test_output_length (void)
{
  enum
  {
    N_IN  = 1024,
    N_OUT = 1024
  };
  float complex *in  = _dc_block (N_IN);
  float complex *out = malloc (N_OUT * sizeof (float complex));
  CHECK (in && out);

  /* rate = 0.5: expect exactly 512 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.5, 0);
    CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    CHECK (n == N_IN / 2);
    RateConverter_destroy (rc);
  }

  /* rate = 0.25: expect exactly 256 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.25, 0);
    CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    CHECK (n == N_IN / 4);
    RateConverter_destroy (rc);
  }

  /* rate = 0.125: expect exactly 128 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 0);
    CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    CHECK (n == N_IN / 8);
    RateConverter_destroy (rc);
  }

  free (in);
  free (out);
}

/* ------------------------------------------------------------------ */

static void
test_dc_gain (void)
{
  static const double rates[] = {
    2.0,        /* interpolation */
    1.0,        /* passthrough Resampler */
    0.5,        /* HB x1 */
    0.25,       /* HB x2 */
    0.125,      /* CIC(8) */
    1.0 / 12.0, /* CIC + Resampler */
    1.0 / 3.0,  /* Resampler (2<=D<8, non-int) */
  };
  const size_t n_rates = sizeof (rates) / sizeof (rates[0]);

  enum
  {
    N_IN  = 65536,
    N_OUT = 65536 * 4
  };
  float complex *in  = _dc_block (N_IN);
  float complex *out = malloc (N_OUT * sizeof (float complex));
  CHECK (in && out);

  for (size_t r = 0; r < n_rates; r++)
    {
      RateConverter_state_t *rc = RateConverter_create (rates[r], 0);
      CHECK (rc != NULL);
      if (!rc)
        continue;

      /* Warm up one block, then measure the next. */
      size_t out_cap = (rates[r] > 1.0) ? (size_t)(N_IN * rates[r] + 4) : N_IN;
      RateConverter_execute (rc, in, N_IN, out, out_cap);
      size_t n2 = RateConverter_execute (rc, in, N_IN, out, out_cap);

      if (n2 > 0)
        {
          float amp = _mean_amp (out, n2);
          int   ok  = (amp > 0.85f && amp < 1.15f);
          if (!ok)
            {
              fprintf (stderr, "FAIL dc_gain rate=%.6g amp=%.4f n_out=%zu\n",
                       rates[r], amp, n2);
              _fails++;
            }
        }
      RateConverter_destroy (rc);
    }

  free (in);
  free (out);
}

/* ------------------------------------------------------------------ */

static void
test_set_rate (void)
{
  enum
  {
    N = 512
  };
  float complex *in  = _dc_block (N);
  float complex *out = malloc (N * sizeof (float complex));
  CHECK (in && out);

  RateConverter_state_t *rc = RateConverter_create (0.5, 0);
  CHECK (rc != NULL);

  size_t n1 = RateConverter_execute (rc, in, N, out, N);
  CHECK (n1 == N / 2);

  RateConverter_set_rate (rc, 0.25);
  CHECK (_near (RateConverter_get_rate (rc), 0.25, 1e-9));

  size_t n2 = RateConverter_execute (rc, in, N, out, N);
  CHECK (n2 == N / 4);

  /* rate <= 0 silently ignored */
  RateConverter_set_rate (rc, 0.0);
  CHECK (_near (RateConverter_get_rate (rc), 0.25, 1e-9));

  RateConverter_destroy (rc);
  free (in);
  free (out);
}

/* ------------------------------------------------------------------ */

static void
test_reset_reproducible (void)
{
  enum
  {
    N = 256
  };
  float complex *in   = _dc_block (N);
  float complex *out1 = malloc (N * sizeof (float complex));
  float complex *out2 = malloc (N * sizeof (float complex));
  CHECK (in && out1 && out2);

  RateConverter_state_t *rc = RateConverter_create (0.125, 0);
  CHECK (rc != NULL);

  size_t n1 = RateConverter_execute (rc, in, N, out1, N);
  RateConverter_reset (rc);
  size_t n2 = RateConverter_execute (rc, in, N, out2, N);

  CHECK (n1 == n2);
  if (n1 == n2 && n1 > 0)
    CHECK (memcmp (out1, out2, n1 * sizeof (float complex)) == 0);

  RateConverter_destroy (rc);
  free (in);
  free (out1);
  free (out2);
}

/* ------------------------------------------------------------------ */

static void
test_execute_max_out (void)
{
  /* Decimation: max_out should bound 65536-sample block output. */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 0);
    CHECK (rc != NULL);
    size_t m = RateConverter_execute_max_out (rc);
    CHECK (m >= 2);
    CHECK (m <= 65536 + 2);
    RateConverter_destroy (rc);
  }

  /* Interpolation: max_out must be >= n_in * rate. */
  {
    RateConverter_state_t *rc = RateConverter_create (4.0, 0);
    CHECK (rc != NULL);
    size_t m = RateConverter_execute_max_out (rc);
    CHECK (m >= (size_t)(65536 * 4));
    RateConverter_destroy (rc);
  }
}

/* ------------------------------------------------------------------ */
/* test_convert: one-shot matches create+execute+destroy.             */
/* ------------------------------------------------------------------ */
static void
test_convert (void)
{
  printf ("\n-- RateConverter_convert --\n");

  const size_t N_IN  = 256;
  const size_t N_OUT = 256; /* rate=1.0 → same length */
  float _Complex in[256], ref[256], out[256];
  for (size_t i = 0; i < N_IN; i++)
    in[i] = 1.0f + 0.0f * _Complex_I;

  /* Reference: stateful path */
  RateConverter_state_t *rc = RateConverter_create (1.0, 0);
  CHECK (rc != NULL);
  size_t n_ref = RateConverter_execute (rc, in, N_IN, ref, N_OUT);
  RateConverter_destroy (rc);

  /* One-shot */
  size_t n_out = RateConverter_convert (1.0, 0, in, N_IN, out, N_OUT);
  CHECK (n_out == n_ref);
  CHECK (memcmp (ref, out, n_ref * sizeof *ref) == 0);

  /* Decimation: rate=0.5 → n_out ≈ n_in/2 */
  float _Complex dec_in[256], dec_out[256];
  for (size_t i = 0; i < 256; i++)
    dec_in[i] = 1.0f + 0.0f * _Complex_I;
  size_t n_dec = RateConverter_convert (0.5, 0, dec_in, 256, dec_out, 256);
  CHECK (n_dec == 128);
}

/* ------------------------------------------------------------------ */
/* test_state_roundtrip: serialize mid-stream, restore into a fresh */
/* converter, resume bit-for-bit; a clobbered envelope rejects. */
/* ------------------------------------------------------------------ */
static void
test_state_roundtrip (void)
{
  enum
  {
    L   = 1024,
    CUT = 401,
    CAP = 1024
  };
  float complex *in   = malloc (L * sizeof (float complex));
  float complex *outA = malloc (CAP * sizeof (float complex));
  float complex *outB = malloc (CAP * sizeof (float complex));
  CHECK (in && outA && outB);
  for (size_t i = 0; i < L; i++)
    in[i] = (float)cos (0.03 * (double)i) + I * (float)sin (0.03 * (double)i);

  RateConverter_state_t *ra = RateConverter_create (0.5, 0);
  size_t                 nA = RateConverter_execute (ra, in, L, outA, CAP);
  RateConverter_destroy (ra);

  RateConverter_state_t *r1   = RateConverter_create (0.5, 0);
  size_t                 nB   = RateConverter_execute (r1, in, CUT, outB, CAP);
  size_t                 sb   = RateConverter_state_bytes (r1);
  void                  *blob = malloc (sb);
  RateConverter_get_state (r1, blob);
  RateConverter_destroy (r1);

  RateConverter_state_t *r2 = RateConverter_create (0.5, 0);
  CHECK (RateConverter_set_state (r2, blob) == DP_OK);
  ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
  CHECK (RateConverter_set_state (r2, blob) == DP_ERR_INVALID);
  ((char *)blob)[0] ^= (char)0xFF;
  nB += RateConverter_execute (r2, in + CUT, L - CUT, outB + nB, CAP - nB);
  RateConverter_destroy (r2);
  free (blob);

  CHECK (nA == nB);
  for (size_t i = 0; i < nA && i < nB; i++)
    CHECK (crealf (outA[i]) == crealf (outB[i])
           && cimagf (outA[i]) == cimagf (outB[i]));

  free (in);
  free (outA);
  free (outB);
}

/* ------------------------------------------------------------------ */

int
main (void)
{
  test_invalid_rate ();
  test_stage_labels ();
  test_output_length ();
  test_dc_gain ();
  test_set_rate ();
  test_reset_reproducible ();
  test_execute_max_out ();
  test_convert ();
  test_state_roundtrip ();

  if (_fails)
    {
      fprintf (stderr, "test_RateConverter_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_RateConverter_core PASSED\n");
  return 0;
}
