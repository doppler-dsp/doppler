/* bench_u8_to_f32_core.c -- what the unbiased mapping costs over the fast one.
 *
 * U8ToF32 is the first stage of an RTL-SDR receive chain: every byte the
 * dongle sends passes through it, at 2x the complex sample rate. It offers
 * two mappings, and the header makes a claim about their price: `shift`
 * is the fast path (integer subtract, convert, power-of-two scale) and
 * `midpoint` trades the integer subtract for a float one. This file is
 * where that claim is a number, and the ratio between the rows is the one
 * to read.
 *
 * `steps` is what a receiver calls; `step` is recorded too because it is
 * the only row that pays the per-sample mode dispatch steps() hoists out.
 * The input is every code in turn, so neither loop sees a constant.
 */
#include "doppler/u8_to_f32/u8_to_f32_core.h"
#include "dp_bench.h"
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 65536
#define ITERATIONS 200
#define N_MODE 2

static const char *const mode_name[N_MODE] = { "shift", "midpoint" };

int
main (void)
{
  uint8_t *in  = malloc (BLOCK * sizeof *in);
  float   *out = malloc (BLOCK * sizeof *out);
  if (!in || !out)
    {
      (void)fprintf (stderr, "OOM\n");
      free (in);
      free (out);
      return 1;
    }
  for (int i = 0; i < BLOCK; i++)
    in[i] = (uint8_t)i;

  u8_to_f32_state_t *obj[N_MODE];
  for (int m = 0; m < N_MODE; m++)
    {
      obj[m] = u8_to_f32_create (m);
      if (!obj[m])
        return 1;
    }

  static double  t_steps[N_MODE][ITERATIONS], t_step[N_MODE][ITERATIONS];
  uint64_t       t0, t1;
  jm_bench_t     _bench = { 0 };
  char           name[64];
  volatile float sink; /* keeps the step() loop from being discarded */

  printf ("=== u8_to_f32 (offset-binary uint8 -> float, %d per call) ===\n",
          BLOCK);
  printf ("%d rounds, min over rounds\n\n", ITERATIONS);

  DP_BENCH_SETTLE (u8_to_f32_steps (obj[0], in, out, BLOCK));

  /* Rounds outside, modes inside: the shift/midpoint ratio is the point of
     the file, so a thermal step must land on both halves of it. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int m = 0; m < N_MODE; m++)
      {
        t0 = jm_bench_now_ns ();
        u8_to_f32_steps (obj[m], in, out, BLOCK);
        t1            = jm_bench_now_ns ();
        t_steps[m][r] = jm_bench_elapsed_sec (t0, t1);

        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BLOCK; i++)
          sink = u8_to_f32_step (obj[m], in[i]);
        t1           = jm_bench_now_ns ();
        t_step[m][r] = jm_bench_elapsed_sec (t0, t1);
      }
  (void)sink;

  for (int m = 0; m < N_MODE; m++)
    {
      (void)snprintf (name, sizeof name, "steps[%s]", mode_name[m]);
      dp_bench_record (&_bench, name, t_steps[m], ITERATIONS, BLOCK, "sample");
    }
  for (int m = 0; m < N_MODE; m++)
    {
      (void)snprintf (name, sizeof name, "step[%s]", mode_name[m]);
      dp_bench_record (&_bench, name, t_step[m], ITERATIONS, BLOCK, "sample");
    }

  printf ("\n  midpoint cost over shift (steps): %.2fx\n",
          dp_bench_min (t_steps[1], ITERATIONS)
              / dp_bench_min (t_steps[0], ITERATIONS));

  jm_bench_write_json (&_bench, "u8_to_f32");
  for (int m = 0; m < N_MODE; m++)
    u8_to_f32_destroy (obj[m]);
  free (in);
  free (out);
  return 0;
}
