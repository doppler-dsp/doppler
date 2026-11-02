/*
 * measure_min_samples.c — capture length for a target resolution bandwidth.
 *
 * Plans a capture for the auto-Kaiser window the measurement objects use: the
 * dynamic-range target (override, else ADC bits, else a deep default) selects
 * the Kaiser beta via the Kaiser-Schafer formula, and that window's ENBW
 * sets the bins-per-RBW.  The ENBW is the one a PSD kernel computes for its
 * own (periodic, #2053) window at a reference length, so the plan reads the
 * window the measurement will actually apply rather than a second copy of
 * it.  RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).
 * A non-positive target_rbw defaults to span/1000 (span = fs/2 real, fs cplx).
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

  /* The chosen window's ENBW, read from a PSD kernel at a reference length:
   * the measurement objects window through that kernel, so this is their
   * window and not a lookalike. */
  enum
  {
    REF = 1024
  };
  dp_psd_kernel_t k = { 0 };
  if (dp_psd_kernel_init (&k, REF, DP_PSD_WINDOW_KAISER, (float)beta, 1, 1.0)
      != DP_OK)
    {
      dp_psd_kernel_free (&k);
      return 0;
    }
  double enbw = k.enbw;
  dp_psd_kernel_free (&k);

  return (size_t)ceil (enbw * fs / target_rbw);
}
