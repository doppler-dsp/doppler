/**
 * @file welch_core.h
 * @brief Welch — averaging PSD estimator and spectral measurement suite.
 *
 * A stateful, C-first periodogram averager.  It composes the existing pieces of
 * the library rather than re-implementing them: an ::fft_state_t forward plan, a
 * spectral window (Hann or Kaiser) with its coherent gain and ENBW, an
 * ::acc_trace_state_t per-bin power averager (mean / EMA / max-hold / min-hold),
 * and the spectral free functions (::magnitude_db_cf32, ::find_peaks_f32,
 * ::obw_from_power, ::noise_floor_db) for the derived measurements.
 *
 * Feed complex baseband frames with welch_accumulate(); each length-n frame is
 * windowed, FFT'd, converted to power, fftshifted to DC-centred order and
 * folded into the running average.  Then read:
 *   - welch_psd_db()   : averaged power spectrum, dB   (peak reads tone power)
 *   - welch_psd_dbhz() : averaged PSD, dB/Hz           (ENBW / fs normalised)
 *   - welch_band_power() / welch_total_band_power() : integrated band power, dB
 *   - welch_occupied_bw() : occupied bandwidth, Hz
 *   - welch_noise_floor() / welch_snr() / welch_sfdr() : level statistics, dB
 *
 * All spectra are DC-centred (fftshift), matching find_peaks_f32's bin ->
 * frequency convention (bin i maps to (i - n/2)/n in normalised frequency, so
 * spectral peaks are obtained idiomatically with
 * ``find_peaks_f32(w.psd_db(), n_peaks, min_db)``).
 *
 * Lifecycle: create -> (accumulate / reset)* -> (measurement getters)* -> destroy
 */
#ifndef WELCH_CORE_H
#define WELCH_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Welch state.  Allocate with welch_create().
 */
typedef struct {
    fft_state_t *fft;          /**< Forward cf32 plan, size nfft.         */
    acc_trace_state_t *avg;    /**< Per-bin power averager, length nfft.  */
    float *w;                  /**< Window, length n.                     */
    float complex *frame;      /**< Windowed + zero-padded, length nfft.  */
    float complex *spec;       /**< FFT output scratch, length nfft.      */
    float *pwr;                /**< DC-centred power scratch, length nfft.*/
    float *dbbuf;              /**< dB-trace scratch, length nfft.        */
    double cg;                 /**< Window coherent gain, sum(w).         */
    double s2;                 /**< Window power, sum(w^2).               */
    double enbw;               /**< Equivalent noise bandwidth, bins.     */
    size_t n;                  /**< Window / frame length (samples).      */
    size_t nfft;               /**< Zero-padded transform length.         */
    double fs;                 /**< Sample rate, Hz.                      */
    double full_scale;         /**< Amplitude that reads 0 dBFS.          */
} welch_state_t;

/**
 * @brief Create an averaging PSD estimator.
 *
 * @param n           Window / frame length in samples.  Must be >= 2.
 * @param fs          Sample rate in Hz (used for dB/Hz and band frequencies).
 * @param window      Window index: 0 = Hann, 1 = Kaiser.
 * @param beta        Kaiser beta (ignored for Hann).
 * @param pad         Zero-pad factor (>= 1); nfft = next_pow2(n * pad).
 * @param full_scale  Amplitude that reads 0 dBFS in the dB getters (> 0).
 * @param mode        Averaging mode index (0=mean, 1=exp, 2=maxhold, 3=minhold).
 * @param alpha       EMA smoothing factor (exp mode only).
 * @return Heap-allocated state, or NULL on invalid argument or OOM.
 * @note Caller must call welch_destroy() when done.
 *
 * @code
 * >>> from doppler.spectral import Welch
 * >>> w = Welch(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean")
 * >>> w.n, w.fs
 * (1024, 1000000.0)
 * >>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
 * True
 * @endcode
 */
welch_state_t *welch_create(size_t n, double fs, int window, float beta,
                            size_t pad, double full_scale, int mode,
                            double alpha);

