/**
 * @file test_resamp_dpmfs.c
 * @brief Unit tests for dp_resamp_dpmfs polyphase resampler.
 *
 * Tests lifecycle, properties, and spectral quality for both
 * interpolation and decimation paths.  Coefficients are generated
 * from the Python doppler.polyphase.fit_dpmfs tool and embedded
 * inline so the test is self-contained.
 *
 * Self-contained, no external framework.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/resamp_dpmfs.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Embedded DPMFS coefficients (M=3, N=19) from:
 *   kaiser_prototype(60.0, 0.4, 0.6, 80.0) → fit_dpmfs(M=3)
 * ========================================================================= */

/* DPMFS coefficients -- M=3, N=19 */
/* passband=0.4, stopband=0.6, atten=60.0 dB */
/* fit residual rms=7.99e-05 */
#define DPMFS_M 3
#define DPMFS_N 19

static const float dpmfs_c0_m0[19]
    = { -1.16934330e-04f, 4.73825610e-04f,  -1.19166356e-03f, 2.45564547e-03f,
        -4.52230871e-03f, 7.80567247e-03f,  -1.31601924e-02f, 2.30139550e-02f,
        -4.93952595e-02f, 9.94322419e-01f,  5.66108637e-02f,  -2.52397228e-02f,
        1.42384656e-02f,  -8.43974855e-03f, 4.92028147e-03f,  -2.70472351e-03f,
        1.33995316e-03f,  -5.53859456e-04f, 8.66927803e-05f };
static const float dpmfs_c0_m1[19]
    = { 8.80004896e-04f,  -3.92260263e-03f, 1.01457266e-02f,  -2.12026592e-02f,
        3.93599384e-02f,  -6.82006106e-02f, 1.14903398e-01f,  -1.99056119e-01f,
        4.16760147e-01f,  1.05088279e-01f,  -5.50753713e-01f, 2.40929484e-01f,
        -1.35243163e-01f, 8.01716074e-02f,  -4.68757749e-02f, 2.59068869e-02f,
        -1.29461270e-02f, 5.43356221e-03f,  -7.75158580e-04f };
static const float dpmfs_c0_m2[19]
    = { 1.69086654e-03f, -3.81081994e-03f, 7.18046678e-03f, -1.22501506e-02f,
        1.98423490e-02f, -3.19187157e-02f, 5.43430112e-02f, -1.10441282e-01f,
        3.21942121e-01f, -4.78356242e-01f, 2.87550747e-01f, -7.96665773e-02f,
        3.81802619e-02f, -2.25741938e-02f, 1.43792443e-02f, -9.17060301e-03f,
        5.57000516e-03f, -3.06685385e-03f, 1.64501579e-03f };
static const float dpmfs_c0_m3[19]
    = { -9.08786838e-04f, 2.71153264e-03f,  -6.01121085e-03f, 1.14890188e-02f,
        -2.01407541e-02f, 3.37455571e-02f,  -5.63596413e-02f, 9.94417891e-02f,
        -1.28961951e-01f, 7.82487020e-02f,  4.00897115e-03f,  -3.03890835e-02f,
        2.10647210e-02f,  -1.26649253e-02f, 6.83699781e-03f,  -3.15257278e-03f,
        1.05630129e-03f,  -5.08069206e-05f, -9.86029860e-04f };

static const float dpmfs_c1_m0[19]
    = { 1.53192726e-03f,  -4.51925257e-03f, 1.00700520e-02f,  -1.94183458e-02f,
        3.43948975e-02f,  -5.83376139e-02f, 9.93433967e-02f,  -1.86422527e-01f,
        5.59528589e-01f,  7.00086713e-01f,  -2.03125641e-01f, 1.05938062e-01f,
        -6.19319230e-02f, 3.66015099e-02f,  -2.08103228e-02f, 1.09243952e-02f,
        -5.00694150e-03f, 1.77628070e-03f,  0.00000000e+00f };
static const float dpmfs_c1_m1[19]
    = { 1.59764744e-03f,  -3.72488005e-03f, 7.29278848e-03f, -1.29298903e-02f,
        2.17107106e-02f,  -3.60014960e-02f, 6.26852065e-02f, -1.30676478e-01f,
        6.75922215e-01f,  -6.07608557e-01f, 2.22359207e-02f, 1.52034941e-03f,
        -2.55652890e-03f, 1.15510228e-03f,  4.18133932e-05f, -6.68466848e-04f,
        8.17867694e-04f,  -6.76259166e-04f, 0.00000000e+00f };
