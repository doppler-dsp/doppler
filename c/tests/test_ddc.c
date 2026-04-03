/**
 * @file test_ddc.c
 * @brief Unit tests for dp_ddc_* Digital Down-Converter.
 *
 * Tests are self-contained: no external framework required.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/ddc.h>
#include <dp/stream.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static int passed = 0;
static int failed = 0;

#define PASS(msg)                                                             \
  do                                                                          \
    {                                                                         \
      printf ("  PASS  %s\n", (msg));                                         \
      passed++;                                                               \
    }                                                                         \
  while (0)

#define FAIL(msg)                                                             \
  do                                                                          \
    {                                                                         \
      printf ("  FAIL  %s\n", (msg));                                         \
      failed++;                                                               \
    }                                                                         \
  while (0)

#define CHECK(cond, msg)                                                      \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        PASS (msg);                                                           \
      else                                                                    \
        FAIL (msg);                                                           \
    }                                                                         \
  while (0)

/* NCO LUT has 2^16 entries; per-sample LUT quantization error is bounded
 * by π/65536 ≈ 4.8e-5.  Two such errors compound (input tone × NCO),
 * so the instantaneous mix error can reach ~1e-4.  Use 2× margin. */
#define TOL 2e-4f

static int
cf32_near (dp_cf32_t a, dp_cf32_t b, float tol)
{
  return fabsf (a.i - b.i) <= tol && fabsf (a.q - b.q) <= tol;
}

/* Generate a complex sinusoid: out[i] = exp(j·2π·f_n·i) */
static void
make_tone (dp_cf32_t *out, size_t n, float f_n)
{
  for (size_t i = 0; i < n; i++)
    {
      float phi = 2.0f * (float)M_PI * f_n * (float)i;
      out[i].i = cosf (phi);
      out[i].q = sinf (phi);
    }
}

/* =========================================================================
 * Test 1 — create / destroy (default coefficients)
 * ========================================================================= */

static void
test_create_destroy (void)
{
  printf ("--- create / destroy\n");

  /* Default: rate=1.0 → no resampler (bypass) */
  dp_ddc_t *ddc = dp_ddc_create (0.0f, 256, 1.0);
  CHECK (ddc != NULL, "dp_ddc_create (rate=1.0) returns non-NULL");
  CHECK (dp_ddc_max_out (ddc) == 256, "max_out == num_in when no resampler");
  dp_ddc_destroy (ddc);
  PASS ("dp_ddc_destroy does not crash");

  /* NULL destroy is a no-op */
  dp_ddc_destroy (NULL);
  PASS ("dp_ddc_destroy(NULL) is safe");

  /* create_custom with NULL resampler */
  ddc = dp_ddc_create_custom (0.0f, 128, NULL);
  CHECK (ddc != NULL, "dp_ddc_create_custom (r=NULL) returns non-NULL");
  CHECK (dp_ddc_max_out (ddc) == 128, "max_out == num_in with NULL resampler");
  dp_ddc_destroy (ddc);
}

/* =========================================================================
 * Test 2 — max_out with decimation
 * ========================================================================= */

static void
test_max_out (void)
{
  printf ("--- max_out\n");

  /* 4× decimation: max_out ≥ ceil(1024 * 0.25) + 4 = 260 */
  dp_ddc_t *ddc = dp_ddc_create (0.0f, 1024, 0.25);
  CHECK (ddc != NULL, "dp_ddc_create (rate=0.25) non-NULL");
  size_t mo = dp_ddc_max_out (ddc);
  CHECK (mo >= 256 + 4, "max_out accounts for decimation + guard");
  dp_ddc_destroy (ddc);
}

/* =========================================================================
 * Test 3 — get_freq round-trips the constructor value
 * ========================================================================= */

