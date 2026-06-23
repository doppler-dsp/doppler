/**
 * @file nprmeas_core.h
 * @brief NPRMeasure — notched-noise Noise Power Ratio.
 *
 * Drive the system with band-limited noise containing a deep notch; NPR is the
 * ratio of the mean in-band noise PSD to the mean PSD that folds into the notch
 * (distortion + quantisation + intermodulation).  The band/notch geometry is an
 * analyze() argument, so one estimator can sweep several notch placements.
 *
 * Lifecycle: create -> `[analyze]*` -> destroy
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
    double beta;            /* auto-selected Kaiser shape (from DR target) */
    size_t spur_guard_bins; /* min notch keep-out (bins) from window skirt */
    size_t n;               /* capture / frame length                     */
    size_t nfft;            /* zero-padded transform length               */
    double fs;              /* sample rate (Hz)                           */
} nprmeas_state_t;

/**
 * @brief Create an NPRMeasure analyser (auto Kaiser window).
 *
 * The window is always Kaiser; its shape is auto-selected so the sidelobes sit
 * below the requested dynamic range (see measure_resolve_dr()).  The chosen
 * window also sets a minimum notch keep-out so active-band noise cannot leak
 * into the notch average through the window skirt.
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
nprmeas_state_t *nprmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

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
 *
 * @code
 * >>> from doppler.measure import NPRMeasure
 * >>> import numpy as np
 * >>> rng = np.random.default_rng(0)
 * >>> n = 1 << 15
 * >>> F = np.fft.rfft(rng.standard_normal(n))
 * >>> f = np.fft.rfftfreq(n)
 * >>> F[(f < 0.05) | (f > 0.45)] = 0                 # band-limit to [0.05,0.45]
 * >>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep
 * >>> x = np.fft.irfft(F, n)
 * >>> x = (0.3*x/np.std(x)).astype(np.float32)
 * >>> r = NPRMeasure(n=n, fs=1.0).analyze(x, 0.05, 0.45, 0.20, 0.25, 0.01)
 * >>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs
 * (True, True)
 *
 * @endcode
 */
npr_meas_t nprmeas_analyze(nprmeas_state_t *state, const float *x, size_t n_in,
                           double active_lo, double active_hi, double notch_lo,
                           double notch_hi, double guard_hz);

/** @brief Capacity (== nfft) of the spectrum_dbfs output buffer. */
size_t nprmeas_spectrum_dbfs_max_out(nprmeas_state_t *state);

/**
 * @brief DC-centred dBFS magnitude spectrum of a capture (length nfft).
 * The same averaged PSD the metrics use, for an analyzer-display backdrop.
 */
size_t nprmeas_spectrum_dbfs(nprmeas_state_t *state, const float *x,
                             size_t x_len, float *out);

#ifdef __cplusplus
}
#endif

#endif /* NPRMEAS_CORE_H */
