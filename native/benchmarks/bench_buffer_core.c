/* bench_buffer_core.c -- what the double mapping is worth.
 *
 * `DECLARE_DP_BUFFER` builds a ring whose backing pages are mapped TWICE,
 * back to back, so the region after the last sample is the region before
 * the first one. The payoff is stated in the header and never measured:
 * a batch that straddles the wrap is still contiguous, so `write` is one
 * `memcpy` regardless of where the head sits, and `wait` hands back a
 * pointer a SIMD loop can run straight off without a copy or a split.
 *
 * The ordinary ring cannot do either. It splits a straddling batch into
 * two memcpys, and a consumer wanting one contiguous run has to copy into
 * scratch. So the claim being made here is not "this is fast" -- it is
 * "the position of the head does not matter", and that is a claim a
 * benchmark can actually settle.
 *
 * The test is two chunk sizes against one power-of-two capacity:
 *
 *   chunk 1024 divides the capacity, so every batch starts at the same
 *   handful of offsets and NO batch ever straddles the wrap.
 *   chunk 1000 does not, so the start offset walks and most batches do.
 *
 * Same bytes moved, same arithmetic, same number of calls per sample. If
 * the two rows agree, the mapping is doing what it says. A gap would mean
 * straddling costs something after all -- a page boundary, a TLB entry,
 * or a memcpy that is not taking the fast path it looks like it takes.
 *
 * Both element widths are here because the buffer is a macro instantiated
 * per type, and f64 moves twice the bytes for the same sample count: it
 * is the row that says whether the cost is per sample or per byte.
 *
 * NOT measured here, and worth saying so: contention. This is one thread
 * alternating producer and consumer, which is the sequencing the acquire /
 * release pairs are written for but not the case they exist for. A real
 * two-thread throughput number needs a different harness and belongs with
 * whoever adds one.
 */
#include "buffer/buffer.h"
#include "dp_bench.h"
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 200
#define CAPACITY 8192 /* complex samples; power of two, fits in L2 */
#define TOTAL 262144  /* samples pushed through per timed round    */

#define N_CHUNK 2
/* 1024 divides CAPACITY (never straddles); 1000 does not (usually does). */
static const size_t chunks[N_CHUNK] = { 1024, 1000 };
#define REF_IDX 0

enum
{
  KIND_F32,
  KIND_F64,
  N_KIND
};

static const char *kind_name[N_KIND] = { "f32", "f64" };

#define N_CFG (N_CHUNK * N_KIND)

/* Read the batch so the contiguity of the returned pointer is load-bearing
   rather than decorative. The sum is returned to keep it alive. */
#define DRAIN(name, type, buf, chunk, acc)                                    \
  do                                                                          \
    {                                                                         \
      const type *_p = dp_##name##_wait ((buf), (chunk));                     \
      /* wait() may now return NULL -- end of stream, or interrupted. This    \
         bench closes neither, so it cannot happen here; the guard is what    \
         keeps that a fact rather than an assumption. */                      \
      if (!_p)                                                                \
        break;                                                                \
      for (size_t _k = 0; _k < (chunk) * 2; _k++)                             \
        (acc) += (double)_p[_k];                                              \
      dp_##name##_consume ((buf), (chunk));                                   \
    }                                                                         \
  while (0)

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  dp_f32_t       *b32   = dp_f32_create (CAPACITY);
  dp_f64_t       *b64   = dp_f64_create (CAPACITY);
  float          *src32 = NULL;
  double         *src64 = NULL;
  double          acc   = 0.0;
  char            name[72];

  if (!b32 || !b64)
    {
      fprintf (stderr, "buffer create failed\n");
      return 1;
    }

  src32 = malloc ((size_t)2 * chunks[0] * sizeof *src32);
  src64 = malloc ((size_t)2 * chunks[0] * sizeof *src64);
  if (!src32 || !src64)
    return 1;
  for (size_t i = 0; i < 2 * chunks[0]; i++)
    {
      src32[i] = (float)(i & 0xff);
      src64[i] = (double)(i & 0xff);
    }

  printf ("=== buffer (VM double-mapped ring, capacity %d samples) ===\n",
          CAPACITY);
  printf ("%d samples per round, %d rounds, min over rounds\n\n", TOTAL,
          ITERATIONS);

  DP_BENCH_SETTLE ({
    (void)dp_f32_write (b32, src32, chunks[0]);
    DRAIN (f32, float, b32, chunks[0], acc);
  });

  /* Rounds outside, configurations inside. The whole result is the
     straddling row read against the aligned one, so a thermal step must
     not land on one of them alone. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CHUNK; c++)
      {
        const size_t chunk = chunks[c];

        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (size_t done = 0; done < TOTAL; done += chunk)
          {
            (void)dp_f32_write (b32, src32, chunk);
            DRAIN (f32, float, b32, chunk, acc);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c * N_KIND + KIND_F32][r] = dp_bench_elapsed (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (size_t done = 0; done < TOTAL; done += chunk)
          {
            (void)dp_f64_write (b64, src64, chunk);
            DRAIN (f64, double, b64, chunk, acc);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c * N_KIND + KIND_F64][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CHUNK; c++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "write_wait_consume[%s,chunk=%zu]",
                        kind_name[k], chunks[c]);
        dp_bench_record (&_bench, name, t[c * N_KIND + k], ITERATIONS, TOTAL,
                         "sample");
      }

  printf ("\n  straddling the wrap, against never straddling it:\n");
  for (int k = 0; k < N_KIND; k++)
    printf ("    %-4s chunk=%zu over chunk=%zu   %.2fx\n", kind_name[k],
            chunks[1], chunks[REF_IDX],
            dp_bench_min (t[1 * N_KIND + k], ITERATIONS)
                / dp_bench_min (t[REF_IDX * N_KIND + k], ITERATIONS));
  printf ("  1.00x is the double mapping doing its job: the head's\n"
          "  position does not change what a batch costs, which is the\n"
          "  entire reason the pages are mapped twice. Anything else is\n"
          "  a copy or a split that the contiguity was supposed to remove.\n");

  printf ("\n  (checksum %.0f -- keeps the drain from being optimised out)\n",
          acc);

  free (src32);
  free (src64);
  dp_f32_destroy (b32);
  dp_f64_destroy (b64);
  jm_bench_write_json (&_bench, "buffer");
  return 0;
}
