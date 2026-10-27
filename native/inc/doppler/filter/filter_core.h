/**
 * @file filter_core.h
 * @brief Filter module — public C API.
 */
#ifndef DP_FILTER_CORE_H
#define DP_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/resample/resample_core.h" /* kaiser_num_taps — used by design_lowpass's
                                        generated out_size allocation expression */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

/**
 * @brief Design a Kaiser-windowed-sinc lowpass FIR (float32 taps).
 *
 * The tap count is chosen automatically from the band edges and the
 * attenuation with the Kaiser FIR-length formula,
 * `n = dp_kaiser_num_taps(1, atten_db, fpass / 2, fstop / 2) | 1` (always
 * odd, so the filter is type-I linear phase with an integer group delay of
 * `(n - 1) / 2` samples). The window shape comes from dp_kaiser_beta(), the
 * FIR-design beta formula, not the window-sidelobe one. The ideal cutoff
 * sits at the midpoint of the transition band, `(fpass + fstop) / 2`. Taps
 * are `2 fc sinc(2 fc m) w[m]` with `fc` in cycles/sample, then divided by
 * their sum, so the DC gain is 1 to float precision (the windowed sinc alone
 * misses it by up to a few percent on a short design).
 *
 * @param fpass     Passband edge, Nyquist-normalized (1.0 == fs/2),
 *                  `0 < fpass < fstop`.
 * @param fstop     Stopband edge, Nyquist-normalized (1.0 == fs/2).
 * @param atten_db  Target stopband attenuation in dB (positive).
 * @param out       Output buffer for the taps. The caller must size it to
 *                  `dp_kaiser_num_taps(1, atten_db, fpass / 2.0,
 *                  fstop / 2.0) | 1` floats; that many are written.
 */
void dp_design_lowpass(double fpass, double fstop, double atten_db, float *out);
#ifdef __cplusplus
}
#endif

#endif /* FILTER_CORE_H */