static const float dpmfs_c1_m2[19]
    = { -2.04953435e-03f, 6.53886562e-03f,  -1.49536403e-02f, 2.91341916e-02f,
        -5.16984127e-02f, 8.70598853e-02f,  -1.44155428e-01f, 2.35224113e-01f,
        -1.27136454e-01f, -1.84056893e-01f, 2.58344233e-01f,  -1.47669032e-01f,
        8.82083699e-02f,  -5.22393323e-02f, 2.94414070e-02f,  -1.51442783e-02f,
        6.66055363e-03f,  -2.12617568e-03f, 0.00000000e+00f };
static const float dpmfs_c1_m3[19]
    = { -6.07282971e-04f, 5.26328688e-04f,  1.07739379e-05f,  -1.23402057e-03f,
        3.26614734e-03f,  -5.67266904e-03f, 4.88680368e-03f,  3.26315686e-02f,
        -1.13870047e-01f, 1.47821829e-01f,  -1.02285333e-01f, 5.41415736e-02f,
        -3.19643356e-02f, 1.92881506e-02f,  -1.13163898e-02f, 6.20048959e-03f,
        -3.01633636e-03f, 1.17914262e-03f,  0.00000000e+00f };

/* Build flat [m][k] arrays expected by dp_resamp_dpmfs_create */
static void
make_coeff_arrays (float c0_out[(DPMFS_M + 1) * DPMFS_N],
                   float c1_out[(DPMFS_M + 1) * DPMFS_N])
{
  const float *c0_rows[4]
      = { dpmfs_c0_m0, dpmfs_c0_m1, dpmfs_c0_m2, dpmfs_c0_m3 };
  const float *c1_rows[4]
      = { dpmfs_c1_m0, dpmfs_c1_m1, dpmfs_c1_m2, dpmfs_c1_m3 };
  for (int m = 0; m <= DPMFS_M; m++)
    {
      memcpy (&c0_out[m * DPMFS_N], c0_rows[m], DPMFS_N * sizeof (float));
      memcpy (&c1_out[m * DPMFS_N], c1_rows[m], DPMFS_N * sizeof (float));
    }
}

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
 * Spectral helpers (identical to test_resamp.c)
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

static double
peak_near (const dp_cf32_t *sig, size_t n, double target_freq, double tol)
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
  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  dp_resamp_dpmfs_t *r
      = dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, c1, 2.0);
  CHECK (r != NULL, "create returns non-NULL");

  dp_resamp_dpmfs_destroy (r);
  PASS ("destroy does not crash");

  dp_resamp_dpmfs_destroy (NULL);
  PASS ("destroy(NULL) is safe");
}

static void
test_properties (void)
{
  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  dp_resamp_dpmfs_t *r
      = dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, c1, 2.5);

  CHECK (fabs (dp_resamp_dpmfs_rate (r) - 2.5) < 1e-12, "rate() returns 2.5");
  CHECK (dp_resamp_dpmfs_num_taps (r) == DPMFS_N, "num_taps() matches N");
  CHECK (dp_resamp_dpmfs_poly_order (r) == DPMFS_M, "poly_order() matches M");

  dp_resamp_dpmfs_destroy (r);
}

