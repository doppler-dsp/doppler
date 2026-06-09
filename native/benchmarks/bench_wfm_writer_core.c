/* bench_wfm_writer_core.c — container write throughput (MSa/s) per format.
 *
 * Isolates the codec (quantise + byte order + framing) from disk by writing to
 * an in-memory stream (open_memstream), so the numbers reflect conversion
 * cost, not the filesystem. Covers the cheap path (cf32 = memcpy), the integer
 * quantiser (ci16), and the text path (CSV). Emits pytest-benchmark JSON. */
#define _POSIX_C_SOURCE 200809L

#include "jm_bench.h"
#include "wfmgen/wfm_writer.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static void
bench_cfg (const char *name, int ft, int stype, const float _Complex *x,
           jm_bench_t *bench)
{
  struct timespec t0, t1;
  double          times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      char  *buf = NULL;
      size_t len = 0;
      FILE  *fp  = open_memstream (&buf, &len);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      wfm_writer_t *w = wfm_writer_open (fp, ft, stype, 0, 1e6, 0.0, BENCH_N);
      wfm_writer_write (w, x, BENCH_N);
      wfm_writer_close (w);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      fclose (fp);
      free (buf);
      times[r] = elapsed_sec (&t0, &t1);
    }
  printf ("  %-26s %8.1f MSa/s\n", name,
          BENCH_N / (times[0] > 0 ? times[0] : 1e-9) / 1e6);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);
}

int
main (void)
{
  float _Complex *x = malloc (BENCH_N * sizeof *x);
  if (!x)
    return 1;
  for (int i = 0; i < BENCH_N; i++)
    x[i] = (float)(0.9 * (i % 100) / 100.0 - 0.45)
           + (float)(0.7 * (i % 64) / 64.0 - 0.35) * I;

  jm_bench_t bench = { 0 };
  printf ("=== wfm_writer benchmark ===\n");
  printf ("block = %d samples, %d iterations (in-memory stream)\n\n", BENCH_N,
          ITERATIONS);

  bench_cfg ("raw cf32 (memcpy)", WFM_FT_RAW, 0, x, &bench);
  bench_cfg ("raw ci16 (quantise)", WFM_FT_RAW, 3, x, &bench);
  bench_cfg ("raw ci8 (quantise)", WFM_FT_RAW, 4, x, &bench);
  bench_cfg ("blue cf32", WFM_FT_BLUE, 0, x, &bench);
  bench_cfg ("csv cf32 (text)", WFM_FT_CSV, 0, x, &bench);

  jm_bench_write_json (&bench, "wfm_writer");
  free (x);
  return 0;
}
