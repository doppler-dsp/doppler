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
#include "fft/fft_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief NPRMeasure state: owned window, FFT plan and one-sided power scratch. */
typedef struct {
    fft_state_t   *fft;     /* forward cf32 plan, length nfft   */
    float         *w;       /* window, length n                 */
    float complex *frame;   /* windowed + zero-padded input     */
    float complex *spec;    /* FFT output                       */
    float         *pwr;     /* one-sided power, cg^2-normalised  */
    double cg;              /* coherent gain  = sum(w)          */
    double s2;              /* sum(w^2)                         */
    double enbw;            /* equivalent noise bandwidth (bins)*/
    size_t n;               /* capture length                   */
    size_t nfft;            /* zero-padded transform length     */
    double fs;              /* sample rate (Hz)                 */
    double full_scale;      /* amplitude that equals 0 dBFS     */
} nprmeas_state_t;

/**
 * @brief Create an NPRMeasure analyser.
 * @param window 0 = Hann, 1 = Kaiser.
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
 * @param active_lo,active_hi  active noise band edges (Hz).
 * @param notch_lo,notch_hi    notch band edges (Hz).
 * @param guard_hz             keep-out around the notch edges (Hz).
 * @return 1 (one result in out[0]); 0 if max_out == 0.
 */
size_t nprmeas_analyze(nprmeas_state_t *state, const float *x, size_t n_in,
                       double active_lo, double active_hi, double notch_lo,
                       double notch_hi, double guard_hz, npr_meas_t *out,
                       size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NPRMEAS_CORE_H */