static void
test_null_args (void)
{
  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  CHECK (dp_resamp_dpmfs_create (0, DPMFS_N, c0, c1, 1.0) == NULL,
         "create rejects M=0");
  CHECK (dp_resamp_dpmfs_create (DPMFS_M, 0, c0, c1, 1.0) == NULL,
         "create rejects N=0");
  CHECK (dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, NULL, c1, 1.0) == NULL,
         "create rejects c0=NULL");
  CHECK (dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, NULL, 1.0) == NULL,
         "create rejects c1=NULL");
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

  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  dp_resamp_dpmfs_t *r
      = dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, c1, r_val);

  dp_cf32_t *x = malloc (N_IN * sizeof (dp_cf32_t));
  for (size_t i = 0; i < N_IN; i++)
    {
      double t0 = 2.0 * M_PI * freqs[0] * (double)i;
      double t1 = 2.0 * M_PI * freqs[1] * (double)i;
      x[i].i = (float)(cos (t0) + cos (t1));
      x[i].q = (float)(sin (t0) + sin (t1));
    }

  size_t max_out = (size_t)(N_IN * r_val) + 4;
  dp_cf32_t *y = calloc (max_out, sizeof (dp_cf32_t));
  size_t n_out = dp_resamp_dpmfs_execute (r, x, N_IN, y, max_out);

  CHECK (n_out > 0, "interp: produced output");

  for (int t = 0; t < 2; t++)
    {
      double f_out = freqs[t] / r_val;
      while (f_out > 0.5)
        f_out -= 1.0;
      while (f_out <= -0.5)
        f_out += 1.0;

      double amp = peak_near (y, n_out, f_out, 0.02);
      char msg[128];
      snprintf (msg, sizeof msg,
                "interp tone %.1f: amp %.3f dB (expect ±3 dB)", freqs[t], amp);
      CHECK (fabs (amp) < 3.0, msg);
    }

  double out_freqs[2] = { freqs[0] / r_val, freqs[1] / r_val };
  double art = artifact_dbc (y, n_out, out_freqs, 2, 0.02);
  {
    char msg[128];
    snprintf (msg, sizeof msg, "interp artifacts: %.1f dBc (expect < -55)",
              art);
    CHECK (art < -55.0, msg);
  }

  free (x);
  free (y);
  dp_resamp_dpmfs_destroy (r);
}

static void
test_decim_spectral (void)
{
  double r_val = 0.50333;
  double f_in[2] = { 0.4 * r_val, 0.6 * r_val };

  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  dp_resamp_dpmfs_t *r
      = dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, c1, r_val);

  dp_cf32_t *x = malloc (N_IN * sizeof (dp_cf32_t));
  for (size_t i = 0; i < N_IN; i++)
    {
      double t0 = 2.0 * M_PI * f_in[0] * (double)i;
      double t1 = 2.0 * M_PI * f_in[1] * (double)i;
      x[i].i = (float)(cos (t0) + cos (t1));
      x[i].q = (float)(sin (t0) + sin (t1));
    }

  size_t max_out = (size_t)(N_IN * r_val) + 4;
  dp_cf32_t *y = calloc (max_out, sizeof (dp_cf32_t));
  size_t n_out = dp_resamp_dpmfs_execute (r, x, N_IN, y, max_out);

  CHECK (n_out > 0, "decim: produced output");

  double f_out_tone1 = f_in[0] / r_val;
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

  double out_freqs[1] = { f_out_tone1 };
  double art = artifact_dbc (y, n_out, out_freqs, 1, 0.02);
  {
    char msg[128];
    snprintf (msg, sizeof msg, "decim artifacts: %.1f dBc (expect < -55)",
              art);
    CHECK (art < -55.0, msg);
  }

  free (x);
  free (y);
  dp_resamp_dpmfs_destroy (r);
}

/* =========================================================================
 * Reset test
 * ========================================================================= */

static void
test_reset (void)
{
  float c0[(DPMFS_M + 1) * DPMFS_N];
  float c1[(DPMFS_M + 1) * DPMFS_N];
  make_coeff_arrays (c0, c1);

  dp_resamp_dpmfs_t *r
      = dp_resamp_dpmfs_create (DPMFS_M, DPMFS_N, c0, c1, 2.0);

  dp_cf32_t x[64];
  for (int i = 0; i < 64; i++)
    {
      x[i].i = (float)cos (2.0 * M_PI * 0.1 * i);
      x[i].q = (float)sin (2.0 * M_PI * 0.1 * i);
    }
  dp_cf32_t y[200];
  dp_resamp_dpmfs_execute (r, x, 64, y, 200);

  dp_resamp_dpmfs_reset (r);
  dp_cf32_t y2[200];
  size_t n1 = dp_resamp_dpmfs_execute (r, x, 64, y2, 200);

  dp_resamp_dpmfs_reset (r);
  dp_cf32_t y3[200];
  size_t n2 = dp_resamp_dpmfs_execute (r, x, 64, y3, 200);

  CHECK (n1 == n2, "reset: same output count");
  int match = 1;
  for (size_t i = 0; i < n1 && i < n2; i++)
    if (y2[i].i != y3[i].i || y2[i].q != y3[i].q)
      match = 0;
  CHECK (match, "reset: identical output after reset");

  dp_resamp_dpmfs_destroy (r);
}

/* =========================================================================
 * Main
 * ========================================================================= */

int
main (void)
{
  printf ("=== dp_resamp_dpmfs unit tests ===\n\n");

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
