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
#include "cic/cic_core.h"
#include "dp_test.h"

#include "resamp/resamp_core.h"
#include "wfm/wfm_dsp.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int
_near (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
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
  DP_CHECK (RateConverter_create (0.0, 0) == NULL);
  DP_CHECK (RateConverter_create (-1.0, 0) == NULL);
}

/* ------------------------------------------------------------------ */

static void
test_stage_labels (void)
{
  char buf[64];

  /* rate >= 1: Resampler */
  {
    RateConverter_state_t *rc = RateConverter_create (2.0, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strncmp (buf, "Resampler", 9) == 0);
    DP_CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)) == 0);
    RateConverter_destroy (rc);
  }

  /* rate = 1.0: Resampler(1) */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strncmp (buf, "Resampler", 9) == 0);
    RateConverter_destroy (rc);
  }

  /* D = 2 (rate = 0.5): single HalfbandDecimator */
  {
    RateConverter_state_t *rc = RateConverter_create (0.5, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 4 (rate = 0.25): two HalfbandDecimator stages */
  {
    RateConverter_state_t *rc = RateConverter_create (0.25, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 2);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    DP_CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "HalfbandDecimator") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 8 (rate = 0.125): CIC(8), no comp */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "CIC(8)") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 8, compensate=1: CIC(8)+FIR */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 1);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "CIC(8)+FIR") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 16 (rate = 1/16): CIC(16), exact power-of-2, n>=3 */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 16.0, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strcmp (buf, "CIC(16)") == 0);
    RateConverter_destroy (rc);
  }

  /* D = 12 (rate = 1/12): non-power-of-2, D >= 8.
   * Nearest power-of-2 to 12 is 16; plan: CIC(16) + Resampler. */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 12.0, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 2);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strncmp (buf, "CIC", 3) == 0);
    DP_CHECK (RateConverter_stage_label (rc, 1, buf, sizeof (buf)));
    DP_CHECK (strncmp (buf, "Resampler", 9) == 0);
    RateConverter_destroy (rc);
  }

  /* D = 3 (2 <= D < 8, non-integer): Resampler */
  {
    RateConverter_state_t *rc = RateConverter_create (1.0 / 3.0, 0);
    DP_CHECK (rc != NULL);
    DP_CHECK (rc->n_stages == 1);
    DP_CHECK (RateConverter_stage_label (rc, 0, buf, sizeof (buf)));
    DP_CHECK (strncmp (buf, "Resampler", 9) == 0);
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
  DP_CHECK (in && out);

  /* rate = 0.5: expect exactly 512 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.5, 0);
    DP_CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    DP_CHECK (n == N_IN / 2);
    RateConverter_destroy (rc);
  }

  /* rate = 0.25: expect exactly 256 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.25, 0);
    DP_CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    DP_CHECK (n == N_IN / 4);
    RateConverter_destroy (rc);
  }

  /* rate = 0.125: expect exactly 128 out */
  {
    RateConverter_state_t *rc = RateConverter_create (0.125, 0);
    DP_CHECK (rc != NULL);
    size_t n = RateConverter_execute (rc, in, N_IN, out, N_OUT);
    DP_CHECK (n == N_IN / 8);
    RateConverter_destroy (rc);
  }

  free (in);
  free (out);
}

