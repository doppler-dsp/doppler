/**
 * @file imdmeas_core.h
 * @brief IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept.
 *
 * Drive two equal-amplitude tones f1<f2; the analyser finds them as the two
 * strongest lobes, integrates each fundamental and the intermodulation products
 * (2f1-f2, 2f2-f1 for IMD3; f2-f1 for IMD2) over their window main lobes (folded
 * into the analysed band), and reports the third/second-order intercepts.
 *
 * Lifecycle: create -> [analyze]* -> destroy
 */
#ifndef IMDMEAS_CORE_H
#define IMDMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "fft/fft_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IMDMeasure state: owned window, FFT plan and one-sided power scratch. */
typedef struct {
    fft_state_t   *fft;
    float         *w;
    float complex *frame;
    float complex *spec;
    float         *pwr;
    double cg;
    double s2;
    double enbw;
    size_t lobe_bins;
    size_t n;
    size_t nfft;
    double fs;
    double full_scale;
} imdmeas_state_t;

/**
 * @brief Create an IMDMeasure analyser.
 * @param n           Capture/frame length (>= 2).
 * @param fs          Sample rate (Hz, > 0).
 * @param window      0 = Hann, 1 = Kaiser.
 * @param beta        Kaiser shape (ignored for Hann).
 * @param pad         Zero-pad factor (>= 1); nfft = next_pow2(n*pad).
 * @param full_scale  Amplitude that equals 0 dBFS (> 0).
 * @return Heap state, or NULL on bad args / allocation failure.
 */
imdmeas_state_t *imdmeas_create(size_t n, double fs, int window, float beta,
                                size_t pad, double full_scale);

/** @brief Destroy an IMDMeasure analyser. @param state May be NULL. */
void imdmeas_destroy(imdmeas_state_t *state);

/** @brief Reset (no-op: each analyze() call is independent). */
void imdmeas_reset(imdmeas_state_t *state);

/**
 * @brief Two-tone IMD/TOI of a real capture (finds the two strongest tones).
 * @return 1 (one result in out[0]); 0 if max_out == 0.
 */
size_t imdmeas_analyze(imdmeas_state_t *state, const float *x, size_t n_in,
                       imd_meas_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
