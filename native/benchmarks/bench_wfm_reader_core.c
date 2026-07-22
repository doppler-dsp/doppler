/* bench_wfm_reader_core.c — container read throughput (MSa/s) per format.
 *
 * The dual of bench_wfm_writer. Each config is written once to a temp file
 * (page-cache-warm), then the timed loop opens + drains it, so the numbers
 * reflect the dequantise + byte-order + parse cost, not disk seeks. Covers the
 * cheap cf32 path, the integer rescale (ci16), BLUE (header parse + raw), and
 * the text path (CSV). Emits pytest-benchmark JSON. */
#define _POSIX_C_SOURCE 200809L

#include "jm_bench.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

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
bench_cfg (const char *name, const char *path, int ft, int stype,
           const float _Complex *x, float _Complex *out, jm_bench_t *bench)
{
  /* Write the capture once (kept warm in the page cache). */
  FILE               *fp = fopen (path, "wb");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, ft, stype, 0, 1e6, 0.0, BENCH_N);
  wfm_writer_write (w, x, BENCH_N);
  wfm_writer_close (w);
  fclose (fp);

  struct timespec t0, t1;
  double          times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      wfm_reader_state_t *rd    = wfm_reader_create (path, stype, 0);
      size_t              total = 0, n;
      while ((n = wfm_reader_read (rd, out + total, BENCH_N - total)) > 0)
        total += n;
      wfm_reader_destroy (rd);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  printf ("  %-26s %8.1f MSa/s\n", name,
          BENCH_N / (times[0] > 0 ? times[0] : 1e-9) / 1e6);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);
  remove (path);
}

int
main (void)
{
  float _Complex *x   = malloc (BENCH_N * sizeof *x);
  float _Complex *out = malloc (BENCH_N * sizeof *out);
  if (!x || !out)
    return 1;
  for (int i = 0; i < BENCH_N; i++)
    x[i] = (float)(0.9 * (i % 100) / 100.0 - 0.45)
           + (float)(0.7 * (i % 64) / 64.0 - 0.35) * I;

  jm_bench_t bench = { 0 };
  printf ("=== wfm_reader benchmark ===\n");
  printf ("block = %d samples, %d iterations (page-cache-warm temp file)\n\n",
          BENCH_N, ITERATIONS);

  bench_cfg ("raw cf32 (reinterpret)", "/tmp/dp_bench.cf32", WFM_FT_RAW, 0, x,
             out, &bench);
  bench_cfg ("raw ci16 (rescale)", "/tmp/dp_bench.ci16", WFM_FT_RAW, 3, x, out,
             &bench);
  bench_cfg ("blue cf32 (parse+raw)", "/tmp/dp_bench.blue", WFM_FT_BLUE, 0, x,
             out, &bench);
  bench_cfg ("csv cf32 (text parse)", "/tmp/dp_bench.csv", WFM_FT_CSV, 0, x,
             out, &bench);

  jm_bench_write_json (&bench, "wfm_reader");
  free (x);
  free (out);
  return 0;
}