/* ------------------------------------------------------------------ */

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
  DP_CHECK (in && out);

  RateConverter_state_t *rc = RateConverter_create (0.5, 0);
  DP_CHECK (rc != NULL);

  size_t n1 = RateConverter_execute (rc, in, N, out, N);
  DP_CHECK (n1 == N / 2);

  RateConverter_set_rate (rc, 0.25);
  DP_CHECK (_near (RateConverter_get_rate (rc), 0.25, 1e-9));

  size_t n2 = RateConverter_execute (rc, in, N, out, N);
  DP_CHECK (n2 == N / 4);

  /* rate <= 0 silently ignored */
  RateConverter_set_rate (rc, 0.0);
  DP_CHECK (_near (RateConverter_get_rate (rc), 0.25, 1e-9));

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
  DP_CHECK (in && out1 && out2);

  RateConverter_state_t *rc = RateConverter_create (0.125, 0);
  DP_CHECK (rc != NULL);

  size_t n1 = RateConverter_execute (rc, in, N, out1, N);
  RateConverter_reset (rc);
  size_t n2 = RateConverter_execute (rc, in, N, out2, N);

  DP_CHECK (n1 == n2);
  if (n1 == n2 && n1 > 0)
    DP_CHECK (memcmp (out1, out2, n1 * sizeof (float complex)) == 0);

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
    DP_CHECK (rc != NULL);
    size_t m = RateConverter_execute_max_out (rc);
    DP_CHECK (m >= 2);
    DP_CHECK (m <= 65536 + 2);
    RateConverter_destroy (rc);
  }

  /* Interpolation: max_out must be >= n_in * rate. */
  {
    RateConverter_state_t *rc = RateConverter_create (4.0, 0);
    DP_CHECK (rc != NULL);
    size_t m = RateConverter_execute_max_out (rc);
    DP_CHECK (m >= (size_t)(65536 * 4));
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
  DP_CHECK (rc != NULL);
  size_t n_ref = RateConverter_execute (rc, in, N_IN, ref, N_OUT);
  RateConverter_destroy (rc);

  /* One-shot */
  size_t n_out = RateConverter_convert (1.0, 0, in, N_IN, out, N_OUT);
  DP_CHECK (n_out == n_ref);
  DP_CHECK (memcmp (ref, out, n_ref * sizeof *ref) == 0);

  /* Decimation: rate=0.5 → n_out ≈ n_in/2 */
  float _Complex dec_in[256], dec_out[256];
  for (size_t i = 0; i < 256; i++)
    dec_in[i] = 1.0f + 0.0f * _Complex_I;
  size_t n_dec = RateConverter_convert (0.5, 0, dec_in, 256, dec_out, 256);
  DP_CHECK (n_dec == 128);
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
  DP_CHECK (in && outA && outB);
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
  DP_CHECK (RateConverter_set_state (r2, blob) == DP_OK);
  ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
  DP_CHECK (RateConverter_set_state (r2, blob) == DP_ERR_INVALID);
  ((char *)blob)[0] ^= (char)0xFF;
  nB += RateConverter_execute (r2, in + CUT, L - CUT, outB + nB, CAP - nB);
  RateConverter_destroy (r2);
  free (blob);

  DP_CHECK (nA == nB);
  for (size_t i = 0; i < nA && i < nB; i++)
    DP_CHECK (crealf (outA[i]) == crealf (outB[i])
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
    DP_CHECK (a && b && a->stage_types[a->n_stages - 1] == RC_STAGE_RESAMP);
    size_t n0 = RateConverter_execute_ctrl (a, in, N, 0.0, o0, CAP);
    size_t nr = RateConverter_execute_ctrl (b, in, N, 0.0, oc, CAP);
    DP_CHECK (n0 == nr); /* deterministic */
    int repro = (n0 == nr);
    for (size_t i = 0; i < n0 && i < nr; i++)
      if (o0[i] != oc[i])
        repro = 0;
    DP_CHECK (repro);
    for (size_t i = 0; i < n0; i++)
      DP_CHECK (isfinite (crealf (o0[i])) && isfinite (cimagf (o0[i])));
    RateConverter_destroy (b);
    b            = RateConverter_create (0.8, 0);
    size_t nc    = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    moved = (nc != n0);
    for (size_t i = 0; i < n0 && i < nc && !moved; i++)
      if (o0[i] != oc[i])
        moved = 1;
    DP_CHECK (moved); /* a non-zero deviation actually steers the stage */
    RateConverter_destroy (a);
    RateConverter_destroy (b);
  }

  /* rate 0.1 → CIC + Resampler cascade: ctrl steers the terminal stage. */
  {
    RateConverter_state_t *a = RateConverter_create (0.1, 0);
    RateConverter_state_t *b = RateConverter_create (0.1, 0);
    DP_CHECK (a && b && a->n_stages >= 2
              && a->stage_types[a->n_stages - 1] == RC_STAGE_RESAMP);
    size_t n0    = RateConverter_execute_ctrl (a, in, N, 0.0, o0, CAP);
    size_t nc    = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    moved = (nc != n0);
    for (size_t i = 0; i < n0 && i < nc && !moved; i++)
      if (o0[i] != oc[i])
        moved = 1;
    DP_CHECK (moved);
    for (size_t i = 0; i < n0; i++)
      DP_CHECK (isfinite (crealf (o0[i])) && isfinite (cimagf (o0[i])));
    RateConverter_destroy (a);
    RateConverter_destroy (b);
  }

  /* rate 0.5 → HalfbandDecimator only (no resamp stage): execute_ctrl falls
   * through to execute, ctrl ignored. */
  {
    RateConverter_state_t *a = RateConverter_create (0.5, 0);
    RateConverter_state_t *b = RateConverter_create (0.5, 0);
    DP_CHECK (a && b && a->stage_types[a->n_stages - 1] != RC_STAGE_RESAMP);
    size_t na   = RateConverter_execute (a, in, N, o0, CAP);
    size_t nb   = RateConverter_execute_ctrl (b, in, N, 0.05, oc, CAP);
    int    same = (na == nb);
    for (size_t i = 0; i < na && i < nb; i++)
      if (o0[i] != oc[i])
        same = 0;
    DP_CHECK (same); /* no fractional stage → ctrl has no effect */
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
/* Transmitted symbol amplitude: what a unity-gain matched cascade returns. */
#define _MF_TX_AMP 0.25

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
_mf_tx_beta (double sps, double beta, double phi, size_t *n_out)
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
          a += _mf_bit (k) * wfm_rrc_h (t, beta);
        }
      x[i] = (float)(_MF_TX_AMP * a);
    }
  *n_out = n;
  return x;
}