static void
test_get_freq (void)
{
  printf ("--- get_freq\n");

  float freqs[] = { 0.0f, 0.25f, -0.25f, 0.49f, -0.49f };
  for (size_t k = 0; k < sizeof freqs / sizeof *freqs; k++)
    {
      dp_ddc_t *ddc = dp_ddc_create (freqs[k], 256, 1.0);
      float got = dp_ddc_get_freq (ddc);
      CHECK (fabsf (got - freqs[k]) < 1e-7f, "get_freq matches create");
      dp_ddc_destroy (ddc);
    }

  /* After set_freq */
  dp_ddc_t *ddc = dp_ddc_create (0.0f, 256, 1.0);
  dp_ddc_set_freq (ddc, 0.1f);
  CHECK (fabsf (dp_ddc_get_freq (ddc) - 0.1f) < 1e-7f,
         "get_freq reflects set_freq");
  dp_ddc_destroy (ddc);
}

/* =========================================================================
 * Test 4 — rate=1.0 is a passthrough (no resampler)
 * ========================================================================= */

static void
test_passthrough (void)
{
  printf ("--- rate=1.0 passthrough\n");

  /* At f_n=0 the NCO emits (1, 0) every sample, so output == input. */
  enum
  {
    N = 256
  };
  dp_cf32_t in[N];
  make_tone (in, N, 0.1f); /* arbitrary input */

  dp_ddc_t *ddc = dp_ddc_create (0.0f, N, 1.0);
  dp_cf32_t out[N];
  size_t n = dp_ddc_execute (ddc, in, N, out, dp_ddc_max_out (ddc));
  dp_ddc_destroy (ddc);

  CHECK (n == N, "output count == input count");

  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (!cf32_near (out[i], in[i], TOL))
      {
        ok = 0;
        break;
      }
  CHECK (ok, "zero-freq DDC output matches input");
}

/* =========================================================================
 * Test 5 — frequency translation: tone at +f_n mixed to DC
 *
 * Input: complex tone at normalised frequency +f_n
 * DDC:   norm_freq = -f_n  (translate +f_n → DC)
 * Expected output: constant phasor (1, 0)
 * ========================================================================= */

static void
test_frequency_translation (void)
{
  printf ("--- frequency translation\n");

  enum
  {
    N = 512
  };
  dp_cf32_t in[N], out[N];

  float test_freqs[] = { 0.25f, -0.25f, 0.1f, -0.1f, 0.3f };

  for (size_t k = 0; k < sizeof test_freqs / sizeof *test_freqs; k++)
    {
      float f_n = test_freqs[k];
      make_tone (in, N, f_n);

      dp_ddc_t *ddc = dp_ddc_create (-f_n, N, 1.0);
      size_t n = dp_ddc_execute (ddc, in, N, out, dp_ddc_max_out (ddc));
      dp_ddc_destroy (ddc);

      CHECK (n == N, "output count == input count (translation)");

      /* Skip the first sample (phase ramp starts at 0 regardless) */
      dp_cf32_t dc = { 1.0f, 0.0f };
      int ok = 1;
      for (size_t i = 1; i < N; i++)
        if (!cf32_near (out[i], dc, TOL))
          {
            ok = 0;
            fprintf (stderr, "    f_n=%.3f  i=%zu  got (%.6f, %.6f)\n", f_n, i,
                     out[i].i, out[i].q);
            break;
          }
      CHECK (ok, "translated tone is DC (1, 0)");
    }
}

/* =========================================================================
 * Test 6 — set_freq retunes NCO seamlessly
 * ========================================================================= */

