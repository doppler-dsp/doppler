/*
 * design_lowpass.c — filter module-level function.
 */
#include "filter/filter_core.h"
#include "resample/resample_core.h" /* kaiser_num_taps, kaiser_beta */
#include "spectral/spectral_core.h" /* kaiser_window */

#include <math.h>
#include <stdlib.h>

/* <<IMPLEMENT: design_lowpass>> */
void
design_lowpass (double fpass, double fstop, double atten_db, float *out)
{
  /* Sizing/beta use the FIR-filter-design Kaiser formula
   * (doppler.resample.kaiser_beta), not the window-sidelobe formula
   * (doppler.spectral.kaiser_beta_for_sidelobe) — the two are ~13 dB
   * apart for the same beta and only the FIR formula is calibrated for
   * a filter's stopband attenuation. kaiser_window() itself is just the
   * window generator and is agnostic to which beta formula fed it.
   *
   * kaiser_num_taps(num_phases, atten, pb, sb) treats pb/sb as
   * cycles/sample (Nyquist == 0.5) *after* dividing by num_phases — see
   * doppler.resample._build_bank's wc = 2*pi*(pb/num_phases + ...). At
   * num_phases=1 that means pb/sb themselves must already be
   * cycles/sample, so our Nyquist-normalised (1.0 == fs/2) fpass/fstop
   * need the /2 conversion below before being passed in. */
  int    n_taps = kaiser_num_taps (1, atten_db, fpass / 2.0, fstop / 2.0) | 1;
  double beta   = kaiser_beta (atten_db);
  float *w      = malloc ((size_t)n_taps * sizeof *w);
  kaiser_window (w, (size_t)n_taps, (float)beta);

  /* Same Nyquist-normalised -> cycles/sample conversion for the cutoff
   * (midpoint of fpass/2 and fstop/2). */
  double fc = (fpass + fstop) / 4.0;
  double m0 = (n_taps - 1) / 2.0;
  for (int i = 0; i < n_taps; i++)
    {
      double m = i - m0;
      double sinc
          = (m == 0.0) ? 2.0 * fc : sin (2.0 * M_PI * fc * m) / (M_PI * m);
      out[i] = (float)(sinc * w[i]);
    }
  free (w);
}
