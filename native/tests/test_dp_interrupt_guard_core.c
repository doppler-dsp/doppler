/* test_dp_interrupt_guard_core — the object face of the interrupt flag.
 *
 * The primitive itself is tested in test_dp_interrupt.c. What is here is
 * the bookkeeping that used to live in the Python binding, where this
 * layer could not reach it at all: arming, restoring exactly what was
 * armed, and refusing to arm anything when it cannot arm everything.
 *
 * Every claim below was proven by sabotage before being trusted -- see
 * docs/dev/contributing/validation.md step 2.
 */

#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#include "dp_test.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------
 * The guard. Each of these pins a sentence the header makes -- the
 * bookkeeping used to live in the Python binding, where none of it could
 * be tested at this layer at all.
 * ------------------------------------------------------------------ */

static void
test_guard_with_no_signals_is_still_a_handle (void)
{
  dp_interrupt ();
  dp_interrupt_guard_t *g = dp_interrupt_guard_create (NULL, 0, 0);
  DP_CHECK (g != NULL);
  if (!g)
    return;

  DP_CHECK_MSG (dp_interrupt_guard_interrupted (g) == 0,
                "arming clears the flag, or the first wait inside the "
                "block that just armed would refuse immediately");

  dp_interrupt_guard_interrupt (g);
  DP_CHECK (dp_interrupt_guard_interrupted (g) != 0);
  dp_interrupt_guard_resume (g);
  DP_CHECK (dp_interrupt_guard_interrupted (g) == 0);

  dp_interrupt_guard_destroy (g);
}

/* The flag is process-wide, so a request through one guard is seen by the
   other. Stated in the header; a reader would reasonably assume otherwise
   of two objects. */
static void
test_two_guards_share_one_flag (void)
{
  dp_interrupt_guard_t *a = dp_interrupt_guard_create (NULL, 0, 0);
  dp_interrupt_guard_t *b = dp_interrupt_guard_create (NULL, 0, 0);
  DP_CHECK (a != NULL && b != NULL);
  if (!a || !b)
    return;

  dp_interrupt_guard_interrupt (a);
  DP_CHECK_MSG (dp_interrupt_guard_interrupted (b) != 0,
                "the guard is a handle to a process-wide facility, not an "
                "instance of one");

  dp_interrupt_guard_resume (b);
  DP_CHECK (dp_interrupt_guard_interrupted (a) == 0);

  dp_interrupt_guard_destroy (a);
  dp_interrupt_guard_destroy (b);
}

static void
test_guard_arms_and_destroy_restores (void)
{
  struct sigaction before;
  DP_CHECK (sigaction (SIGUSR1, NULL, &before) == 0);

  const int32_t         sigs[] = { SIGUSR1 };
  dp_interrupt_guard_t *g      = dp_interrupt_guard_create (sigs, 1, 0);
  DP_CHECK (g != NULL);
  if (!g)
    return;

  raise (SIGUSR1);
  DP_CHECK_MSG (dp_interrupt_guard_interrupted (g) != 0,
                "construction is what arms: the handler must be live on "
                "return, not at some later enter()");

  dp_interrupt_guard_destroy (g);

  struct sigaction after;
  DP_CHECK (sigaction (SIGUSR1, NULL, &after) == 0);
  DP_CHECK_MSG (after.sa_sigaction == before.sa_sigaction
                    && after.sa_handler == before.sa_handler,
                "destroy restores the handler the guard displaced");
}

/* Destroy must NOT clear the flag: a caller that was interrupted still
   needs to see that it was, after the block that noticed has exited. */
static void
test_destroy_does_not_swallow_the_interrupt (void)
{
  dp_interrupt_guard_t *g = dp_interrupt_guard_create (NULL, 0, 0);
  DP_CHECK (g != NULL);
  if (!g)
    return;
  dp_interrupt_guard_interrupt (g);
  dp_interrupt_guard_destroy (g);

  DP_CHECK_MSG (dp_interrupted () != 0,
                "a guard that exits after being interrupted must leave the "
                "fact visible, or the caller cannot act on it");
  dp_resume ();
}