/**
 * @brief Destroy a Welch instance and release all memory.
 * @param state  May be NULL (no-op).
 */
void welch_destroy(welch_state_t *state);

/**
 * @brief Discard the running average; the next accumulate re-seeds it.
 * @param state  Must be non-NULL.
 */
void welch_reset(welch_state_t *state);

/**
 * @brief Window, FFT and fold complex baseband frames into the average.
 * Processes floor(n_in / n) full frames; a trailing partial frame is ignored.
 *
 * @param state  Must be non-NULL.
 * @param x      Complex baseband samples (cf32).
 * @param x_len  Number of samples in @p x.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import Welch
 * >>> n = 64
 * >>> w = Welch(n=n, fs=1.0, window="hann", mode="mean")
 * >>> k = 8
 * >>> x = np.exp(2j*np.pi*k*np.arange(n)/n).astype(np.complex64)
 * >>> for _ in range(4):
 * ...     w.accumulate(x)
 * >>> psd = w.psd_db()
 * >>> psd.shape
 * (64,)
 * >>> int(np.argmax(psd)) == n // 2 + k
 * True
 * >>> w.count
 * 4
 * @endcode
 */
void welch_accumulate(welch_state_t *state, const float complex *x,
                      size_t x_len);

/**
 * @brief Window, zero-pad, FFT and fold real frames into the average.
 * The real-input counterpart to welch_accumulate(): each length-n frame is
 * windowed, zero-padded to nfft, transformed and folded as a DC-centred
 * two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins
 * carry equal power).  Read the one-sided fold with welch_power_onesided().
 * Processes floor(n_in / n) full frames.
 *
 * @param state  Must be non-NULL.
 * @param x      Real samples (f32).
 * @param x_len  Number of samples in @p x.
 */
void welch_accumulate_real(welch_state_t *state, const float *x, size_t x_len);

/** @brief Output capacity hint for welch_power_twosided(); equals nfft. */
size_t welch_power_twosided_max_out(welch_state_t *state);

/**
 * @brief Averaged linear power, DC-centred two-sided (length nfft).
 * Coherent-gain normalised (out[k] = avg|X[k]|^2 / cg^2); full_scale is NOT
 * applied (callers that want a dBFS reference divide by full_scale^2).  This is
 * the raw spectral estimate the measurement kernels integrate over.  Returns 0
 * (and writes nothing) before any frame is accumulated.
 *
 * @param state  Must be non-NULL.
 * @param cap    Caller buffer capacity (must be >= nfft).
 * @param out    Destination, at least nfft float32 elements.
 * @return nfft, or 0 if empty.
 */
size_t welch_power_twosided(welch_state_t *state, size_t cap, float *out);

/** @brief Output capacity hint for welch_power_onesided(); equals nfft/2+1. */
size_t welch_power_onesided_max_out(welch_state_t *state);

/**
 * @brief Averaged linear power, one-sided (length nfft/2 + 1).
 * Folds the DC-centred two-sided estimate onto [0, fs/2]: the DC and Nyquist
 * bins are kept as-is, every interior bin is the sum of its +k and -k halves
 * (so a real-input tone reads 2*avg|X[k]|^2 / cg^2 there).  Coherent-gain
 * normalised; full_scale is NOT applied.  Returns 0 before any accumulate.
 *
 * @param state  Must be non-NULL.
 * @param cap    Caller buffer capacity (must be >= nfft/2 + 1).
 * @param out    Destination, at least nfft/2 + 1 float32 elements.
 * @return nfft/2 + 1, or 0 if empty.
 */
size_t welch_power_onesided(welch_state_t *state, size_t cap, float *out);

/** @brief Output capacity hint for psd_db(); equals nfft. */
size_t welch_psd_db_max_out(welch_state_t *state);

/**
 * @brief Averaged power spectrum in dB, DC-centred.
 * Normalised by window coherent gain so a full-scale tone reads its true power
 * (a unit-amplitude tone peaks near 0 dB).  Returns 0 (Python None) before any
 * frame is accumulated.
 *
 * @param state  Must be non-NULL.
 * @param n      Caller buffer capacity (ignored; buffer is pre-sized to n).
 * @param out    Destination, at least n float32 elements.
 * @return n, or 0 if empty.
 */
