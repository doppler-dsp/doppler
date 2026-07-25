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
 *   - matched-filter terminal bank: always-append rule, bank sizing,
 *     droop fold, symbol recovery, push==block, state, set_rate
 */

#include "RateConverter/RateConverter_core.h"

#include "resamp/resamp_core.h"
#include "wfm/wfm_dsp.h"

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

/* execute_ctrl forwards a scalar rate deviation to the terminal resamp stage:
 * ctrl == 0 reproduces execute() bit-for-bit; ctrl != 0 changes the output
 * (the steered accumulator crosses at different times); and a cascade with no
 * terminal resamp stage falls through to execute() with ctrl ignored. */
static void
test_execute_ctrl (void)
{
  enum
  {
    N   = 4096,
    CAP = 8192
  };
  float _Complex *in = malloc (N * sizeof (float _Complex));
  float _Complex *o0 = malloc (CAP * sizeof (float _Complex));
  float _Complex *oc = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < (size_t)N; i++)
    {
      double ph = 2.0 * M_PI * 0.02 * (double)i;
      in[i]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }

  /* NB: for decimation resamp_execute uses the transposed-form polyphase path
   * while the ctrl port uses the unified accumulator (a different algorithm),
   * so execute_ctrl(ctrl=0) != execute() by construction — we assert the ctrl
   * port's own invariants (reproducible + a non-zero deviation steers). */

  /* rate 0.8 → a single Resampler stage. */
  {
    RateConverter_state_t *a = RateConverter_create (0.8, 0);
    RateConverter_state_t *b = RateConverter_create (0.8, 0);
    CHECK (a && b && a->stage_types[a->n_stages - 1] == RC_STAGE_RESAMP);
    size_t n0 = RateConverter_execute_ctrl (a, in, N, 0.0, o0, CAP);
    size_t nr = RateConverter_execute_ctrl (b, in, N, 0.0, oc, CAP);
    CHECK (n0 == nr); /* deterministic */
    int repro = (n0 == nr);
    for (size_t i = 0; i < n0 && i < nr; i++)
      if (o0[i] != oc[i])
        repro = 0;
    CHECK (repro);
    for (size_t i = 0; i < n0; i++)
      CHECK (isfinite (crealf (o0[i])) && isfinite (cimagf (o0[i])));
    RateConverter_destroy (b);
    b            = RateConverter_create (0.8, 0);
    size_t nc    = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    moved = (nc != n0);
    for (size_t i = 0; i < n0 && i < nc && !moved; i++)
      if (o0[i] != oc[i])
        moved = 1;
    CHECK (moved); /* a non-zero deviation actually steers the stage */
    RateConverter_destroy (a);
    RateConverter_destroy (b);
  }

  /* rate 0.1 → CIC + Resampler cascade: ctrl steers the terminal stage. */
  {
    RateConverter_state_t *a = RateConverter_create (0.1, 0);
    RateConverter_state_t *b = RateConverter_create (0.1, 0);
    CHECK (a && b && a->n_stages >= 2
           && a->stage_types[a->n_stages - 1] == RC_STAGE_RESAMP);
    size_t n0    = RateConverter_execute_ctrl (a, in, N, 0.0, o0, CAP);
    size_t nc    = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    moved = (nc != n0);
    for (size_t i = 0; i < n0 && i < nc && !moved; i++)
      if (o0[i] != oc[i])
        moved = 1;
    CHECK (moved);
    for (size_t i = 0; i < n0; i++)
      CHECK (isfinite (crealf (o0[i])) && isfinite (cimagf (o0[i])));
    RateConverter_destroy (a);
    RateConverter_destroy (b);
  }

  /* rate 0.5 → HalfbandDecimator only (no resamp stage): execute_ctrl falls
   * through to execute, ctrl ignored. */
  {
    RateConverter_state_t *a = RateConverter_create (0.5, 0);
    RateConverter_state_t *b = RateConverter_create (0.5, 0);
    CHECK (a && b && a->stage_types[a->n_stages - 1] != RC_STAGE_RESAMP);
    size_t na   = RateConverter_execute (a, in, N, o0, CAP);
    size_t nb   = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    same = (na == nb);
    for (size_t i = 0; i < na && i < nb; i++)
      if (o0[i] != oc[i])
        same = 0;
    CHECK (same); /* no fractional stage → ctrl has no effect */
    RateConverter_destroy (a);
    RateConverter_destroy (b);
  }

  free (in);
  free (o0);
  free (oc);
}

