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
 * Lifecycle: create -> `[analyze / analyze_complex / time_stats]*` -> destroy
 *
 * @code
 * // 16-bit ADC: window auto-picked for ~100 dB dynamic range.
 * tonemeas_state_t *m = tonemeas_create(8192, 1.0, 8, 1.0, 16, 0.0, 0);
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
 * Allocate with tonemeas_create().  `nfft = next_pow2(n * MEASURE_PAD)` is the
 * zero-padded transform length; `enbw` is the window's equivalent-noise
 * bandwidth (bins); `lobe_bins` is the main-lobe half-width L over which a
 * component's power is integrated; `spur_guard_bins` (>= L) is the wider
 * fundamental keep-out used by the worst-spur search so the fundamental's own
 * sidelobes are never reported as a spur.
 */
typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg)   */
    float         *pwr;     /* metric working buffer, length nfft          */
    unsigned char *excl;    /* DC/fundamental/harmonic exclusion mask      */
    double enbw;            /* window equivalent noise bandwidth (bins)    */
    double beta;            /* auto-selected Kaiser shape (from DR target)  */
    size_t lobe_bins;       /* main-lobe half-width L, for power integration*/
    size_t spur_guard_bins; /* fundamental keep-out for spur search (>= L)  */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    size_t n_harm;          /* harmonics tracked (k = 2..n_harm)           */
    double fs;              /* sample rate (Hz)                            */
    size_t dc_guard;        /* extra bins excluded beyond L around DC      */
} tonemeas_state_t;

/**
 * @brief Create a ToneMeasure analyser (auto Kaiser window).
 *
 * The window is always Kaiser; its shape is chosen automatically so the
 * sidelobes sit below the requested dynamic range (see measure_resolve_dr()),
 * keeping the resolution bandwidth as fine as @p n allows.  The realised RBW
 * is reported back in every result (::tone_meas_t::rbw_hz).
 *
 * @param n                Capture/frame length (>= 2).
 * @param fs               Sample rate (Hz, > 0).
 * @param n_harmonics      Harmonics to track (k = 2..n_harmonics).
 * @param full_scale       Amplitude that equals 0 dBFS (> 0).  Ignored if
 *                         bits > 0.
 * @param bits             ADC depth: bits>0 sets the 0-dBFS reference to
 *                         2^(bits-1) and, unless overridden, the dynamic-range
 *                         target (6.02*bits + 1.76 + headroom).
 * @param dynamic_range_db Explicit sidelobe/dynamic-range target (dB); used
 *                         when > 0, else derived from @p bits (or a deep
 *                         default when both are 0).
 * @param dc_guard         Extra bins excluded beyond L around DC.
 * @return Heap state, or NULL on bad args / allocation failure.
 * @note Caller must tonemeas_destroy() when done.
 */
tonemeas_state_t *tonemeas_create(size_t n, double fs, size_t n_harmonics,
                                  double full_scale, size_t bits,
                                  double dynamic_range_db, size_t dc_guard);

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