static void
test_guard_saves_and_restores_the_latency (void)
{
  dp_set_interrupt_latency_ms (0); /* the documented default */
  const unsigned before = dp_interrupt_latency_ms ();

  dp_interrupt_guard_t *g = dp_interrupt_guard_create (NULL, 0, before + 25u);
  DP_CHECK (g != NULL);
  if (!g)
    return;
  DP_CHECK (dp_interrupt_latency_ms () == before + 25u);
  dp_interrupt_guard_destroy (g);
  DP_CHECK_MSG (dp_interrupt_latency_ms () == before,
                "the override is undone by the guard that made it");

  /* Zero means "leave it alone", and that is not the same as "set it to
     the default" -- a guard that restored unconditionally would clobber a
     latency somebody else had chosen. */
  dp_set_interrupt_latency_ms (before + 40u);
  g = dp_interrupt_guard_create (NULL, 0, 0);
  DP_CHECK (g != NULL);
  if (!g)
    return;
  DP_CHECK (dp_interrupt_latency_ms () == before + 40u);
  dp_interrupt_guard_destroy (g);
  DP_CHECK_MSG (dp_interrupt_latency_ms () == before + 40u,
                "latency_ms = 0 must not touch the process setting, in "
                "either direction");
  dp_set_interrupt_latency_ms (0);
}

/* A failed create arms NOTHING. Without the unwind, the handlers installed
   before the failing one would be live with no guard able to remove them. */
static void
test_failed_create_arms_nothing (void)
{
  struct sigaction before;
  DP_CHECK (sigaction (SIGUSR1, NULL, &before) == 0);

  dp_set_interrupt_latency_ms (0);
  const unsigned latency_before = dp_interrupt_latency_ms ();

  /* SIGKILL cannot be caught, so the second entry fails and the first has
     to be given back. */
  const int32_t         sigs[] = { SIGUSR1, SIGKILL };
  dp_interrupt_guard_t *g
      = dp_interrupt_guard_create (sigs, 2, latency_before + 25u);
  DP_CHECK_MSG (g == NULL, "a create that cannot arm everything fails");

  struct sigaction after;
  DP_CHECK (sigaction (SIGUSR1, NULL, &after) == 0);
  DP_CHECK_MSG (after.sa_sigaction == before.sa_sigaction
                    && after.sa_handler == before.sa_handler,
                "the handler installed before the failure was given back");
  DP_CHECK_MSG (dp_interrupt_latency_ms () == latency_before,
                "and so was the latency it had already overridden");
}

static void
test_guard_rejects_more_signals_than_slots (void)
{
  static const int32_t many[] = { SIGUSR1, SIGUSR2, SIGCHLD,   SIGCONT, SIGURG,
                                  SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF };
  DP_CHECK (dp_interrupt_guard_create (many, 9, 0) == NULL);

  /* DUPLICATES are the case only the up-front count check catches, and the
     reason it is not redundant with slot exhaustion. dp_interrupt_on_signal
     returns DP_OK for a signal already installed ("already ours"), so nine
     copies of one signal are nine successes -- and without the check they
     would walk g->sigs[] one past its end. Nine DISTINCT signals cannot
     show this: the ninth is refused by the slot table regardless, so that
     case passes either way. */
  static const int32_t dupes[] = { SIGUSR1, SIGUSR1, SIGUSR1, SIGUSR1, SIGUSR1,
                                   SIGUSR1, SIGUSR1, SIGUSR1, SIGUSR1 };
  DP_CHECK_MSG (dp_interrupt_guard_create (dupes, 9, 0) == NULL,
                "a signal list longer than the guard can record is refused "
                "before any of it is installed");
  DP_CHECK_MSG (dp_interrupt_guard_create (NULL, 3, 0) == NULL,
                "a count with no array is a caller error, not a read of "
                "whatever was at address zero");
}

static void
test_guard_null_is_a_no_op (void)
{
  dp_interrupt_guard_destroy (NULL); /* must not crash */
  dp_resume ();
  dp_interrupt_guard_interrupt (NULL);
  DP_CHECK_MSG (dp_interrupted () != 0,
                "NULL still reaches the flag, because the flag is not what "
                "a guard holds");
  dp_interrupt_guard_resume (NULL);
  DP_CHECK (dp_interrupted () == 0);
  DP_CHECK (dp_interrupt_guard_interrupted (NULL) == 0);
}

int
main (void)
{
  test_guard_with_no_signals_is_still_a_handle ();
  test_two_guards_share_one_flag ();
  test_guard_arms_and_destroy_restores ();
  test_destroy_does_not_swallow_the_interrupt ();
  test_guard_saves_and_restores_the_latency ();
  test_failed_create_arms_nothing ();
  test_guard_rejects_more_signals_than_slots ();
  test_guard_null_is_a_no_op ();

  DP_TEST_END ("test_dp_interrupt_guard_core");
}
