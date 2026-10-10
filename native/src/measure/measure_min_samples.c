/*
 * measure_min_samples.c — capture length for a target resolution bandwidth.
 *
 * Plans a capture for the auto-Kaiser window the measurement objects use: the
 * dynamic-range target (override, else ADC bits, else a deep default) selects
 * the Kaiser beta via the Kaiser-Schafer formula, and that window's ENBW
 * (measured by spectral_core's kaiser_enbw on a reference window) sets the
 * bins-per-RBW.  RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).
 * The reference window is PSD's own, the periodic form the measurement
 * objects' composed PSD builds, from dp_psd_window (#2053): the symmetric
 * window it used to read spans n - 1 intervals, so its ENBW was 1024/1023
 * high and every plan 0.1% long, 2 to 21 samples over the smallest capture
 * that meets the RBW (1 kHz to 100 Hz at 1 MHz).  A non-positive target_rbw
 * defaults to span/1000 (span = fs/2 real, fs cplx).
 */
#include "doppler/measure/measure_core.h"

#include "doppler/psd/psd_core.h"
#include "doppler/spectral/spectral_core.h"

#include <math.h>
#include <stdlib.h>

size_t
dp_measure_min_samples (double fs, double target_rbw, size_t bits,
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
  double beta = dp_kaiser_beta_for_sidelobe (dr);

  /* Measure the chosen window's ENBW from a reference window: PSD's, at
   * index 1 (Kaiser), with room for the n + 1 points it is built from.  The
   * periodic Kaiser's ENBW barely depends on its length (at most 5.9e-8
   * relative between 1024 points and 1711 to 21958, measured), so 1024
   * stands for any n. */
  enum
  {
    REF = 1024
  };
  float *w = (float *)malloc ((REF + 1) * sizeof (float));
  if (!w)
    return 0;
  dp_psd_window (w, REF, 1, (float)beta);
  double enbw = (double)dp_kaiser_enbw (w, REF);
  free (w);

  return (size_t)ceil (enbw * fs / target_rbw);
}
