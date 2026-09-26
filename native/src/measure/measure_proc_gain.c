/*
 * measure_proc_gain.c — FFT processing gain in dB: 10*log10(nfft / 2).
 */
#include "doppler/measure/measure_core.h"

#include <math.h>

double
dp_measure_proc_gain (size_t nfft)
{
  return 10.0 * log10 ((double)nfft / 2.0);
}
