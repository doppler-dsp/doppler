/* bench_delay_core.c — no step() to benchmark */
#include "delay/delay_core.h"
#include <stdio.h>

int
main (void)
{
  printf ("=== delay benchmark ===\n");
  printf ("  (no step() generated; implement step() to enable)\n");
  return 0;
}