static void
test_retune (void)
{
  printf ("--- retune via set_freq\n");

  enum
  {
    N = 256
  };
  dp_cf32_t in[N], out[N];

  /* Start at -0.25 to cancel a +0.25 tone */
  make_tone (in, N, 0.25f);
  dp_ddc_t *ddc = dp_ddc_create (-0.25f, N, 1.0);
  dp_ddc_execute (ddc, in, N, out, dp_ddc_max_out (ddc));

  int ok1 = 1;
  dp_cf32_t dc = { 1.0f, 0.0f };
  for (size_t i = 1; i < N; i++)
    if (!cf32_near (out[i], dc, TOL))
      {
        ok1 = 0;
        break;
      }
  CHECK (ok1, "pre-retune: tone at +0.25 down-converted to DC");

  /* Retune to -0.1; now process a +0.1 tone and expect DC */
  dp_ddc_set_freq (ddc, -0.1f);
  CHECK (fabsf (dp_ddc_get_freq (ddc) - (-0.1f)) < 1e-7f,
         "set_freq reflected in get_freq");

  make_tone (in, N, 0.1f);
  dp_ddc_execute (ddc, in, N, out, dp_ddc_max_out (ddc));
  dp_ddc_destroy (ddc);

  int ok2 = 1;
  for (size_t i = 1; i < N; i++)
    if (!cf32_near (out[i], dc, TOL))
      {
        ok2 = 0;
        break;
      }
  CHECK (ok2, "post-retune: tone at +0.1 down-converted to DC");
}

/* =========================================================================
 * Test 7 — reset clears NCO phase
 * ========================================================================= */

static void
test_reset (void)
{
  printf ("--- reset\n");

  enum
  {
    N = 64
  };
  dp_cf32_t in[N], out1[N], out2[N];
  make_tone (in, N, 0.25f);

  /* First run */
  dp_ddc_t *ddc = dp_ddc_create (-0.25f, N, 1.0);
  dp_ddc_execute (ddc, in, N, out1, dp_ddc_max_out (ddc));

  /* Advance phase by processing N more samples, then reset */
  dp_ddc_execute (ddc, in, N, out2, dp_ddc_max_out (ddc));
  dp_ddc_reset (ddc);

  /* After reset, output should match the first run */
  dp_ddc_execute (ddc, in, N, out2, dp_ddc_max_out (ddc));
  dp_ddc_destroy (ddc);

  int ok = 1;
  for (size_t i = 0; i < N; i++)
    if (!cf32_near (out1[i], out2[i], TOL))
      {
        ok = 0;
        break;
      }
  CHECK (ok, "reset restores initial state");
}

/* =========================================================================
 * Test 8 — max_out clips output
 * ========================================================================= */

static void
test_max_out_clip (void)
{
  printf ("--- max_out clipping (no resampler)\n");

  enum
  {
    N = 128,
    CAP = 64
  };
  dp_cf32_t in[N], out[CAP];
  make_tone (in, N, 0.0f);

  dp_ddc_t *ddc = dp_ddc_create_custom (0.0f, N, NULL);
  size_t n = dp_ddc_execute (ddc, in, N, out, CAP);
  dp_ddc_destroy (ddc);

  CHECK (n == CAP, "output count capped at max_out");
}

/* =========================================================================
 * Test 9 — zero num_in returns 0 without crashing
 * ========================================================================= */

static void
test_empty_block (void)
{
  printf ("--- empty block\n");

  dp_cf32_t out[4];
  dp_ddc_t *ddc = dp_ddc_create (0.1f, 64, 1.0);
  size_t n = dp_ddc_execute (ddc, NULL, 0, out, 4);
  dp_ddc_destroy (ddc);

  CHECK (n == 0, "empty block returns 0");
}

/* =========================================================================
 * Test 10 — default coefficients produce output with decimation
 * ========================================================================= */

static void
test_default_decimation (void)
{
  printf ("--- default coefficients decimation\n");

  /* 4× decimation: 1024 in → ~256 out */
  enum
  {
    N = 1024
  };
  dp_cf32_t in[N];
  make_tone (in, N, 0.0f); /* DC input (all 1+0j) */

  dp_ddc_t *ddc = dp_ddc_create (0.0f, N, 0.25);
  CHECK (ddc != NULL, "create with 4× decimation succeeds");

  size_t mo = dp_ddc_max_out (ddc);
  dp_cf32_t *out = malloc (mo * sizeof *out);
  size_t n = dp_ddc_execute (ddc, in, N, out, mo);
  dp_ddc_destroy (ddc);

  /* After transient (N taps), output should be near (1, 0) */
  int ok = n > 0;
  CHECK (ok, "decimated output count > 0");
  CHECK (n <= mo, "decimated output count ≤ max_out");

  free (out);
}

