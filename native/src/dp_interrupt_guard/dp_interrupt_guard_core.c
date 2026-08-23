/* dp_interrupt_guard_core.c — arming, and undoing exactly what was armed. */

#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#include "clib_common.h"

#include <signal.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * The guard — arming, and undoing exactly what was armed.
 *
 * This lived in native/src/stream/stream_ext.c as a PyObject, which put
 * real logic in a binding: it saved and restored the latency, tracked
 * which handlers it had installed, and unwound a partial install. That is
 * the one thing doppler does not do in a wrapper, and it is also why the
 * guard had no C test -- there was no C to test.
 * ------------------------------------------------------------------ */

struct dp_interrupt_guard
{
  int      sigs[DP_INTERRUPT_MAX_SIGNALS];
  size_t   nsigs;      /* how many of sigs[] this guard installed */
  unsigned latency_ms; /* 0 = did not touch the process setting */
  unsigned saved_latency_ms;
};

dp_interrupt_guard_t *
dp_interrupt_guard_create (const int32_t *signals, size_t n_signals,
                           uint32_t latency_ms)
{
  if (n_signals > DP_INTERRUPT_MAX_SIGNALS || (n_signals > 0 && !signals))
    return NULL;

  dp_interrupt_guard_t *g = (dp_interrupt_guard_t *)calloc (1, sizeof *g);
  if (!g)
    return NULL;

  /* Arming clears the flag: a stale one would refuse the first wait
     inside the block that just armed it. */
  dp_resume ();

  if (latency_ms)
    {
      g->latency_ms       = latency_ms;
      g->saved_latency_ms = dp_interrupt_latency_ms ();
      dp_set_interrupt_latency_ms (latency_ms);
    }

  for (size_t i = 0; i < n_signals; i++)
    {
      if (dp_interrupt_on_signal ((int)signals[i]) != DP_OK)
        {
          /* A failed create arms NOTHING. Unwind what this call did, in
             reverse, so a caller is never left holding handlers it has no
             guard to remove. */
          for (size_t j = 0; j < g->nsigs; j++)
            (void)dp_restore_signal (g->sigs[g->nsigs - 1 - j]);
          if (g->latency_ms)
            dp_set_interrupt_latency_ms (g->saved_latency_ms);
          free (g);
          return NULL;
        }
      g->sigs[g->nsigs++] = signals[i];
    }

  return g;
}

void
dp_interrupt_guard_destroy (dp_interrupt_guard_t *guard)
{
  if (!guard)
    return;

  /* Reverse order, so a signal armed by two nested guards is handed back
     to the outer one rather than to whatever was there first. */
  for (size_t i = 0; i < guard->nsigs; i++)
    (void)dp_restore_signal (guard->sigs[guard->nsigs - 1 - i]);

  if (guard->latency_ms)
    dp_set_interrupt_latency_ms (guard->saved_latency_ms);

  /* The flag is deliberately NOT cleared: a caller that was interrupted
     still needs to see that it was, after the block exits. */
  free (guard);
}

void
dp_interrupt_guard_interrupt (dp_interrupt_guard_t *guard)
{
  (void)guard; /* process-wide; the guard is the caller, not the owner */
  dp_interrupt ();
}

int
dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard)
{
  (void)guard;
  return dp_interrupted ();
}

void
dp_interrupt_guard_resume (dp_interrupt_guard_t *guard)
{
  (void)guard;
  dp_resume ();
}

uint32_t
dp_interrupt_guard_latency_ms (const dp_interrupt_guard_t *guard)
{
  (void)guard;
  return (uint32_t)dp_interrupt_latency_ms ();
}
