/*
 * search_fft_hook.c -- the failable allocators test_correlator_regrid_failure
 * routes search_core.c's and correlator_core.c's builds through. The target
 * renames dp_fft_create to search_test_fft_create, and the test defines
 * malloc, calloc and realloc over search_test_malloc / _calloc / _realloc for
 * the two included sources only, so this file, compiled apart, calls the real
 * ones.
 *
 * One counter for all three: a regrid's builds are numbered in the order
 * they run, plans and buffers alike, and the Nth one fails.
 */
#include "doppler/fft/fft_core.h"

#include <stdlib.h>

int g_calls;   /* builds since the last arm */
int g_fail_at; /* the build that fails; 0 = none */

/* Counts one build; true when it is the one armed to fail. */
static int
fails (void)
{
  g_calls++;
  return g_fail_at && g_calls == g_fail_at;
}

dp_fft_state_t *
search_test_fft_create (size_t n, int sign, int nthreads)
{
  return fails () ? NULL : dp_fft_create (n, sign, nthreads);
}

void *
search_test_malloc (size_t n)
{
  return fails () ? NULL : malloc (n);
}

void *
search_test_calloc (size_t n, size_t size)
{
  return fails () ? NULL : calloc (n, size);
}

void *
search_test_realloc (void *p, size_t n)
{
  return fails () ? NULL : realloc (p, n);
}
