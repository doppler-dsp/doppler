/* bench_lo_core.c — no step() to benchmark */
#include "lo/lo_core.h"
#include <stdio.h>

int
main (void)
{
  printf ("=== lo benchmark ===\n");
  printf ("  (no step() generated; implement step() to enable)\n");
  return 0;
}
