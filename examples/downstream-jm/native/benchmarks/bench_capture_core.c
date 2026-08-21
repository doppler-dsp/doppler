/* bench_capture_core.c — how fast iqtools reads a capture.
 *
 * Measures the two calls a caller makes: `capture_read`, which decodes a
 * block of samples, and `capture_summary`, which returns the file's metadata.
 *
 * The steps below are the shape of any benchmark in a just-makeit project:
 * build a fixture, warm up, time each call over several rounds, record the
 * timings, write the JSON that `make bench` collects.
 */
#include "capture/capture_core.h"
#include "jm_bench.h"
#include "wfm_writer/wfm_writer_core.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_SAMPLES 65536
#define BLOCK 4096
#define ITERATIONS 200
#define WARMUP_SEC 0.25
#define FS 2400000.0
#define FC 1200000000.0
#define ST_CI16 3
#define ENDIAN_LE 0
#define FIXTURE "bench_capture_fixture.blue"

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* Report the FASTEST round. Interruptions can only add time, so the minimum
   is the closest estimate of what the code itself costs. */
static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int i = 1; i < n; i++)
    if (t[i] < m)
      m = t[i];
  return m;
}

/* Step 1 — write a capture to read back, using doppler's own writer, so the
   fixture comes from the same code path a real capture would. */
static int
write_fixture (void)
{
  float complex      *buf = malloc (NUM_SAMPLES * sizeof *buf);
  wfm_writer_state_t *w;
  int                 rc;

  if (!buf)
    return -1;
  for (size_t i = 0; i < NUM_SAMPLES; i++)
    buf[i] = (float)(i % 64) / 128.0f + I * (float)(i % 32) / 128.0f;

  w = wfm_writer_create (FIXTURE, FS, (int)WFM_FT_BLUE, ST_CI16, ENDIAN_LE, FC,
                         0, 0.0, 0.0, false);
  if (w == NULL)
    {
      free (buf);
      return -1;
    }
  rc = (wfm_writer_write (w, buf, NUM_SAMPLES) == NUM_SAMPLES) ? 0 : -1;
  if (wfm_writer_close (w) != 0)
    rc = -1;
  free (buf);
  return rc;
}

int
main (void)
{
  capture_state_t *obj;
  float complex   *out;
  struct timespec  t0, t1;
  jm_bench_t       _bench = { 0 };
  static double    t_read[ITERATIONS], t_sum[ITERATIONS];
  volatile double  sink      = 0.0;
  size_t           per_round = 0;

  if (write_fixture () != 0)
    {
      fprintf (stderr, "bench_capture: could not write %s\n", FIXTURE);
      return 1;
    }

  /* Step 2 — open the capture and allocate one output block. */
  obj = capture_create (FIXTURE);
  if (obj == NULL)
    {
      fprintf (stderr, "bench_capture: could not open %s\n", FIXTURE);
      return 1;
    }
  out = malloc (BLOCK * sizeof *out);
  if (out == NULL)
    return 1;

  printf ("=== capture benchmark ===\n");
  printf ("%zu samples, %d-sample blocks, %d rounds\n\n",
          capture_get_num_samples (obj), BLOCK, ITERATIONS);

  /* Step 3 — warm up, so the first timed round does not pay for the CPU
     ramping up to speed. */
  {
    struct timespec w0, w1;
    clock_gettime (CLOCK_MONOTONIC, &w0);
    do
      {
        capture_reset (obj);
        sink += (double)capture_read (obj, BLOCK, out, BLOCK);
        clock_gettime (CLOCK_MONOTONIC, &w1);
      }
    while (elapsed_sec (&w0, &w1) < WARMUP_SEC);
  }

  /* Step 4 — time both calls, once per round. `capture_reset` rewinds to the
     start of the file so every round reads the same samples. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      size_t got = 0, n;

      capture_reset (obj);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      while ((n = capture_read (obj, BLOCK, out, BLOCK)) > 0)
        got += n;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_read[r] = elapsed_sec (&t0, &t1);
      per_round = got;

      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += capture_summary (obj).fs_hz;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_sum[r] = elapsed_sec (&t0, &t1);
    }

  /* Step 5 — record each entry. The last argument is the unit of work per
     round, so the JSON reports samples per second for `read` and calls per
     second for `summary`. */
  jm_bench_add (&_bench, "read", t_read, ITERATIONS, (int)per_round);
  jm_bench_add (&_bench, "summary", t_sum, ITERATIONS, 1);

  {
    const double s = min_sec (t_read, ITERATIONS);
    printf ("  %-10s %8.2f ns/sample  %9.2f MSa/s\n", "read",
            s / (double)per_round * 1e9, (double)per_round / s / 1e6);
    printf ("  %-10s %8.2f ns/call\n", "summary",
            min_sec (t_sum, ITERATIONS) * 1e9);
  }

  /* Step 6 — write bench_capture_core.json, which `make bench` collects. */
  (void)sink;
  jm_bench_write_json (&_bench, "capture");

  free (out);
  capture_destroy (obj);
  remove (FIXTURE);
  return 0;
}