/* ------------------------------------------------------------------ */
/* Matched-filter terminal bank                                        */
/* ------------------------------------------------------------------ */

#define _MF_BETA 0.35
#define _MF_SPAN 8
#define _MF_NSYM 500

/* Deterministic +-1 BPSK. */
static int
_mf_bit (int k)
{
  unsigned x = (unsigned)k * 1103515245u + 12345u;
  return ((x >> 16) & 1) ? 1 : -1;
}

/* Analytic RRC-shaped BPSK at `sps' samples/symbol, timing phase `phi'
 * symbols.  Amplitude is kept well inside cic_core's Q15 full scale: a CIC
 * stage quantizes at its boundary, so overdriving it clips and the measured
 * EVM collapses for reasons unrelated to the matched filter. */
static float _Complex *
_mf_tx (double sps, double phi, size_t *n_out)
{
  size_t          n = (size_t)(_MF_NSYM * sps) + 64;
  float _Complex *x = calloc (n, sizeof *x);
  if (!x)
    return NULL;
  for (size_t i = 0; i < n; i++)
    {
      double a = 0.0;
      for (int k = 0; k < _MF_NSYM; k++)
        {
          double t = ((double)i - (k + _MF_SPAN) * sps) / sps - phi;
          if (fabs (t) > _MF_SPAN)
            continue;
          a += _mf_bit (k) * wfm_rrc_h (t, _MF_BETA);
        }
      x[i] = (float)(0.25 * a);
    }
  *n_out = n;
  return x;
}

/* Best EVM over strobe alignment: open loop, the cascade's own strobe phase
 * is arbitrary, so the minimum isolates the filter from the timing loop that
 * does not exist yet (that is Layer 2's job). */
static double
_mf_evm (const float _Complex *y, size_t ny)
{
  double best = 1e9;
  for (int par = 0; par < 2; par++)
    for (int lag = 0; lag < 140; lag++)
      {
        double num = 0.0;
        int    cnt = 0;
        for (int k = 40; k < _MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            num += _mf_bit (k) * creal (y[i]);
            cnt++;
          }
        if (cnt < 100)
          continue;
        double g = num / cnt, e = 0.0, p = 0.0;
        for (int k = 40; k < _MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            double d = creal (y[i]) - g * _mf_bit (k);
            e += d * d + cimag (y[i]) * cimag (y[i]);
            p += g * g;
          }
        double v = sqrt (e / p);
        if (v < best)
          best = v;
      }
  return best;
}

/* Min EVM in dB over a sweep of transmit timing phases. */
static double
_mf_best_evm_db (double sps, int compensate)
{
  double best = 1e9;
  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x = _mf_tx (sps, j / 16.0, &n);
      float _Complex *y = calloc (n, sizeof *y);
      if (!x || !y)
        {
          free (x);
          free (y);
          return 0.0;
        }
      RateConverter_state_t *rc = RateConverter_create_matched (
          2.0 / sps, compensate, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
      CHECK (rc != NULL);
      if (rc)
        {
          size_t ny = RateConverter_execute (rc, x, n, y, n);
          double e  = _mf_evm (y, ny);
          if (e < best)
            best = e;
          RateConverter_destroy (rc);
        }
      free (x);
      free (y);
    }
  return 20.0 * log10 (best);
}

static size_t
_mf_terminal_taps (const RateConverter_state_t *s)
{
  return resamp_get_num_taps (
      (const resamp_state_t *)s->stage_ptrs[s->n_stages - 1]);
}

