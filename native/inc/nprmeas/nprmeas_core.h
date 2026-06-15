/**
 * @file nprmeas_core.h
 * @brief NPRMeasure — notched-noise Noise Power Ratio.
 *
 * Drive the system with band-limited noise containing a deep notch; NPR is the
 * ratio of the mean in-band noise PSD to the mean PSD that folds into the notch
 * (distortion + quantisation + intermodulation).  The band/notch geometry is an
 * analyze() argument, so one estimator can sweep several notch placements.
 *
 * Lifecycle: create -> [analyze]* -> destroy
 */
#ifndef NPRMEAS_CORE_H
#define NPRMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "psd/psd_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief NPRMeasure state: owned window, FFT plan and one-sided power scratch. */
typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg) */
    float         *pwr;     /* metric working buffer, one-sided power     */
    double enbw;            /* window equivalent noise bandwidth (bins)   */
    size_t n;               /* capture / frame length                     */
    size_t nfft;            /* zero-padded transform length               */
    double fs;              /* sample rate (Hz)                           */
    double full_scale;      /* amplitude that equals 0 dBFS               */
} nprmeas_state_t;

/**
 * @brief Create an NPRMeasure analyser.
 * @param n           Capture/frame length (>= 2).
 * @param fs          Sample rate (Hz, > 0).
 * @param window      0 = Hann, 1 = Kaiser.
 * @param beta        Kaiser shape (ignored for Hann).
 * @param pad         Zero-pad factor (>= 1); nfft = next_pow2(n*pad).
 * @param full_scale  Amplitude that equals 0 dBFS (> 0).
 * @return Heap state, or NULL on bad args / allocation failure.
 */
nprmeas_state_t *nprmeas_create(size_t n, double fs, int window, float beta,
                                size_t pad, double full_scale);

/** @brief Destroy an NPRMeasure analyser. @param state May be NULL. */
void nprmeas_destroy(nprmeas_state_t *state);

/** @brief Reset (no-op: each analyze() call is independent). */
void nprmeas_reset(nprmeas_state_t *state);

/**
 * @brief NPR of a notched-noise capture.
 * @param state      The analyser.
 * @param x          Real time-domain capture.
 * @param n_in       Number of input samples.
 * @param active_lo  Active noise band lower edge (Hz).
 * @param active_hi  Active noise band upper edge (Hz).
 * @param notch_lo   Notch lower edge (Hz).
 * @param notch_hi   Notch upper edge (Hz).
 * @param guard_hz   Keep-out around the notch edges (Hz).
 * @return the NPR metric record (by value).
 */
npr_meas_t nprmeas_analyze(nprmeas_state_t *state, const float *x, size_t n_in,
                           double active_lo, double active_hi, double notch_lo,
                           double notch_hi, double guard_hz);

#ifdef __cplusplus
}
#endif

#endif /* NPRMEAS_CORE_H */
