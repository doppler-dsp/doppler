/* test_dp_interrupt — the flag every blocking wait in doppler consults.
 *
 * Small surface, and worth its own test for one reason: it is the primitive
 * three transports now depend on to be stoppable, so a regression here is
 * silent everywhere at once. See docs/design/io-termination.md.
 */

#include "dp_interrupt.h"

#include "dp_test.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/* Set by the previous handler, to prove chaining actually calls it. */
static volatile sig_atomic_t prior_ran = 0;

static void
prior_handler (int sig)
{
  (void)sig;
  prior_ran = 1;
}

static void
test_flag_round_trip (void)
{
  dp_resume ();
  DP_CHECK (dp_interrupted () == 0);

  dp_interrupt ();
  DP_CHECK (dp_interrupted () != 0);

  /* Sticky: asking twice does not clear it. One handler firing may have to
     release several parked loops. */
  DP_CHECK (dp_interrupted () != 0);

  dp_resume ();
  DP_CHECK (dp_interrupted () == 0);
}

static void
test_latency_knob (void)
{
  dp_set_interrupt_latency_ms (250);
  DP_CHECK (dp_interrupt_latency_ms () == 250);

  /* 0 selects the default rather than meaning "never wake". */
  dp_set_interrupt_latency_ms (0);
  DP_CHECK (dp_interrupt_latency_ms () == DP_INTERRUPT_LATENCY_DEFAULT_MS);
}

static void
test_signal_sets_the_flag (void)
{
  dp_resume ();
  DP_CHECK (dp_interrupt_on_signal (SIGUSR1) == DP_OK);

  raise (SIGUSR1);
  DP_CHECK (dp_interrupted () != 0);

  DP_CHECK (dp_restore_signal (SIGUSR1) == DP_OK);
  dp_resume ();
}

static void
test_installing_twice_is_not_an_error (void)
{
  DP_CHECK (dp_interrupt_on_signal (SIGUSR1) == DP_OK);
  /* Second install must NOT chain to ourselves -- that would make the
     handler call itself through the saved slot. */
  DP_CHECK (dp_interrupt_on_signal (SIGUSR1) == DP_OK);
  DP_CHECK (dp_restore_signal (SIGUSR1) == DP_OK);
}

static void
test_chaining_calls_the_previous_handler (void)
{
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags   = 0;
  sa.sa_handler = prior_handler;
  DP_CHECK (sigaction (SIGUSR2, &sa, NULL) == 0);

  prior_ran = 0;
  dp_resume ();
  DP_CHECK (dp_interrupt_on_signal (SIGUSR2) == DP_OK);

  raise (SIGUSR2);

  /* Both halves: ours ran, and so did the one we displaced. Without the
     chain, "Ctrl+C works during a receive" becomes "Ctrl+C works ONLY
     during a receive". */
  DP_CHECK (dp_interrupted () != 0);
  DP_CHECK (prior_ran == 1);

  DP_CHECK (dp_restore_signal (SIGUSR2) == DP_OK);
  dp_resume ();
}

static void
test_restoring_what_was_never_installed_fails (void)
{
  DP_CHECK (dp_restore_signal (SIGUSR2) == DP_ERR_INVALID);
}

static void
test_restore_puts_the_old_handler_back (void)
{
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags   = 0;
  sa.sa_handler = prior_handler;
  DP_CHECK (sigaction (SIGUSR2, &sa, NULL) == 0);

  DP_CHECK (dp_interrupt_on_signal (SIGUSR2) == DP_OK);
  DP_CHECK (dp_restore_signal (SIGUSR2) == DP_OK);

  /* After restoring, the signal must reach the ORIGINAL handler and must
     no longer set our flag. */
  prior_ran = 0;
  dp_resume ();
  raise (SIGUSR2);
  DP_CHECK (prior_ran == 1);
  DP_CHECK (dp_interrupted () == 0);

  sa.sa_handler = SIG_DFL;
  DP_CHECK (sigaction (SIGUSR2, &sa, NULL) == 0);
}

int
main (void)
{
  test_flag_round_trip ();
  test_latency_knob ();
  test_signal_sets_the_flag ();
  test_installing_twice_is_not_an_error ();
  test_chaining_calls_the_previous_handler ();
  test_restoring_what_was_never_installed_fails ();
  test_restore_puts_the_old_handler_back ();

  DP_TEST_END ("test_dp_interrupt");
}
