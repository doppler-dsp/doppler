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
#ifndef DP_IMDMEAS_CORE_H
#define DP_IMDMEAS_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/measure/measure_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/dp_complex.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IMDMeasure state: owned window, FFT plan and one-sided power scratch. */
typedef struct {
    dp_psd_state_t *psd;     /**< Owned averaging PSD core (Kaiser window,
                                  zero-padded FFT, mean trace); built at
                                  create, reset by every analyze call. */
    float         *pwr;     /**< Owned scratch of nfft floats: the one-sided
                                 power analyze() reads, or the two-sided power
                                 spectrum_dbfs() converts.  Overwritten per
                                 call. */
    double enbw;            /**< Window equivalent noise bandwidth (bins of
                                 the unpadded n-point grid); set at create. */
    double beta;            /**< Kaiser shape chosen at create from the
                                 dynamic-range target (measure_resolve_dr). */
    size_t lobe_bins;       /**< Main-lobe half-width L (bins of the padded
                                 nfft grid): each tone/product's power is
                                 summed over bins c-L..c+L.  Set at create. */
    size_t spur_guard_bins; /**< Tone keep-out (bins, >= L) for the two-tone
                                 search: the tones must sit more than this
                                 far from DC and from each other.  Set at
                                 create. */
    size_t n;               /**< Capture / frame length, samples.            */
    size_t nfft;            /**< Zero-padded transform length, bins.         */
    double fs;              /**< Sample rate, Hz.                            */
} dp_imdmeas_state_t;

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
dp_imdmeas_state_t *dp_imdmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

/** @brief Destroy an IMDMeasure analyser. @param state May be NULL. */
void dp_imdmeas_destroy(dp_imdmeas_state_t *state);

/**
 * @brief Reset the analyser (a no-op: each analyze() call is independent).
 *
 * Every analyze() / spectrum_dbfs() call re-windows and re-transforms its own
 * capture from scratch, so nothing is carried between calls to clear.  The
 * method exists only so IMDMeasure honours the same reset() contract as every
 * other doppler object, letting a generic pipeline reset each stage uniformly.
 *
 * @param state  The analyser (left unchanged).
 *
 * @code
 * >>> from doppler.measure import IMDMeasure
 * >>> m = IMDMeasure(n=4096, fs=1.0)
 * >>> m.reset()            # stateless: provided only for API uniformity
 * >>> m.reset() is None    # returns nothing; safe to call anytime
 * True
 *
 * @endcode
 */
void dp_imdmeas_reset(dp_imdmeas_state_t *state);

/**
 * @brief Two-tone IMD/TOI of a real capture (finds the two strongest tones).
 *
 * Averages the Kaiser-windowed power spectrum over floor(n_in / n) whole
 * frames (a trailing partial frame is dropped), takes the two strongest
 * bins more than spur_guard_bins from DC and from each other as f1 < f2,
 * and integrates each tone and the IM products 2f1-f2, 2f2-f1 and f2-f1
 * (folded into `[0, fs/2]`) over +/- lobe_bins.  `imd3_dbc` is the worse of
 * the two third-order products; both IMD ratios are relative to the mean
 * tone power.  TOI = P_tone + |IMD3|/2 and SOI = P_tone + |IMD2| (dBFS).
 *
 * @param state  The analyser.
 * @param x      Real capture (float32), length @p n_in.
 * @param n_in   Number of samples in @p x; at least n for a result.
 * @return the IMD metric record (by value; zeroed if no two tones are found
 *         or @p n_in < n).
 *
 * @code
 * >>> from doppler.measure import IMDMeasure
 * >>> import numpy as np
 * >>> t = np.arange(4096)
 * >>> # two equal tones at 200 & 250 cycles, plus 3rd-order
 * >>> # products 40 dB down
 * >>> x = (np.cos(2*np.pi*200*t/4096) + np.cos(2*np.pi*250*t/4096)
 * ...      + 0.01*np.cos(2*np.pi*150*t/4096)
 * ...      + 0.01*np.cos(2*np.pi*300*t/4096)).astype(np.float32)
 * >>> r = IMDMeasure(n=4096, fs=1.0).analyze(x)
 * >>> round(r.f1, 4), round(r.f2, 4), round(r.imd3_dbc, 0)
 * (0.0488, 0.061, -40.0)
 *
 * @endcode
 */
imd_meas_t dp_imdmeas_analyze(dp_imdmeas_state_t *state, const float *x, size_t n_in);

/**
 * @brief Capacity (== nfft) of the spectrum_dbfs output buffer.
 * @param state  The analyser.
 * @return nfft, the zero-padded transform length (bins).
 */
size_t dp_imdmeas_spectrum_dbfs_max_out(dp_imdmeas_state_t *state);

/**
 * @brief DC-centred dBFS magnitude spectrum of a capture (length nfft).
 *
 * The same windowed, zero-padded PSD the IMD metrics are read off, laid out
 * DC-centred (fftshifted) and normalised to dBFS for an analyzer-display
 * backdrop.  Use it to see the two fundamentals and the intermodulation
 * products that analyze() integrates.
 *
 * @param state    The analyser.
 * @param x        Real time-domain capture (length @p x_len); averaged
 *                 over its whole n-sample frames like analyze().
 * @param x_len    Number of input samples.
 * @param out      Destination buffer (length >= @p max_out).
 * @param max_out  Capacity of @p out (== nfft).
 * @return Number of bins written to @p out: min(nfft, @p max_out), or 0
 *         when @p x_len < n (no whole frame to transform).
 *
 * @code
 * >>> from doppler.measure import IMDMeasure
 * >>> import numpy as np
 * >>> t = np.arange(4096)
 * >>> x = (0.5*np.cos(2*np.pi*200*t/4096)
 * ...      + 0.5*np.cos(2*np.pi*250*t/4096)).astype(np.float32)
 * >>> s = IMDMeasure(n=4096, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS
 * >>> s.shape
 * (8192,)
 * >>> round(float(s.max()), 1)   # each tone splits into two images
 * -12.0
 *
 * @endcode
 */
size_t dp_imdmeas_spectrum_dbfs(dp_imdmeas_state_t *state, const float *x,
                             size_t x_len, float *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
