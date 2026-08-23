/* bench_dp_interrupt_guard_core.c — what a stoppable wait actually pays.
 *
 * The question is not "how fast is reading a flag". It is whether making
 * every blocking wait in doppler consult one costs anything in the loop
 * that consults it, because that is the objection to the whole design:
 * the ring's wait() now checks the flag on every spin iteration, and the
 * NATS wait checks it every slice.
 *
 * Two measurements, and they answer different questions:
 *
 *   interrupted   the hot path -- what a spin iteration pays to be
 *                 stoppable at all. Compared against `raw_load`, a bare
 *                 read of a volatile sig_atomic_t, which is the floor:
 *                 the accessor cannot be cheaper than the load inside it,
 *                 so the gap is what the call and the guard indirection
 *                 cost.
 *   arm_disarm    create + destroy on one signal. NOT a hot path -- it is
 *                 two sigaction(2) calls and a malloc, measured because
 *                 an application arms per RUN and a test harness arms per
 *                 CASE, and the second is where a surprise would show.
 *
 * A volatile sink keeps the loops from being optimised away.
 */
#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#include "jm_bench.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

/* The arming loop is syscall-bound, so a 65536-deep run would take
   minutes and say nothing more than a short one. */
#define ARM_N 512

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static volatile sig_atomic_t bench_flag = 0;
static volatile long         sink       = 0;

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  double          times[ITERATIONS];

  printf ("=== dp_interrupt_guard benchmark ===\n");
  printf ("query = %d calls, arm = %d cycles, %d iterations\n\n", BENCH_N,
          ARM_N, ITERATIONS);

  dp_interrupt_guard_t *g = dp_interrupt_guard_create (NULL, 0, 0);
  if (!g)
    {
      fprintf (stderr, "cannot create a guard\n");
      return 1;
    }

  /* The floor: a bare volatile load, which is what the accessor wraps. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += bench_flag;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink += acc;
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "raw_load", times, ITERATIONS, BENCH_N);

  /* What a spin iteration actually pays to be stoppable. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += dp_interrupt_guard_interrupted (g);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink += acc;
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "interrupted", times, ITERATIONS, BENCH_N);

  dp_interrupt_guard_destroy (g);

  /* Arming: two sigaction calls and a malloc, per cycle. */
  const int32_t one[] = { SIGUSR1 };
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < ARM_N; i++)
        {
          dp_interrupt_guard_t *a = dp_interrupt_guard_create (one, 1, 0);
          sink += (a != NULL);
          dp_interrupt_guard_destroy (a);
        }
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "arm_disarm", times, ITERATIONS, ARM_N);

  jm_bench_write_json (&_bench, "dp_interrupt_guard");
  return 0;
}
