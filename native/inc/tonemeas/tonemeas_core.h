/**
 * @file tonemeas_core.h
 * @brief ToneMeasure — single-tone ADC/converter spectral measurement.
 *
 * Owns a window + zero-padded FFT and analyses one time-domain capture (real or
 * complex) into the full single-tone metric bag (::tone_meas_t).  Each
 * component's power is integrated over its window MAIN LOBE and the noise sum
 * excludes the leakage bins around DC, the fundamental and each harmonic — the
 * IEEE Std 1241 method — so a full-scale tone reads ~0 dBFS regardless of where
 * it lands between FFT bins.
 *
 * Lifecycle: create -> [analyze / analyze_complex / time_stats]* -> destroy
 *
 * @code
 * tonemeas_state_t *m = tonemeas_create(8192, 1.0, 1, 12.0f, 2, 8, 1.0, 0);
 * tone_meas_t r = tonemeas_analyze(m, capture, 8192);  // r.enob, r.sfdr_dbc...
 * tonemeas_destroy(m);
 * @endcode
 */
#ifndef TONEMEAS_CORE_H
#define TONEMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "psd/psd_core.h"
#include <complex.h>
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include "acc_trace/acc_trace_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ToneMeasure state: owned window, FFT plan and analysis scratch.
 *
 * Allocate with tonemeas_create().  `nfft = next_pow2(n * pad)` is the
 * zero-padded transform length; `cg`/`s2`/`enbw` are the window's coherent
 * gain, power and equivalent-noise bandwidth (bins); `lobe_bins` is the
 * main-lobe half-width L over which component power is integrated.
 */
typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg)   */
    float         *pwr;     /* metric working buffer, length nfft          */
    unsigned char *excl;    /* DC/fundamental/harmonic exclusion mask      */
    double enbw;            /* window equivalent noise bandwidth (bins)    */
    size_t lobe_bins;       /* main-lobe half-width L (nfft bins)          */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    size_t n_harm;          /* harmonics tracked (k = 2..n_harm)           */
    double fs;              /* sample rate (Hz)                            */
    double full_scale;      /* amplitude that equals 0 dBFS                */
    size_t dc_guard;        /* extra bins excluded beyond L around DC      */
    int    window;          /* 0 hann, 1 kaiser (for amp_uncert_db)        */
} tonemeas_state_t;

/**
 * @brief Create a ToneMeasure analyser.
 * @param n            Capture/frame length (>= 2).
 * @param fs           Sample rate (Hz, > 0).
 * @param window       0 = Hann, 1 = Kaiser.
 * @param beta         Kaiser shape (ignored for Hann).
 * @param pad          Zero-pad factor (>= 1); nfft = next_pow2(n*pad).
 * @param n_harmonics  Harmonics to track (k = 2..n_harmonics).
 * @param full_scale   Amplitude that equals 0 dBFS (> 0).
 * @param dc_guard     Extra bins excluded beyond L around DC.
 * @return Heap state, or NULL on bad args / allocation failure.
 * @note Caller must tonemeas_destroy() when done.
 */
tonemeas_state_t *tonemeas_create(size_t n, double fs, int window, float beta,
                                  size_t pad, size_t n_harmonics,
                                  double full_scale, size_t dc_guard);

/** @brief Destroy a ToneMeasure analyser. @param state May be NULL. */
void tonemeas_destroy(tonemeas_state_t *state);

/** @brief Reset (no-op: the analyser is stateless between calls). */
void tonemeas_reset(tonemeas_state_t *state);

/**
 * @brief Analyse a real capture into the single-tone metric bag.
 * @return the metric record (by value).
 */
tone_meas_t tonemeas_analyze(tonemeas_state_t *state, const float *x,
                             size_t n_in);

/** @brief Analyse a complex baseband capture (two-sided spectrum). */
tone_meas_t tonemeas_analyze_complex(tonemeas_state_t *state,
                                     const float complex *x, size_t n_in);

/** @brief Time-domain statistics of a real capture. */
time_stats_t tonemeas_time_stats(tonemeas_state_t *state, const float *x,
                                 size_t n_in);

/** @brief Capacity (== nfft) of the spectrum_dbfs output buffer. */
size_t tonemeas_spectrum_dbfs_max_out(tonemeas_state_t *state);

/**
 * @brief DC-centred dBFS magnitude spectrum of a real capture (length nfft).
 * @return Number of samples written (nfft).
 */
size_t tonemeas_spectrum_dbfs(tonemeas_state_t *state, const float *x,
                              size_t x_len, float *out);

#ifdef __cplusplus
}
#endif

#endif /* TONEMEAS_CORE_H */
