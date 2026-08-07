/*
 * bench_dp_tlm_capture_core.c — what losslessness costs at the boundary.
 *
 * The capture's claim is that being lossless is not merely safe but CHEAP:
 * one memcpy per block, nothing added to the emit path, and no background
 * thread racing the producer. bench_dp_tlm_core.c measures the emit path; this
 * measures the other half, the boundary itself.
 *
 * Three arms, all in memory mode so the number is the capture's own work and
 * not the filesystem's:
 *
 *   uncaptured  set_now with no capture open — the bare assignment every
 *               uninstrumented pipeline pays. The baseline the other two are
 *               only meaningful against.
 *   boundary    set_now with a capture open and the ring EMPTY. The cost of
 *               the delegation and the drain's bookkeeping when there is
 *               nothing to move: what a mostly-quiet probe set actually pays
 *               per block.
 *   drain       set_now with a capture open and the ring full to the block
 *               bound. The memcpy, per record, which is the number the design
 *               claims is the whole price of never dropping.
 *
 * jm scaffolds this file once and never rewrites it (the same
 * materialise-if-missing rule as _core.c), and what it scaffolds calls
 * `dp_tlm_capture_create` — a function that does not exist, because the
 * capture's constructor is `dp_tlm_capture_open` via `create_fn`. So this is
 * hand-owned by necessity, not by preference. See just-makeit#806 for the
 * general shape of that problem.
 */
#include "dp_tlm/dp_tlm_core.h"
#include "dp_tlm_capture/dp_tlm_capture_core.h"
#include "jm_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Deliberately small: a boundary is a per-BLOCK cost, so the interesting
   number is per boundary, and a realistic block is hundreds of samples, not
   millions. */
#define BLOCK 256
#define BLOCKS 4096
#define ITERATIONS 50

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  double          times[ITERATIONS];
  double          _s;

  printf ("=== dp_tlm_capture benchmark ===\n");
  printf ("block = %d samples,  %d blocks,  %d iterations\n\n", BLOCK, BLOCKS,
          ITERATIONS);

#define ARM(label, per_block)                                                 \
  do                                                                          \
    {                                                                         \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        {                                                                     \
          dp_tlm_t *t = dp_tlm_create (1 << 16);                              \
          if (!t)                                                             \
            {                                                                 \
              fprintf (stderr, "OOM\n");                                      \
              return 1;                                                       \
            }                                                                 \
          int id = dp_tlm_probe (t, "bench.x", 1);                            \
          (void)id;                                                           \
          setup;                                                              \
          clock_gettime (CLOCK_MONOTONIC, &t0);                               \
          for (int b = 0; b < BLOCKS; b++)                                    \
            {                                                                 \
              per_block;                                                      \
              dp_tlm_set_now (t, (uint64_t)b * BLOCK);                        \
            }                                                                 \
          clock_gettime (CLOCK_MONOTONIC, &t1);                               \
          times[r] = elapsed_sec (&t0, &t1);                                  \
          teardown;                                                           \
          dp_tlm_destroy (t);                                                 \
        }                                                                     \
      jm_bench_add (&_bench, label, times, ITERATIONS, BLOCKS);               \
      _s = 0.0;                                                               \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        _s += times[r];                                                       \
      printf ("  %-12s %10.3f us/boundary\n", label,                          \
              (_s / ITERATIONS) / BLOCKS * 1e6);                              \
    }                                                                         \
  while (0)

  {
#define setup (void)0
#define teardown (void)0
    ARM ("uncaptured", (void)0);
#undef setup
#undef teardown
  }

  {
    dp_tlm_capture_t *cap = NULL;
#define setup cap = dp_tlm_capture_open_memory (t, BLOCK, NULL)
#define teardown dp_tlm_capture_destroy (cap)
    ARM ("boundary", (void)0);
#undef setup
#undef teardown
  }

  {
    dp_tlm_capture_t *cap = NULL;
    /* Fill to the per-block bound so the drain has a full block to move --
       the arm that actually exercises the memcpy the design is priced on. */
#define setup cap = dp_tlm_capture_open_memory (t, BLOCK, NULL)
#define teardown dp_tlm_capture_destroy (cap)
    ARM ("drain",
         for (int i = 0; i < BLOCK; i++) dp_tlm_emit (t, 0, (double)i));
#undef setup
#undef teardown
  }

#undef ARM

  jm_bench_write_json (&_bench, "dp_tlm_capture");
  return 0;
}
