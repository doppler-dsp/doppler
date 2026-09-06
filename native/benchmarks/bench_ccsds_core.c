/* bench_ccsds_core.c — the standard's literals, on the Python-facing side.
 *
 * `asm_bits` is 32 shifts, so the question is not "how fast is it" — it is
 * whether the ALIAS costs anything over the expansion it delegates to. This
 * face exists so `doppler.ccsds.asm_bits()` reaches `ccsds_tm_asm_bits`
 * without Python transcribing 0x1ACFFC1D a third time (doppler#900,
 * doppler#1220), and an alias that showed up in a profile would be an
 * argument for a constant instead.
 *
 * So both rows are measured against each other:
 *
 *   asm_bits            the module face, one call deep
 *   ccsds_tm_asm_bits   the expansion itself
 *
 * A `volatile` sink prevents the loops being optimised away.
 */
#include "ccsds/ccsds_core.h"
#include "ccsds_tm/ccsds_tm.h"
#include "jm_bench.h"
#include <stdio.h>
#include <time.h>

/* The marker is 32 bits and that is not a tunable, so the block is a call
 * count rather than a sample count: one "sample" is one full expansion. */
#define BENCH_N 65536
#define ITERATIONS 200

static volatile uint32_t sink;

static double
elapsed_sec (const struct timespec *t0, const struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t_face[ITERATIONS], t_raw[ITERATIONS];
  uint8_t         bits[CCSDS_TM_ASM_BITS];

  printf ("=== ccsds benchmark ===\n");
  printf ("block = %d expansions,  %d iterations\n\n", BENCH_N, ITERATIONS);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        {
          asm_bits (bits);
          sink += bits[i & 31];
        }
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_face[r] = elapsed_sec (&t0, &t1);

      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        {
          ccsds_tm_asm_bits (bits);
          sink += bits[i & 31];
        }
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_raw[r] = elapsed_sec (&t0, &t1);
    }

  jm_bench_add (&_bench, "asm_bits", t_face, ITERATIONS, BENCH_N);
  jm_bench_add (&_bench, "ccsds_tm_asm_bits", t_raw, ITERATIONS, BENCH_N);

  jm_bench_write_json (&_bench, "ccsds");
  return 0;
}
