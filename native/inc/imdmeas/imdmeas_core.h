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
#include "psd/psd_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IMDMeasure state: owned window, FFT plan and one-sided power scratch. */
typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg)   */
    float         *pwr;     /* metric working buffer, one-sided power       */
    double enbw;            /* window equivalent noise bandwidth (bins)     */
    size_t lobe_bins;       /* main-lobe half-width L                       */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    double fs;              /* sample rate (Hz)                            */
    double full_scale;      /* amplitude that equals 0 dBFS                */
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
 * @return the IMD metric record (by value; zeroed if no two tones are found).
 */
imd_meas_t imdmeas_analyze(imdmeas_state_t *state, const float *x, size_t n_in);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
