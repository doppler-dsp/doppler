/**
 * @file spectrogram_certify.c
 * @brief The measurements the Spectrogram's characterization and
 * certification are built from.
 *
 * The Spectrogram has no Python binding until #1894's slice 3b, so its
 * evidence follows `docs/dev/contributing/validation.md` "Certifying a
 * component with no binding": **this file measures; a validator renders and
 * asserts.** Nothing here decides whether a number is acceptable.
 *
 * Run with no arguments for a readable run (`make validate-c`), or with
 * `--emit` for the CSV blocks a validator parses.
 *
 * ## U5 -- the dB floor (docs/design/spectrogram-measurements.md §5.4)
 *
 * PSD's kernel clamps power at 1e-20 before the log, so a row cannot read
 * below -200 dB. Three measurements, every window, nfft 1024:
 *
 * - `floor_tone`: a single on-bin tone of amplitude 10^(L/20), so of power
 *   L dBFS, across L from -120 to -250. Its bin reads L while L is above the
 *   floor, and the clamp below it.
 * - `floor_zero`: an all-zero frame, the digital silence a caller might want
 *   to tell apart.
 * - `floor_noise`: complex Gaussian noise of total power L dBFS (E|z|^2 = 1
 *   from dp_rng_test.h, scaled). One frame's bin power is exponential with
 *   mean P * ENBW / n against PSD's tone reference, so its median sits at
 *   L + 10 log10(ENBW / n) + 10 log10(ln 2): the expected column, computed
 *   from the window's own ENBW, against which the measured median and the
 *   share of bins at the floor are read.
 *
 * Samples this small are ordinary floats (float32's smallest normal is about
 * 1.2e-38, an amplitude of -759 dBFS), and in-tree sources make them: a wfm
 * source's `level` is in dBFS with no lower bound (wfm_compose.h, `level`;
 * applied as a gain of 10^(level/20) in wfm_compose.c).
 */
#include "doppler/spectrogram/spectrogram_core.h"

#include "dp_rng_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFFT 1024
#define TONE 37 /* the tone's bin: on the grid, away from DC */

static const char *const WINDOW[4]
    = { "hann", "kaiser", "blackman-harris", "rect" };

/** @brief One row of the first NFFT samples of @p x, under @p window. */
static int
row_of (int window, const float _Complex *x, float *row, double *enbw)
{
  /* Kaiser at beta 8, a display's usual choice; beta is ignored otherwise */
  dp_spectrogram_state_t *s = dp_spectrogram_create (
      NFFT, NFFT, window, window == 1 ? 8.0f : 0.0f, DP_SPECTROGRAM_DB);
  if (!s)
    return -1;
  size_t w = dp_spectrogram_push (s, x, NFFT, row, NFFT);
  if (enbw)
    *enbw = s->psd->enbw;
  dp_spectrogram_destroy (s);
  return w == NFFT ? 0 : -1;
}

static size_t
at_floor (const float *row)
{
  size_t k = 0;
  for (size_t i = 0; i < NFFT; i++)
    k += row[i] == -200.0f;
  return k;
}

static int
cmp_float (const void *a, const void *b)
{
  const float x = *(const float *)a, y = *(const float *)b;
  return (x > y) - (x < y);
}

static int
floor_tone (int emit)
{
  static const double level[]
      = { -120, -150, -180, -190, -195, -199, -200, -201, -205, -210, -250 };
  const size_t nl = sizeof level / sizeof level[0];
  float _Complex x[NFFT];
  float row[NFFT];
  printf (emit ? "# floor_tone\nwindow,level_dbfs,tone_bin_db,"
                 "tone_bin_minus_level_db,bins_at_floor\n"
               : "\nfloor_tone: an on-bin tone of power L dBFS, its bin\n");
  for (int w = 0; w < 4; w++)
    for (size_t j = 0; j < nl; j++)
      {
        const double a = pow (10.0, level[j] / 20.0);
        for (size_t i = 0; i < NFFT; i++)
          x[i] = (float)(a * cos (2.0 * M_PI * TONE * (double)i / NFFT))
                 + (float)(a * sin (2.0 * M_PI * TONE * (double)i / NFFT)) * I;
        if (row_of (w, x, row, NULL))
          return 1;
        const float b = row[NFFT / 2 + TONE];
        printf (emit ? "%s,%.0f,%.4f,%.4f,%zu\n"
                     : "  %-16s L %6.0f  bin %10.4f  (%+.4f)  at floor %zu\n",
                WINDOW[w], level[j], b, b - level[j], at_floor (row));
      }
  return 0;
}

static int
floor_zero (int emit)
{
  float _Complex x[NFFT] = { 0 };
  float row[NFFT];
  printf (emit ? "# floor_zero\nwindow,bins_at_floor,bins,min_db,max_db\n"
               : "\nfloor_zero: an all-zero frame\n");
  for (int w = 0; w < 4; w++)
    {
      if (row_of (w, x, row, NULL))
        return 1;
      float lo = row[0], hi = row[0];
      for (size_t i = 1; i < NFFT; i++)
        {
          lo = row[i] < lo ? row[i] : lo;
          hi = row[i] > hi ? row[i] : hi;
        }
      printf (emit ? "%s,%zu,%d,%.4f,%.4f\n"
                   : "  %-16s at floor %zu of %d  min %.4f  max %.4f\n",
              WINDOW[w], at_floor (row), NFFT, lo, hi);
    }
  return 0;
}

static int
floor_noise (int emit)
{
  static const double level[] = { -150, -160, -170, -175, -180, -190, -210 };
  const size_t        nl      = sizeof level / sizeof level[0];
  float _Complex x[NFFT];
  float row[NFFT], sorted[NFFT];
  printf (emit ? "# floor_noise\nwindow,level_dbfs,expected_median_db,"
                 "median_db,bins_at_floor\n"
               : "\nfloor_noise: complex noise of total power L dBFS, one "
                 "frame\n");
  for (int w = 0; w < 4; w++)
    for (size_t j = 0; j < nl; j++)
      {
        uint32_t     seed = 1894u + (uint32_t)(100 * w + j);
        const double g    = pow (10.0, level[j] / 20.0);
        for (size_t i = 0; i < NFFT; i++)
          x[i] = (float _Complex) (g * dp_cgauss (&seed));
        double enbw = 0.0;
        if (row_of (w, x, row, &enbw))
          return 1;
        memcpy (sorted, row, sizeof row);
        qsort (sorted, NFFT, sizeof *sorted, cmp_float);
        const double median = 0.5 * (sorted[NFFT / 2 - 1] + sorted[NFFT / 2]);
        const double want
            = level[j] + 10.0 * log10 (enbw / NFFT) + 10.0 * log10 (log (2.0));
        printf (emit ? "%s,%.0f,%.4f,%.4f,%zu\n"
                     : "  %-16s L %6.0f  expected %10.4f  median %10.4f  "
                       "at floor %zu\n",
                WINDOW[w], level[j], want, median, at_floor (row));
      }
  return 0;
}

int
main (int argc, char **argv)
{
  const int emit = argc > 1 && strcmp (argv[1], "--emit") == 0;
  if (!emit)
    printf ("spectrogram_certify: nfft %d, every window\n", NFFT);
  if (floor_tone (emit) || floor_zero (emit) || floor_noise (emit))
    {
      (void)fprintf (stderr, "spectrogram_certify: a row could not be made\n");
      return 1;
    }
  return 0;
}