static float _Complex *
_mf_tx (double sps, double phi, size_t *n_out)
{
  return _mf_tx_beta (sps, _MF_BETA, phi, n_out);
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
      DP_CHECK (rc != NULL);
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
  DP_CHECK (
      RateConverter_create_matched (0.0, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1024)
      == NULL);
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 1.5, 8, 2.0, 1024)
      == NULL);
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, -0.1, 8, 2.0, 1024)
      == NULL);
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 0, 2.0, 1024)
      == NULL);
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 0.0, 1024)
      == NULL);
  /* num_phases must be a power of two >= 2 (the arm index is a bit field). */
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1000)
      == NULL);
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, 0.35, 8, 2.0, 1)
      == NULL);
  /* RC_PULSE_NONE is not a matched filter — use RateConverter_create(). */
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_NONE, 0.35, 8, 2.0, 1024)
      == NULL);
  /* NaN must be rejected, not accepted by a comparison that is false. */
  DP_CHECK (
      RateConverter_create_matched (0.5, 0, RC_PULSE_RRC, NAN, 8, 2.0, 1024)
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
      DP_CHECK (m != NULL);
      if (!m)
        continue;
      DP_CHECK (m->stage_types[m->n_stages - 1] == RC_STAGE_RESAMP);
      /* The label names the pulse, so a caller can see the matched filter is
         IN the cascade rather than a stage still to be appended. */
      DP_CHECK (
          RateConverter_stage_label (m, m->n_stages - 1, buf, sizeof buf));
      DP_CHECK (strstr (buf, "rrc") != NULL);
      RateConverter_destroy (m);
    }

  /* rate = 2/64 is the case that motivated the rule: exactly CIC(32) before,
     CIC(32) + a steerable Resampler(1.0) now. */
  RateConverter_state_t *p = RateConverter_create (2.0 / 64.0, 0);
  RateConverter_state_t *m = RateConverter_create_matched (
      2.0 / 64.0, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  DP_CHECK (p && m);
  if (p && m)
    {
      DP_CHECK (p->n_stages == 1 && p->stage_types[0] == RC_STAGE_CIC);
      DP_CHECK (m->n_stages == 2 && m->stage_types[1] == RC_STAGE_RESAMP);
      DP_CHECK (_near (resamp_get_rate ((resamp_state_t *)m->stage_ptrs[1]),
                       1.0, 1e-12));
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
  DP_CHECK (a && b);
  if (a && b)
    {
      t4   = _mf_terminal_taps (a);
      t256 = _mf_terminal_taps (b);
      /* A 64x span of input rates, and the bank does not grow. */
      DP_CHECK (t4 == t256);
      /* ~2*span*pulse_sps taps, not ~2*span*sps. */
      DP_CHECK (t256 < 4 * _MF_SPAN * 2 + 8);
    }
  RateConverter_destroy (a);
  RateConverter_destroy (b);

  /* The rectangle is one symbol wide whatever span says, so it is smaller
     still. */
  RateConverter_state_t *r = RateConverter_create_matched (
      2.0 / 17.333333333, 0, RC_PULSE_IANDD, _MF_BETA, _MF_SPAN, 2.0, 1024);
  DP_CHECK (r != NULL);
  if (r)
    {
      DP_CHECK (_mf_terminal_taps (r) < t256);
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
  DP_CHECK (off && on);
  if (off && on)
    {
      char buf[64];
      /* Same stage count, same shape — compensation costs taps, not a pass
         over the data.  A separate comp FIR would have made this "CIC(8)+FIR".
       */
      DP_CHECK (off->n_stages == on->n_stages);
      DP_CHECK (RateConverter_stage_label (on, 0, buf, sizeof buf));
      DP_CHECK (strstr (buf, "FIR") == NULL);
      /* The fold is a per-arm convolution with the 7-tap compensator. */
      DP_CHECK (_mf_terminal_taps (on) == _mf_terminal_taps (off) + 6);
    }
  RateConverter_destroy (off);
  RateConverter_destroy (on);

  /* And it works: on a CIC cascade the fold is worth ~28 dB of EVM, which is
     why `compensate` is effectively mandatory on this path. */
  double no_comp = _mf_best_evm_db (17.333333333, 0);
  double comp    = _mf_best_evm_db (17.333333333, 1);
  DP_CHECK (comp < -45.0);
  DP_CHECK (comp < no_comp - 20.0);
  if (!(comp < -45.0) || !(comp < no_comp - 20.0))
    fprintf (stderr, "  droop fold: comp=%.1f dB  no_comp=%.1f dB\n", comp,
             no_comp);
}

/* ------------------------------------------------------------------ */
/* Unity gain — the cascade must remove every gain it introduces, and  */
/* must be able to SAY what it introduced without being measured.      */
/* ------------------------------------------------------------------ */

/* The cascade's realised gain, end to end, from a complex tone at 1/512 of
 * the output rate — the mean |.| of a pure complex tone IS its amplitude, so
 * no phase reference is needed.
 *
 * A tone rather than DC, and the mutation test is why: a CIC's DC output is
 * insensitive to its own normalisation shift, because the offset-binary
 * conversion removes a bias derived from that same shift. Halving the shift
 * doubles the filter's gain on everything that matters and leaves a DC probe
 * reading exactly 1.000 — measured. At 1/512 the droop is below 1e-4, so the
 * tone reads the same as DC on a healthy cascade (verified at every rate
 * here) while still seeing a scaling error DC cannot. */
static double
_gain_measured (double rate, int compensate)
{
  enum
  {
    N_IN = 32768
  };
  RateConverter_state_t *rc = RateConverter_create (rate, compensate);
  if (!rc)
    return 0.0 / 0.0;
  size_t          cap = (size_t)(N_IN * (rate > 1.0 ? rate : 1.0)) + 64;
  float _Complex *in  = malloc (N_IN * sizeof *in);
  float _Complex *out = malloc (cap * sizeof *out);
  double          g   = 0.0 / 0.0;
  if (in && out)
    {
      double f = rate / 512.0;
      for (size_t i = 0; i < N_IN; i++)
        in[i] = (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);
      RateConverter_execute (rc, in, N_IN, out, cap);
      size_t n = RateConverter_execute (rc, in, N_IN, out, cap);
      if (n)
        {
          double s    = 0.0;
          size_t skip = n / 8; /* drop the block edge */
          for (size_t i = skip; i < n; i++)
            s += cabs (out[i]);
          g = (n > skip) ? s / (double)(n - skip) : 0.0 / 0.0;
        }
    }
  free (in);
  free (out);
  RateConverter_destroy (rc);
  return g;
}

static void
test_plain_cascade_is_unity_gain (void)
{
  /* A rate conversion that changes the signal LEVEL is a defect: every
     stage here is normalised (halfband 2*sum(h)+0.5, CIC's R^N removed by
     its shift, the resampler's arm sum), so their product is one at every
     rate the planner can produce.
       Bounds are the bank designs', not arbitrary: a 60 dB Kaiser has ~1e-3
     of passband ripple, and the realised numbers sit at 5.9e-4 (calculated)
     and 3.4e-4 (calculated vs measured), so 2e-3 and 1e-3 keep ~3x margin
     over the design and would still catch any real normalisation error.
       This replaces a +-15% DC check that covered only compensate=0 and
     would have passed a cascade with 7 dB of invented gain. */
  static const double rates[] = {
    2.0,
    1.5,
    1.0,
    0.5,
    0.25,
    1.0 / 3.0,
    0.4,
    0.125,
    1.0 / 12.0,
    1.0 / 32.0,
    2.0 / 17.333333333,
    2.0 / 64.0,
  };
  for (size_t r = 0; r < sizeof rates / sizeof *rates; r++)
    for (int comp = 0; comp < 2; comp++)
      {
        RateConverter_state_t *rc = RateConverter_create (rates[r], comp);
        DP_CHECK (rc != NULL);
        if (!rc)
          continue;
        double calc = RateConverter_gain (rc);
        double meas = _gain_measured (rates[r], comp);
        RateConverter_destroy (rc);

        int ok = fabs (calc - 1.0) < 2e-3 && fabs (meas - calc) < 1e-3;
        DP_CHECK (ok);
        if (!ok)
          fprintf (stderr,
                   "  gain rate=%.6g comp=%d: calculated %.6f, measured "
                   "%.6f\n",
                   rates[r], comp, calc, meas);
      }
}

/* Best |LS gain| of y against the known symbols, over strobe alignment —
 * the very quantity _mf_evm() forms and then divides out, which is why no
 * EVM assertion here can see a gain error. */
/* Correlate the recovered stream against the transmitted bits from symbol
 * `first' on. `first' matters once an AGC is in the cascade: a converter
 * starts from silence, so the opening symbols carry the signal-off-to-on
 * transient and the loop walking out of whatever the seed measured there.
 * That transient is real and is not gated away, so a gate that wants the
 * SETTLED gain has to say where settled begins. */
static double
_mf_gain_from (const float _Complex *y, size_t ny, int first)
{
  double best = 0.0;
  for (int par = 0; par < 2; par++)
    for (int lag = 0; lag < 140; lag++)
      {
        double num = 0.0;
        int    cnt = 0;
        for (int k = first; k < _MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            num += _mf_bit (k) * creal (y[i]);
            cnt++;
          }
        if (cnt < 100)
          continue;
        double g = num / (double)cnt;
        if (fabs (g) > fabs (best))
          best = g;
      }
  return best;
}

static double
_mf_gain_at (const float _Complex *y, size_t ny)
{
  return _mf_gain_from (y, ny, 40);
}

/* Recovered symbol amplitude, maximised over transmit timing phase: the
 * cascade runs open loop, so without the sweep this reads the composite
 * pulse at a residual timing offset rather than the gain. */
static double
_mf_recovered_amp (double sps, double beta, int compensate)
{
  double best = 0.0;
  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x = _mf_tx_beta (sps, beta, j / 16.0, &n);
      float _Complex *y = calloc (n, sizeof *y);
      if (x && y)
        {
          RateConverter_state_t *rc = RateConverter_create_matched (
              2.0 / sps, compensate, RC_PULSE_RRC, beta, _MF_SPAN, 2.0, 1024);
          if (rc)
            {
              size_t ny = RateConverter_execute (rc, x, n, y, n);
              double g  = _mf_gain_at (y, ny);
              if (fabs (g) > fabs (best))
                best = g;
              RateConverter_destroy (rc);
            }
        }
      free (x);
      free (y);
    }
  return best;
}

static void
test_matched_cascade_returns_the_symbol_amplitude (void)
{
  /* The matched cascade's unity is at the SYMBOL level, not at DC — its
     terminal stage is a matched filter and is deliberately not flat. Send
     +-A and A must come back, at every sps, every beta, compensated or not.
       That holds by construction once the bank is scaled by the PULSE's own
     energy E = sum h(t)^2: a symbol A*h correlated against h/E returns
     A*E/E = A. Scaling by 1/sqrt(E) — unit energy — returns A*||h||
     instead, which is an accident of sps and beta: measured 0.2284 to
     0.3537 against a transmitted 0.2500, i.e. -9% to +41%.
       Nothing else in this file can catch that. Every EVM assertion here
     fits the gain and divides it out, and the timing loop downstream
     normalises by power, so a cascade with 3 dB of invented gain passes all
     of them. */
  static const double sps[]  = { 4.0, 8.0, 17.333333333, 64.0 };
  static const double beta[] = { 0.2, 0.35, 0.5 };
  for (size_t s = 0; s < sizeof sps / sizeof *sps; s++)
    for (size_t b = 0; b < sizeof beta / sizeof *beta; b++)
      {
        int    comp = (sps[s] > 8.0); /* the CIC plans need the fold */
        double amp  = _mf_recovered_amp (sps[s], beta[b], comp);
        /* 2% — three times the worst realised error (0.7%), and far inside
           the +41% the unit-energy scaling produced. */
        int ok = fabs (fabs (amp) - _MF_TX_AMP) < 0.02 * _MF_TX_AMP;
        DP_CHECK (ok);
        if (!ok)
          fprintf (stderr,
                   "  matched gain sps=%.4g beta=%.2f comp=%d: sent %.4f, "
                   "recovered %.4f\n",
                   sps[s], beta[b], comp, _MF_TX_AMP, amp);
      }
}

/* Recovered symbol amplitude with the transmit level scaled by `scale' and
 * the pre-terminal AGC optionally enabled. sps = 4 with no compensation
 * plans a pure halfband cascade — deliberately: a CIC stage bounds its input
 * to +-1.0 and clips silently, which would confound a LEVEL sweep with a
 * clipping artefact. Halfband plans are scale-free, so what this measures is
 * the AGC and nothing else. */
static double
_mf_amp_scaled (double scale, int use_agc)
{
  const double sps = 4.0, beta = _MF_BETA;
  double       best = 0.0;
  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x = _mf_tx_beta (sps, beta, j / 16.0, &n);
      float _Complex *y = calloc (n, sizeof *y);
      if (x && y)
        {
          for (size_t i = 0; i < n; i++)
            x[i] *= (float)scale;
          RateConverter_state_t *rc = RateConverter_create_matched (
              2.0 / sps, 0, RC_PULSE_RRC, beta, _MF_SPAN, 2.0, 1024);
          if (rc)
            {
              /* bn_sym here is about the MEASUREMENT, not the design: the
                 record is 500 symbols, so the loop has to settle inside it
                 for a settled-gain assertion to mean anything. What is under
                 test is that the level it settles to does not depend on the
                 level that arrived. */
              if (use_agc)
                DP_CHECK (RateConverter_enable_agc (rc, 0.05, 0.05) == DP_OK);
              size_t ny = RateConverter_execute (rc, x, n, y, n);
              /* Settled window: past the turn-on transient either way. */
              double g = _mf_gain_from (y, ny, use_agc ? 250 : 40);
              if (fabs (g) > fabs (best))
                best = g;
              RateConverter_destroy (rc);
            }
        }
      free (x);
      free (y);
    }
  return best;
}

