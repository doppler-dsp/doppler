#include "welch/welch_core.h"
#include <complex.h>
#include <math.h>
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

int
main (void)
{
  int          _fails = 0;
  const size_t N      = 64;

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    CHECK (welch_create (1, 1.0, 0, 0.0f, 0, 0.1) == NULL); /* n < 2   */
    CHECK (welch_create (N, 0.0, 0, 0.0f, 0, 0.1) == NULL); /* fs <= 0 */
    CHECK (welch_create (N, 1.0, 2, 0.0f, 0, 0.1) == NULL); /* window  */
    CHECK (welch_create (N, 1.0, 0, 0.0f, 9, 0.1) == NULL); /* mode    */
    welch_destroy (NULL);                                   /* no crash */

    welch_state_t *w = welch_create (N, 1.0e6, 1, 8.0f, 0, 0.1);
    CHECK (w != NULL);
    CHECK (w->n == N);
    CHECK (w->fs == 1.0e6);
    CHECK (w->enbw > 1.0); /* any non-rectangular window has ENBW > 1 bin */

    /* psd_db before any frame → 0 (None in Python). */
    float db[64];
    CHECK (welch_psd_db (w, N, db) == 0);
    welch_destroy (w);
  }

  /* ── DC tone lands at the centre bin after fftshift ─────────────────── */
  {
    welch_state_t *w = welch_create (N, 1.0, 0, 0.0f, 0, 0.1);
    float complex  x[64];
    for (size_t i = 0; i < N; i++)
      x[i] = 1.0f + 0.0f * I;
    welch_accumulate (w, x, N);
    float db[64];
    CHECK (welch_psd_db (w, N, db) == N);
    CHECK (argmax (db, N) == N / 2); /* DC at index n/2 */
    welch_destroy (w);
  }

  /* ── tone at bin k maps to index n/2 + k; counts frames ─────────────── */
  {
    welch_state_t *w = welch_create (N, 1.0, 0, 0.0f, 0, 0.1);
    const int      k = 8;
    float complex  x[64];
    fill_tone (x, N, k);
    /* feed 3 full frames + a trailing partial that must be ignored */
    float complex buf[64 * 3 + 7];
    for (size_t f = 0; f < 3; f++)
      for (size_t i = 0; i < N; i++)
        buf[f * N + i] = x[i];
    for (size_t i = 0; i < 7; i++)
      buf[3 * N + i] = x[i];
    welch_accumulate (w, buf, 3 * N + 7);
    CHECK (w->avg->count == 3);
    float db[64];
    welch_psd_db (w, N, db);
    CHECK (argmax (db, N) == N / 2 + (size_t)k);
    welch_destroy (w);
  }

  /* ── psd_dbhz differs from psd_db by a constant offset ──────────────── */
  {
    welch_state_t *w = welch_create (32, 2.0, 0, 0.0f, 0, 0.1);
    float complex  x[32];
    fill_tone (x, 32, 5);
    welch_accumulate (w, x, 32);
    float a[32], b[32];
    welch_psd_db (w, 32, a);
    welch_psd_dbhz (w, 32, b);
    float off0 = a[0] - b[0];
    for (size_t i = 0; i < 32; i++)
      CHECK (fabsf ((a[i] - b[i]) - off0) < 1e-3f);
    welch_destroy (w);
  }

  /* ── band power: a partition sums (in power) to the total ───────────── */
  {
    welch_state_t *w = welch_create (N, 1.0, 0, 0.0f, 0, 0.1);
    float complex  x[64];
    fill_tone (x, N, 10);
    for (int r = 0; r < 4; r++)
      welch_accumulate (w, x, N);

    /* whole span split into two halves */
    double bands[4] = { -0.5, 0.0, 0.0, 0.5 };
    float  per[2];
    size_t nb = welch_band_power (w, bands, 4, per);
    CHECK (nb == 2);
    double total = welch_total_band_power (w, bands, 4);
    /* total power = sum of the two halves' linear powers */
    double lin = pow (10.0, per[0] / 10.0) + pow (10.0, per[1] / 10.0);
    CHECK (fabs (10.0 * log10 (lin) - total) < 1e-2);

    /* a band entirely outside the span integrates to the floor */
    double far[2] = { 10.0, 11.0 };
    float  pf[1];
    welch_band_power (w, far, 2, pf);
    CHECK (pf[0] < -150.0f);
    welch_destroy (w);
  }

  /* ── occupied bandwidth: narrow for a tone, ~full for flat noise ────── */
  {
    welch_state_t *w = welch_create (N, 1.0, 0, 0.0f, 0, 0.1);
    float complex  x[64];
    fill_tone (x, N, 4);
    for (int r = 0; r < 4; r++)
      welch_accumulate (w, x, N);
    double obw = welch_occupied_bw (w, 0.99);
    CHECK (obw > 0.0 && obw < 0.5); /* a tone occupies a small fraction */
    welch_destroy (w);
  }

  /* ── noise floor / SNR / SFDR are finite on a two-tone signal ───────── */
  {
    welch_state_t *w = welch_create (N, 1.0, 1, 8.0f, 0, 0.1);
    float complex  x[64];
    for (size_t i = 0; i < N; i++)
      {
        double p1 = 2.0 * M_PI * 6.0 * (double)i / (double)N;
        double p2 = 2.0 * M_PI * 20.0 * (double)i / (double)N;
        x[i]      = (float complex) ((cos (p1) + sin (p1) * I)
                                     + 0.1 * (cos (p2) + sin (p2) * I));
      }
    for (int r = 0; r < 8; r++)
      welch_accumulate (w, x, N);

    double nf  = welch_noise_floor (w);
    double snr = welch_snr (w, 0.0, 0.2); /* band around bin 6 */
    double sf  = welch_sfdr (w, -120.0f);
    CHECK (isfinite (nf));
    CHECK (isfinite (snr) && snr > 0.0); /* carrier above the floor    */
    CHECK (isfinite (sf) && sf > 0.0);   /* carrier above the spur     */

    /* reset clears the average */
    welch_reset (w);
    CHECK (w->avg->count == 0);
    float db[64];
    CHECK (welch_psd_db (w, N, db) == 0);
    welch_destroy (w);
  }

  if (_fails)
    {
      fprintf (stderr, "test_welch_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_welch_core PASSED\n");
  return 0;
}