size_t welch_psd_db(welch_state_t *state, size_t n, float *out);

/** @brief Output capacity hint for psd_dbhz(); equals n. */
size_t welch_psd_dbhz_max_out(welch_state_t *state);

/**
 * @brief Averaged power spectral density in dB/Hz, DC-centred.
 * Normalised by ``fs * sum(w^2)`` (ENBW-aware), the standard one-sided-free PSD
 * scaling.  Differs from psd_db() by the constant ``10*log10(cg^2/(fs*s2))``.
 * Returns 0 (Python None) before any frame is accumulated.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import Welch
 * >>> w = Welch(n=32, fs=2.0, window="hann", mode="mean")
 * >>> w.accumulate(np.ones(32, dtype=np.complex64))
 * >>> a = w.psd_db(); b = w.psd_dbhz()
 * >>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant
 * True
 * @endcode
 */
size_t welch_psd_dbhz(welch_state_t *state, size_t n, float *out);

/** @brief Output capacity hint for band_power(); 0 (binding sizes from bands). */
size_t welch_band_power_max_out(welch_state_t *state);

/**
 * @brief Integrated power per band in dB.
 * @p bands is a flat array of [lo0, hi0, lo1, hi1, ...] band edges in Hz; the
 * output holds one dB value per band (n_bands = bands_len / 2).  Edges are
 * clamped to the analysed span; a band fully outside the span integrates to the
 * dB floor.  Returns 0 before any frame is accumulated.
 *
 * @param state      Must be non-NULL.
 * @param bands      Flat [lo,hi,...] band edges, Hz.
 * @param bands_len  Number of edge values (2 * n_bands).
 * @param out        Destination, at least n_bands float32 elements.
 * @return n_bands, or 0 if empty.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import Welch
 * >>> w = Welch(n=64, fs=1.0, window="hann", mode="mean")
 * >>> w.accumulate(np.ones(64, dtype=np.complex64))
 * >>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
 * >>> pb.shape
 * (2,)
 * @endcode
 */
size_t welch_band_power(welch_state_t *state, const double *bands,
                        size_t bands_len, float *out);

/**
 * @brief Total integrated power across all bands in dB.
 * @param state      Must be non-NULL.
 * @param bands      Flat [lo,hi,...] band edges, Hz.
 * @param bands_len  Number of edge values (2 * n_bands).
 * @return Total band power in dB (dB floor if empty).
 */
double welch_total_band_power(welch_state_t *state, const double *bands,
                              size_t bands_len);

/**
 * @brief Occupied bandwidth in Hz holding @p fraction of the total power.
 * @param state     Must be non-NULL.
 * @param fraction  Power fraction in (0, 1], e.g. 0.99.
 * @return Occupied bandwidth in Hz (0 if empty or no power).
 */
double welch_occupied_bw(welch_state_t *state, double fraction);

/**
 * @brief Noise-floor estimate: median of the averaged dB spectrum.
 * @return Median dB level (0 if empty).
 */
double welch_noise_floor(welch_state_t *state);

/**
 * @brief In-band SNR in dB: peak level in [lo_hz, hi_hz] minus the noise floor.
 * @param state  Must be non-NULL.
 * @param lo_hz  Band lower edge, Hz.
 * @param hi_hz  Band upper edge, Hz.
 * @return SNR in dB (0 if empty).
 */
double welch_snr(welch_state_t *state, double lo_hz, double hi_hz);

/**
 * @brief Spurious-free dynamic range in dB from the two strongest peaks.
 * @param state   Must be non-NULL.
 * @param min_db  Minimum peak level considered, dB.
 * @return Carrier-minus-highest-spur level in dB (0 if fewer than two peaks).
 */
double welch_sfdr(welch_state_t *state, float min_db);
#ifdef __cplusplus
}
#endif

#endif /* WELCH_CORE_H */
