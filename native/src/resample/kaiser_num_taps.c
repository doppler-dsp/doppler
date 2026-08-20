/*
 * kaiser_num_taps.c — resample module-level function.
 */
#include "resample/resample_core.h"
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int
kaiser_num_taps (int num_phases, double atten, double pb, double sb)
{
  /* A bank has at least one branch. Without this the final division is an
     INTEGER divide by num_phases, so num_phases == 0 is not a wrong answer
     but a SIGFPE -- and this is a public module function, so a Python
     caller passing 0 took the interpreter down with it rather than getting
     an exception. Found by the gh-1034 smoke test, which calls every
     all-scalar function with zeros for exactly this reason. */
  if (num_phases < 1)
    return 0;

  double pb_ph   = pb / (double)num_phases;
  double sb_ph   = sb / (double)num_phases;
  double tw      = sb_ph - pb_ph;
  size_t proto   = (size_t)(1.0 + (atten - 8.0) / (2.285 * (2.0 * M_PI * tw)));
  size_t halflen = proto / 2;
  size_t htaps   = 2 * halflen + 1;
  return (int)(htaps / (size_t)num_phases + 1);
}
