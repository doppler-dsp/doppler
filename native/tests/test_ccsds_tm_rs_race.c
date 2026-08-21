/*
 * test_ccsds_tm_rs_race.c — the CCSDS RS tables are derived exactly once,
 * however many threads ask for them at the same moment.
 *
 * This is a SEPARATE binary from test_ccsds_tm_rs.c, and that is the whole
 * design: the race is on the FIRST call, so a process that has already
 * encoded anything on the main thread cannot reach it. Nothing here touches
 * the code before the worker threads do.
 *
 * `rs.c` used to build its tables behind a plain `ready` flag. Two threads
 * arriving first would both read `ready == 0`, both call `rs_init`, and --
 * the part that makes it undefined behaviour rather than a wasted
 * initialisation -- one could read `ccsds`'s tables while the other was
 * still filling them. `pthread_once` fixed it (gh-817).
 *
 * The check has two halves, because the interesting failure is silent:
 *
 *   - every thread must get the SAME generator polynomial, byte for byte.
 *     A torn read gives a g(x) that is neither the right one nor obviously
 *     wrong, and a codeword built on it fails no local check -- it simply
 *     cannot be decoded by any other implementation.
 *   - the parity every thread computes over one shared information block
 *     must agree, and must agree with the parity computed afterwards on the
 *     main thread, once the tables are unambiguously settled.
 *
 * Run under ThreadSanitizer (`make test-tsan`) this also fails on the race
 * ITSELF rather than on its consequences, which is the evidence the fix
 * actually wants: a benign-looking pass here proves the outcome, and TSan
 * proves the mechanism. Reverting rs.c's `pthread_once` to the old flag is
 * what makes both go red.
 */
#include "ccsds_tm/ccsds_tm_rs.h"
#include "dp_test.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define NTHREADS 8

typedef struct
{
  pthread_barrier_t *gate;
  uint8_t            gen[CCSDS_TM_RS_2E + 1];
  uint8_t            parity[CCSDS_TM_RS_2E];
} worker_t;

/* One shared information block, so every thread encodes the same input and
 * any disagreement in the output is a disagreement about the FIELD. */
static uint8_t info[CCSDS_TM_RS_K];

static void *
worker (void *arg)
{
  worker_t *w = (worker_t *)arg;

  /* Every thread waits here and is released together, so the first calls
     land as close to simultaneously as the scheduler allows. Without this
     the threads start staggered by their own creation cost and the first
     one is usually finished before the second begins -- which is how a
     race like this hides from a test that does spawn threads. */
  pthread_barrier_wait (w->gate);

  memcpy (w->gen, ccsds_tm_rs_generator (), sizeof w->gen);
  ccsds_tm_rs_encode (info, w->parity);
  return NULL;
}

int
main (void)
{
  pthread_barrier_t gate;
  pthread_t         th[NTHREADS];
  static worker_t   w[NTHREADS];

  for (size_t i = 0; i < sizeof info; i++)
    info[i] = (uint8_t)(i * 7u + 1u);

  if (pthread_barrier_init (&gate, NULL, NTHREADS) != 0)
    {
      fprintf (stderr, "barrier init failed\n");
      return 1;
    }

  for (int i = 0; i < NTHREADS; i++)
    {
      w[i].gate = &gate;
      if (pthread_create (&th[i], NULL, worker, &w[i]) != 0)
        {
          fprintf (stderr, "pthread_create failed\n");
          return 1;
        }
    }
  for (int i = 0; i < NTHREADS; i++)
    pthread_join (th[i], NULL);
  pthread_barrier_destroy (&gate);

  /* Only now does the main thread touch the code -- after every worker has
     already raced for it. */
  uint8_t settled_gen[CCSDS_TM_RS_2E + 1];
  uint8_t settled_parity[CCSDS_TM_RS_2E];
  memcpy (settled_gen, ccsds_tm_rs_generator (), sizeof settled_gen);
  ccsds_tm_rs_encode (info, settled_parity);

  /* g(x) is degree 2E, so it has 2E+1 coefficients and the leading one is
     1 -- a table that was read while being written typically fails this
     before it fails anything else. */
  DP_CHECK (settled_gen[0] == 1u);

  int gen_agree = 1, parity_agree = 1;
  for (int i = 0; i < NTHREADS; i++)
    {
      if (memcmp (w[i].gen, settled_gen, sizeof settled_gen) != 0)
        gen_agree = 0;
      if (memcmp (w[i].parity, settled_parity, sizeof settled_parity) != 0)
        parity_agree = 0;
    }

  DP_CHECK_MSG (gen_agree,
                "every thread must derive the SAME g(x) -- a thread that "
                "read the field tables mid-write gets a self-consistent "
                "polynomial that no other implementation can decode");
  DP_CHECK_MSG (parity_agree,
                "the same information block must encode to the same parity "
                "on every thread, and afterwards on the main thread");

  DP_TEST_END ("ccsds_tm_rs_race");
}
