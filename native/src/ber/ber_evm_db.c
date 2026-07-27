/*
 * ber_evm_db.c — ber module-level function. Implementation notes and the
 * measured consequences of getting the window or the order wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "ber/ber_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double
ber_evm_db (const float complex *rx, size_t rx_len, size_t lo, size_t hi,
            int m)
{
  size_t n;
  double smr = 0.0, smi = 0.0, p = 0.0, scale, phi, cr, sr, step, errsq = 0.0;

  if (!rx)
    return 0.0;
  if (hi > rx_len)
    hi = rx_len;
  if (hi <= lo || hi - lo < 20)
    return 0.0;
  if (m < 2)
    m = 2;
  n = hi - lo;

  /* sum z^m (repeated multiply — m is 2, 4 or 8 in practice) and the mean
     power, in one pass. arg(sum z^m)/m is the M-fold generalisation of the
     familiar BPSK squaring angle. */
  for (size_t i = lo; i < hi; i++)
    {
      double re = (double)crealf (rx[i]), im = (double)cimagf (rx[i]);
      double zr = re, zi = im;
      p += re * re + im * im;
      for (int q = 1; q < m; q++)
        {
          double nr = zr * re - zi * im;
          zi        = zr * im + zi * re;
          zr        = nr;
        }
      smr += zr;
      smi += zi;
    }
  scale = sqrt (p / (double)n);
  if (scale < 1e-20)
    return 0.0;
  phi  = atan2 (smi, smr) / (double)m; /* constellation rotation */
  cr   = cos (-phi);
  sr   = sin (-phi);
  step = 2.0 * M_PI / (double)m;
  for (size_t i = lo; i < hi; i++)
    {
      double re = (double)crealf (rx[i]), im = (double)cimagf (rx[i]);
      double dr = (re * cr - im * sr) / scale; /* de-rotated, unit power */
      double di = (re * sr + im * cr) / scale;
      /* nearest of the m unit-modulus points */
      double th = step * (double)lround (atan2 (di, dr) / step);
      double er = dr - cos (th), ei = di - sin (th);
      errsq += er * er + ei * ei;
    }
  {
    double evm = sqrt (errsq / (double)n); /* |ref| = 1 */
    return (evm > 0.0) ? 20.0 * log10 (evm) : -120.0;
  }
}
