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
