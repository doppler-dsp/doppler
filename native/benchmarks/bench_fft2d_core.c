/* bench_fft2d_core.c -- a 2-D transform is not priced by its bin count.
 *
 * `fft2d_create(ny, nx, ...)` takes two dimensions and the caller usually
 * has some freedom in how to split a given number of bins between them --
 * a range-Doppler map, an ambiguity surface and a spectrogram are all
 * "ny by nx" for a product the application fixes and a shape it does not.
 * Nothing in the signature says the shape matters.
 *
 * It does, because a separable 2-D FFT is a pass of `ny` row transforms
 * followed by a pass of `nx` column transforms, and only one of those
 * passes walks memory contiguously. The column pass strides by `nx`
 * complex samples per element, so a tall, narrow array and a short, wide
 * one of the SAME bin count meet the cache very differently. Four shapes
 * at a constant 65536 bins -- 64x1024, 256x256, 1024x64 and the extreme
 * 16x4096 -- turn that into a number, and the row to compare them against
 * is the square one.
 *
 * The second axis is the same format question `bench_fft_core.c` asks, cut
 * down to the two faces this component offers: cf64 against cf32, and the
 * in-place cf32 that exists to save a buffer rather than time.
 *
 * A caller who reads this and picks the cheap shape has done real work; a
 * caller whose shape is fixed by the physics at least knows what it costs.
 */
#include "dp_bench.h"
#include "fft2d/fft2d_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define BINS 65536
#define N_SHAPE 4
#define FFT_FORWARD (-1)

/* Constant bin count, four aspect ratios. The square one is the baseline
   the others are read against. */
static const size_t shape_ny[N_SHAPE] = { 16, 64, 256, 1024 };
static const size_t shape_nx[N_SHAPE] = { 4096, 1024, 256, 64 };
#define SQUARE_IDX 2

enum
{
  CFG_CF32,
  CFG_CF64,
  CFG_INPLACE,
  N_KIND
};

static const char *kind_name[N_KIND] = { "cf32", "cf64", "inplace_cf32" };

#define N_CFG (N_SHAPE * N_KIND)

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  fft2d_state_t  *plan[N_SHAPE] = { 0 };
  float complex  *in32 = NULL, *out32 = NULL;
  double complex *in64 = NULL, *out64 = NULL;
  char            name[64];

  in32  = malloc (BINS * sizeof *in32);
  out32 = malloc (BINS * sizeof *out32);
  in64  = malloc (BINS * sizeof *in64);
  out64 = malloc (BINS * sizeof *out64);
  if (!in32 || !out32 || !in64 || !out64)
    return 1;

  for (size_t i = 0; i < BINS; i++)
    {
      const double re = cos (0.031 * (double)i);
      const double im = sin (0.017 * (double)i);
      in32[i]         = (float complex) (re + im * I);
      in64[i]         = re + im * I;
    }

  for (int s = 0; s < N_SHAPE; s++)
    {
      plan[s] = fft2d_create (shape_ny[s], shape_nx[s], FFT_FORWARD, 1);
      if (!plan[s])
        return 1;
    }

  printf ("=== fft2d (forward, single-threaded, %d bins throughout) ===\n",
          BINS);
  printf ("%d rounds, min over rounds\n\n", ITERATIONS);

  DP_BENCH_SETTLE (
      fft2d_execute_cf32 (plan[SQUARE_IDX], in32, BINS, out32, BINS));

  /* Rounds outside, (shape, format) inside. The whole point of the file is
     that four shapes of one bin count differ, so the four must be measured
     against each other rather than one after the other. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int s = 0; s < N_SHAPE; s++)
      for (int k = 0; k < N_KIND; k++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          switch (k)
            {
            case CFG_CF32:
              fft2d_execute_cf32 (plan[s], in32, BINS, out32, BINS);
              break;
            case CFG_CF64:
              fft2d_execute_cf64 (plan[s], in64, BINS, out64, BINS);
              break;
            case CFG_INPLACE:
              fft2d_execute_inplace_cf32 (plan[s], in32, BINS, out32, BINS);
              break;
            default:
              break;
            }
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t[s * N_KIND + k][r] = dp_bench_elapsed (&t0, &t1);
        }

  for (int s = 0; s < N_SHAPE; s++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "execute_%s[%zux%zu]", kind_name[k],
                        shape_ny[s], shape_nx[s]);
        dp_bench_record (&_bench, name, t[s * N_KIND + k], ITERATIONS, BINS,
                         "bin");
      }

  printf ("\n  shape cost at a constant %d bins (%zux%zu = 1.00x):\n", BINS,
          shape_ny[SQUARE_IDX], shape_nx[SQUARE_IDX]);
  for (int s = 0; s < N_SHAPE; s++)
    printf ("    %4zu x %-5zu  %.2fx   %6.3f ns/bin\n", shape_ny[s],
            shape_nx[s],
            dp_bench_min (t[s * N_KIND + CFG_CF32], ITERATIONS)
                / dp_bench_min (t[SQUARE_IDX * N_KIND + CFG_CF32], ITERATIONS),
            dp_bench_min (t[s * N_KIND + CFG_CF32], ITERATIONS) / (double)BINS
                * 1e9);
  printf ("  Same bin count, same arithmetic, one strided pass. Whatever\n"
          "  spread is here is the memory system answering, which is why\n"
          "  a caller free to choose the split should choose it here and\n"
          "  not from the bin count. Check whether the spread is SYMMETRIC\n"
          "  in the two elongations: if tall and wide cost the same, the\n"
          "  penalty is not the column stride on its own -- it is how well\n"
          "  either pass blocks when the other dimension is small.\n");

  for (int s = 0; s < N_SHAPE; s++)
    fft2d_destroy (plan[s]);
  free (in32);
  free (out32);
  free (in64);
  free (out64);
  jm_bench_write_json (&_bench, "fft2d");
  return 0;
}
