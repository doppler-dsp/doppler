/* dp_interrupt — the one flag every blocking wait in doppler consults.
 *
 * Moved here from native/src/stream/stream_core.c when it acquired a second
 * caller. That move is the whole point: stream_core lives in the optional
 * libdoppler_stream component, so a core-only build -- a file writer, a ring
 * buffer, no NATS anywhere -- could not link the primitive it needed. Nothing
 * had to be rewritten to move it, because it never had a NATS dependency: a
 * volatile sig_atomic_t and four accessors over libc.
 *
 * See docs/design/io-termination.md.
 */

#include "dp_interrupt.h"

#include "clib_common.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* Process-wide, and sig_atomic_t because a signal handler writes it. That
   type is the ONLY thing the C standard promises can be assigned from a
   handler without tearing, which is what makes dp_interrupt() safe to call
   from one -- and being safe to call from a handler is the entire point of
   the API. */
static volatile sig_atomic_t dp_interrupt_flag = 0;

/* Not sig_atomic_t: only ordinary code writes it, and a wait slice reading
   a torn value would at worst wait a wrong-but-bounded time once. */
static unsigned dp_interrupt_latency = DP_INTERRUPT_LATENCY_DEFAULT_MS;

void
dp_interrupt (void)
{
  dp_interrupt_flag = 1;
}

void
dp_resume (void)
{
  dp_interrupt_flag = 0;
}

int
dp_interrupted (void)
{
  return dp_interrupt_flag != 0;
}

void
dp_set_interrupt_latency_ms (unsigned ms)
{
  dp_interrupt_latency = ms ? ms : DP_INTERRUPT_LATENCY_DEFAULT_MS;
}

unsigned
dp_interrupt_latency_ms (void)
{
  return dp_interrupt_latency;
}

/* Eight is not a budget anyone reasoned about, it is "more signals than a
   program sensibly interrupts on". Installing on a ninth fails loudly
   rather than silently forgetting the handler it replaced. */
#define DP_SIG_SLOTS 8
static struct
{
  int              sig;
  struct sigaction prev;
  int              used;
} dp_sig_slots[DP_SIG_SLOTS];

static void
dp_sig_forward (int sig, siginfo_t *info, void *uctx)
{
  dp_interrupt_flag = 1;

  /* Chain. The previous handler is usually an interpreter's own, and its
     absence is how "Ctrl+C works during a receive" turns into "Ctrl+C
     works ONLY during a receive". */
  for (int i = 0; i < DP_SIG_SLOTS; i++)
    {
      if (!dp_sig_slots[i].used || dp_sig_slots[i].sig != sig)
        continue;
      struct sigaction *p = &dp_sig_slots[i].prev;
      if ((p->sa_flags & SA_SIGINFO) && p->sa_sigaction)
        p->sa_sigaction (sig, info, uctx);
      else if (p->sa_handler != SIG_DFL && p->sa_handler != SIG_IGN
               && p->sa_handler)
        p->sa_handler (sig);
      return;
    }
}

int
dp_interrupt_on_signal (int sig)
{
  int slot = -1;
  for (int i = 0; i < DP_SIG_SLOTS; i++)
    {
      if (dp_sig_slots[i].used && dp_sig_slots[i].sig == sig)
        return DP_OK; /* already ours; installing twice chains to us */
      if (slot < 0 && !dp_sig_slots[i].used)
        slot = i;
    }
  if (slot < 0)
    return DP_ERR_INVALID;

  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = dp_sig_forward;
  sa.sa_flags     = SA_SIGINFO | SA_RESTART;
  sigemptyset (&sa.sa_mask);

  if (sigaction (sig, &sa, &dp_sig_slots[slot].prev) != 0)
    return DP_ERR_INVALID;

  dp_sig_slots[slot].sig  = sig;
  dp_sig_slots[slot].used = 1;
  return DP_OK;
}

int
dp_restore_signal (int sig)
{
  for (int i = 0; i < DP_SIG_SLOTS; i++)
    {
      if (!dp_sig_slots[i].used || dp_sig_slots[i].sig != sig)
        continue;
      int rc               = sigaction (sig, &dp_sig_slots[i].prev, NULL);
      dp_sig_slots[i].used = 0;
      return (rc == 0) ? DP_OK : DP_ERR_INVALID;
    }
  return DP_ERR_INVALID;
}

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
  int      sigs[DP_SIG_SLOTS];
  size_t   nsigs;      /* how many of sigs[] this guard installed */
  unsigned latency_ms; /* 0 = did not touch the process setting */
  unsigned saved_latency_ms;
};

dp_interrupt_guard_t *
dp_interrupt_guard_create (const int *signals, size_t n_signals,
                           unsigned latency_ms)
{
  if (n_signals > DP_SIG_SLOTS || (n_signals > 0 && !signals))
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
      if (dp_interrupt_on_signal (signals[i]) != DP_OK)
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
