/* bench_fft_core.c -- the N log N claim, and what the input format costs.
 *
 * Two questions, and neither is answerable from the signatures.
 *
 * **Does it scale the way the textbook says?** A radix-2 FFT is O(N log N),
 * so the cost per BIN should rise with log2(N) and nothing else -- 1.33x
 * from 4096 to 65536, four more butterfly stages over twelve. Real
 * transforms stop obeying that when the working set leaves cache, and
 * where they stop is a property of the machine that no one can derive
 * from the source. Four sizes spanning 256 to 65536 bins put the knee
 * somewhere readable.
 *
 * **What does the caller's sample format cost?** `fft` takes five: cf64,
 * cf32, in-place cf32, ci16 and ci8. The last two are what a radio
 * actually hands over -- interleaved fixed point straight off an ADC --
 * and they convert on the way in, so the transform they run is the cf32
 * one plus a scale-and-widen pass over N samples. Naively that pass is
 * O(N) against the transform's O(N log N) and should shrink as a share
 * of the total -- but the per-call overhead shrinks with N as well, and
 * faster, so the measured multiple moves the other way. The comparison
 * that means something is the one at the largest N, where the transform
 * is paying for itself; the small-N rows mostly say what a call costs
 * before it does any work.
 *
 * The in-place face is here for the opposite reason: it should cost the
 * same as `cf32` and exists only to save a buffer. A row that says
 * otherwise is a finding about the copy, not about the FFT.
 */
#include "dp_bench.h"
#include "fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define N_SIZE 4
#define FFT_FORWARD (-1)

static const size_t sizes[N_SIZE] = { 256, 4096, 16384, 65536 };

enum
{
  CFG_CF32,
  CFG_CF64,
  CFG_INPLACE,
  CFG_CI16,
  CFG_CI8,
  N_KIND
};

static const char *kind_name[N_KIND]
    = { "cf32", "cf64", "inplace_cf32", "ci16", "ci8" };

#define N_CFG (N_SIZE * N_KIND)

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  fft_state_t    *plan[N_SIZE] = { 0 };
  const size_t    n_max        = sizes[N_SIZE - 1];
  float complex  *in32 = NULL, *out32 = NULL;
  double complex *in64 = NULL, *out64 = NULL;
  int16_t        *in16 = NULL;
  int8_t         *in8  = NULL;
  char            name[64];

  in32  = malloc (n_max * sizeof *in32);
  out32 = malloc (n_max * sizeof *out32);
  in64  = malloc (n_max * sizeof *in64);
  out64 = malloc (n_max * sizeof *out64);
  in16  = malloc (2 * n_max * sizeof *in16);
  in8   = malloc (2 * n_max * sizeof *in8);
  if (!in32 || !out32 || !in64 || !out64 || !in16 || !in8)
    return 1;

  /* A two-tone input rather than a constant: the transform's cost does not
     depend on the data, but a denormal-producing input would, and zeros
     invite one. */
  for (size_t i = 0; i < n_max; i++)
    {
      const double re
          = cos (0.031 * (double)i) + 0.5 * cos (0.211 * (double)i);
      const double im
          = sin (0.031 * (double)i) + 0.5 * sin (0.211 * (double)i);
      in32[i]         = (float complex) (re + im * I);
      in64[i]         = re + im * I;
      in16[2 * i]     = (int16_t)(re * 8000.0);
      in16[2 * i + 1] = (int16_t)(im * 8000.0);
      in8[2 * i]      = (int8_t)(re * 40.0);
      in8[2 * i + 1]  = (int8_t)(im * 40.0);
    }

  for (int s = 0; s < N_SIZE; s++)
    {
      plan[s] = fft_create (sizes[s], FFT_FORWARD, 1);
      if (!plan[s])
        return 1;
    }

  printf ("=== fft (forward, single-threaded) ===\n");
  printf ("%d rounds, min over rounds\n\n", ITERATIONS);

  DP_BENCH_SETTLE (
      fft_execute_cf32 (plan[0], in32, sizes[0], out32, sizes[0]));

  /* Rounds outside, (size, format) inside: every number here is read
     against another one -- format against format at one size, size
     against size at one format -- so no configuration may own the ramp. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int s = 0; s < N_SIZE; s++)
      for (int k = 0; k < N_KIND; k++)
        {
          const size_t n = sizes[s];
          clock_gettime (CLOCK_MONOTONIC, &t0);
          switch (k)
            {
            case CFG_CF32:
              fft_execute_cf32 (plan[s], in32, n, out32, n);
              break;
            case CFG_CF64:
              fft_execute_cf64 (plan[s], in64, n, out64, n);
              break;
            case CFG_INPLACE:
              fft_execute_inplace_cf32 (plan[s], in32, n, out32, n);
              break;
            case CFG_CI16:
              fft_execute_ci16 (plan[s], in16, n, out32);
              break;
            case CFG_CI8:
              fft_execute_ci8 (plan[s], in8, n, out32);
              break;
            default:
              break;
            }
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t[s * N_KIND + k][r] = dp_bench_elapsed (&t0, &t1);
        }

  for (int s = 0; s < N_SIZE; s++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "execute_%s[n=%zu]", kind_name[k],
                        sizes[s]);
        dp_bench_record (&_bench, name, t[s * N_KIND + k], ITERATIONS,
                         sizes[s], "bin");
      }

  printf ("\n  per-bin cost against log2(N) -- flat means N log N holds:\n");
  for (int s = 0; s < N_SIZE; s++)
    {
      const double sec = dp_bench_min (t[s * N_KIND + CFG_CF32], ITERATIONS);
      printf ("    n=%-6zu  %6.3f ns/bin   %6.4f ns/bin/stage\n", sizes[s],
              sec / (double)sizes[s] * 1e9,
              sec / (double)sizes[s] / log2 ((double)sizes[s]) * 1e9);
    }

  printf ("\n  input format over cf32, at each size:\n");
  printf ("    %-8s", "n");
  for (int k = 1; k < N_KIND; k++)
    printf ("  %14s", kind_name[k]);
  printf ("\n");
  for (int s = 0; s < N_SIZE; s++)
    {
      printf ("    %-8zu", sizes[s]);
      for (int k = 1; k < N_KIND; k++)
        printf ("  %13.2fx",
                dp_bench_min (t[s * N_KIND + k], ITERATIONS)
                    / dp_bench_min (t[s * N_KIND + CFG_CF32], ITERATIONS));
      printf ("\n");
    }
  printf ("  Read these DOWN the column, not across one row. At small N\n"
          "  the per-call overhead is a large share of every face, which\n"
          "  compresses each multiple toward 1.00x and hides the format's\n"
          "  own cost; the bottom row is the one where the transform is\n"
          "  paying for itself and the difference is the format.\n");

  for (int s = 0; s < N_SIZE; s++)
    fft_destroy (plan[s]);
  free (in32);
  free (out32);
  free (in64);
  free (out64);
  free (in16);
  free (in8);
  jm_bench_write_json (&_bench, "fft");
  return 0;
}
