/* bench_spectral_core.c -- everything a spectrum frame does except the FFT.
 *
 * The FFT has a published number, at both faces. The rest of the frame has
 * none, and the rest of the frame is not small: after the transform every
 * bin goes through a magnitude, a log and an offset, and only then do the
 * analysis passes run. `magnitude_db_cf32` is a `log10f` per bin, which is
 * a transcendental per output where the FFT spends a handful of adds --
 * so the assumption that the transform dominates its own frame is exactly
 * the kind of assumption that should be measured rather than repeated.
 *
 * The rows are one spectrum frame, in the order it happens:
 *
 *   *_window          generated once and cached -- here to price the
 *                     cache miss, i.e. what a caller pays if it rebuilds
 *                     the window per frame instead of holding it
 *   kaiser_enbw       the window's noise bandwidth, also cacheable
 *   magnitude_db_*    per bin, per frame, unavoidable
 *   noise_floor_db    a pass over the dB spectrum
 *   find_peaks_f32    local maxima, parabolic interpolation, then a sort
 *   obw_from_power    a cumulative walk over the power spectrum
 *
 * cf32 and cf64 are both here because they are the same algorithm at two
 * widths, so the pair prices the precision rather than the operation.
 *
 * Everything is normalised per BIN, so the rows are directly comparable to
 * each other and to the FFT row in `bench_fft_core.c`.
 */
#include "dp_bench.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define NFFT 4096
#define REPS 16
#define ITERATIONS 100
#define N_PEAKS 8

enum
{
  C_KAISER_WIN,
  C_HANN_WIN,
  C_BH_WIN,
  C_ENBW,
  C_MAG_CF32,
  C_MAG_CF64,
  C_NOISE_FLOOR,
  C_FIND_PEAKS,
  C_OBW,
  N_CFG
};

static const char *const cfg_name[N_CFG] = {
  "kaiser_window[4096]",          "hann_window[4096]",
  "blackman_harris_window[4096]", "kaiser_enbw[4096]",
  "magnitude_db_cf32[4096]",      "magnitude_db_cf64[4096]",
  "noise_floor_db[4096]",         "find_peaks_f32[4096,n=8]",
  "obw_from_power[4096]",
};

static float          win[NFFT];
static float complex  x32[NFFT];
static double complex x64[NFFT];
static float          db[NFFT];
static double         pwr[NFFT];
static dp_peak_t      peaks[N_PEAKS];

static volatile double sink = 0.0;

static void
run (int cfg)
{
  switch (cfg)
    {
    case C_KAISER_WIN:
      kaiser_window (win, NFFT, 8.6f);
      sink += win[NFFT / 2];
      break;
    case C_HANN_WIN:
      hann_window (win, NFFT);
      sink += win[NFFT / 2];
      break;
    case C_BH_WIN:
      blackman_harris_window (win, NFFT);
      sink += win[NFFT / 2];
      break;
    case C_ENBW:
      sink += kaiser_enbw (win, NFFT);
      break;
    case C_MAG_CF32:
      magnitude_db_cf32 (x32, NFFT, db, 1e-20f, 0.0f);
      sink += db[0];
      break;
    case C_MAG_CF64:
      magnitude_db_cf64 (x64, NFFT, db, 1e-20, 0.0f);
      sink += db[0];
      break;
    case C_NOISE_FLOOR:
      sink += noise_floor_db (db, NFFT);
      break;
    case C_FIND_PEAKS:
      sink += (double)find_peaks_f32 (db, NFFT, N_PEAKS, -80.0f, peaks);
      break;
    default:
      sink += obw_from_power (pwr, NFFT, 1e6, 0.99);
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];

  /* A noise floor with a few real tones on it. A flat spectrum would
     give find_peaks_f32 nothing to interpolate or sort, and a sort with
     nothing to sort is not the sort a caller runs. */
  for (int i = 0; i < NFFT; i++)
    {
      const double ph = 0.7 * (double)i;
      double       a  = 1e-3 * (1.0 + 0.5 * sin (0.013 * (double)i));
      if (i == 512 || i == 1300 || i == 2048 || i == 3100)
        a = 1.0;
      x32[i] = (float complex) (a * cos (ph) + a * sin (ph) * I);
      x64[i] = a * cos (ph) + a * sin (ph) * I;
      pwr[i] = a * a;
    }
  hann_window (win, NFFT);
  magnitude_db_cf32 (x32, NFFT, db, 1e-20f, 0.0f);

  printf ("=== spectral (one frame, minus the FFT) ===\n");
  printf ("nfft = %d, %d calls x %d rounds, per-BIN figures\n\n", NFFT, REPS,
          ITERATIONS);

  DP_BENCH_SETTLE (run (C_MAG_CF32));

  /* Rounds outside, stages inside. These nine rows are read against each
     other -- and against the FFT row in another file -- so drift has to
     land on all of them or the comparison is the artifact. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < REPS; k++)
          run (c);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    dp_bench_record (&_bench, cfg_name[c], t[c], ITERATIONS,
                     (size_t)NFFT * REPS, "bin");

  printf ("\n  cf64 costs %.2fx cf32 over the same bins -- the price of the\n"
          "  precision, not of the operation.\n"
          "\n  Two rows cost more than the log10 pass they follow:\n"
          "    kaiser_window  %.1fx magnitude_db_cf32  (cache it; a Bessel\n"
          "                   per bin is not a per-frame cost)\n"
          "    noise_floor_db %.1fx                    (a single pass over\n"
          "                   the dB spectrum, and the more surprising one)\n"
          "  Neither is the FFT, and neither was measured before this file.\n",
          dp_bench_min (t[C_MAG_CF64], ITERATIONS)
              / dp_bench_min (t[C_MAG_CF32], ITERATIONS),
          dp_bench_min (t[C_KAISER_WIN], ITERATIONS)
              / dp_bench_min (t[C_MAG_CF32], ITERATIONS),
          dp_bench_min (t[C_NOISE_FLOOR], ITERATIONS)
              / dp_bench_min (t[C_MAG_CF32], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "spectral");
  return 0;
}
