/*
 * measure_proc_gain.c — FFT processing gain in dB: 10*log10(nfft / 2).
 */
#include "measure/measure_core.h"

#include <math.h>

double
measure_proc_gain (size_t nfft)
{
  return 10.0 * log10 ((double)nfft / 2.0);
}
