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
#include "dp_interrupt_guard/dp_interrupt_guard_procglobal.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* The state one process shares: the flag, and the wait slice.

   `sig_atomic_t` because a signal handler writes it. That type is the ONLY
   thing the C standard promises can be assigned from a handler without
   tearing, which is what makes dp_interrupt() safe to call from one -- and
   being safe to call from a handler is the entire point of the API.

   INTERNAL -- it is not in dp_interrupt.h, because nothing outside this
   file has ever needed its shape. The rendezvous below hands it across as
   an opaque `void *`, which is jm's whole contract. */
typedef struct
{
  volatile sig_atomic_t flag;       /* set by a handler; read by waits */
  unsigned              latency_ms; /* wait slice, milliseconds */
} dp_interrupt_shared_t;

/* The state a C build uses: one archive, one copy, nothing to adopt.
   `dp_interrupt_own_state` is the storage; `dp_interrupt_shared` is what
   every read and write actually goes through. They are the same object
   until somebody adopts.

   That indirection exists for ONE reason, and it is not C. A Python
   extension links this file STATICALLY, so every module that wants the flag
   gets its own copy, and CPython loads extensions RTLD_LOCAL so the copies
   never unify -- a stop requested in doppler.interrupt could not reach a
   ring wait in doppler.buffer (doppler#976). One module now owns the state
   and the rest adopt a pointer to it.

   A C binary adopts nothing and pays one pointer dereference on a path
   measured at sub-nanosecond; wfmgen and the C tests behave exactly as
   before. */
static dp_interrupt_shared_t dp_interrupt_own_state
    = { 0, DP_INTERRUPT_LATENCY_DEFAULT_MS };

/* Read from a signal handler, so it is only ever assigned ONCE, at import,
   before any handler is installed. Never reassigned while a handler could
   run -- which is what makes dereferencing it from one safe. */
static dp_interrupt_shared_t *dp_interrupt_shared = &dp_interrupt_own_state;

#define dp_interrupt_flag (dp_interrupt_shared->flag)
#define dp_interrupt_latency (dp_interrupt_shared->latency_ms)

/* ── the process-global rendezvous (just-makeit gh-1117) ─────────────────
   These two are the half jm cannot write, and they carry the COMPONENT's
   prefix -- `dp_interrupt_guard` -- rather than this file's, because the
   component is what jm binds: `dp_interrupt` has no manifest fragment,
   while `dp_interrupt_guard`'s core carries these objects. jm generates
   the other half: the owning module (doppler.interrupt) publishes a
   capsule over `_state_ptr()`, and every other linking module imports it
   and calls `_state_adopt()` from its own PyInit_.

   Their prototypes are in the generated
   dp_interrupt_guard/dp_interrupt_guard_procglobal.h, included above so a
   signature that drifts from jm's contract fails to COMPILE rather than
   failing to link in somebody's extension module. */
void *
dp_interrupt_guard_state_ptr (void)
{
  return (void *)dp_interrupt_shared;
}

void
dp_interrupt_guard_state_adopt (void *shared)
{
  /* Adopting the owner's state means adopting its latency too; a module
     that kept its own would answer latency_ms() with a number no wait in
     the process uses. NULL is ignored rather than fatal -- a failed
     rendezvous leaves this module on its own flag, which is exactly the
     pre-existing behaviour and strictly better than a crash at import. */
  if (shared)
    dp_interrupt_shared = (dp_interrupt_shared_t *)shared;
}

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
#define DP_SIG_SLOTS ((int)DP_INTERRUPT_MAX_SIGNALS)
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
