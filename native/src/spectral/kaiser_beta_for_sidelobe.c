/*
 * kaiser_beta_for_sidelobe.c — Kaiser beta from a window sidelobe-level
 * target.
 *
 * Maps a desired peak-sidelobe attenuation A (dB) of the *window's own*
 * spectrum to the shape parameter beta that achieves it.  This is the
 * window-design companion to kaiser_window()/kaiser_enbw() and is what the
 * measurement suite uses to pick the minimum beta whose sidelobes sit below
 * the required dynamic range — so window leakage never caps SFDR/SNR.
 *
 * NB this is NOT the same as doppler.resample's kaiser_beta(): that uses the
 * Kaiser-Schafer *FIR filter* formula (beta = 0.1102*(A-8.7)), where A is a
 * filter stopband ripple.  A Kaiser window's own peak sidelobe is ~13 dB
 * higher than the equal-beta filter's stopband, so a window needs the larger
 * beta this function returns.  Verified against an FFT of the window: this
 * formula gives beta=12 for a 90 dB target, and a beta-12 window measures a
 * -90 dB peak sidelobe.
 */
#include "spectral/spectral_core.h"

#include <math.h>

double
kaiser_beta_for_sidelobe (double atten_db)
{
  /* Kaiser window-design formula (Kaiser 1974; Oppenheim & Schafer §7.5). */
  if (atten_db > 60.0)
    return 0.12438 * (atten_db + 6.3);
  if (atten_db > 13.26)
    return 0.76609 * pow (atten_db - 13.26, 0.4)
           + 0.09834 * (atten_db - 13.26);
  return 0.0; /* rectangular: peak sidelobe already ~ -13.3 dB */
}
