#include "dp_state_test.h"
#include "psd/psd_core.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

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

/* argmax over a float array. */
static size_t
argmax (const float *a, size_t n)
{
  size_t m = 0;
  for (size_t i = 1; i < n; i++)
    if (a[i] > a[m])
      m = i;
  return m;
}

/* Fill x[0..n-1] with a unit-amplitude complex tone at FFT bin k. */
static void
fill_tone (float complex *x, size_t n, int k)
{
  for (size_t i = 0; i < n; i++)
    {
      double ph = 2.0 * M_PI * (double)k * (double)i / (double)n;
      x[i]      = (float complex) (cos (ph) + sin (ph) * I);
    }
}

/* Fill x[0..n-1] with a real cosine of amplitude A at FFT bin k. */
static void
fill_real_tone (float *x, size_t n, int k, double a)
{
  for (size_t i = 0; i < n; i++)
    x[i] = (float)(a * cos (2.0 * M_PI * (double)k * (double)i / (double)n));
}

/* Deterministic [-1,1] sample from a tiny LCG (portable, seeded). */
static float
lcg_unit (uint64_t *st)
{
  *st = *st * 6364136223846793005ULL + 1442695040888963407ULL;
  return (float)((double)(*st >> 33) / (double)(1ULL << 31) - 1.0);
}

/* Population standard deviation of a[lo..hi). */
static double
stddev (const float *a, size_t lo, size_t hi)
{
  double mean = 0.0;
  for (size_t i = lo; i < hi; i++)
    mean += a[i];
  mean /= (double)(hi - lo);
  double var = 0.0;
  for (size_t i = lo; i < hi; i++)
    var += (a[i] - mean) * (a[i] - mean);
  return sqrt (var / (double)(hi - lo));
}

