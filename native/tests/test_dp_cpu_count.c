/* dp_cpu_count() counts the CPUs this process may run on, not the machine's
 * online CPUs (doppler#2679 on Linux, doppler#2684 on Windows).
 *
 * Every "use every core" path (Corr2D/DelayDoppler and AsyncDsssPool at
 * nthreads = 0, dp_parallel's pool) sizes itself from dp_cpu_count(), so
 * under `taskset`, `start /affinity`, a container cpuset or a cgroup cpuset
 * it used to start one worker per machine CPU and oversubscribe the CPUs it
 * was given.
 *
 * The test pins its own process to one CPU, then to two, and requires the
 * count to follow the mask each time. It needs two usable CPUs to say
 * anything -- on a one-CPU box the online count is already 1 and a pass
 * would prove nothing -- so there it SKIPS (exit 77) rather than passes. On
 * Windows it also skips on a machine with several processor groups, where
 * dp_cpu_count() deliberately does not read the (one-group) mask.
 *
 * macOS skips: it has no process affinity mask to honor.
 */
#include "doppler/dp_thread.h"
#include "dp_test.h"

#if defined(__linux__)
#include <sched.h>

static cpu_set_t g_allowed; /* the mask the process started with */

/* Stores the starting mask in g_allowed and its CPU count in the int
   that usable points to; returns 0 on success. */
static int
start_mask (int *usable)
{
  CPU_ZERO (&g_allowed);
  if (sched_getaffinity (0, sizeof g_allowed, &g_allowed) != 0)
    return -1;
  *usable = CPU_COUNT (&g_allowed);
  return 0;
}

/* Pins this process to the first `want` CPUs it started with; 0 on
   success. */
static int
pin_to_first (int want)
{
  cpu_set_t pick;
  CPU_ZERO (&pick);
  int got = 0;
  for (int c = 0; c < CPU_SETSIZE && got < want; c++)
    if (CPU_ISSET (c, &g_allowed))
      {
        CPU_SET (c, &pick);
        got++;
      }
  return sched_setaffinity (0, sizeof pick, &pick);
}

#elif defined(_WIN32)

static DWORD_PTR g_allowed; /* the mask the process started with */

/* Stores the starting mask in g_allowed and its CPU count in the int
   that usable points to; returns 0 on success. */
static int
start_mask (int *usable)
{
  DWORD_PTR sys = 0;
  if (!GetProcessAffinityMask (GetCurrentProcess (), &g_allowed, &sys))
    return -1;
  int k = 0;
  for (unsigned b = 0; b < 8 * sizeof g_allowed; b++)
    k += (int)((g_allowed >> b) & 1u);
  *usable = k;
  return 0;
}

/* Pins this process to the first `want` CPUs it started with; 0 on
   success. */
static int
pin_to_first (int want)
{
  DWORD_PTR pick = 0;
  int       got  = 0;
  for (unsigned b = 0; b < 8 * sizeof g_allowed && got < want; b++)
    if ((g_allowed >> b) & 1u)
      {
        pick |= (DWORD_PTR)1 << b;
        got++;
      }
  return SetProcessAffinityMask (GetCurrentProcess (), pick) ? 0 : -1;
}

#endif

#if !defined(__linux__) && !defined(_WIN32)
int
main (void)
{
  return 77;
}
#else
int
main (void)
{
#if defined(_WIN32)
  if (GetActiveProcessorGroupCount () != 1)
    {
      printf ("SKIP: several processor groups; the mask covers one\n");
      return 77;
    }
#endif
  int usable = 0;
  DP_REQUIRE (start_mask (&usable) == 0);

  DP_CHECK_MSG (dp_cpu_count () == usable,
                "the unpinned count is not the affinity mask's size");
  if (usable < 2)
    {
      printf ("SKIP: %d usable CPU(s), nothing to pin down to\n", usable);
      return 77;
    }

  DP_REQUIRE (pin_to_first (1) == 0);
  const int one = dp_cpu_count ();
  DP_CHECK_MSG (one == 1,
                "pinned to one CPU, the count did not follow the mask");

  DP_REQUIRE (pin_to_first (2) == 0);
  const int two = dp_cpu_count ();
  DP_CHECK_MSG (two == 2,
                "pinned to two CPUs, the count did not follow the mask");

  printf ("usable %d; pinned to 1 -> %d, pinned to 2 -> %d\n", usable, one,
          two);
  DP_TEST_END ("test_dp_cpu_count");
}
#endif
