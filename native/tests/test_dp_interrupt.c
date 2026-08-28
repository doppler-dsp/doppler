/* test_dp_interrupt — the flag every blocking wait in doppler consults.
 *
 * Small surface, and worth its own test for one reason: it is the primitive
 * three transports now depend on to be stoppable, so a regression here is
 * silent everywhere at once. See docs/design/io-termination.md.
 */

#include "dp_interrupt.h"

#include "dp_interrupt_guard/dp_interrupt_guard_procglobal.h"
#include "dp_test.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
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

/* Set by a SA_SIGINFO predecessor, to prove the three-argument chain. */
static volatile sig_atomic_t prior_si_ran = 0;

static void
prior_sigaction (int sig, siginfo_t *info, void *uctx)
{
  (void)sig;
  (void)info;
  (void)uctx;
  prior_si_ran = 1;
}

/* The displaced handler may be a SA_SIGINFO one -- CPython's is -- so the
   chain has to call it through sa_sigaction with all three arguments, not
   through sa_handler. Getting this wrong silently drops the interpreter's
   own handler. */
static void
test_chaining_calls_a_sigaction_predecessor (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = prior_sigaction;
  sa.sa_flags     = SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  DP_CHECK (sigaction (SIGUSR2, &sa, NULL) == 0);

  prior_si_ran = 0;
  dp_resume ();
  DP_CHECK (dp_interrupt_on_signal (SIGUSR2) == DP_OK);

  raise (SIGUSR2);

  DP_CHECK (dp_interrupted () != 0);
  DP_CHECK (prior_si_ran == 1);

  DP_CHECK (dp_restore_signal (SIGUSR2) == DP_OK);
  dp_resume ();

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = SIG_DFL;
  DP_CHECK (sigaction (SIGUSR2, &sa, NULL) == 0);
}

/* A signal that cannot be caught fails the install rather than reporting a
   success nobody can rely on. */
static void
test_uncatchable_signal_is_refused (void)
{
  DP_CHECK (dp_interrupt_on_signal (SIGKILL) == DP_ERR_INVALID);
}

/* The slot table is finite. Filling it must refuse the next install rather
   than silently forget the handler it would have displaced. */
static void
test_slot_exhaustion_is_refused (void)
{
  static const int sigs[] = { SIGUSR1, SIGUSR2, SIGCHLD, SIGCONT,
                              SIGURG,  SIGXCPU, SIGXFSZ, SIGVTALRM };
  const int        n      = (int)(sizeof sigs / sizeof sigs[0]);

  for (int i = 0; i < n; i++)
    DP_CHECK (dp_interrupt_on_signal (sigs[i]) == DP_OK);

  /* A ninth, distinct from all eight above. */
  DP_CHECK (dp_interrupt_on_signal (SIGPROF) == DP_ERR_INVALID);

  for (int i = 0; i < n; i++)
    DP_CHECK (dp_restore_signal (sigs[i]) == DP_OK);
  dp_resume ();
}

/* The process-global rendezvous, at the layer that owns it.
 *
 * The DEFECT it exists for (doppler#976) is invisible here by construction:
 * it needs several `.so` files in one process, so the gate that reproduces
 * it is src/doppler/tests/test_interrupt_is_process_wide.py. What C can
 * pin, and what nothing else does, is that the two accessors just-makeit
 * generates calls to actually redirect the reads -- an adopt that silently
 * did nothing would leave that Python gate proving only that an import
 * succeeded.
 *
 * Restores the module's own state at the end, because every other test in
 * this file reads through the same pointer. */
static void
test_adopting_a_foreign_state_redirects_the_flag (void)
{
  void *own = dp_interrupt_guard_state_ptr ();
  DP_CHECK (own != NULL);

  dp_resume ();
  dp_set_interrupt_latency_ms (250);
  DP_CHECK (dp_interrupt_latency_ms () == 250);

  /* A second state. The contract is `void *` and carries no size, so the
     test cannot ask how big one is -- it over-allocates generously and
     over-aligns, which is sound for a buffer only ever WRITTEN through.
     It must not be seeded by copying the real state: `own` points at an
     object of the shape this TU uses, not at 64 bytes, and a memcpy of
     `sizeof foreign` from it reads past the end of a global. That is the
     bug ASAN caught here -- the buffer knew a size the source did not.
     Zero is the seed it wanted anyway: static storage is already zeroed,
     which for any layout of this state is a clear flag. */
  static union
  {
    unsigned char bytes[sizeof (void *) * 8];
    long double   align_ld;
    void         *align_p;
  } foreign;
  dp_interrupt_guard_state_adopt (&foreign);
  DP_CHECK (dp_interrupt_guard_state_ptr () == (void *)&foreign);

  /* Writes land in the adopted state, not the original. */
  dp_set_interrupt_latency_ms (37);
  DP_CHECK (dp_interrupt_latency_ms () == 37);
  dp_interrupt ();
  DP_CHECK (dp_interrupted ());

  /* NULL is ignored rather than obeyed: a failed rendezvous must not
     replace a working state with an unusable one. */
  dp_interrupt_guard_state_adopt (NULL);
  DP_CHECK (dp_interrupt_guard_state_ptr () == (void *)&foreign);

  /* Back to this TU's own state, which must still hold what it held. */
  dp_interrupt_guard_state_adopt (own);
  DP_CHECK (dp_interrupt_guard_state_ptr () == own);
  DP_CHECK (dp_interrupt_latency_ms () == 250);
  DP_CHECK (!dp_interrupted ());

  dp_set_interrupt_latency_ms (0);
  dp_resume ();
}

int
main (void)
{
  test_flag_round_trip ();
  test_adopting_a_foreign_state_redirects_the_flag ();
  test_latency_knob ();
  test_signal_sets_the_flag ();
  test_installing_twice_is_not_an_error ();
  test_chaining_calls_the_previous_handler ();
  test_restoring_what_was_never_installed_fails ();
  test_restore_puts_the_old_handler_back ();
  test_chaining_calls_a_sigaction_predecessor ();
  test_uncatchable_signal_is_refused ();
  test_slot_exhaustion_is_refused ();

  DP_TEST_END ("test_dp_interrupt");
}