int
main (void)
{
  int          _fails = 0;
  const size_t N      = 64;

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    CHECK (psd_create (1, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1) == NULL); /* n<2  */
    CHECK (psd_create (N, 0.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1) == NULL); /* fs   */
    CHECK (psd_create (N, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.1) == NULL); /* win  */
    CHECK (psd_create (N, 1.0, 0, 0.0f, 1, 0.0, 0, 0, 0.1) == NULL); /* fscl */
    CHECK (psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 9, 0.1) == NULL); /* mode */
    psd_destroy (NULL);                                              /* ok   */

    psd_state_t *w = psd_create (N, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    CHECK (w != NULL);
    CHECK (w->n == N);
    CHECK (w->fs == 1.0e6);
    CHECK (w->enbw > 1.0); /* any non-rectangular window has ENBW > 1 bin */

    /* psd_db before any frame → 0 (None in Python). */
    float db[64];
    CHECK (psd_psd_db (w, N, db) == 0);
    psd_destroy (w);
  }

  /* ── DC tone lands at the centre bin after fftshift ─────────────────── */
  {
    psd_state_t  *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float complex x[64];
    for (size_t i = 0; i < N; i++)
      x[i] = 1.0f + 0.0f * I;
    psd_accumulate (w, x, N);
    float db[64];
    CHECK (psd_psd_db (w, N, db) == N);
    CHECK (argmax (db, N) == N / 2); /* DC at index n/2 */
    psd_destroy (w);
  }

  /* ── tone at bin k maps to index n/2 + k; counts frames ─────────────── */
  {
    psd_state_t  *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    const int     k = 8;
    float complex x[64];
    fill_tone (x, N, k);
    /* feed 3 full frames + a trailing partial that must be ignored */
    float complex buf[64 * 3 + 7];
    for (size_t f = 0; f < 3; f++)
      for (size_t i = 0; i < N; i++)
        buf[f * N + i] = x[i];
    for (size_t i = 0; i < 7; i++)
      buf[3 * N + i] = x[i];
    psd_accumulate (w, buf, 3 * N + 7);
    CHECK (w->avg->count == 3);
    float db[64];
    psd_psd_db (w, N, db);
    CHECK (argmax (db, N) == N / 2 + (size_t)k);
    psd_destroy (w);
  }

  /* ── psd_dbhz differs from psd_db by a constant offset ──────────────── */
  {
    psd_state_t  *w = psd_create (32, 2.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float complex x[32];
    fill_tone (x, 32, 5);
    psd_accumulate (w, x, 32);
    float a[32], b[32];
    psd_psd_db (w, 32, a);
    psd_psd_dbhz (w, 32, b);
    float off0 = a[0] - b[0];
    for (size_t i = 0; i < 32; i++)
      CHECK (fabsf ((a[i] - b[i]) - off0) < 1e-3f);
    psd_destroy (w);
  }

  /* ── band power: a partition sums (in power) to the total ───────────── */
  {
    psd_state_t  *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float complex x[64];
    fill_tone (x, N, 10);
    for (int r = 0; r < 4; r++)
      psd_accumulate (w, x, N);

    /* whole span split into two halves */
    double bands[4] = { -0.5, 0.0, 0.0, 0.5 };
    float  per[2];
    size_t nb = psd_band_power (w, bands, 4, per);
    CHECK (nb == 2);
    double total = psd_total_band_power (w, bands, 4);
    /* total power = sum of the two halves' linear powers */
    double lin = pow (10.0, per[0] / 10.0) + pow (10.0, per[1] / 10.0);
    CHECK (fabs (10.0 * log10 (lin) - total) < 1e-2);

    /* a band entirely outside the span integrates to the floor */
    double far[2] = { 10.0, 11.0 };
    float  pf[1];
    psd_band_power (w, far, 2, pf);
    CHECK (pf[0] < -150.0f);
    psd_destroy (w);
  }

  /* ── occupied bandwidth: narrow for a tone, ~full for flat noise ────── */
  {
    psd_state_t  *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float complex x[64];
    fill_tone (x, N, 4);
    for (int r = 0; r < 4; r++)
      psd_accumulate (w, x, N);
    double obw = psd_occupied_bw (w, 0.99);
    CHECK (obw > 0.0 && obw < 0.5); /* a tone occupies a small fraction */
    psd_destroy (w);
  }

  /* ── noise floor / SNR / SFDR are finite on a two-tone signal ───────── */
  {
    psd_state_t  *w = psd_create (N, 1.0, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    float complex x[64];
    for (size_t i = 0; i < N; i++)
      {
        double p1 = 2.0 * M_PI * 6.0 * (double)i / (double)N;
        double p2 = 2.0 * M_PI * 20.0 * (double)i / (double)N;
        x[i]      = (float complex) ((cos (p1) + sin (p1) * I)
                                     + 0.1 * (cos (p2) + sin (p2) * I));
      }
    for (int r = 0; r < 8; r++)
      psd_accumulate (w, x, N);

    double nf  = psd_noise_floor (w);
    double snr = psd_snr (w, 0.0, 0.2); /* band around bin 6 */
    double sf  = psd_sfdr (w, -120.0f);
    CHECK (isfinite (nf));
    CHECK (isfinite (snr) && snr > 0.0); /* carrier above the floor    */
    CHECK (isfinite (sf) && sf > 0.0);   /* carrier above the spur     */

    /* reset clears the average */
    psd_reset (w);
    CHECK (w->avg->count == 0);
    float db[64];
    CHECK (psd_psd_db (w, N, db) == 0);
    psd_destroy (w);
  }

  /* ── cg^2 normalisation: a constant of amplitude A reads A^2 at DC ──── */
  {
    const double A = 3.0;
    float        x[64];
    for (size_t i = 0; i < N; i++)
      x[i] = (float)A;
    /* exact and pad-invariant: X[DC] = A*sum(w), so |X[DC]|^2/cg^2 = A^2 */
    for (size_t pad = 1; pad <= 2; pad++)
      {
        psd_state_t *w = psd_create (N, 1.0, 0, 0.0f, pad, 1.0, 0, 0, 0.1);
        psd_accumulate_real (w, x, N);
        float  two[128];
        size_t nfft = psd_power_twosided (w, w->nfft, two);
        CHECK (nfft == w->nfft);
        CHECK (fabs (two[w->nfft / 2] - A * A) < 1e-3); /* DC bin */
        float one[65];
        psd_power_onesided (w, w->nfft / 2 + 1, one);
        CHECK (fabs (one[0] - A * A) < 1e-3); /* one-sided DC */
        psd_destroy (w);
      }
  }

  /* ── ENBW: Hann ~1.5 bins; Kaiser(beta=8) wider ─────────────────────── */
  {
    psd_state_t *h = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    psd_state_t *k = psd_create (N, 1.0, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    CHECK (fabs (h->enbw - 1.5) < 0.05);
    CHECK (k->enbw > h->enbw); /* a Kaiser(8) main lobe is wider than Hann */
    psd_destroy (h);
    psd_destroy (k);
  }

  /* ── one-sided fold conserves energy: sum(one) == sum(two) ──────────── */
  {
    psd_state_t *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float        x[64];
    fill_real_tone (x, N, 7, 0.5);
    psd_accumulate_real (w, x, N);
    float two[64], one[33];
    psd_power_twosided (w, N, two);
    size_t no = psd_power_onesided (w, N / 2 + 1, one);
    CHECK (no == N / 2 + 1);
    double st = 0.0, so = 0.0;
    for (size_t i = 0; i < N; i++)
      st += two[i];
    for (size_t i = 0; i < no; i++)
      so += one[i];
    CHECK (fabs (st - so) < 1e-4 * st);
    psd_destroy (w);
  }

  /* ── zero-padding scales total power by nfft/n (the cal cores correct) ─ */
  {
    float x[64];
    fill_real_tone (x, N, 9, 0.7);
    double tot[3] = { 0 };
    for (size_t pad = 1; pad <= 2; pad++)
      {
        psd_state_t *w = psd_create (N, 1.0, 0, 0.0f, pad, 1.0, 0, 0, 0.1);
        psd_accumulate_real (w, x, N);
        float  two[128];
        size_t nfft = psd_power_twosided (w, w->nfft, two);
        for (size_t i = 0; i < nfft; i++)
          tot[pad] += two[i];
        psd_destroy (w);
      }
    /* nfft doubles from pad=1 (64) to pad=2 (128): total scales ~2x */
    CHECK (fabs (tot[2] / tot[1] - 2.0) < 0.05);
  }

  /* ── mean of K identical frames equals a single frame (exact) ───────── */
  {
    float x[64];
    fill_real_tone (x, N, 11, 0.4);
    psd_state_t *w = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    psd_accumulate_real (w, x, N);
    float a[33];
    psd_power_onesided (w, N / 2 + 1, a);
    psd_reset (w);
    float buf[64 * 5];
    for (size_t f = 0; f < 5; f++)
      for (size_t i = 0; i < N; i++)
        buf[f * N + i] = x[i];
    psd_accumulate_real (w, buf, 5 * N);
    float b[33];
    psd_power_onesided (w, N / 2 + 1, b);
    for (size_t i = 0; i < N / 2 + 1; i++)
      CHECK (fabsf (a[i] - b[i]) < 1e-6f * (fabsf (a[i]) + 1e-6f));
    psd_destroy (w);
  }

  /* ── maxhold >= minhold per bin over differing frames ───────────────── */
  {
    float f1[64], f2[64];
    fill_real_tone (f1, N, 8, 1.0);
    fill_real_tone (f2, N, 16, 1.0);
    float buf[128];
    for (size_t i = 0; i < N; i++)
      buf[i] = f1[i];
    for (size_t i = 0; i < N; i++)
      buf[N + i] = f2[i];

    psd_state_t *mx
        = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, ACC_TRACE_MAXHOLD, 0.1);
    psd_state_t *mn
        = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, ACC_TRACE_MINHOLD, 0.1);
    psd_accumulate_real (mx, buf, 2 * N);
    psd_accumulate_real (mn, buf, 2 * N);
    float pmx[64], pmn[64];
    psd_power_twosided (mx, N, pmx);
    psd_power_twosided (mn, N, pmn);
    for (size_t i = 0; i < N; i++)
      CHECK (pmx[i] >= pmn[i] - 1e-6f);
    psd_destroy (mx);
    psd_destroy (mn);
  }

  /* ── averaging tightens the noise-floor estimate (deterministic) ────── */
  {
    const size_t K   = 64;
    uint64_t     rng = 0x1234567890abcdefULL;
    float       *buf = (float *)malloc (K * N * sizeof (float));
    for (size_t i = 0; i < K * N; i++)
      buf[i] = lcg_unit (&rng);

    psd_state_t *w1 = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    psd_state_t *wK = psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    psd_accumulate_real (w1, buf, N);     /* 1 frame  */
    psd_accumulate_real (wK, buf, K * N); /* K frames */
    float p1[64], pK[64];
    psd_power_twosided (w1, N, p1);
    psd_power_twosided (wK, N, pK);
    /* white noise: the K-averaged spectrum is flatter (smaller spread). */
    CHECK (stddev (pK, 1, N) < stddev (p1, 1, N));
    free (buf);
    psd_destroy (w1);
    psd_destroy (wK);
  }

  if (_fails)
    {
      fprintf (stderr, "test_psd_core FAILED (%d)\n", _fails);
      return 1;
    }
  /* serializable state — delegates to the acc_trace averager child. */
  {
    float complex frame[64];
    for (int i = 0; i < 64; i++)
      frame[i] = (float)(i % 8) - 4.0f + 0.3f * I;
    psd_state_t *a = psd_create (64, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    psd_state_t *b = psd_create (64, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    CHECK (a != NULL && b != NULL);
    psd_accumulate (a, frame, 64);
    psd_accumulate (a, frame, 64);
    DP_STATE_ROUNDTRIP_TEST (psd, a, b);
    CHECK (b->avg->count == a->avg->count);
    psd_destroy (a);
    psd_destroy (b);
  }

  printf ("test_psd_core PASSED\n");
  return 0;
}