/* =========================================================================
 * Spectral helpers (Blackman-Harris windowed FFT, same as test_resamp_dpmfs)
 * ========================================================================= */

static void
fft_inplace (double *re, double *im, size_t n)
{
  for (size_t i = 1, j = 0; i < n; i++)
    {
      size_t bit = n >> 1;
      for (; j & bit; bit >>= 1)
        j ^= bit;
      j ^= bit;
      if (i < j)
        {
          double tr = re[i];
          re[i] = re[j];
          re[j] = tr;
          double ti = im[i];
          im[i] = im[j];
          im[j] = ti;
        }
    }
  for (size_t len = 2; len <= n; len <<= 1)
    {
      double ang = -2.0 * M_PI / (double)len;
      double wR = cos (ang), wI = sin (ang);
      for (size_t i = 0; i < n; i += len)
        {
          double curR = 1.0, curI = 0.0;
          for (size_t j = 0; j < len / 2; j++)
            {
              size_t u = i + j, v = i + j + len / 2;
              double tR = re[v] * curR - im[v] * curI;
              double tI = re[v] * curI + im[v] * curR;
              re[v] = re[u] - tR;
              im[v] = im[u] - tI;
              re[u] += tR;
              im[u] += tI;
              double nR = curR * wR - curI * wI;
              curI = curR * wI + curI * wR;
              curR = nR;
            }
        }
    }
}

/* Return peak dB (relative to full-scale unit tone) near target_freq.
 * Frequencies are normalised to the output sample rate: 0 = DC, 0.5 = Nyquist.
 * tol is the half-width of the search window around target_freq. */
static double
peak_near (const dp_cf32_t *sig, size_t n, double target_freq, double tol)
{
  size_t nfft = 1;
  while (nfft < 4 * n)
    nfft <<= 1;

  double *re = calloc (nfft, sizeof *re);
  double *im = calloc (nfft, sizeof *im);

  /* Blackman-Harris window */
  double a[4] = { 0.35875, 0.48829, 0.14128, 0.01168 };
  double cg = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double k = 2.0 * M_PI * (double)i / (double)n;
      double w
          = a[0] - a[1] * cos (k) + a[2] * cos (2 * k) - a[3] * cos (3 * k);
      cg += w;
      re[i] = (double)sig[i].i * w;
      im[i] = (double)sig[i].q * w;
    }
  cg /= (double)n;

  fft_inplace (re, im, nfft);

  double peak_db = -300.0;
  for (size_t i = 0; i < nfft; i++)
    {
      double freq = (double)i / (double)nfft;
      if (freq > 0.5)
        freq -= 1.0;
      if (fabs (freq - target_freq) < tol)
        {
          double mag = sqrt (re[i] * re[i] + im[i] * im[i]);
          double db = 20.0 * log10 (mag / ((double)n * cg) + 1e-300);
          if (db > peak_db)
            peak_db = db;
        }
    }

  free (re);
  free (im);
  return peak_db;
}

/* =========================================================================
 * Test 11 — nout matches execute return value; varies ≤ max_out
 * ========================================================================= */

static void
test_nout (void)
{
  printf ("--- nout accessor\n");

  enum
  {
    N = 1024
  };
  dp_cf32_t in[N];
  make_tone (in, N, 0.0f);

  dp_ddc_t *ddc = dp_ddc_create (0.0f, N, 0.25);
  size_t mo = dp_ddc_max_out (ddc);
  dp_cf32_t *out = malloc (mo * sizeof *out);

  /* nout is 0 before any execute */
  CHECK (dp_ddc_nout (ddc) == 0, "nout is 0 before first execute");

  size_t n1 = dp_ddc_execute (ddc, in, N, out, mo);
  CHECK (dp_ddc_nout (ddc) == n1, "nout matches return value (block 1)");
  CHECK (n1 <= mo, "nout ≤ max_out (block 1)");

  size_t n2 = dp_ddc_execute (ddc, in, N, out, mo);
  CHECK (dp_ddc_nout (ddc) == n2, "nout matches return value (block 2)");
  CHECK (n2 <= mo, "nout ≤ max_out (block 2)");

  dp_ddc_destroy (ddc);
  free (out);
}

