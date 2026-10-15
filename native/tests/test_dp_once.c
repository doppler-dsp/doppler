/* dp_once and the lazily built tables it guards (doppler#2328).
 *
 * The library builds four tables on first use: the AWGN and LO sine LUTs,
 * the CCSDS marker's bits and its Reed-Solomon field tables. Each used to be
 * guarded by a plain `ready` flag (rs.c by a private pthread_once), and two
 * threads making their first object at once raced: one could see the flag
 * set and read a table still being written. Plan.prepare's parallel builds
 * and any GIL-free caller do exactly that.
 *
 * 1. The FIRST use of every table, from several threads at once, in a fresh
 *    process (this is the first thing the program does). Every thread's
 *    output must equal a serial reference built afterwards: an AWGN draw, an
 *    LO tone, and a whole CCSDS frame, encoded (the R-S tables) and described
 *    (the marker's bits). Under `make test-tsan` a guard that is not a
 *    happens-before edge is reported even when the outputs happen to agree,
 *    which is the proof: put a plain flag back in awgn_core.c and TSan names
 *    the race.
 * 2. dp_once itself: several threads, one build, and every caller sees it
 *    finished.
 */
#include "doppler/awgn/awgn_core.h"
#include "doppler/ccsds_tm/ccsds_tm.h"
#include "doppler/ccsds_tm/ccsds_tm_frame.h"
#include "doppler/conv/conv_core.h"
#include "doppler/dp_once.h"
#include "doppler/dp_thread.h"
#include "doppler/lo/lo_core.h"
#include "dp_test.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#define THREADS 8
#define NS 4096
#define MAX_SYM 8192

static const ccsds_tm_frame_cfg_t CFG
    = { .rs_depth = 1, .randomise = 1, .attach_asm = 1, .convolutional = 1 };

typedef struct
{
  int id;
  float _Complex awgn[NS];
  float _Complex lo[NS];
  uint8_t marker[CCSDS_TM_ASM_BITS];
  uint8_t sym[MAX_SYM];
  size_t  nsym;
  int     ok;
} job_t;

static atomic_int arrived;

/* Every thread waits here until all have arrived, so the first uses below
   start together instead of one thread finishing the build first. */
static void
barrier (void)
{
  atomic_fetch_add (&arrived, 1);
  while (atomic_load (&arrived) < THREADS)
    ;
}

static void
info_block (uint8_t *frame, uint8_t *bits)
{
  for (size_t i = 0; i < CCSDS_TM_RS_K; i++)
    frame[i] = (uint8_t)(i * 7u + 1u);
  for (size_t i = 0; i < CCSDS_TM_RS_K; i++)
    for (unsigned b = 0; b < 8u; b++)
      bits[i * 8u + b] = (uint8_t)((frame[i] >> (7u - b)) & 1u);
}

/* One thread's first use of all four tables. */
static void
first_use (job_t *j)
{
  dp_awgn_state_t *g = dp_awgn_create (1000u + (uint64_t)j->id, 1.0f);
  dp_lo_state_t   *l = dp_lo_create (0.01 * (double)(j->id + 1));
  j->ok              = g != NULL && l != NULL;
  if (g)
    dp_awgn_generate (g, NS, j->awgn, NS);
  if (l)
    dp_lo_steps (l, NS, j->lo, NS);
  dp_awgn_destroy (g);
  dp_lo_destroy (l);

  uint8_t          frame[CCSDS_TM_RS_K], bits[CCSDS_TM_RS_K * 8u];
  wfm_frame_desc_t d;
  conv_enc_t       enc;
  info_block (frame, bits);
  dp_conv_enc_init (&enc);
  j->nsym = dp_ccsds_tm_frame_encode (&CFG, &enc, frame, CCSDS_TM_RS_K, j->sym,
                                      MAX_SYM);
  if (dp_ccsds_tm_frame_describe (&CFG, CCSDS_TM_RS_K, bits, &d) == 0
      && d.field[0].seq.bits)
    memcpy (j->marker, d.field[0].seq.bits, sizeof j->marker);
  else
    j->ok = 0;
}

DP_THREAD_FN (worker, arg)
{
  barrier ();
  first_use ((job_t *)arg);
  DP_THREAD_RETURN;
}

static job_t jobs[THREADS], ref;

static int
check_first_use_from_many_threads (void)
{
  dp_thread_t t[THREADS];
  atomic_store (&arrived, 0);
  for (int i = 0; i < THREADS; i++)
    {
      jobs[i].id = i;
      DP_REQUIRE (dp_thread_create (&t[i], worker, &jobs[i]) == 0);
    }
  for (int i = 0; i < THREADS; i++)
    DP_CHECK (dp_thread_join (t[i]) == 0);

  /* The serial reference, with every table long built. */
  for (int i = 0; i < THREADS; i++)
    {
      ref.id = i;
      first_use (&ref);
      DP_CHECK_MSG (jobs[i].ok && ref.ok, "every object was made");
      DP_CHECK_MSG (memcmp (jobs[i].awgn, ref.awgn, sizeof ref.awgn) == 0,
                    "an AWGN made first, concurrently, draws what a serial "
                    "one does: its sine table was whole");
      DP_CHECK_MSG (memcmp (jobs[i].lo, ref.lo, sizeof ref.lo) == 0,
                    "an LO made first, concurrently, plays what a serial "
                    "one does: its sine table was whole");
      DP_CHECK_MSG (jobs[i].nsym == ref.nsym && ref.nsym > 0
                        && memcmp (jobs[i].sym, ref.sym, ref.nsym) == 0,
                    "a CCSDS frame encoded first, concurrently, equals a "
                    "serial one: its Reed-Solomon tables were whole");
      DP_CHECK_MSG (memcmp (jobs[i].marker, ref.marker, sizeof ref.marker)
                        == 0,
                    "a frame described first, concurrently, carries the "
                    "whole marker");
    }
  return 0;
}

/* dp_once on its own: one build, seen finished by every caller. */
static int        builds;
static int        seen[THREADS];
static dp_once_t  counted = DP_ONCE_INIT;
static atomic_int done_in_build;

static void
count_build (void)
{
  builds++;
  /* Long enough that the other threads arrive while this one is building. */
  for (volatile int spin = 0; spin < 200000; spin++)
    ;
  atomic_store (&done_in_build, 1);
}

DP_THREAD_FN (counter, arg)
{
  int i = *(int *)arg;
  barrier ();
  dp_once (&counted, count_build);
  seen[i] = builds + 10 * atomic_load (&done_in_build);
  DP_THREAD_RETURN;
}

static int
check_once_runs_once (void)
{
  dp_thread_t t[THREADS];
  int         ids[THREADS];
  atomic_store (&arrived, 0);
  for (int i = 0; i < THREADS; i++)
    {
      ids[i] = i;
      DP_REQUIRE (dp_thread_create (&t[i], counter, &ids[i]) == 0);
    }
  for (int i = 0; i < THREADS; i++)
    DP_CHECK (dp_thread_join (t[i]) == 0);
  DP_CHECK_MSG (builds == 1, "dp_once ran its build exactly once");
  for (int i = 0; i < THREADS; i++)
    DP_CHECK_MSG (seen[i] == 11,
                  "every caller returned after the build had finished");
  return 0;
}

int
main (void)
{
  /* First: nothing in this process has touched a table yet. */
  if (check_first_use_from_many_threads () || check_once_runs_once ())
    return 1;
  DP_TEST_END ("test_dp_once");
}
