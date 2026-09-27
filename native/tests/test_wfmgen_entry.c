/**
 * @file test_wfmgen_entry.c
 * @brief dp_doppler_wfmgen() leaves the caller's signal handlers as it found
 *        them, on every exit path (doppler#1594).
 *
 * The in-process entry installs SIGINT/SIGTERM handlers FIRST, so a stop
 * signal cannot kill a run before its tail is written. Its header promised a
 * caller "no signal handlers" and "safe to call repeatedly"; in fact the
 * handlers it installed were never put back, so a host that called it once
 * had its own SIGINT handling replaced for the rest of the process. Both an
 * early return (`--version`, before any spec is built) and a full run (the
 * `done:` path) are checked, because a fix at one exit is how the other
 * stays broken.
 *
 * POSIX only: this reads the handler back with sigaction(). On Windows
 * dp_interrupt uses SetConsoleCtrlHandler, which has no read-back.
 */

#include "doppler/wfm/wfmgen.h"
#include "dp_test.h"

#include <signal.h>
#include <stdio.h>

static void
host_handler (int sig)
{
  (void)sig;
}

static int
handler_is_host (int sig)
{
  struct sigaction now;
  if (sigaction (sig, NULL, &now) != 0)
    return 0;
  return now.sa_handler == host_handler;
}

int
main (void)
{
  struct sigaction sa = { 0 };
  sa.sa_handler       = host_handler;
  sigemptyset (&sa.sa_mask);
  DP_REQUIRE (sigaction (SIGINT, &sa, NULL) == 0);
  DP_REQUIRE (sigaction (SIGTERM, &sa, NULL) == 0);

  char *early[] = { "wfmgen", "--version", NULL };
  DP_CHECK (dp_doppler_wfmgen (2, early) == 0);
  DP_CHECK_MSG (handler_is_host (SIGINT) && handler_is_host (SIGTERM),
                "an early return (--version) restores the host's handlers");

  char *full[] = { "wfmgen", "--type",   "tone",      "--count",
                   "8",      "--output", "/dev/null", NULL };
  DP_CHECK (dp_doppler_wfmgen (7, full) == 0);
  DP_CHECK_MSG (handler_is_host (SIGINT) && handler_is_host (SIGTERM),
                "a full run (the done: path) restores the host's handlers");

  DP_TEST_END ("test_wfmgen_entry");
}
