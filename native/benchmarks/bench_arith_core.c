/* bench_arith_core.c -- the saturating fixed-point vector kernels.
 *
 * These exist so a fixed-point path can be cheaper than a float one, and
 * the thing that could make them NOT cheaper is on every element: each
 * kernel widens to the next integer size, tests the result against the
 * narrow range, and clamps. That is a compare and a select per lane where
 * a wrapping add would have none. So the question this file answers is
 * **what does saturation cost per element**, and the second one is
 * whether Q8 -- twice the lanes in a machine word -- actually converts
 * that into throughput over Q15, or whether the clamp eats the win.
 *
 * `shl_i64` / `shr_i64` are the third row on purpose: they saturate
 * against a range no accumulator overflows in practice, so they are the
 * closest thing here to the cost of the loop with the clamp taken out.
 *
 * The block is 8 KiB per array, chosen to sit in L1/L2 rather than to be
 * realistic -- a kernel measured out of DRAM reports the memory system,
 * and the memory system is the same for all fourteen entries.
 */
#include "arith/arith_core.h"
#include "dp_bench.h"
#include <stdint.h>
#include <stdio.h>

#define BLOCK 8192
#define REPS 16
#define ITERATIONS 200

enum
{
  OP_ADD_Q15,
  OP_SUB_Q15,
  OP_MUL_Q15,
  OP_DOT_Q15,
  OP_SHL_Q15,
  OP_SHR_Q15,
  OP_ADD_Q8,
  OP_SUB_Q8,
  OP_MUL_Q8,
  OP_DOT_Q8,
  OP_SHL_Q8,
  OP_SHR_Q8,
  OP_SHL_I64,
  OP_SHR_I64,
  N_OPS
};

static const char *const op_name[N_OPS] = {
  "add_q15", "sub_q15", "mul_q15", "dot_q15", "shl_q15", "shr_q15", "add_q8",
  "sub_q8",  "mul_q8",  "dot_q8",  "shl_q8",  "shr_q8",  "shl_i64", "shr_i64",
};

static int16_t a15[BLOCK], b15[BLOCK], o15[BLOCK];
static int8_t  a8[BLOCK], b8[BLOCK], o8[BLOCK];
static int64_t a64[BLOCK], o64[BLOCK];

static volatile int64_t sink = 0;

/* One call of one kernel over the whole block. */
static void
run (int op)
{
  switch (op)
    {
    case OP_ADD_Q15:
      add_q15 (a15, BLOCK, b15, BLOCK, o15);
      break;
    case OP_SUB_Q15:
      sub_q15 (a15, BLOCK, b15, BLOCK, o15);
      break;
    case OP_MUL_Q15:
      mul_q15 (a15, BLOCK, b15, BLOCK, o15);
      break;
    case OP_DOT_Q15:
      sink += dot_q15 (a15, BLOCK, b15, BLOCK);
      break;
    case OP_SHL_Q15:
      shl_q15 (a15, BLOCK, o15, 2);
      break;
    case OP_SHR_Q15:
      shr_q15 (a15, BLOCK, o15, 2);
      break;
    case OP_ADD_Q8:
      add_q8 (a8, BLOCK, b8, BLOCK, o8);
      break;
    case OP_SUB_Q8:
      sub_q8 (a8, BLOCK, b8, BLOCK, o8);
      break;
    case OP_MUL_Q8:
      mul_q8 (a8, BLOCK, b8, BLOCK, o8);
      break;
    case OP_DOT_Q8:
      sink += dot_q8 (a8, BLOCK, b8, BLOCK);
      break;
    case OP_SHL_Q8:
      shl_q8 (a8, BLOCK, o8, 2);
      break;
    case OP_SHR_Q8:
      shr_q8 (a8, BLOCK, o8, 2);
      break;
    case OP_SHL_I64:
      shl_i64 (a64, BLOCK, o64, 2);
      break;
    default:
      shr_i64 (a64, BLOCK, o64, 2);
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_OPS][ITERATIONS];

  /* A quarter of the inputs saturate. The clamp is a branch on most
     targets, so an input that never trips it would measure the
     always-predicted case and report a cost nobody pays. */
  for (int i = 0; i < BLOCK; i++)
    {
      const int big = (i & 3) == 0;
      a15[i]        = (int16_t)(big ? 30000 - (i & 511) : (i & 4095) - 2048);
      b15[i]        = (int16_t)(big ? 30000 + (i & 255) : (i & 2047) - 1024);
      a8[i]         = (int8_t)(big ? 110 - (i & 15) : (i & 63) - 32);
      b8[i]         = (int8_t)(big ? 110 + (i & 7) : (i & 31) - 16);
      a64[i]        = big ? INT64_MAX / 2 + i : (int64_t)i - BLOCK / 2;
    }

  printf ("=== arith (saturating fixed-point vector kernels) ===\n");
  printf ("block = %d elements x %d calls, %d rounds, ~25%% saturating\n\n",
          BLOCK, REPS, ITERATIONS);

  DP_BENCH_SETTLE (run (OP_MUL_Q15));

  /* Rounds outside, kernels inside: the fourteen numbers below are read
     against each other, so any drift must land on all of them. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int op = 0; op < N_OPS; op++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < REPS; k++)
          run (op);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[op][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int op = 0; op < N_OPS; op++)
    dp_bench_record (&_bench, op_name[op], t[op], ITERATIONS,
                     (size_t)BLOCK * REPS, "elem");

  printf ("\n  Q15 / Q8 at the same element count -- above 1.0 the narrower\n"
          "  type is buying lanes, as it should:\n"
          "    add %.2fx   mul %.2fx   dot %.2fx\n"
          "  and the two shifts, which do NOT:\n"
          "    shl %.2fx   shr %.2fx\n"
          "  Same source transliterated between widths -- same per-element\n"
          "  branch, same half-LSB rounding -- so a ratio below 1.0 there is\n"
          "  a codegen result, not an algorithmic one. doppler#905.\n",
          dp_bench_min (t[OP_ADD_Q15], ITERATIONS)
              / dp_bench_min (t[OP_ADD_Q8], ITERATIONS),
          dp_bench_min (t[OP_MUL_Q15], ITERATIONS)
              / dp_bench_min (t[OP_MUL_Q8], ITERATIONS),
          dp_bench_min (t[OP_DOT_Q15], ITERATIONS)
              / dp_bench_min (t[OP_DOT_Q8], ITERATIONS),
          dp_bench_min (t[OP_SHL_Q15], ITERATIONS)
              / dp_bench_min (t[OP_SHL_Q8], ITERATIONS),
          dp_bench_min (t[OP_SHR_Q15], ITERATIONS)
              / dp_bench_min (t[OP_SHR_Q8], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "arith");
  return 0;
}
