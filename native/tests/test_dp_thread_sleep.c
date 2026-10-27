/* dp_thread_sleep_us sleeps at least the time it was asked for, even when
 * signals keep interrupting it (doppler#2647).
 *
 * nanosleep() returns early with EINTR when a signal handler runs, and the
 * sleep used to end there, short of the "at least `us`" its doc promises. A
 * repeating interval timer delivers SIGALRM every 2 ms to a handler
 * installed WITHOUT SA_RESTART (nanosleep is never restarted anyway), and a
 * 40 ms sleep must still take 40 ms. The handler counts its calls, so the
 * test also proves the sleep really was interrupted: a pass with no signal
 * delivered would prove nothing.
 *
 * POSIX only: the Windows branch is Sleep(), which signals do not cut short.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "doppler/dp_thread.h"
#include "dp_test.h"

#if defined(_WIN32)
int
main (void)
{
  return 0;
}
#else
#include <signal.h>
#include <sys/time.h>
#include <time.h>

static volatile sig_atomic_t g_alarms = 0;

static void
on_alarm (int sig)
{
  (void)sig;
  g_alarms++;
}

static double
now_s (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int
main (void)
{
  struct sigaction sa;
  sa.sa_handler = on_alarm;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0; /* no SA_RESTART: every delivery interrupts the sleep */
  DP_REQUIRE (sigaction (SIGALRM, &sa, NULL) == 0);

  struct itimerval it = { { 0, 2000 }, { 0, 2000 } }; /* every 2 ms */
  DP_REQUIRE (setitimer (ITIMER_REAL, &it, NULL) == 0);

  const unsigned us = 40000u;
  const double   t0 = now_s ();
  dp_thread_sleep_us (us);
  const double slept = now_s () - t0;

  struct itimerval off = { { 0, 0 }, { 0, 0 } };
  (void)setitimer (ITIMER_REAL, &off, NULL);

  DP_CHECK_MSG (g_alarms > 0, "no signal arrived: the test proved nothing");
  DP_CHECK_MSG (slept >= (double)us * 1e-6,
                "a signal cut the sleep short of the time asked for");
  printf ("slept %.1f ms of 40 ms across %d signals\n", slept * 1e3,
          (int)g_alarms);
  DP_TEST_END ("test_dp_thread_sleep");
}
#endif
