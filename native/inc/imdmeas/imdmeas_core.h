/**
 * @file imdmeas_core.h
 * @brief IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept.
 *
 * Drive two equal-amplitude tones f1<f2; the analyser finds them as the two
 * strongest lobes, integrates each fundamental and the intermodulation products
 * (2f1-f2, 2f2-f1 for IMD3; f2-f1 for IMD2) over their window main lobes (folded
 * into the analysed band), and reports the third/second-order intercepts.
 *
 * Lifecycle: create -> `[analyze]*` -> destroy
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
    double beta;            /* auto-selected Kaiser shape (from DR target)   */
    size_t lobe_bins;       /* main-lobe half-width L, for power integration*/
    size_t spur_guard_bins; /* tone keep-out for the two-tone search (>= L) */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    double fs;              /* sample rate (Hz)                            */
} imdmeas_state_t;

/**
 * @brief Create an IMDMeasure analyser (auto Kaiser window).
 *
 * The window is always Kaiser; its shape is auto-selected so the sidelobes sit
 * below the requested dynamic range (see measure_resolve_dr()), keeping the
 * resolution bandwidth as fine as @p n allows.
 *
 * @param n                Capture/frame length (>= 2).
 * @param fs               Sample rate (Hz, > 0).
 * @param full_scale       Amplitude that equals 0 dBFS (> 0).  Ignored if
 *                         bits > 0.
 * @param bits             ADC depth: bits>0 sets the 0-dBFS reference to
 *                         2^(bits-1) and, unless overridden, the dynamic-range
 *                         target.
 * @param dynamic_range_db Explicit sidelobe/dynamic-range target (dB); used
 *                         when > 0, else derived from @p bits.
 * @return Heap state, or NULL on bad args / allocation failure.
 */
imdmeas_state_t *imdmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

/** @brief Destroy an IMDMeasure analyser. @param state May be NULL. */
void imdmeas_destroy(imdmeas_state_t *state);

/** @brief Reset (no-op: each analyze() call is independent). */
void imdmeas_reset(imdmeas_state_t *state);

/**
 * @brief Two-tone IMD/TOI of a real capture (finds the two strongest tones).
 * @return the IMD metric record (by value; zeroed if no two tones are found).
 *
 * @code
 * >>> from doppler.measure import IMDMeasure
 * >>> import numpy as np
 * >>> t = np.arange(4096)
 * >>> # two equal tones at 200 & 250 cycles + 3rd-order products 40 dB down
 * >>> x = (np.cos(2*np.pi*200*t/4096) + np.cos(2*np.pi*250*t/4096)
 * ...      + 0.01*np.cos(2*np.pi*150*t/4096)
 * ...      + 0.01*np.cos(2*np.pi*300*t/4096)).astype(np.float32)
 * >>> r = IMDMeasure(n=4096, fs=1.0).analyze(x)
 * >>> round(r.f1, 4), round(r.f2, 4), round(r.imd3_dbc, 0)
 * (0.0488, 0.061, -40.0)
 *
 * @endcode
 */
imd_meas_t imdmeas_analyze(imdmeas_state_t *state, const float *x, size_t n_in);

/** @brief Capacity (== nfft) of the spectrum_dbfs output buffer. */
size_t imdmeas_spectrum_dbfs_max_out(imdmeas_state_t *state);

/**
 * @brief DC-centred dBFS magnitude spectrum of a capture (length nfft).
 * The same averaged PSD the metrics use, for an analyzer-display backdrop.
 */
size_t imdmeas_spectrum_dbfs(imdmeas_state_t *state, const float *x,
                             size_t x_len, float *out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