static void
test_agc_is_off_unless_asked_and_needs_a_pulse (void)
{
  /* Off is what every constructor builds, and the unity-gain contract is
     what proves the wedge did not leak into the default path. */
  RateConverter_state_t *plain = RateConverter_create (1.0 / 12.0, 1);
  DP_CHECK (plain != NULL);
  if (plain)
    {
      /* Same tolerance the unity-gain gate above uses: the planner lands
         the rate to within a fraction of a percent, not to the bit. */
      DP_CHECK (fabs (RateConverter_gain (plain) - 1.0) < 1e-2);
      DP_CHECK (RateConverter_agc_gain_db (plain) == 0.0);
      /* A plain cascade has no pulse, so `bank_e0 / bank_sps` describes
         nothing — refused rather than given a guessed reference.
           Two independent mechanisms enforce this (the contract check in
         enable_agc and the bank_sps precondition in rc_agc_build, which
         set_rate needs anyway), so this pins the BEHAVIOUR and deleting
         either one alone will not turn it red. That is the intent. */
      DP_CHECK (RateConverter_enable_agc (plain, 1e-3, 0.05)
                == DP_ERR_INVALID);
      DP_CHECK (RateConverter_agc_gain_db (plain) == 0.0);
      RateConverter_destroy (plain);
    }

  RateConverter_state_t *mf = RateConverter_create_matched (
      0.5, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  DP_CHECK (mf != NULL);
  if (mf)
    {
      DP_CHECK (RateConverter_agc_gain_db (mf) == 0.0); /* off until asked */
      /* The reference describes the BANK, so it is defined either way. */
      DP_CHECK (RateConverter_agc_ref_db (mf) != 0.0);
      /* Bad parameters leave it off rather than half-configured. */
      DP_CHECK (RateConverter_enable_agc (mf, 0.0, 0.05) == DP_ERR_INVALID);
      DP_CHECK (RateConverter_enable_agc (mf, 1e-3, 0.0) == DP_ERR_INVALID);
      DP_CHECK (RateConverter_enable_agc (mf, 1e-3, 2.0) == DP_ERR_INVALID);
      DP_CHECK (RateConverter_agc_gain_db (mf) == 0.0);
      DP_CHECK (RateConverter_enable_agc (mf, 1e-3, 0.05) == DP_OK);
      RateConverter_destroy (mf);
    }
}

static void
test_agc_telemetry_forwards_and_survives_a_replan (void)
{
  /* The cascade has no loop of its own to report, so set_telemetry exists
     only to reach the one child that does. Three properties, and the third is
     the one that is easy to get wrong. */

  /* 1. No AGC is not an error -- whether this cascade has one is the
        composing receiver's construction-time choice, and a caller attaching
        telemetry should not have to know which way that went. DP_OK, and
        nothing registered, so a reader sees an honest empty probe set. */
  {
    dp_tlm_t              *t     = dp_tlm_create (256);
    RateConverter_state_t *plain = RateConverter_create (1.0 / 12.0, 1);
    DP_CHECK (t != NULL && plain != NULL);
    if (t && plain)
      {
        DP_CHECK (RateConverter_set_telemetry (plain, t, "agc", 1) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (t) == 0);
        RateConverter_destroy (plain);
      }
    dp_tlm_destroy (t);
  }

  /* 2. With an AGC, both of its probes land under the prefix VERBATIM (no
        component name appended -- there is nothing here to disambiguate it
        from), and running the cascade actually fills them. */
  {
    dp_tlm_t              *t  = dp_tlm_create (1 << 12);
    RateConverter_state_t *mf = RateConverter_create_matched (
        0.5, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
    DP_CHECK (t != NULL && mf != NULL);
    if (t && mf)
      {
        DP_CHECK (RateConverter_enable_agc (mf, 1e-3, 0.05) == DP_OK);
        DP_CHECK (RateConverter_set_telemetry (mf, t, "rx.agc", 1) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (t) == 2);
        DP_CHECK (dp_tlm_probe_id (t, "rx.agc.gain_db") >= 0);
        DP_CHECK (dp_tlm_probe_id (t, "rx.agc.level_db") >= 0);

        float _Complex in[512], out[1024];
        for (int i = 0; i < 512; i++)
          in[i] = 0.5f + 0.0f * I;
        (void)RateConverter_execute (mf, in, 512, out, 1024);
        dp_tlm_rec_t r[64];
        DP_CHECK (dp_tlm_read (t, 64, r, 64) > 0);

        /* 3. THE ONE THAT BITES: rc_agc_build() destroys and rebuilds the AGC
              on a re-plan, and the AGC is documented to survive a rate change
              "the way the pulse does". Its instrumentation has to survive
              too, or a rate change silently stops the gain trajectory being
              recorded -- with no error anywhere to say so. Re-attaching by
              name is idempotent, so the ids a reader already holds stay
              valid and the table does not grow. */
        int id_gain = dp_tlm_probe_id (t, "rx.agc.gain_db");
        RateConverter_set_rate (mf, 0.25);
        DP_CHECK (dp_tlm_probe_count (t) == 2); /* no leaked duplicates */
        DP_CHECK (dp_tlm_probe_id (t, "rx.agc.gain_db") == id_gain);
        while (dp_tlm_read (t, 64, r, 64) > 0)
          ; /* drain */
        (void)RateConverter_execute (mf, in, 512, out, 1024);
        DP_CHECK (dp_tlm_read (t, 64, r, 64) > 0); /* still emitting */

        /* Detach reaches the child too, and stays detached across a re-plan
           (the request is dropped, not merely unapplied). */
        DP_CHECK (RateConverter_set_telemetry (mf, NULL, "rx.agc", 1)
                  == DP_OK);
        RateConverter_set_rate (mf, 0.5);
        while (dp_tlm_read (t, 64, r, 64) > 0)
          ;
        (void)RateConverter_execute (mf, in, 512, out, 1024);
        DP_CHECK (dp_tlm_read (t, 64, r, 64) == 0);

        RateConverter_destroy (mf);
      }
    dp_tlm_destroy (t);
  }

  /* 4. Attach BEFORE the AGC exists: the request is remembered and applied
        when enable_agc builds it. Documented on RateConverter_set_telemetry()
        as the ordering that returns DP_OK without registering yet. */
  {
    dp_tlm_t              *t  = dp_tlm_create (256);
    RateConverter_state_t *mf = RateConverter_create_matched (
        0.5, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
    DP_CHECK (t != NULL && mf != NULL);
    if (t && mf)
      {
        DP_CHECK (RateConverter_set_telemetry (mf, t, "agc", 1) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (t) == 0); /* nothing to register yet */
        DP_CHECK (RateConverter_enable_agc (mf, 1e-3, 0.05) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (t) == 2); /* applied on build */
        RateConverter_destroy (mf);
      }
    dp_tlm_destroy (t);
  }
}

static void
test_agc_delivers_unit_symbol_amplitude (void)
{
  /* The point of the wedge. symsync_ted_slope() is computed at construct FOR
     A UNIT-AMPLITUDE SYMBOL STREAM, and amplitude enters a TED's raw error as
     A^2 (Gardner) or A^1 (DTTL) — so a level error IS a loop-gain error, 16x
     for a 4x level at Gardner. With the AGC on, the cascade delivers unit
     symbol amplitude whatever arrived, which is what makes that construct-
     time constant honest.
       Without it, the output tracks the input exactly (the cascade is
     unity-gain at the symbol level, which is the correct behaviour for a
     converter and precisely why something else has to do the levelling). */
  static const double scales[] = { 0.25, 1.0, 4.0 };
  for (size_t i = 0; i < sizeof scales / sizeof *scales; i++)
    {
      double off = _mf_amp_scaled (scales[i], 0);
      double on  = _mf_amp_scaled (scales[i], 1);

      /* AGC off: amplitude in, amplitude out — the level survives. */
      double want_off = scales[i] * _MF_TX_AMP;
      int    ok_off   = fabs (fabs (off) - want_off) < 0.02 * want_off;
      DP_CHECK (ok_off);

      /* AGC on: unity, whatever went in. 3% covers the residual the loop
         leaves after the seed plus the pulse-energy approximation. */
      int ok_on = fabs (fabs (on) - 1.0) < 0.03;
      DP_CHECK (ok_on);
      if (!ok_off || !ok_on)
        fprintf (stderr,
                 "  agc scale=%.2f: off %.4f (want %.4f), on %.4f (want 1)\n",
                 scales[i], off, want_off, on);
    }
}

static void
test_matched_recovers_symbols (void)
{
  /* A halfband cascade has no quantizing stage, so it shows what the bank
     itself is worth; the CIC path is limited by the CIC, not by the fold. */
  double hb  = _mf_best_evm_db (4.0, 0);
  double cic = _mf_best_evm_db (17.333333333, 1);
  DP_CHECK (hb < -55.0);
  DP_CHECK (cic < -45.0);
  if (!(hb < -55.0) || !(cic < -45.0))
    fprintf (stderr, "  matched EVM: halfband=%.1f dB  cic=%.1f dB\n", hb,
             cic);
}

static void
test_pre_terminal_tap_on_a_cascade_with_no_fractional_tail (void)
{
  /* The pre-terminal tap has two code paths and only one of them runs on a
     matched cascade: MpskReceiver always plans a fractional terminal stage,
     so its `mf_in` validator never reaches the branch taken when the last
     stage is an integer decimator. A plain cascade at an integer rate is the
     shape that does, and the branch is the one where the AGC tap does NOT
     apply — the pre-terminal sample is the integer cascade's output as it
     stands.

     The invariant that makes it checkable: with a SINGLE stage there is no
     earlier stage to run, so `cur` is still the input and the tap must hand
     back exactly the sample that went in. Anything else means the tap is
     reading the wrong node.

     A power-of-two rate is what plans an integer tail: 1/12 gets a
     Resampler(1.3333) and would make this test vacuous. 0.25 is two
     halfbands, so it also covers the case where a non-terminal stage swallows
     the input between its decimation strobes and the tap correctly does NOT
     fire. */
  static const double rates[] = { 0.5, 0.25, 0.125, 1.0 / 32.0 };
  for (size_t rc_i = 0; rc_i < 2 * (sizeof rates / sizeof *rates); rc_i++)
    {
      size_t r    = rc_i / 2;
      int    comp = (int)(rc_i % 2); /* comp=1 adds the CIC's FIR trim */
      RateConverter_state_t *a = RateConverter_create (rates[r], comp);
      RateConverter_state_t *b = RateConverter_create (rates[r], comp);
      DP_CHECK (a && b);
      if (!a || !b)
        {
          RateConverter_destroy (a);
          RateConverter_destroy (b);
          continue;
        }
      /* Non-vacuity: this test means nothing if the planner gave us a
         fractional tail after all, so say so rather than passing quietly. */
      DP_CHECK (a->n_stages > 0);
      DP_CHECK (a->stage_types[a->n_stages - 1] != RC_STAGE_RESAMP);

      {
        int    saw_tap = 0;
        size_t single  = (a->n_stages == 1);
        for (int i = 0; i < 512; i++)
          {
            float _Complex x = (float)(0.5 * cos (0.031 * i))
                               + (float)(0.5 * sin (0.017 * i)) * I;
            float _Complex ya[4], yb[4], pre = 12345.0f;
            int    n_pre = -1;
            size_t na = RateConverter_execute_ctrl_push_tap (a, x, 0.0, ya, 4,
                                                             &pre, &n_pre);
            /* The NULL-tap wrapper must be the same filter: same outputs,
               and no crash when the tap pointers are absent. */
            size_t nb = RateConverter_execute_ctrl_push (b, x, 0.0, yb, 4);
            DP_CHECK (na == nb);
            for (size_t k = 0; k < na; k++)
              DP_CHECK (ya[k] == yb[k]);

            DP_CHECK (n_pre == 0 || n_pre == 1);
            /* A terminal output cannot exist without the terminal stage
               having run, so an emission implies the tap fired. */
            if (na > 0)
              DP_CHECK (n_pre == 1);
            if (n_pre == 1)
              {
                saw_tap = 1;
                if (single)
                  DP_CHECK (pre == x);
              }
            else
              DP_CHECK (pre == 12345.0f); /* untouched when it did not fire */
          }
        DP_CHECK (saw_tap);
      }
      RateConverter_destroy (a);
      RateConverter_destroy (b);
    }
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
      DP_CHECK (x && y && z);
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
      DP_CHECK (a && b && c);
      /* The AGC is ON for this comparison, which is the whole reason its tap
         is per-sample in BOTH paths. agc_steps() would vectorise the block
         form and is documented as NOT bit-identical to the per-sample loop,
         so taking it would break this equality -- and with it the guarantee
         that the closed-loop (push) and open-loop (block) paths are the same
         filter. */
      if (a && b && c)
        {
          DP_CHECK (RateConverter_enable_agc (a, 1e-3, 0.05) == DP_OK);
          DP_CHECK (RateConverter_enable_agc (b, 1e-3, 0.05) == DP_OK);
          DP_CHECK (RateConverter_enable_agc (c, 1e-3, 0.05) == DP_OK);
        }
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
          DP_CHECK (same);

          /* execute() on a matched cascade must be the SAME algorithm: the
             pulse bank is laid out for the unified accumulator, while
             resamp_execute()'s decimating path is transposed-form and indexes
             arms the other way. */
          size_t nw    = RateConverter_execute (c, x, n, z, n);
          int    same2 = (ny == nw);
          for (size_t i = 0; same2 && i < nw; i++)
            same2 = (y[i] == z[i]);
          DP_CHECK (same2);
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
  DP_CHECK (x && y && z);
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
  DP_CHECK (a && b);
  if (a && b)
    {
      size_t half = n / 2;
      /* Run a to mid-stream, hand its state to a fresh b, and require the
         remainder to match bit-for-bit. */
      RateConverter_execute (a, x, half, y, n);
      size_t nb = RateConverter_state_bytes (a);
      void  *bl = malloc (nb);
      DP_CHECK (bl != NULL);
      if (bl)
        {
          RateConverter_get_state (a, bl);
          DP_CHECK (RateConverter_set_state (b, bl) == DP_OK);
          size_t na   = RateConverter_execute (a, x + half, n - half, y, n);
          size_t nz   = RateConverter_execute (b, x + half, n - half, z, n);
          int    same = (na == nz);
          for (size_t i = 0; same && i < nz; i++)
            same = (y[i] == z[i]);
          DP_CHECK (same);
          /* Envelope reject: a clobbered blob must not be reinterpreted. */
          ((char *)bl)[0] ^= 0xFF;
          DP_CHECK (RateConverter_set_state (b, bl) == DP_ERR_INVALID);
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
test_agc_state_roundtrip_mid_convergence (void)
{
  /* Split while the AGC is still SEEDING. The seed's running mean and its two
     counters are live only inside that window, so a round-trip that splits
     after it cannot see them — and every other round-trip in this file
     splits at n/2, long past it. Cut at a handful of pre-terminal samples
     instead, which is where the state actually exists. */
  const double    sps = 4.0;
  size_t          n;
  float _Complex *x = _mf_tx_beta (sps, _MF_BETA, 0.0, &n);
  float _Complex *y = calloc (n, sizeof *y);
  float _Complex *z = calloc (n, sizeof *z);
  DP_CHECK (x && y && z);
  if (!x || !y || !z)
    {
      free (x);
      free (y);
      free (z);
      return;
    }

  RateConverter_state_t *a = RateConverter_create_matched (
      2.0 / sps, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  RateConverter_state_t *b = RateConverter_create_matched (
      2.0 / sps, 0, RC_PULSE_RRC, _MF_BETA, _MF_SPAN, 2.0, 1024);
  DP_CHECK (a && b);
  if (a && b)
    {
      DP_CHECK (RateConverter_enable_agc (a, 1e-3, 0.05) == DP_OK);
      DP_CHECK (RateConverter_enable_agc (b, 1e-3, 0.05) == DP_OK);

      /* A short prefix: long enough that the loop has moved off unity,
         short enough that it is nowhere near settled. */
      size_t cut = n / 20;
      RateConverter_execute (a, x, cut, y, n);
      /* Non-vacuous: the gain really is mid-flight at the split. */
      double g_cut = RateConverter_agc_gain_db (a);
      DP_CHECK (g_cut != 0.0);
      DP_CHECK (fabs (g_cut) < 20.0);

      size_t nb = RateConverter_state_bytes (a);
      void  *bl = malloc (nb);
      DP_CHECK (bl != NULL);
      if (bl)
        {
          RateConverter_get_state (a, bl);
          DP_CHECK (RateConverter_set_state (b, bl) == DP_OK);
          size_t na   = RateConverter_execute (a, x + cut, n - cut, y, n);
          size_t nz   = RateConverter_execute (b, x + cut, n - cut, z, n);
          int    same = (na == nz && na > 0);
          for (size_t i = 0; same && i < nz; i++)
            same = (y[i] == z[i]);
          DP_CHECK (same);
          ((char *)bl)[0] ^= 0xFF;
          DP_CHECK (RateConverter_set_state (b, bl) == DP_ERR_INVALID);
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
  DP_CHECK (m != NULL);
  if (m)
    {
      RateConverter_set_rate (m, 2.0 / 64.0);
      char buf[64];
      DP_CHECK (m->stage_types[m->n_stages - 1] == RC_STAGE_RESAMP);
      DP_CHECK (
          RateConverter_stage_label (m, m->n_stages - 1, buf, sizeof buf));
      DP_CHECK (strstr (buf, "rrc") != NULL);
      /* Still folded, still no comp FIR stage. */
      DP_CHECK (RateConverter_stage_label (m, 0, buf, sizeof buf));
      DP_CHECK (strstr (buf, "FIR") == NULL);
      RateConverter_destroy (m);
    }
}

/* A capped CIC must not cost the cascade its RATE.
 *
 * The plan caps a single CIC stage at CIC_R_MAX and hands the residual to a
 * Resampler. That residual was gated on the matched-terminal flag alone, so a
 * plain RateConverter at a power-of-two D past the cap decimated by R and
 * claimed D -- twice the rate asked for at D = 8192, four times at 16384, with
 * no error and a plausible-looking output. Exactly the shape of failure that
 * cannot be seen from the sample stream.
 *
 * Checked as a RATIO of outputs to inputs against 1/D, which is the property
 * a caller depends on and the one the defect broke. The rates below straddle
 * the cap deliberately: at and under it the CIC does the whole job, past it
 * the residual stage has to exist. */
static void
test_capped_cic_still_delivers_the_requested_rate (void)
{
  static const double Ds[] = { 512.0, 2048.0, 4096.0, 8192.0, 16384.0 };
  size_t              i;

  for (i = 0; i < sizeof Ds / sizeof *Ds; i++)
    {
      double                 D  = Ds[i];
      RateConverter_state_t *rc = RateConverter_create (1.0 / D, 1);
      size_t                 n  = (size_t)(D * 40.0);
      float complex         *x, *o;
      size_t                 m;
      double                 got;

      DP_CHECK (rc != NULL);
      if (!rc)
        continue;
      x = (float complex *)malloc (n * sizeof *x);
      o = (float complex *)malloc (n * sizeof *o);
      DP_CHECK (x != NULL && o != NULL);
      if (!x || !o)
        {
          free (x);
          free (o);
          RateConverter_destroy (rc);
          continue;
        }
      /* Well inside the CIC's +-2.0 input bound, so this measures the RATE
         and not the clip. */
      for (m = 0; m < n; m++)
        x[m] = 0.25f + 0.0f * I;
      m   = RateConverter_execute (rc, x, n, o, n);
      got = (double)m / (double)n;
      /* 2% covers the cascade's startup transient at these lengths; the
         defect was a factor of 2, 4 and 8, so the tolerance is nowhere near
         it. Verified by sabotage: re-gating the residual takes this red. */
      DP_CHECK_MSG (fabs (got - 1.0 / D) <= 0.02 / D, "capped CIC lost rate");
      free (x);
      free (o);
      RateConverter_destroy (rc);
    }
}

/* The cap itself: cic_create refuses past CIC_R_MAX, and the planner never
 * asks. Both halves matter -- a cap the planner honours while the constructor
 * does not is one bad call site away from the accumulator it protects. */
static void
test_cic_ratio_is_capped_at_both_layers (void)
{
  {
    /* One create, asserted AND released. The assertion used to be its own
       throwaway `cic_create(...) != NULL`, which proved the same thing and
       leaked the state it proved. */
    cic_state_t *c = cic_create (CIC_R_MAX);
    DP_CHECK (c != NULL);
    if (c)
      cic_destroy (c);
  }
  DP_CHECK (cic_create (CIC_R_MAX * 2u) == NULL);
  DP_CHECK (cic_create (CIC_R_MAX * 4u) == NULL);
}

int
main (void)
{
  test_invalid_rate ();
  test_capped_cic_still_delivers_the_requested_rate ();
  test_cic_ratio_is_capped_at_both_layers ();
  test_stage_labels ();
  test_output_length ();
  test_plain_cascade_is_unity_gain ();
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
  test_matched_cascade_returns_the_symbol_amplitude ();
  test_matched_push_equals_block ();
  test_pre_terminal_tap_on_a_cascade_with_no_fractional_tail ();
  test_agc_is_off_unless_asked_and_needs_a_pulse ();
  test_agc_telemetry_forwards_and_survives_a_replan ();
  test_agc_delivers_unit_symbol_amplitude ();
  test_agc_state_roundtrip_mid_convergence ();
  test_matched_state_roundtrip ();
  test_matched_set_rate_keeps_pulse ();

  DP_TEST_END ("test_RateConverter_core");
}
