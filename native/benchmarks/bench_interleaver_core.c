/*
 * bench_interleaver_core.c — what a block interleave costs per bit.
 *
 * The scaffold jm materialised measured nothing ("no step() to benchmark")
 * and wrote an empty benchmarks[] JSON, which is the gh-806 shape:
 * `bench-coverage-check` counts the component as measured while the number
 * that would show a regression does not exist.
 *
 * Four arms, chosen so the answer says something a caller can act on:
 *
 *   bit-1x8k    unit_bits=1, one 8192-bit block. Bit interleaving, which is
 *               a byte move per bit -- the worst ratio of bookkeeping to
 *               payload this object has.
 *   octet-1x8k  unit_bits=8 over the same 8192 bits. Same payload, an eighth
 *               as many memcpy calls, each eight times longer. The pair is
 *               the point: it prices the UNIT, which is the parameter a
 *               caller actually chooses.
 *   deep        rows=256 over the same block. A deep interleaver strides
 *               further between writes, so this is where cache behaviour
 *               shows up rather than instruction count.
 *   soft        the float path over the same geometry as bit-1x8k, four
 *               bytes per unit instead of one -- what a receiver pays to
 *               de-interleave LLRs rather than hard decisions.
 *
 * Reported per BIT so the arms are comparable; the block sizes are equal by
 * construction, so the ratios are the whole reading.
 */
#include "doppler/interleaver/interleaver_core.h"
#include "jm_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BITS 8192
#define ITERATIONS 200

int
main (void)
{
  uint8_t *in = malloc (BITS), *out = malloc (BITS);
  float   *fin  = malloc (BITS * sizeof *fin);
  float   *fout = malloc (BITS * sizeof *fout);
  if (!in || !out || !fin || !fout)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  for (int i = 0; i < BITS; i++)
    {
      in[i]  = (uint8_t)(i & 1);
      fin[i] = (float)i;
    }

  jm_bench_t _bench = { 0 };
  double     times[ITERATIONS];
  uint64_t   t0, t1;
  double     _s;

  printf ("=== interleaver benchmark ===\n");
  printf ("block = %d bits, %d iterations\n\n", BITS, ITERATIONS);

#define ARM(label, setup, body)                                               \
  do                                                                          \
    {                                                                         \
      dp_interleaver_state_t *il = setup;                                     \
      if (!il)                                                                \
        {                                                                     \
          fprintf (stderr, "create failed for %s\n", label);                  \
          return 1;                                                           \
        }                                                                     \
      body; /* warm the buffers before timing */                              \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        {                                                                     \
          t0 = jm_bench_now_ns ();                                            \
          body;                                                               \
          t1       = jm_bench_now_ns ();                                      \
          times[r] = jm_bench_elapsed_sec (t0, t1);                           \
        }                                                                     \
      jm_bench_add (&_bench, label, times, ITERATIONS, BITS);                 \
      _s = 0.0;                                                               \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        _s += times[r];                                                       \
      printf ("  %-12s %8.2f Mbit/s\n", label,                                \
              (double)BITS / (_s / ITERATIONS) / 1e6);                        \
      dp_interleaver_destroy (il);                                            \
    }                                                                         \
  while (0)

  ARM ("bit-1x8k", dp_interleaver_create (8, BITS / 8, 1),
       dp_interleaver_interleave (il, in, BITS, out, BITS));
  ARM ("octet-1x8k", dp_interleaver_create (8, BITS / 64, 8),
       dp_interleaver_interleave (il, in, BITS, out, BITS));
  ARM ("deep", dp_interleaver_create (256, BITS / 256, 1),
       dp_interleaver_interleave (il, in, BITS, out, BITS));
  ARM ("soft", dp_interleaver_create (8, BITS / 8, 1),
       dp_interleaver_deinterleave_soft (il, fin, BITS, fout, BITS));

#undef ARM

  jm_bench_write_json (&_bench, "interleaver");
  free (in);
  free (out);
  free (fin);
  free (fout);
  return 0;
}
