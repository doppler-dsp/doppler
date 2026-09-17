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

#ifdef _WIN32

/* Windows: one console handler, one armed mask, and ONE table read both
 * ways.
 *
 * Chaining is not the problem here that it is on POSIX -- it is the
 * platform's default. `SetConsoleCtrlHandler` REGISTERS into a list rather
 * than replacing: handlers run in reverse registration order and returning
 * FALSE passes the event to the next one. So "do not silently disable the
 * handler this program already had" is the API's contract rather than
 * something to hand-roll, and the slot table the POSIX path needs for
 * save-and-restore has no counterpart -- removal is the same call with
 * FALSE.
 *
 * That is also why this does NOT use `signal(SIGINT, ...)`. The CRT
 * implements SIGINT on top of its own console handler, so installing over
 * it displaces whatever the CRT was about to tell -- CPython's SIGINT
 * handler, in an extension build. Registering alongside and returning FALSE
 * lets the chain run through the CRT to the interpreter, which is exactly
 * the property `dp_sig_forward` hand-builds on POSIX.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef SIGBREAK
#define SIGBREAK 21
#endif

/* The mapping, and the only place it lives. Install reads sig -> events;
 * the handler reads event -> "is any armed signal interested". One table,
 * both directions, so the two cannot disagree.
 *
 * SIGINT and SIGBREAK are exact: Windows raises CTRL_C_EVENT and
 * CTRL_BREAK_EVENT and they mean what the signals mean.
 *
 * **SIGTERM is doppler's choice, not a platform fact.** The Windows CRT
 * accepts SIGTERM in signal() but the OS never raises it -- only
 * raise(SIGTERM) does. Mapping it onto the close/logoff/shutdown family is
 * this library deciding that "a supervisor is politely asking us to stop"
 * means those events, because that is what wfmgen installs SIGTERM FOR. The
 * alternative -- refusing SIGTERM on Windows -- makes a paced run
 * uninterruptible by anything except Ctrl+C, which is worse and quieter.
 * Stated here and in dp_interrupt.h so a reader does not assume parity.
 *
 * A signal with no entry is refused (DP_ERR_INVALID) rather than silently
 * accepted: an install that reports success and can never fire is the
 * failure this whole file exists to avoid.
 */
static const struct
{
  int   sig;
  DWORD events;
} dp_win_sig_map[] = {
  { SIGINT, 1u << CTRL_C_EVENT },
  { SIGBREAK, 1u << CTRL_BREAK_EVENT },
  { SIGTERM, (1u << CTRL_CLOSE_EVENT) | (1u << CTRL_LOGOFF_EVENT)
                 | (1u << CTRL_SHUTDOWN_EVENT) },
};

static DWORD dp_win_armed;     /* CTRL_* events we currently answer to */
static int   dp_win_installed; /* our handler is in the chain */

static DWORD
dp_win_events_for (int sig)
{
  for (size_t i = 0; i < sizeof dp_win_sig_map / sizeof *dp_win_sig_map; i++)
    if (dp_win_sig_map[i].sig == sig)
      return dp_win_sig_map[i].events;
  return 0;
}

static BOOL WINAPI
dp_win_ctrl_handler (DWORD type)
{
  if (type < 32 && (dp_win_armed & (1u << type)))
    dp_interrupt ();

  /* ALWAYS FALSE. We observe the event, we never consume it: returning TRUE
     would stop the chain and strand every handler registered before ours,
     including the CRT's. The flag is the whole payload. */
  return FALSE;
}

int
dp_interrupt_on_signal (int sig)
{
  DWORD ev = dp_win_events_for (sig);
  if (!ev)
    return DP_ERR_INVALID;

  if (!dp_win_installed)
    {
      if (!SetConsoleCtrlHandler (dp_win_ctrl_handler, TRUE))
        return DP_ERR_INVALID;
      dp_win_installed = 1;
    }
  dp_win_armed |= ev;
  return DP_OK;
}

int
dp_restore_signal (int sig)
{
  DWORD ev = dp_win_events_for (sig);
  if (!ev || !(dp_win_armed & ev))
    return DP_ERR_INVALID;

  dp_win_armed &= ~ev;
  if (!dp_win_armed && dp_win_installed)
    {
      SetConsoleCtrlHandler (dp_win_ctrl_handler, FALSE);
      dp_win_installed = 0;
    }
  return DP_OK;
}

#else /* POSIX */

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

#endif /* _WIN32 */