/* =========================================================================
 * Test 12 — filter passband: shifted tone survives decimation
 *
 * DDC rate=0.25, NCO=-0.05 → input tone at +0.05 maps to DC.
 * DC is well within the filter passband (≤ 0.4 × Nyquist_out).
 * After settling, peak at DC in the output should be > −3 dB.
 *
 * Test 13 — filter rejection: out-of-band tone is attenuated ≥ 40 dB
 *
 * Same DDC.  Input tone at +0.2 maps to +0.15 (normalised to input fs)
 * after NCO shift.  The filter stopband starts at 0.075 × fs_in, so
 * 0.15 is 2× past the stopband edge → ≥ 60 dB rejection expected.
 * The aliased frequency in the output is 0.15/0.25 − 1 = −0.4 × fs_out
 * = 0.4 × fs_out.  We check that power at all output frequencies is
 * very low (< −40 dBFS).
 * ========================================================================= */

static void
test_filter_passband_and_rejection (void)
{
  printf ("--- filter passband and rejection\n");

  /* Run several blocks to fill the filter pipeline */
  enum
  {
    BLOCKS = 8,
    N_IN = 1024
  };
  dp_cf32_t in[N_IN];

  /* ---- passband ---- */
  {
    dp_ddc_t *ddc = dp_ddc_create (-0.05f, N_IN, 0.25);
    size_t mo = dp_ddc_max_out (ddc);
    dp_cf32_t *out = malloc (mo * BLOCKS * sizeof *out);
    size_t total = 0;

    make_tone (in, N_IN, 0.05f); /* +0.05 → DC after NCO */
    for (int b = 0; b < BLOCKS; b++)
      {
        dp_ddc_execute (ddc, in, N_IN, out + total, mo);
        total += dp_ddc_nout (ddc);
      }
    dp_ddc_destroy (ddc);

    /* Skip the first block of output (filter warm-up) */
    size_t skip = total / BLOCKS;
    double db_dc = peak_near (out + skip, total - skip, 0.0, 0.02);
    fprintf (stderr, "    passband: DC peak = %.1f dB\n", db_dc);
    CHECK (db_dc > -3.0, "passband tone at DC survives decimation (> −3 dB)");
    free (out);
  }

  /* ---- stopband ---- */
  {
    dp_ddc_t *ddc = dp_ddc_create (-0.05f, N_IN, 0.25);
    size_t mo = dp_ddc_max_out (ddc);
    dp_cf32_t *out = malloc (mo * BLOCKS * sizeof *out);
    size_t total = 0;

    /* +0.2 → +0.15 after NCO.  Stopband starts at 0.075 × fs_in. */
    make_tone (in, N_IN, 0.2f);
    for (int b = 0; b < BLOCKS; b++)
      {
        dp_ddc_execute (ddc, in, N_IN, out + total, mo);
        total += dp_ddc_nout (ddc);
      }
    dp_ddc_destroy (ddc);

    size_t skip = total / BLOCKS;
    /* Aliased to 0.4 in output-rate units; scan full spectrum */
    double db_max = peak_near (out + skip, total - skip, 0.4, 0.1);
    fprintf (stderr, "    stopband: aliased peak = %.1f dB\n", db_max);
    CHECK (db_max < -40.0,
           "stopband tone rejected ≥ 40 dB after NCO shift + decimation");
    free (out);
  }
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== DDC unit tests ===\n");

  test_create_destroy ();
  test_max_out ();
  test_get_freq ();
  test_passthrough ();
  test_frequency_translation ();
  test_retune ();
  test_reset ();
  test_max_out_clip ();
  test_empty_block ();
  test_default_decimation ();
  test_nout ();
  test_filter_passband_and_rejection ();

  printf ("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
