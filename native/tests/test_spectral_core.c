/*
 * test_spectral_core.c — the spectral module's ten free functions.
 *
 * Windows, dB conversion, and the three estimators a spectrum analyser
 * builds on. Every one of them has a closed form or a defining property,
 * so nothing here is a golden array: a window is checked against its own
 * definition, ENBW against the rectangular case it is normalised to, and
 * the estimators against inputs whose answer is known by construction.
 *
 * The estimators are the ones worth the words. `noise_floor_db` is a
 * MEDIAN, and the reason is that a mean would be dragged up by exactly
 * the tones a noise floor is supposed to sit beneath — so the test puts
 * tones in and checks the answer does not move. `obw_from_power` excludes
 * (1-frac)/2 of the power at each end, which is the standard definition
 * and is not the same as "the width where the power is above a
 * threshold"; a symmetric synthetic spectrum makes the difference
 * checkable.
 */
#include "dp_test.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N 64

int
main (void)
{
  static float w[1024];

  /* ── hann_window: its own definition, endpoints, and symmetry ───── */
  {
    hann_window (w, N);
    for (size_t i = 0; i < N; i++)
      {
        const double want
            = 0.5 - 0.5 * cos (2.0 * M_PI * (double)i / (double)(N - 1));
        DP_CHECK (fabs ((double)w[i] - want) < 1e-5);
      }
    /* A Hann window starts and ends at zero -- that is what makes it
       continuous when the frame is repeated, which is the whole point. */
    DP_CHECK (fabs ((double)w[0]) < 1e-6);
    DP_CHECK (fabs ((double)w[N - 1]) < 1e-6);
    for (size_t i = 0; i < N; i++)
      DP_CHECK (fabs ((double)w[i] - (double)w[N - 1 - i]) < 1e-6);
  }

  /* ── blackman_harris_window: symmetric, bounded, and NOT Hann ───── */
  {
    static float bh[N];
    blackman_harris_window (bh, N);
    for (size_t i = 0; i < N; i++)
      {
        DP_CHECK (bh[i] >= -1e-6f && bh[i] <= 1.0f + 1e-6f);
        DP_CHECK (fabs ((double)bh[i] - (double)bh[N - 1 - i]) < 1e-6);
      }
    /* Peaks at the centre, at unity. */
    DP_CHECK (fabs ((double)bh[N / 2] - 1.0) < 0.02);

    /* It is a DIFFERENT window from Hann -- a copy-paste that returned
       Hann here would satisfy every check above. Its lower sidelobes
       cost main-lobe width, which shows up as a larger ENBW. */
    hann_window (w, N);
    DP_CHECK (kaiser_enbw (bh, N) > kaiser_enbw (w, N));
  }

  /* ── kaiser_window / kaiser_enbw ────────────────────────────────── */
  {
    /* beta = 0 IS the rectangular window, and a rectangular window has
       ENBW exactly 1.0 by the definition ENBW = N*sum(w^2)/(sum w)^2. */
    kaiser_window (w, N, 0.0f);
    for (size_t i = 0; i < N; i++)
      DP_CHECK (fabs ((double)w[i] - 1.0) < 1e-5);
    DP_CHECK (fabs ((double)kaiser_enbw (w, N) - 1.0) < 1e-5);

    /* Every tapered window costs noise bandwidth, and more taper costs
       more: ENBW must rise monotonically with beta. */
    double prev = 0.0;
    for (float beta = 0.0f; beta <= 12.0f; beta += 1.0f)
      {
        kaiser_window (w, N, beta);
        const double e = (double)kaiser_enbw (w, N);
        DP_CHECK (e >= prev - 1e-6);
        DP_CHECK (e >= 1.0 - 1e-6); /* nothing beats rectangular */
        prev = e;
      }

    /* The header's worked example, on the Hann window. */
    hann_window (w, 8);
    DP_CHECK (fabs ((double)kaiser_enbw (w, 8) - 1.7143) < 1e-3);
  }

  /* ── kaiser_beta_for_sidelobe: inverse of the Kaiser design rule ─── */
  {
    /* Monotone -- a deeper sidelobe target needs a heavier taper. */
    double prev = -1.0;
    for (double a = 21.0; a <= 120.0; a += 5.0)
      {
        const double b = kaiser_beta_for_sidelobe (a);
        DP_CHECK (b >= prev);
        prev = b;
      }
    /* Below 21 dB a rectangular window already meets the target. */
    DP_CHECK (kaiser_beta_for_sidelobe (10.0) <= 1e-9);

    /* The inverse really does invert: a window designed for -80 dB has
       lower sidelobes than one designed for -40 dB, measured on the
       windows themselves rather than on the formula. */
    static float w40[256], w80[256];
    kaiser_window (w40, 256, (float)kaiser_beta_for_sidelobe (40.0));
    kaiser_window (w80, 256, (float)kaiser_beta_for_sidelobe (80.0));
    DP_CHECK (kaiser_enbw (w80, 256) > kaiser_enbw (w40, 256));
  }

  /* ── magnitude_db_cf32 / cf64: the same answer, two input widths ── */
  {
    float complex  x32[4];
    double complex x64[4];
    float          o32[4], o64[4];

    x32[0] = 1.0f + 0.0f * I;  /* |x| = 1     ->   0 dB */
    x32[1] = 0.0f + 10.0f * I; /* |x| = 10    ->  20 dB */
    x32[2] = 3.0f + 4.0f * I;  /* |x| = 5     ->  ~14 dB */
    x32[3] = 0.0f + 0.0f * I;  /* zero -> the floor, not -inf */
    for (int i = 0; i < 4; i++)
      x64[i] = (double complex)x32[i];

    magnitude_db_cf32 (x32, 4, o32, 1e-12f, 0.0f);
    DP_CHECK (fabs ((double)o32[0] - 0.0) < 1e-3);
    DP_CHECK (fabs ((double)o32[1] - 20.0) < 1e-3);
    DP_CHECK (fabs ((double)o32[2] - 20.0 * log10 (5.0)) < 1e-3);
    /* The floor is what stops a zero bin becoming -inf and poisoning
       every average taken over the spectrum afterwards. */
    DP_CHECK (isfinite ((double)o32[3]));
    DP_CHECK ((double)o32[3] < -200.0);

    /* offset_db is a calibration constant added to EVERY bin. */
    magnitude_db_cf32 (x32, 4, o64, 1e-12f, -13.5f);
    for (int i = 0; i < 4; i++)
      DP_CHECK (fabs ((double)o64[i] - ((double)o32[i] - 13.5)) < 1e-3);

    /* The CF64 face is the same function at a wider input -- a
       divergence here is the kind that only shows up in one pipeline. */
    magnitude_db_cf64 (x64, 4, o64, 1e-12, 0.0f);
    for (int i = 0; i < 3; i++)
      DP_CHECK (fabs ((double)o64[i] - (double)o32[i]) < 1e-3);
  }

  /* ── find_peaks_f32: the header's example, then the ordering ────── */
  {
    static float db[32];
    dp_peak_t    pk[4];

    for (int i = 0; i < 32; i++)
      db[i] = -60.0f;
    db[7] = -15.0f;
    db[8] = -10.0f;
    db[9] = -15.0f;

    size_t n = find_peaks_f32 (db, 32, 2, -30.0f, pk);
    DP_CHECK (n == 1); /* one peak clears the gate, not three */
    DP_CHECK (fabs ((double)pk[0].freq_norm - (-0.25)) < 1e-6);
    DP_CHECK (fabs ((double)pk[0].amplitude_db - (-10.0)) < 1e-6);

    /* Two peaks, returned strongest first -- "sorted descending" is the
       contract, and a caller taking result[0] depends on it. */
    db[20] = -20.0f;
    db[21] = -5.0f;
    db[22] = -20.0f;
    n      = find_peaks_f32 (db, 32, 4, -30.0f, pk);
    DP_CHECK (n == 2);
    DP_CHECK (pk[0].amplitude_db > pk[1].amplitude_db);
    DP_CHECK (fabs ((double)pk[0].amplitude_db - (-5.0)) < 1e-6);

    /* n_peaks caps the output. */
    DP_CHECK (find_peaks_f32 (db, 32, 1, -30.0f, pk) == 1);
    /* A gate above everything returns nothing -- and an empty result is
       not an error. */
    DP_CHECK (find_peaks_f32 (db, 32, 4, 0.0f, pk) == 0);
  }

  /* ── noise_floor_db: a MEDIAN, so tones must not move it ────────── */
  {
    static float db[101];
    for (int i = 0; i < 101; i++)
      db[i] = -70.0f;

    DP_CHECK (fabs (noise_floor_db (db, 101) - (-70.0)) < 1e-6);

    /* Twenty huge spurs -- 20% of the bins -- must not shift a median
       estimate. A MEAN over this input lands near -55 dB, which is the
       error this function exists to avoid. */
    for (int i = 0; i < 20; i++)
      db[i * 5] = 0.0f;
    DP_CHECK (fabs (noise_floor_db (db, 101) - (-70.0)) < 1e-6);

    /* It must not modify its input -- the docstring says the sort is on
       a copy, and a caller reusing the buffer depends on it. */
    DP_CHECK (db[1] == -70.0f);
    DP_CHECK (db[0] == 0.0f);

    /* A tilted floor gives back its own middle value. */
    for (int i = 0; i < 101; i++)
      db[i] = (float)(-100 + i);
    DP_CHECK (fabs (noise_floor_db (db, 101) - (-50.0)) < 1e-6);

    DP_CHECK (noise_floor_db (db, 0) == 0.0);
  }

  /* ── obw_from_power: (1-frac)/2 excluded at EACH end ─────────────── */
  {
    static double pwr[100];
    const double  fs = 1000.0;

    /* Flat spectrum: 90% occupied bandwidth is 90% of fs, because the
       excluded 5% at each end is 5% of the bins. */
    for (int i = 0; i < 100; i++)
      pwr[i] = 1.0;
    DP_CHECK (fabs (obw_from_power (pwr, 100, fs, 0.90) - 900.0) < 20.0);
    DP_CHECK (fabs (obw_from_power (pwr, 100, fs, 0.50) - 500.0) < 20.0);

    /* Monotone in frac: asking for more power cannot need less band. */
    double prev = -1.0;
    for (double f = 0.1; f <= 0.99; f += 0.1)
      {
        const double b = obw_from_power (pwr, 100, fs, f);
        DP_CHECK (b >= prev - 1e-9);
        prev = b;
      }

    /* Power concentrated in the middle ten bins: the occupied bandwidth
       collapses to about a tenth of the span, whatever the span is. */
    for (int i = 0; i < 100; i++)
      pwr[i] = (i >= 45 && i < 55) ? 1.0 : 0.0;
    DP_CHECK (obw_from_power (pwr, 100, fs, 0.99) < 200.0);

    /* Any constant per-bin normalisation cancels in the ratio, which is
       what lets a caller pass an unscaled periodogram. */
    static double scaled[100];
    for (int i = 0; i < 100; i++)
      scaled[i] = pwr[i] * 12345.0;
    DP_CHECK (fabs (obw_from_power (scaled, 100, fs, 0.99)
                    - obw_from_power (pwr, 100, fs, 0.99))
              < 1e-9);

    /* Degenerate inputs return 0 rather than a plausible bandwidth. */
    DP_CHECK (obw_from_power (pwr, 0, fs, 0.99) == 0.0);
    for (int i = 0; i < 100; i++)
      pwr[i] = 0.0;
    DP_CHECK (obw_from_power (pwr, 100, fs, 0.99) == 0.0);
  }

  DP_TEST_END ("test_spectral_core");
}
