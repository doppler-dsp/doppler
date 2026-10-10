/**
 * @file spectrogram_certify.c
 * @brief The measurements the Spectrogram's characterization and
 * certification are built from.
 *
 * The Spectrogram has no Python binding until #1894's slice 3b, so its
 * evidence follows `docs/dev/contributing/validation.md` "Certifying a
 * component with no binding": **this file measures; the certification's
 * validator (#1941's A4, step 5, still to come) renders and asserts.**
 * Nothing here decides whether a number is acceptable.
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
 *   from dp_rng_test.h, scaled), NOISE_FRAMES frames per window and level.
 *   A bin's power is exponential with mean mu = P * ENBW / n against PSD's
 *   tone reference, so its median sits at L + 10 log10(ENBW / n)
 *   + 10 log10(ln 2), and a bin reads the floor with probability
 *   1 - exp(-1e-20 / mu). Both expectations are computed from the window's
 *   own ENBW and printed beside the frames' mean median and mean count.
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
/* noise frames averaged per window and level: one frame's median scatters
   by about 0.2 dB, more than the gap between two windows' ENBW */
#define NOISE_FRAMES 256

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
  printf (emit ? "# floor_noise\nwindow,level_dbfs,frames,expected_median_db,"
                 "mean_median_db,expected_at_floor,mean_at_floor,"
                 "max_at_floor\n"
               : "\nfloor_noise: complex noise of total power L dBFS, the "
                 "mean over frames\n");
  for (int w = 0; w < 4; w++)
    for (size_t j = 0; j < nl; j++)
      {
        uint32_t     seed = 1894u + (uint32_t)(100 * w + j);
        const double g    = pow (10.0, level[j] / 20.0);
        double       enbw = 0.0, med_sum = 0.0, floor_sum = 0.0;
        size_t       floor_max = 0;
        for (int f = 0; f < NOISE_FRAMES; f++)
          {
            for (size_t i = 0; i < NFFT; i++)
              x[i] = (float _Complex) (g * dp_cgauss (&seed));
            if (row_of (w, x, row, &enbw))
              return 1;
            memcpy (sorted, row, sizeof row);
            qsort (sorted, NFFT, sizeof *sorted, cmp_float);
            med_sum += 0.5 * (sorted[NFFT / 2 - 1] + sorted[NFFT / 2]);
            const size_t k = at_floor (row);
            floor_sum += (double)k;
            floor_max = k > floor_max ? k : floor_max;
          }
        /* each bin's power is exponential with mean mu against the tone
           reference, so P(bin <= floor) = 1 - exp(-floor / mu) */
        const double mu         = pow (10.0, level[j] / 10.0) * enbw / NFFT;
        const double want_med   = 10.0 * log10 (mu) + 10.0 * log10 (log (2.0));
        const double want_floor = NFFT * -expm1 (-1e-20 / mu);
        printf (emit ? "%s,%.0f,%d,%.4f,%.4f,%.4f,%.4f,%zu\n"
                     : "  %-16s L %6.0f  frames %d  median: expected %9.4f "
                       "mean %9.4f  at floor: expected %8.3f mean %8.3f "
                       "max %zu\n",
                WINDOW[w], level[j], NOISE_FRAMES, want_med,
                med_sum / NOISE_FRAMES, want_floor, floor_sum / NOISE_FRAMES,
                floor_max);
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
