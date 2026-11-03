/*
 * acq_fft_hook.c -- the failable plan builder test_acq_regrid_failure
 * routes acq_core.c's plans through (the target renames dp_fft_create to
 * acq_test_fft_create for that test's own translation unit only, so this
 * file, compiled apart, calls the real one).
 */
#include "doppler/fft/fft_core.h"

int g_calls;   /* plans built since the last arm */
int g_fail_at; /* the call that fails; 0 = none */

dp_fft_state_t *
acq_test_fft_create (size_t n, int sign, int nthreads)
{
  g_calls++;
  if (g_fail_at && g_calls == g_fail_at)
    return NULL;
  return dp_fft_create (n, sign, nthreads);
}
