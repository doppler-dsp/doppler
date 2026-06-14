/*
 * measure_min_samples.c — capture length for a target resolution bandwidth.
 *
 * RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).  The Kaiser ENBW
 * is measured from a reference window (reusing spectral_core's kaiser_enbw)
 * rather than a closed form; Hann is the constant 1.5.
 */
#include "measure/measure_core.h"

#include "spectral/spectral_core.h"

#include <math.h>
#include <stdlib.h>

size_t
measure_min_samples (double fs, double target_rbw, int window, float beta)
{
  if (fs <= 0.0 || target_rbw <= 0.0)
    return 0;
  double enbw;
  if (window == 1) /* Kaiser: measure ENBW from a reference window */
    {
      enum
      {
        REF = 1024
      };
      float *w = (float *)malloc (REF * sizeof (float));
      if (!w)
        return 0;
      kaiser_window (w, REF, beta);
      enbw = (double)kaiser_enbw (w, REF);
      free (w);
    }
  else
    enbw = 1.5; /* Hann */
  return (size_t)ceil (enbw * fs / target_rbw);
}
