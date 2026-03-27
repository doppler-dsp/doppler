/**
 * @file test_resamp.c
 * @brief Unit tests for dp_resamp_cf32 polyphase resampler.
 *
 * Tests lifecycle, properties, and spectral quality for both
 * interpolation and decimation paths.  Spectral tests mirror
 * the Python reference (test_resampler.py).
 *
 * Self-contained, no external framework.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/resamp.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Test harness
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

/* =========================================================================
 * Kaiser prototype (pure C, matches Python kaiser_prototype)
 * ========================================================================= */

static double
kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

static int
kaiser_taps (double atten, double pb, double sb)
{
  return (int)(1.0 + (atten - 8.0) / 2.285 / (2.0 * M_PI * (sb - pb)));
}

/* I0 — Bessel function of the first kind, order 0. */
static double
bessel_i0 (double x)
{
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 30; k++)
    {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum += term;
      if (term < 1e-20 * sum)
        break;
    }
  return sum;
}

static double
kaiser_win (int n, int N, double beta)
{
  double mid = (N - 1) / 2.0;
  double arg = 2.0 * (n - mid) / (N - 1);
  return bessel_i0 (beta * sqrt (1.0 - arg * arg)) / bessel_i0 (beta);
}

/**
 * Build the polyphase bank.  Caller must free() the returned
 * pointer.  Sets *out_phases and *out_taps.
 */
static float *
build_bank (size_t *out_phases, size_t *out_taps)
{
  double atten = 60.0;
  double pb = 0.4;
  double sb = 0.6;
  double img_db = 80.0;
  double db_bit = 6.02;

  int log2L = (int)ceil ((20.0 * log10 (pb) + img_db) / db_bit);
  size_t L = (size_t)1 << log2L;

  int halflen = kaiser_taps (atten, pb / L, sb / L) / 2;
  int htaps = 2 * halflen + 1;
  size_t taps_per_phase = (size_t)(htaps / (int)L) + 1;
  size_t proto_len = L * taps_per_phase;

  double beta = kaiser_beta (atten);
  double wc = 2.0 * M_PI * (pb / L + (sb - pb) / (2.0 * L));

  /* Build prototype */
  double *g = calloc (proto_len, sizeof (double));
  for (int i = 0; i < htaps; i++)
    {
      double m = i - halflen;
      double w = kaiser_win (i, htaps, beta);
      double sinc_val = (m == 0.0) ? 1.0 : sin (wc * m) / (wc * m);
      g[i] = w * wc / M_PI * sinc_val * (double)L;
    }

  /* Reshape into [taps_per_phase][L] then transpose → [L][tpp] */
  float *bank = malloc (L * taps_per_phase * sizeof (float));
  for (size_t p = 0; p < L; p++)
    for (size_t t = 0; t < taps_per_phase; t++)
      bank[p * taps_per_phase + t] = (float)g[t * L + p];

  free (g);

  *out_phases = L;
  *out_taps = taps_per_phase;
  return bank;
}

/* =========================================================================
 * Spectral helpers
 * ========================================================================= */

