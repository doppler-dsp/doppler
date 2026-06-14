/*
 * dp_coherent_freq.c — nearest leakage-free coherent test frequency.
 *
 * Snaps f_target to J*fs/N with J the nearest integer cycle count coprime with
 * N: an integer number of cycles in the capture (no leakage) and J coprime to
 * N (quantisation-noise correlation minimised) — the IEEE-1241 coherent setup.
 */
#include "measure/measure_core.h"

#include <math.h>

static size_t
gcd_sz (size_t a, size_t b)
{
  while (b)
    {
      size_t t = a % b;
      a        = b;
      b        = t;
    }
  return a;
}

double
dp_coherent_freq (double fs, double f_target, size_t N)
{
  if (N < 2 || fs <= 0.0)
    return 0.0;
  long J = (long)lround (f_target / fs * (double)N);
  if (J < 1)
    J = 1;
  if (J > (long)N - 1)
    J = (long)N - 1;
  for (long d = 0; d <= (long)N; d++) /* nudge to nearest coprime J */
    {
      if (J + d <= (long)N - 1 && gcd_sz ((size_t)(J + d), N) == 1)
        {
          J += d;
          break;
        }
      if (J - d >= 1 && gcd_sz ((size_t)(J - d), N) == 1)
        {
          J -= d;
          break;
        }
    }
  return (double)J * fs / (double)N;
}
