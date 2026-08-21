/* bench_ddcr_core.c -- what the real-input path is worth, per input sample.
 *
 * `ddcr` is `ddc` with a real input: the same LO mix, cascade and
 * resampler, fed `float` instead of `float _Complex`. It exists because
 * that is what a real ADC hands over, and the alternative -- widening
 * every sample to a complex value with a zero imaginary part and calling
 * `ddc` -- is one memory pass and half the mixer's multiplies thrown away.
 * The question is how much of that shows up as time. Half the input
 * bandwidth is the optimistic bound; whether the mixer's real-input
 * shortcut survives past the first cascade stage, where the signal is
 * complex either way, is not something the signature can say.
 *
 * The row that answers it lives in `bench_ddc_core.c`, at the same rate
 * and the same block length in input samples -- deliberately, so the two
 * files can be read against each other. This file cannot make that
 * comparison itself (`ddc` is a different component and a different
 * binary), which is exactly why both use `rate = 0.25`, `BLOCK = 32768`
 * and a per-INPUT-sample unit. Change one and the pair stops meaning
 * anything.
 *
 * The three faces are the same three, for the same reason: `execute` for a
 * block, `execute_ctrl` for a block that steers, and `execute_ctrl_push`
 * for the one-sample-at-a-time form a closed loop is obliged to use. If
 * the push penalty here matches `ddc`'s, then it is the per-call overhead
 * rather than anything about the sample type -- which is the useful
 * decomposition, since the loop's granularity is not negotiable.
 */
#include "ddcr/ddcr_core.h"
#include "dp_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
/* Held in step with bench_ddc_core.c -- the pair is the measurement. */
#define BLOCK 32768
#define RATE 0.25
#define NORM_FREQ (-0.1)

enum
{
  CFG_EXECUTE,
  CFG_CTRL,
  CFG_PUSH,
  N_CFG
};

static const char *face_name[N_CFG]
    = { "execute", "execute_ctrl", "execute_ctrl_push" };

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  ddcr_state_t   *ddcr = ddcr_create (NORM_FREQ, RATE);
  float          *in   = NULL;
  float complex  *out  = NULL;
  size_t          cap, emitted = 0;
  char            name[64];

  if (!ddcr)
    return 1;

  in = malloc (BLOCK * sizeof *in);
  if (!in)
    return 1;
  for (size_t i = 0; i < BLOCK; i++)
    in[i] = cosf (0.11f * (float)i);

  cap = ddcr_execute_max_out (ddcr);
  if (cap < BLOCK)
    cap = BLOCK;
  out = malloc (cap * sizeof *out);
  if (!out)
    return 1;

  printf ("=== ddcr (real-input LO mix + cascade + resample) ===\n");
  printf ("rate = %.2f, %d input samples, %d rounds, min over rounds\n\n",
          RATE, BLOCK, ITERATIONS);

  DP_BENCH_SETTLE ((void)ddcr_execute (ddcr, in, BLOCK, out, cap));

  /* Rounds outside, faces inside: the push row is read as a multiple of
     the execute row, so both must see the same machine. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      emitted = ddcr_execute (ddcr, in, BLOCK, out, cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t[CFG_EXECUTE][r] = dp_bench_elapsed (&t0, &t1);

      clock_gettime (CLOCK_MONOTONIC, &t0);
      (void)ddcr_execute_ctrl (ddcr, in, BLOCK, 0.0, 0.0, out, cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t[CFG_CTRL][r] = dp_bench_elapsed (&t0, &t1);

      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (size_t i = 0; i < BLOCK; i++)
        (void)ddcr_execute_ctrl_push (ddcr, in[i], 0.0, 0.0, out, cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t[CFG_PUSH][r] = dp_bench_elapsed (&t0, &t1);
    }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "%s[rate=%.2f]", face_name[c], RATE);
      dp_bench_record (&_bench, name, t[c], ITERATIONS, BLOCK, "in-sample");
    }

  printf ("\n  %zu outputs from %d real inputs\n", emitted, BLOCK);
  printf ("  face cost (%s = 1.00x):\n", face_name[CFG_EXECUTE]);
  for (int c = 1; c < N_CFG; c++)
    printf ("    %-20s %.2fx\n", face_name[c],
            dp_bench_min (t[c], ITERATIONS)
                / dp_bench_min (t[CFG_EXECUTE], ITERATIONS));
  printf ("  Compare the execute row's ns/in-sample against ddc's at the\n"
          "  same rate: that difference is what a real input buys, and it\n"
          "  is the only place the two components can be compared. A push\n"
          "  penalty matching ddc's says the per-call overhead, not the\n"
          "  sample type, is what the one-at-a-time form costs.\n");

  free (in);
  free (out);
  ddcr_destroy (ddcr);
  jm_bench_write_json (&_bench, "ddcr");
  return 0;
}
