/* bench_async_dsss_pool_core.c -- what a pool costs per sample, and what
 * its workers buy back.
 *
 * The question the API does not answer: a pool of N receivers does N
 * receivers' work on every sample (every receiver searches the whole
 * range, docs/design/async-dsss-parallel.md §2), and push() spreads them
 * over its workers, one receiver per task. Does a pool of 3 on 3 workers
 * cost about what one receiver does, or does the hand-out and the join eat
 * the gain at a block this size?
 *
 * Predicted: serial ~3x one receiver's search
 * (bench_async_dsss_receiver_core's search row), and 3 workers back within
 * ~1.3x of one receiver, the controller's serial step being a few status
 * reads a block. Measured 2026-10-22 on swiftgo-ai9465 (`make bench
 * BENCH_ARGS="--c-only async_dsss_pool"`, then `make bench-rx`): one
 * receiver searches at 53.9 ns/sample; the pool of 3 at 112.1 serial and
 * 56.6 on 3 workers -- 1.05x one receiver, 1.98x over serial. The workers
 * half holds. The serial half does not: 2.08x one receiver, not 3x, from
 * two separate processes, and why is not measured. A first cut that fed
 * each push in one call per receiver measured 47.1 on 3 workers (2.42x);
 * feeding exactly one code period per call, which makes the pool's result
 * independent of the blocking (doppler#2548 review), costs a worker join
 * every 2,046 samples, and that is the difference.
 *
 * Both rows feed a pool of 3 at the specification point, the configuration the
 * (docs/design/async-dsss-receiver.md) receiver's own search row times, noise
 * alone from the shipped awgn, so every receiver searches all along; a row is
 * recorded only if every receiver searched for every sample fed (a false
 * alarm's refine would be timed as search). The two thread counts are
 * interleaved round by round, so the ratio is measured on one machine state.
 */
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/awgn/awgn_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 65536
#define LEAD 4
#define ITERATIONS 20
#define N_RX 3

static uint32_t
xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
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

/* Every receiver searching for every sample fed, or a message. The
   receivers have been fed the whole periods pushed: the pool carries the
   rest to the next push. */
static int
all_searching (dp_async_dsss_pool_state_t *p, uint64_t pushed, const char *tag)
{
  const uint64_t fed = pushed - p->carry_len;
  for (size_t i = 0; i < N_RX; i++)
    {
      async_dsss_receiver_status_t st = dp_async_dsss_pool_status (p, i);
      if (st.state != ASYNC_DSSS_RX_SEARCHING || st.state_samples != fed
          || st.refused_hits != 0)
        {
          (void)fprintf (stderr,
                         "bench_async_dsss_pool[%s]: receiver %zu state=%d "
                         "state_samples=%llu of %llu -- not a search all "
                         "along\n",
                         tag, i, st.state,
                         (unsigned long long)st.state_samples,
                         (unsigned long long)fed);
          return 0;
        }
    }
  return 1;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };
  /* A synthetic 1023-chip code: its exact chips do not matter for a cost
     benchmark (the receiver's bench makes the same choice). */
  static uint8_t code[1023];
  uint32_t       cs = 7;
  for (size_t c = 0; c < 1023; c++)
    code[c] = (uint8_t)(xorshift32 (&cs) & 1u);

  const double chip = 3.069e6, sym = 2700.0;
  const size_t spc  = 2;
  const double fs   = chip * (double)spc;
  const double span = fs / (double)(1023 * spc); /* 1/T_E */
  const double cn0  = 10.0 + 10.0 * log10 (sym);
  const size_t n    = (LEAD + ITERATIONS) * (size_t)BLOCK;

  dp_awgn_state_t *g = dp_awgn_create (
      7919u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs)), 1.0f));
  float _Complex             *x = malloc (n * sizeof *x);
  dp_async_dsss_pool_state_t *p[2];
  static const int            threads[2] = { 1, N_RX };
  for (int k = 0; k < 2; k++)
    p[k] = dp_async_dsss_pool_create (code, 1023, N_RX, chip, sym, spc, 2, cn0,
                                      1e-3, 0.9, span, 4, 8, 0, 0.5, 4, 14.0,
                                      64, 8, false, 100000, 0.0, 2.0, false,
                                      0.0, 0.0, threads[k]);
  if (!g || !x || !p[0] || !p[1])
    {
      (void)fprintf (stderr, "bench_async_dsss_pool: setup failed\n");
      return 1;
    }
  (void)dp_awgn_generate (g, n, x, n);

  printf ("=== async_dsss_pool benchmark: %d receivers at the specification "
          "point, "
          "searching ===\n",
          N_RX);
  printf ("block = %d samples, %d rounds; workers available: %d\n\n", BLOCK,
          ITERATIONS, dp_async_dsss_pool_get_threads (p[1]));

  size_t pos = 0;
  for (size_t b = 0; b < LEAD; b++, pos += BLOCK)
    for (int k = 0; k < 2; k++)
      (void)dp_async_dsss_pool_push (p[k], x + pos, BLOCK);
  double times[2][ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++, pos += BLOCK)
    for (int k = 0; k < 2; k++)
      {
        uint64_t t0 = jm_bench_now_ns ();
        (void)dp_async_dsss_pool_push (p[k], x + pos, BLOCK);
        uint64_t t1 = jm_bench_now_ns ();
        times[k][r] = jm_bench_elapsed_sec (t0, t1);
      }
  static const char *names[2]
      = { "push[search,n=3,threads=1]", "push[search,n=3,threads=3]" };
  double sec[2];
  for (int k = 0; k < 2; k++)
    {
      if (!all_searching (p[k], pos, names[k]))
        return 1;
      jm_bench_add_unit (&_bench, names[k], times[k], ITERATIONS, BLOCK,
                         JM_BENCH_SAMPLE);
      sec[k] = min_sec (times[k], ITERATIONS);
      printf ("  %-30s %8.3f ms/block  %7.2f ns/sample  %6.2fx real time\n",
              names[k], sec[k] * 1e3, sec[k] / (double)BLOCK * 1e9,
              (double)BLOCK / sec[k] / fs);
    }
  printf ("\n  3 workers over serial: %.2fx faster (%d workers ran)\n",
          sec[0] / sec[1], dp_async_dsss_pool_get_threads (p[1]));

  jm_bench_write_json (&_bench, "async_dsss_pool");
  for (int k = 0; k < 2; k++)
    dp_async_dsss_pool_destroy (p[k]);
  dp_awgn_destroy (g);
  free (x);
  return 0;
}
