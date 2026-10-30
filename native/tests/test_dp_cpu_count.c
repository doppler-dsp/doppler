/* dp_cpu_count() counts the CPUs this process may run on, not the machine's
 * online CPUs (doppler#2679).
 *
 * Every "use every core" path (Corr2D/DelayDoppler and AsyncDsssPool at
 * nthreads = 0, dp_parallel's pool) sizes itself from dp_cpu_count(), so
 * under `taskset`, a container cpuset or a cgroup cpuset it used to start
 * one worker per machine CPU and oversubscribe the CPUs it was given.
 *
 * The test pins its own process to one CPU, then to two, and requires the
 * count to follow the mask each time. It needs two usable CPUs to say
 * anything -- on a one-CPU box the online count is already 1 and a pass
 * would prove nothing -- so there it SKIPS (exit 77) rather than passes.
 *
 * Linux only: macOS has no affinity mask to honor, and the Windows branch
 * is GetActiveProcessorCount().
 */
#include "doppler/dp_thread.h"
#include "dp_test.h"

#if !defined(__linux__)
int
main (void)
{
  return 77;
}
#else
#include <sched.h>

/* Pins this process to the first `want` CPUs of `allowed`; returns 0. */
static int
pin_to_first (const cpu_set_t *allowed, int want)
{
  cpu_set_t pick;
  CPU_ZERO (&pick);
  int got = 0;
  for (int c = 0; c < CPU_SETSIZE && got < want; c++)
    if (CPU_ISSET (c, allowed))
      {
        CPU_SET (c, &pick);
        got++;
      }
  return sched_setaffinity (0, sizeof pick, &pick);
}

int
main (void)
{
  cpu_set_t allowed;
  CPU_ZERO (&allowed);
  DP_REQUIRE (sched_getaffinity (0, sizeof allowed, &allowed) == 0);
  const int  usable = CPU_COUNT (&allowed);
  const long online = sysconf (_SC_NPROCESSORS_ONLN);

  DP_CHECK_MSG (dp_cpu_count () == usable,
                "the unpinned count is not the affinity mask's size");
  if (usable < 2)
    {
      printf ("SKIP: %d usable CPU(s), nothing to pin down to\n", usable);
      return 77;
    }

  DP_REQUIRE (pin_to_first (&allowed, 1) == 0);
  const int one = dp_cpu_count ();
  DP_CHECK_MSG (one == 1,
                "pinned to one CPU, the count did not follow the mask");

  DP_REQUIRE (pin_to_first (&allowed, 2) == 0);
  const int two = dp_cpu_count ();
  DP_CHECK_MSG (two == 2,
                "pinned to two CPUs, the count did not follow the mask");

  printf ("online %ld, usable %d; pinned to 1 -> %d, pinned to 2 -> %d\n",
          online, usable, one, two);
  DP_TEST_END ("test_dp_cpu_count");
}
#endif