/* In-place Cooley-Tukey radix-2 FFT (n must be power of 2). */
static void
fft_inplace (double *re, double *im, size_t n)
{
  /* Bit-reversal permutation */
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

  /* Butterfly stages */
  for (size_t len = 2; len <= n; len <<= 1)
    {
      double ang = -2.0 * M_PI / (double)len;
      double wR = cos (ang), wI = sin (ang);
      for (size_t i = 0; i < n; i += len)
        {
          double curR = 1.0, curI = 0.0;
          for (size_t j = 0; j < len / 2; j++)
            {
              size_t u = i + j;
              size_t v = i + j + len / 2;
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

/**
 * Find the peak dB value of the FFT within ±tol of the target
 * normalised frequency.  Returns -300 if nothing found.
 */
static double
peak_near (const dp_cf32_t *sig, size_t n, double target_freq, double tol)
{
  /* 4× zero-padded Blackman-Harris */
  size_t nfft = 4 * n;
  /* Round up to power of 2 */
  size_t nfft2 = 1;
  while (nfft2 < nfft)
    nfft2 <<= 1;
  nfft = nfft2;

  double *re = calloc (nfft, sizeof (double));
  double *im = calloc (nfft, sizeof (double));

  /* Blackman-Harris 4-term */
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

/**
 * Find the peak dB of the largest artifact (anything outside
 * ±tol of each signal frequency), relative to sig_peak.
 */
static double
artifact_dbc (const dp_cf32_t *sig, size_t n, const double *sig_freqs,
              size_t nfreqs, double tol)
{
  size_t nfft = 4 * n;
  size_t nfft2 = 1;
  while (nfft2 < nfft)
    nfft2 <<= 1;
  nfft = nfft2;

  double *re = calloc (nfft, sizeof (double));
  double *im = calloc (nfft, sizeof (double));

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

  double sig_peak = -300.0;
  double artifact_peak = -300.0;

  for (size_t i = 0; i < nfft; i++)
    {
      double freq = (double)i / (double)nfft;
      if (freq > 0.5)
        freq -= 1.0;
      double mag = sqrt (re[i] * re[i] + im[i] * im[i]);
      double db = 20.0 * log10 (mag / ((double)n * cg) + 1e-300);

      /* Is this bin near a signal tone? */
      int near_sig = 0;
      for (size_t f = 0; f < nfreqs; f++)
        if (fabs (freq - sig_freqs[f]) < tol)
          near_sig = 1;

      if (near_sig)
        {
          if (db > sig_peak)
            sig_peak = db;
        }
      else
        {
          if (db > artifact_peak)
            artifact_peak = db;
        }
    }

  free (re);
  free (im);
  return artifact_peak - sig_peak;
}

/* =========================================================================
 * Lifecycle tests
 * ========================================================================= */

static void
test_create_destroy (void)
{
  size_t L, N;
  float *bank = build_bank (&L, &N);

  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, 2.0);
  CHECK (r != NULL, "create returns non-NULL");

  dp_resamp_cf32_destroy (r);
  PASS ("destroy does not crash");

  dp_resamp_cf32_destroy (NULL);
  PASS ("destroy(NULL) is safe");

  free (bank);
}

static void
test_properties (void)
{
  size_t L, N;
  float *bank = build_bank (&L, &N);

  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, 2.5);

  CHECK (fabs (dp_resamp_cf32_rate (r) - 2.5) < 1e-12, "rate() returns 2.5");
  CHECK (dp_resamp_cf32_num_phases (r) == L, "num_phases() matches");
  CHECK (dp_resamp_cf32_num_taps (r) == N, "num_taps() matches");

  dp_resamp_cf32_destroy (r);
  free (bank);
}

static void
test_null_args (void)
{
  CHECK (dp_resamp_cf32_create (0, 4, NULL, 1.0) == NULL,
         "create rejects num_phases=0");
  CHECK (dp_resamp_cf32_create (4, 0, NULL, 1.0) == NULL,
         "create rejects num_taps=0");
}

/* =========================================================================
 * Spectral quality tests
 * ========================================================================= */

#define N_IN 8192

static void
test_interp_spectral (void)
{
  double r_val = 2.0333;
  double freqs[2] = { 0.1, 0.4 };

  size_t L, N;
  float *bank = build_bank (&L, &N);
  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, r_val);

  /* Generate two-tone input */
  dp_cf32_t *x = malloc (N_IN * sizeof (dp_cf32_t));
  for (size_t i = 0; i < N_IN; i++)
    {
      double theta0 = 2.0 * M_PI * freqs[0] * (double)i;
      double theta1 = 2.0 * M_PI * freqs[1] * (double)i;
      x[i].i = (float)(cos (theta0) + cos (theta1));
      x[i].q = (float)(sin (theta0) + sin (theta1));
    }

  size_t max_out = (size_t)(N_IN * r_val) + 4;
  dp_cf32_t *y = calloc (max_out, sizeof (dp_cf32_t));
  size_t n_out = dp_resamp_cf32_execute (r, x, N_IN, y, max_out);

  CHECK (n_out > 0, "interp: produced output");

  /* Check each tone */
  for (int t = 0; t < 2; t++)
    {
      double f_out = freqs[t] / r_val;
      /* Wrap to (-0.5, 0.5] */
      while (f_out > 0.5)
        f_out -= 1.0;
      while (f_out <= -0.5)
        f_out += 1.0;

      double amp = peak_near (y, n_out, f_out, 0.02);
      char msg[128];
      snprintf (msg, sizeof msg,
                "interp tone %.1f: amp %.3f dB "
                "(expect ±3 dB)",
                freqs[t], amp);
      CHECK (fabs (amp) < 3.0, msg);
    }

  /* Artifact check */
  double out_freqs[2] = { freqs[0] / r_val, freqs[1] / r_val };
  double art = artifact_dbc (y, n_out, out_freqs, 2, 0.02);
  char msg[128];
  snprintf (msg, sizeof msg, "interp artifacts: %.1f dBc (expect < -60)", art);
  CHECK (art < -60.0, msg);

  free (x);
  free (y);
  dp_resamp_cf32_destroy (r);
  free (bank);
}

static void
test_decim_spectral (void)
{
  double r_val = 0.50333;
  double f_in[2] = { 0.4 * r_val, 0.6 * r_val };

  size_t L, N;
  float *bank = build_bank (&L, &N);
  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, r_val);

  /* Generate two-tone input */
  dp_cf32_t *x = malloc (N_IN * sizeof (dp_cf32_t));
  for (size_t i = 0; i < N_IN; i++)
    {
      double theta0 = 2.0 * M_PI * f_in[0] * (double)i;
      double theta1 = 2.0 * M_PI * f_in[1] * (double)i;
      x[i].i = (float)(cos (theta0) + cos (theta1));
      x[i].q = (float)(sin (theta0) + sin (theta1));
    }

  size_t max_out = (size_t)(N_IN * r_val) + 4;
  dp_cf32_t *y = calloc (max_out, sizeof (dp_cf32_t));
  size_t n_out = dp_resamp_cf32_execute (r, x, N_IN, y, max_out);

  CHECK (n_out > 0, "decim: produced output");

  /* Passband tone (tone 1 at 0.4·Fout) */
  double f_out_tone1 = f_in[0] / r_val; /* should be 0.4 */
  while (f_out_tone1 > 0.5)
    f_out_tone1 -= 1.0;
  while (f_out_tone1 <= -0.5)
    f_out_tone1 += 1.0;

  double amp = peak_near (y, n_out, f_out_tone1, 0.02);
  {
    char msg[128];
    snprintf (msg, sizeof msg, "decim passband: amp %.3f dB (expect ±3 dB)",
              amp);
    CHECK (fabs (amp) < 3.0, msg);
  }

  /* Artifact check — tone 2 should be rejected */
  double out_freqs[1] = { f_out_tone1 };
  double art = artifact_dbc (y, n_out, out_freqs, 1, 0.02);
  {
    char msg[128];
    snprintf (msg, sizeof msg, "decim artifacts: %.1f dBc (expect < -60)",
              art);
    CHECK (art < -60.0, msg);
  }

  free (x);
  free (y);
  dp_resamp_cf32_destroy (r);
  free (bank);
}

/* =========================================================================
 * Reset test — verify state is cleared between uses
 * ========================================================================= */

static void
test_reset (void)
{
  size_t L, N;
  float *bank = build_bank (&L, &N);

  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, 2.0);

  /* Run some samples through */
  dp_cf32_t x[64];
  for (int i = 0; i < 64; i++)
    {
      x[i].i = (float)cos (2.0 * M_PI * 0.1 * i);
      x[i].q = (float)sin (2.0 * M_PI * 0.1 * i);
    }
  dp_cf32_t y[200];
  dp_resamp_cf32_execute (r, x, 64, y, 200);

  /* Reset and run the same input — should get same output */
  dp_resamp_cf32_reset (r);
  dp_cf32_t y2[200];
  size_t n1 = dp_resamp_cf32_execute (r, x, 64, y2, 200);

  dp_resamp_cf32_reset (r);
  dp_cf32_t y3[200];
  size_t n2 = dp_resamp_cf32_execute (r, x, 64, y3, 200);

  CHECK (n1 == n2, "reset: same output count");
  int match = 1;
  for (size_t i = 0; i < n1 && i < n2; i++)
    if (y2[i].i != y3[i].i || y2[i].q != y3[i].q)
      match = 0;
  CHECK (match, "reset: identical output after reset");

  dp_resamp_cf32_destroy (r);
  free (bank);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int
main (void)
{
  printf ("=== dp_resamp_cf32 unit tests ===\n\n");

  printf ("--- lifecycle ---\n");
  test_create_destroy ();
  test_properties ();
  test_null_args ();

  printf ("\n--- reset ---\n");
  test_reset ();

  printf ("\n--- interpolation spectral quality ---\n");
  test_interp_spectral ();

  printf ("\n--- decimation spectral quality ---\n");
  test_decim_spectral ();

  printf ("\n=== %d passed, %d failed ===\n", passed, failed);
  return failed ? 1 : 0;
}
