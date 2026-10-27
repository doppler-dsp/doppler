/*
 * kaiser_num_taps.c — resample module-level function.
 */
#include "doppler/resample/resample_core.h"

int
dp_kaiser_num_taps (int num_phases, double atten, double pb, double sb)
{
  /* A bank has at least one branch. Without this the final division is an
     INTEGER divide by num_phases, so num_phases == 0 is not a wrong answer
     but a SIGFPE -- and this is a public module function, so a Python
     caller passing 0 took the interpreter down with it rather than getting
     an exception. Found by the gh-1034 smoke test, which calls every
     all-scalar function with zeros for exactly this reason. */
  if (num_phases < 1)
    return 0;

  /* The formula lives once, header-inline, so resamp's built-in bank uses
     the same code without linking this module (doppler#2652). */
  return (int)dp_kaiser_num_taps_eval ((size_t)num_phases, atten, pb, sb);
}
