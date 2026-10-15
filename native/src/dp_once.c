/* dp_once: the platform's once primitive behind one signature (doppler#2328).
 *
 * POSIX hands `fn` straight to pthread_once, whose callback has the same
 * type. Windows' InitOnceExecuteOnce takes a callback of its own shape and
 * one data pointer, so `fn` travels in a box on the caller's stack: the
 * callback runs only inside this call, while the box is alive. Casting the
 * function pointer itself to PVOID would also work on Windows, but ISO C
 * does not define it, and the box costs nothing.
 *
 * Both primitives block the losers until the build returns and publish its
 * writes to them; that is the whole contract dp_once.h states.
 */
#include "doppler/dp_once.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* dp_once_t keeps the INIT_ONCE as one opaque pointer so the header needs no
   <windows.h>; a negative array size refuses the build if that ever stops
   being true. */
typedef char
    dp_once_fits_init_once[sizeof (INIT_ONCE) == sizeof (void *) ? 1 : -1];

typedef struct
{
  void (*fn) (void);
} dp_once_box_t;

static BOOL CALLBACK
dp_once_run (PINIT_ONCE once, PVOID param, PVOID *ctx)
{
  (void)once;
  (void)ctx;
  ((dp_once_box_t *)param)->fn ();
  return TRUE;
}

void
dp_once (dp_once_t *once, void (*fn) (void))
{
  dp_once_box_t box = { fn };
  InitOnceExecuteOnce ((PINIT_ONCE)&once->opaque, dp_once_run, &box, NULL);
}
#else
void
dp_once (dp_once_t *once, void (*fn) (void))
{
  pthread_once (&once->once, fn);
}
#endif
