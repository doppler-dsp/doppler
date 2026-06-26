/* bench_dll_core.c — the DLL early/prompt/late code-tracking loop.
 *
 *   steps — end-to-end throughput (MSa/s) of the per-sample E/P/L correlate
 *           + per-period discriminator + loop update over a 64k burst.
 */
#include "dll/dll_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200
#define SF 63
#define SPS 4

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  /* deterministic code + a carrier-free spread burst */
  uint8_t  code[SF];
  uint32_t st = 1u;
  for (int i = 0; i < SF; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      code[i] = (st & 1u);
    }
  float complex *rx  = malloc (BENCH_N * sizeof (*rx));
  float complex *out = malloc (BENCH_N * sizeof (*out));
  if (!rx || !out)
    return 1;
  double cph = 0.0;
  for (int k = 0; k < BENCH_N; k++)
    {
      size_t idx = (size_t)cph % SF;
      rx[k]      = (code[idx] & 1u) ? -1.0f : 1.0f;
      cph += 1.0 / SPS;
    }

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== dll benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  dll_state_t *d = dll_create (code, SF, SPS, 0.0, 0.005, 0.707, 0.5);
  dll_steps (d, rx, SF * SPS * 2, out, BENCH_N); /* warmup */

  double times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      dll_reset (d);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      dll_steps (d, rx, BENCH_N, out, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "steps", times, ITERATIONS, BENCH_N);
  double sum = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    sum += times[r];
  printf ("  steps    %8.1f MSa/s\n",
          (double)BENCH_N / (sum / ITERATIONS) / 1e6);

  jm_bench_write_json (&_bench, "dll");
  dll_destroy (d);
  free (rx);
  free (out);
  return 0;
}
