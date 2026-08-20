/* bench_interp_table_core.c — table lookup, three interpolation methods.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * The method is a constructor argument, and the three differ in how many
 * table entries each output touches: nearest reads one, linear two, cubic
 * four. That is the whole measurement -- whether the cost tracks the entry
 * count (arithmetic-bound) or flattens (lookup-bound), because a caller
 * choosing accuracy is implicitly choosing one of those two regimes.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "interp_table/interp_table_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define TABLE_N 1024
#define ITERATIONS 100

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  double complex *table = malloc (TABLE_N * sizeof *table);
  double         *in    = malloc (BENCH_N * sizeof *in);
  double complex *out   = malloc (BENCH_N * sizeof *out);
  if (!table || !in || !out)
    return 1;

  for (int i = 0; i < TABLE_N; i++)
    table[i] = cos (i * 0.01) + sin (i * 0.017) * I;
  /* Indices spread across the table rather than a ramp: a monotone sweep
     would keep every lookup in the same cache line and measure the L1
     hit, not the interpolation. */
  uint32_t lfsr = 0x4D2Bu;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr  = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      in[i] = (double)(lfsr % (TABLE_N - 4)) + 0.37;
    }

  printf ("=== interp_table benchmark ===\n");
  printf ("table = %d entries, block = %d lookups, %d rounds\n\n", TABLE_N,
          BENCH_N, ITERATIONS);

  const char *const mname[3]
      = { "execute[nearest]", "execute[linear]", "execute[cubic]" };
  static double t_ex[3][ITERATIONS];

  for (int m = 0; m < 3; m++)
    {
      interp_table_state_t *s = interp_table_create (table, TABLE_N, m);
      if (!s)
        {
          (void)fprintf (stderr,
                         "bench_interp_table: create(method=%d) "
                         "returned NULL\n",
                         m);
          return 1;
        }
      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += interp_table_execute (s, in, BENCH_N, out, BENCH_N);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_ex[m][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, mname[m], t_ex[m], ITERATIONS, BENCH_N);
      double s2 = min_sec (t_ex[m], ITERATIONS);
      printf ("  %-20s %7.2f ns/sample  %8.1f MSa/s\n", mname[m],
              s2 / (double)BENCH_N * 1e9, (double)BENCH_N / s2 / 1e6);
      interp_table_destroy (s);
    }

  printf ("\n  cubic/nearest = %.2fx over 4x the table reads, linear/nearest\n"
          "  = %.2fx over 2x. A ratio well under the read count means the\n"
          "  lookup, not the arithmetic, sets the cost -- so buying accuracy\n"
          "  here is cheaper than the operation count suggests.\n",
          min_sec (t_ex[2], ITERATIONS) / min_sec (t_ex[0], ITERATIONS),
          min_sec (t_ex[1], ITERATIONS) / min_sec (t_ex[0], ITERATIONS));

  (void)sink;
  free (table);
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "interp_table");
  return 0;
}
