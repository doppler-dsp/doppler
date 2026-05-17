/* bench_nco_core.c — no step() to benchmark */
#include "nco/nco_core.h"
#include <stdio.h>

int
main (void)
{
  printf ("=== nco benchmark ===\n");
  printf ("  (no step() generated; implement step() to enable)\n");
  return 0;
}