static void
test_matched_invalid_params (void)
{
  /* rate, beta, span, pulse_sps, num_phases and the pulse tag are all
     rejected rather than silently coerced. */
  CHECK (
      RateConverter_create_matched (0.0, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1024)
      == NULL);
  CHECK (RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 1.5, 8, 2.0, 1024)
         == NULL);
  CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, -0.1, 8, 2.0, 1024)
      == NULL);
  CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 0, 2.0, 1024)
      == NULL);
  CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 0.0, 1024)
      == NULL);
  /* num_phases must be a power of two >= 2 (the arm index is a bit field). */
  CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1000)
      == NULL);
  CHECK (RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1)
         == NULL);
  /* RC_PULSE_NONE is not a matched filter — use RateConverter_create(). */
  CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_NONE, 0.35, 8, 2.0, 1024)
      == NULL);
  /* NaN must be rejected, not accepted by a comparison that is false. */
  CHECK (RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, NAN, 8, 2.0, 1024)
         == NULL);
}

static void
test_matched_always_has_terminal_stage (void)
{
  /* The whole point: the plain planner drops the fractional stage whenever
     the integer stages already land the rate, leaving nothing for a timing
     loop to steer.  A pulse forces it to exist — at rate 1.0 if need be. */
  const double sps[] = { 4.0, 8.0, 16.0, 17.333333333, 64.0, 256.0 };
  char         buf[64];
  for (size_t i = 0; i < sizeof sps / sizeof sps[0]; i++)
    {
      RateConverter_state_t *m = RateConverter_create_matched (
          2.0 / sps[i], 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
      CHECK (m != NULL);
      if (!m)
        continue;
      CHECK (m->stage_types[m->n_stages - 1] == RC_STAGE_RESAMP);
      /* The label names the pulse, so a caller can see the matched filter is
         IN the cascade rather than a stage still to be appended. */
      CHECK (RateConverter_stage_label (m, m->n_stages - 1, buf, sizeof buf));
      CHECK (strstr (buf, "rrc") != NULL);
      RateConverter_destroy (m);
    }

  /* rate = 2/64 is the case that motivated the rule: exactly CIC(32) before,
     CIC(32) + a steerable Resampler(1.0) now. */
  RateConverter_state_t *p = RateConverter_create (2.0 / 64.0, 0);
  RateConverter_state_t *m = RateConverter_create_matched (
      2.0 / 64.0, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (p && m);
  if (p && m)
    {
      CHECK (p->n_stages == 1 && p->stage_types[0] == RC_STAGE_CIC);
      CHECK (m->n_stages == 2 && m->stage_types[1] == RC_STAGE_RESAMP);
      CHECK (_near (resamp_get_rate ((resamp_state_t *)m->stage_ptrs[1]), 1.0,
                    1e-12));
    }
  RateConverter_destroy (p);
  RateConverter_destroy (m);
}

static void
test_matched_bank_is_constant_in_input_rate (void)
{
  /* Sizing the bank by the POST-decimation rate is what makes this usable:
     matched-filtering at the input rate costs taps proportional to the input
     samples per symbol (thousands at sps=256).  Here the integer stages have
     already done the bulk decimation, so the tap count barely moves. */
  size_t                 t4 = 0, t256 = 0;
  RateConverter_state_t *a = RateConverter_create_matched (
      2.0 / 4.0, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  RateConverter_state_t *b = RateConverter_create_matched (
      2.0 / 256.0, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (a && b);
  if (a && b)
    {
      t4   = _mf_terminal_taps (a);
      t256 = _mf_terminal_taps (b);
      /* A 64x span of input rates, and the bank does not grow. */
      CHECK (t4 == t256);
      /* ~2*span*pulse_sps taps, not ~2*span*sps. */
      CHECK (t256 < 4 * _MF_SPAN * 2 + 8);
    }
  RateConverter_destroy (a);
  RateConverter_destroy (b);

  /* The rectangle is one symbol wide whatever span says, so it is smaller
     still. */
  RateConverter_state_t *r = RateConverter_create_matched (
      2.0 / 17.333333333, 0, RC_PULSE_IANDD, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (r != NULL);
  if (r)
    {
      CHECK (_mf_terminal_taps (r) < t256);
      RateConverter_destroy (r);
    }
}

static void
test_matched_droop_folds_into_bank (void)
{
  RateConverter_state_t *off = RateConverter_create_matched (
      2.0 / 17.333333333, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  RateConverter_state_t *on = RateConverter_create_matched (
      2.0 / 17.333333333, 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (off && on);
  if (off && on)
    {
      char buf[64];
      /* Same stage count, same shape — compensation costs taps, not a pass
         over the data.  A separate comp FIR would have made this "CIC(8)+FIR".
       */
      CHECK (off->n_stages == on->n_stages);
      CHECK (RateConverter_stage_label (on, 0, buf, sizeof buf));
      CHECK (strstr (buf, "FIR") == NULL);
      /* The fold is a per-arm convolution with the 7-tap compensator. */
      CHECK (_mf_terminal_taps (on) == _mf_terminal_taps (off) + 6);
    }
  RateConverter_destroy (off);
  RateConverter_destroy (on);

  /* And it works: on a CIC cascade the fold is worth ~28 dB of EVM, which is
     why `compensate` is effectively mandatory on this path. */
  double no_comp = _mf_best_evm_db (17.333333333, 0);
  double comp    = _mf_best_evm_db (17.333333333, 1);
  CHECK (comp < -45.0);
  CHECK (comp < no_comp - 20.0);
  if (!(comp < -45.0) || !(comp < no_comp - 20.0))
    fprintf (stderr, "  droop fold: comp=%.1f dB  no_comp=%.1f dB\n", comp,
             no_comp);
}

static void
test_matched_recovers_symbols (void)
{
  /* A halfband cascade has no quantizing stage, so it shows what the bank
     itself is worth; the CIC path is limited by the CIC, not by the fold. */
  double hb  = _mf_best_evm_db (4.0, 0);
  double cic = _mf_best_evm_db (17.333333333, 1);
  CHECK (hb < -55.0);
  CHECK (cic < -45.0);
  if (!(hb < -55.0) || !(cic < -45.0))
    fprintf (stderr, "  matched EVM: halfband=%.1f dB  cic=%.1f dB\n", hb,
             cic);
}

static void
test_matched_push_equals_block (void)
{
  /* The per-input streaming form is the one a closed loop can use; it has to
     agree with the cheap block form at constant ctrl or the loop and the
     open-loop path would be different filters. */
  const double sps[] = { 4.0, 17.333333333, 64.0 };
  for (size_t s = 0; s < 3; s++)
    {
      size_t          n;
      float _Complex *x = _mf_tx (sps[s], 0.3, &n);
      float _Complex *y = calloc (n, sizeof *y);
      float _Complex *z = calloc (n, sizeof *z);
      CHECK (x && y && z);
      if (!x || !y || !z)
        {
          free (x);
          free (y);
          free (z);
          continue;
        }
      RateConverter_state_t *a = RateConverter_create_matched (
          2.0 / sps[s], 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
      RateConverter_state_t *b = RateConverter_create_matched (
          2.0 / sps[s], 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
      RateConverter_state_t *c = RateConverter_create_matched (
          2.0 / sps[s], 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
      CHECK (a && b && c);
      if (a && b && c)
        {
          size_t ny = RateConverter_execute_ctrl (a, x, n, 0.0, y, n);
          size_t nz = 0;
          for (size_t i = 0; i < n; i++)
            nz += RateConverter_execute_ctrl_push (b, x[i], 0.0, z + nz,
                                                   n - nz);
          int same = (ny == nz);
          for (size_t i = 0; same && i < nz; i++)
            same = (y[i] == z[i]);
          CHECK (same);

          /* execute() on a matched cascade must be the SAME algorithm: the
             pulse bank is laid out for the unified accumulator, while
             resamp_execute()'s decimating path is transposed-form and indexes
             arms the other way. */
          size_t nw    = RateConverter_execute (c, x, n, z, n);
          int    same2 = (ny == nw);
          for (size_t i = 0; same2 && i < nw; i++)
            same2 = (y[i] == z[i]);
          CHECK (same2);
        }
      RateConverter_destroy (a);
      RateConverter_destroy (b);
      RateConverter_destroy (c);
      free (x);
      free (y);
      free (z);
    }
}

static void
test_matched_state_roundtrip (void)
{
  size_t          n;
  float _Complex *x = _mf_tx (17.333333333, 0.2, &n);
  float _Complex *y = calloc (n, sizeof *y);
  float _Complex *z = calloc (n, sizeof *z);
  CHECK (x && y && z);
  if (!x || !y || !z)
    {
      free (x);
      free (y);
      free (z);
      return;
    }

  RateConverter_state_t *a = RateConverter_create_matched (
      2.0 / 17.333333333, 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  RateConverter_state_t *b = RateConverter_create_matched (
      2.0 / 17.333333333, 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (a && b);
  if (a && b)
    {
      size_t half = n / 2;
      /* Run a to mid-stream, hand its state to a fresh b, and require the
         remainder to match bit-for-bit. */
      RateConverter_execute (a, x, half, y, n);
      size_t nb = RateConverter_state_bytes (a);
      void  *bl = malloc (nb);
      CHECK (bl != NULL);
      if (bl)
        {
          RateConverter_get_state (a, bl);
          CHECK (RateConverter_set_state (b, bl) == DP_OK);
          size_t na   = RateConverter_execute (a, x + half, n - half, y, n);
          size_t nz   = RateConverter_execute (b, x + half, n - half, z, n);
          int    same = (na == nz);
          for (size_t i = 0; same && i < nz; i++)
            same = (y[i] == z[i]);
          CHECK (same);
          /* Envelope reject: a clobbered blob must not be reinterpreted. */
          ((char *)bl)[0] ^= 0xFF;
          CHECK (RateConverter_set_state (b, bl) == DP_ERR_INVALID);
          free (bl);
        }
    }
  RateConverter_destroy (a);
  RateConverter_destroy (b);
  free (x);
  free (y);
  free (z);
}

static void
test_matched_set_rate_keeps_pulse (void)
{
  /* set_rate() re-plans; the pulse is configuration, not part of the plan,
     so it must survive — including the always-append rule. */
  RateConverter_state_t *m = RateConverter_create_matched (
      2.0 / 17.333333333, 1, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  CHECK (m != NULL);
  if (m)
    {
      RateConverter_set_rate (m, 2.0 / 64.0);
      char buf[64];
      CHECK (m->stage_types[m->n_stages - 1] == RC_STAGE_RESAMP);
      CHECK (RateConverter_stage_label (m, m->n_stages - 1, buf, sizeof buf));
      CHECK (strstr (buf, "rrc") != NULL);
      /* Still folded, still no comp FIR stage. */
      CHECK (RateConverter_stage_label (m, 0, buf, sizeof buf));
      CHECK (strstr (buf, "FIR") == NULL);
      RateConverter_destroy (m);
    }
}

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
  test_execute_ctrl ();
  test_matched_invalid_params ();
  test_matched_always_has_terminal_stage ();
  test_matched_bank_is_constant_in_input_rate ();
  test_matched_droop_folds_into_bank ();
  test_matched_recovers_symbols ();
  test_matched_push_equals_block ();
  test_matched_state_roundtrip ();
  test_matched_set_rate_keeps_pulse ();

  if (_fails)
    {
      fprintf (stderr, "test_RateConverter_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_RateConverter_core PASSED\n");
  return 0;
}
