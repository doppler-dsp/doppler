/*
 * measure_min_samples.c — capture length for a target resolution bandwidth.
 *
 * Plans a capture for the auto-Kaiser window the measurement objects use: the
 * dynamic-range target (override, else ADC bits, else a deep default) selects
 * the Kaiser beta via the Kaiser-Schafer formula, and that window's ENBW
 * (measured by spectral_core's kaiser_enbw on a reference window) sets the
 * bins-per-RBW.  RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).
 * A non-positive target_rbw defaults to span/1000 (span = fs/2 real, fs cplx).
 */
#include "measure/measure_core.h"

#include "spectral/spectral_core.h"

#include <math.h>
#include <stdlib.h>

size_t
measure_min_samples (double fs, double target_rbw, size_t bits,
                     double dynamic_range_db, int complex_input)
{
  if (fs <= 0.0)
    return 0;

  if (target_rbw <= 0.0)
    {
      double span = complex_input ? fs : fs * 0.5;
      target_rbw  = span / 1000.0;
    }

  double dr   = measure_resolve_dr (dynamic_range_db, bits);
  double beta = kaiser_beta_for_sidelobe (dr);

  /* Measure the chosen window's ENBW from a reference window. */
  enum
  {
    REF = 1024
  };
  float *w = (float *)malloc (REF * sizeof (float));
  if (!w)
    return 0;
  kaiser_window (w, REF, (float)beta);
  double enbw = (double)kaiser_enbw (w, REF);
  free (w);

  return (size_t)ceil (enbw * fs / target_rbw);
}
