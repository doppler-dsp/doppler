/* bench_wfm_reader_core.c — file type read throughput (MSa/s) per format.
 *
 * The dual of bench_wfm_writer. Each config is written once to a temp file
 * (page-cache-warm), then the timed loop opens + drains it, so the numbers
 * reflect the dequantise + byte-order + parse cost, not disk seeks. Covers the
 * cheap cf32 path, the integer rescale (ci16), BLUE (header parse + raw), and
 * the text path (CSV), then the random-access asymmetry: a strided seek is
 * constant time, a CSV seek scans. Emits pytest-benchmark JSON. */
#define _POSIX_C_SOURCE 200809L

#include "jm_bench.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

#include "dp_complex.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100

static void
bench_cfg (const char *name, const char *path, int ft, int stype,
           const float _Complex *x, float _Complex *out, jm_bench_t *bench)
{
  /* Write the capture once (kept warm in the page cache). */
  FILE               *fp = fopen (path, "wb");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, ft, stype, 0, 1e6, 0.0, BENCH_N, 0.0);
  wfm_writer_write (w, x, BENCH_N);
  wfm_writer_close (w);
  fclose (fp);

  uint64_t t0, t1;
  double   times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0                        = jm_bench_now_ns ();
      wfm_reader_state_t *rd    = wfm_reader_create (path, stype, 0);
      size_t              total = 0, n;
      while ((n = wfm_reader_read (rd, BENCH_N - total, out + total,
                                   BENCH_N - total))
             > 0)
        total += n;
      wfm_reader_destroy (rd);
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  printf ("  %-26s %8.1f MSa/s\n", name,
          BENCH_N / (times[0] > 0 ? times[0] : 1e-9) / 1e6);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);
  remove (path);
}

/* Random access, and the asymmetry docs/design/capture-files.md section 6
   claims: a strided container seeks in constant time, a CSV scans because it
   is delimited and has no offset to compute.

   Alternating between the midpoint and the start is what stops the CSV's
   forward-only scan from amortising itself away -- each PAIR pays one full
   walk to the midpoint, which is the cost a caller actually meets. `batch` is
   per container for that reason: a scan is four orders slower, so timing the
   same count of them would take minutes to say what ten already say. */
static void
bench_seek (const char *name, const char *path, int ft, int stype,
            size_t batch, const float _Complex *x, jm_bench_t *bench)
{
  FILE               *fp = fopen (path, "wb");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, ft, stype, 0, 1e6, 0.0, BENCH_N, 0.0);
  wfm_writer_write (w, x, BENCH_N);
  wfm_writer_close (w);
  fclose (fp);

  wfm_reader_state_t *rd = wfm_reader_create (path, stype, 0);
  uint64_t            t0, t1;
  double              times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (size_t i = 0; i < batch; i++)
        {
          wfm_reader_seek (rd, (int64_t)(BENCH_N / 2));
          wfm_reader_seek (rd, 0);
        }
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  wfm_reader_destroy (rd);
  printf ("  %-26s %8.1f kseek/s\n", name,
          (double)(2 * batch) / (times[0] > 0 ? times[0] : 1e-9) / 1e3);
  jm_bench_add (bench, name, times, ITERATIONS, 2 * batch);
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

  printf ("\n");
  bench_seek ("blue seek (strided)", "/tmp/dp_bench_sk.blue", WFM_FT_BLUE, 0,
              1000, x, &bench);
  bench_seek ("csv seek (scan)", "/tmp/dp_bench_sk.csv", WFM_FT_CSV, 0, 10, x,
              &bench);

  jm_bench_write_json (&bench, "wfm_reader");
  free (x);
  free (out);
  return 0;
}
