/**
 * @file psd_core.h
 * @brief PSD — averaging power-spectral-density estimator (Welch's method),
 *        its per-frame kernel, and spectral measurement suite.
 *
 * Two objects live here. ::dp_psd_kernel_t turns ONE frame into nfft
 * DC-centred power bins and reads them against full scale, with no
 * averaging: the window, the transform and the dBFS reference. PSD holds
 * one by value and folds each frame it makes into a running average; the
 * Spectrogram holds one and emits each frame as a row. A caller that wants
 * single-frame spectra holds a kernel, not a PSD.
 *
 * A stateful, C-first periodogram averager (Welch's method).  It composes the existing pieces of
 * the library rather than re-implementing them: an ::dp_fft_state_t forward plan, a
 * spectral window (Hann, Kaiser, Blackman-Harris or rectangular) with its
 * coherent gain and ENBW, an
 * ::dp_acc_trace_state_t per-bin power averager (mean / EMA / max-hold / min-hold),
 * and the spectral free functions (::dp_magnitude_db_cf32, ::dp_find_peaks_f32,
 * ::dp_obw_from_power, ::dp_noise_floor_db) for the derived measurements.
 *
 * Feed complex baseband frames with dp_psd_accumulate(); each length-n frame is
 * windowed, FFT'd, converted to power, fftshifted to DC-centred order and
 * folded into the running average.  Then read:
 *   - dp_psd_psd_db()   : averaged power spectrum, dB   (peak reads tone power)
 *   - dp_psd_psd_dbhz() : averaged PSD, dB/Hz           (ENBW / fs normalised)
 *   - dp_psd_band_power() / dp_psd_total_band_power() : integrated band power, dB
 *   - dp_psd_occupied_bw() : occupied bandwidth, Hz
 *   - dp_psd_noise_floor() / dp_psd_snr() / dp_psd_sfdr() : level statistics, dB
 *
 * All spectra are DC-centred (fftshift), matching find_peaks_f32's bin ->
 * frequency convention (bin i maps to (i - nfft/2)/nfft in normalised
 * frequency, so
 * spectral peaks are obtained idiomatically with
 * ``find_peaks_f32(w.psd_db(), n_peaks, min_db)``).
 *
 * Lifecycle: create -> (accumulate / reset)* -> (measurement getters)* -> destroy
 *
 * The kernel's: dp_psd_kernel_init() -> (dp_psd_frame_power / _linear /
 * _db)* -> dp_psd_kernel_free().
 */
#ifndef DP_PSD_CORE_H
#define DP_PSD_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The window of dp_psd_create() and dp_psd_kernel_init(), and of the
 *        objects built on them: all four for the Spectrogram (which holds a
 *        kernel), a subset for carrier acquisition (dp_carrier_acq_create()
 *        says which).
 *
 * HANN is 0, so a zeroed configuration is Hann. The Python faces name the
 * same windows "hann", "kaiser", "blackman-harris" and "rect", bound to these
 * constants by name (just-makeit.toml, `[[enum]] psd_window`); carrier
 * acquisition's face names its subset (`carrier_acq_window`).
 */
/* SSOT: enum=psd_window */
enum {
    DP_PSD_WINDOW_HANN = 0,            /**< Hann: the default.              */
    DP_PSD_WINDOW_KAISER = 1,          /**< Kaiser, shaped by `beta`.       */
    DP_PSD_WINDOW_BLACKMAN_HARRIS = 2, /**< 4-term Blackman-Harris.         */
    DP_PSD_WINDOW_RECT = 3, /**< Rectangular: no taper, best resolution,
                                 worst leakage.                         */
};

/**
 * @brief PSD's per-frame kernel: one frame of n samples in, nfft DC-centred
 *        power bins out, and their reading against the full-scale reference.
 *
 * The one place a frame becomes a spectrum. It is held BY VALUE by every
 * object that turns frames into spectra -- PSD, which folds each into its
 * average, and the Spectrogram, which emits each one -- so neither owns a
 * copy of the window, the transform or the dBFS reference, and neither
 * carries the other's machinery. Fill it with dp_psd_kernel_init() and
 * release it with dp_psd_kernel_free(); the frame calls below use its
 * scratch, so one kernel serves one thread at a time.
 */
typedef struct {
    dp_fft_state_t *fft;        /**< Forward cf32 plan, size nfft.         */
    float *w;                   /**< Window, length n.                     */
    float _Complex *frame;      /**< Windowed + zero-padded, length nfft.  */
    float _Complex *spec;       /**< FFT output scratch, length nfft.      */
    float *pwr;                 /**< DC-centred power scratch, length nfft.*/
    double cg;                  /**< Window coherent gain, sum(w).         */
    double s2;                  /**< Window power, sum(w^2).               */
    size_t n;                   /**< Window / frame length (samples).      */
    size_t nfft;                /**< Zero-padded transform length.         */
    double full_scale;          /**< Amplitude that reads 0 dBFS.          */
} dp_psd_kernel_t;

/**
 * @brief Fill a kernel: the window, its gains, the scratch and the plan.
 *
 * @p k is zeroed first, so it is safe to call dp_psd_kernel_free() on it
 * whatever this returns. On a refusal or an allocation failure nothing is
 * left allocated.
 *
 * @param k           The kernel to fill (caller-owned storage).
 * @param n           Frame length in samples; at least 2.
 * @param window      DP_PSD_WINDOW_HANN, _KAISER, _BLACKMAN_HARRIS or
 *                    _RECT.
 * @param beta        Kaiser beta (ignored by every other window). One that
 *                    makes the window non-finite is refused.
 * @param pad         Zero-pad factor (>= 1); nfft = next_pow_two(n * pad).
 * @param full_scale  Amplitude that reads 0 dBFS; finite and > 0.
 * @return DP_OK; DP_ERR_INVALID for an argument out of range, a window with
 *         no gain at n (Hann at 2) or an n * pad too large to size;
 *         DP_ERR_MEMORY when an allocation fails.
 */
int dp_psd_kernel_init(dp_psd_kernel_t *k, size_t n, int window, float beta,
                       size_t pad, double full_scale);

/**
 * @brief Release what dp_psd_kernel_init() allocated, and zero the kernel.
 * @param k  A kernel dp_psd_kernel_init() was called on (any outcome), a
 *           zeroed one, or NULL, which does nothing (as every destroy).
 */
void dp_psd_kernel_free(dp_psd_kernel_t *k);

/**
 * @brief PSD state.  Allocate with dp_psd_create().
 */
typedef struct {
    dp_psd_kernel_t k;         /**< The per-frame kernel; owns the window. */
    dp_acc_trace_state_t *avg;    /**< Per-bin power averager, length nfft.  */
    float *dbbuf;              /**< dB-trace scratch, length nfft.        */
    double enbw;               /**< Equivalent noise bandwidth, bins.     */
    double fs;                 /**< Sample rate, Hz.                      */
    size_t bits;               /**< ADC depth that set full_scale, else 0.*/
    size_t hop;                /**< Samples between frame starts, 1..n.   */
    dp_f32_t *ring;            /**< The framer's ring, 2n, owned by the framer. */
    dp_f32_framer_t fr;        /**< Splits the stream into n-sample frames; carries the partial. */
} dp_psd_state_t;

/**
 * @brief Create an averaging PSD estimator.
 *
 * @param n           Window / frame length in samples.  Must be >= 2.
 * @param fs          Sample rate in Hz (used for dB/Hz and band frequencies);
 *                    finite and > 0.
 * @param window      The window: "hann" (the default), "kaiser",
 *                    "blackman-harris" or "rect" (no taper: best resolution,
 *                    worst leakage).
 * @param beta        Kaiser beta (ignored by every other window). One that
 *                    makes the window non-finite -- NaN, or large enough to
 *                    overflow it -- is refused.
 * @param pad         Zero-pad factor (>= 1; 0 is refused);
 *                    nfft = next_pow_two(n * pad).
 * @param full_scale  Amplitude that reads 0 dBFS in the dB getters; finite
 *                    and > 0.
 *                    Ignored when @p bits > 0.
 * @param bits        ADC depth: when > 0, sets full_scale = 2^(bits-1) (the
 *                    single definition of the dBFS reference); 0 = use
 *                    @p full_scale directly.  At most 64.
 * @param mode        Averaging mode: "mean", "exp", "maxhold" or "minhold".
 * @param alpha       EMA smoothing factor, exp mode only, where it must lie in
 *                    (0, 1]: the rule is the AccTrace averager's, whose
 *                    create refuses anything else, so this does too.
 *                    Ignored, and so not checked, in the other modes.
 * @return Heap-allocated state, or NULL on invalid argument or OOM. A window
 *         whose coherent gain sum(w) is 0 -- Hann at n = 2, whose symmetric
 *         form is all zeros -- is refused too: every reading divides by it.
 *         So is an @p n or @p n * @p pad too large for its buffers to be
 *         sized, rather than wrapped to a small one.
 * @note Caller must call dp_psd_destroy() when done.
 *
 * @code
 * >>> from doppler.spectral import PSD
 * >>> w = PSD(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean")
 * >>> w.n, w.fs
 * (1024, 1000000.0)
 * >>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
 * True
 * @endcode
 */
dp_psd_state_t *dp_psd_create(size_t n, double fs, int window, float beta,
                            size_t pad, double full_scale, size_t bits,
                            int mode, double alpha);

/**
 * @brief Create an averaging PSD estimator whose frames are spaced by a hop.
 *
 * Frame k covers stream samples k*hop through k*hop + n - 1. A hop equal to n
 * gives
 * non-overlapping frames. A trailing partial frame is carried to the next
 * accumulate call, not dropped.
 *
 * @param n           Window / frame length in samples.  Must be >= 2.
 * @param fs          Sample rate in Hz (used for dB/Hz and band frequencies);
 *                    finite and > 0.
 * @param window      The window: "hann" (the default), "kaiser",
 *                    "blackman-harris" or "rect" (no taper: best resolution,
 *                    worst leakage).
 * @param beta        Kaiser beta (ignored by every other window). One that
 *                    makes the window non-finite -- NaN, or large enough to
 *                    overflow it -- is refused.
 * @param pad         Zero-pad factor (>= 1; 0 is refused);
 *                    nfft = next_pow_two(n * pad).
 * @param full_scale  Amplitude that reads 0 dBFS in the dB getters; finite
 *                    and > 0.
 *                    Ignored when @p bits > 0.
 * @param bits        ADC depth: when > 0, sets full_scale = 2^(bits-1) (the
 *                    single definition of the dBFS reference); 0 = use
 *                    @p full_scale directly.  At most 64.
 * @param mode        Averaging mode: "mean", "exp", "maxhold" or "minhold".
 * @param alpha       EMA smoothing factor, exp mode only, where it must lie in
 *                    (0, 1]: the rule is the AccTrace averager's, whose
 *                    create refuses anything else, so this does too.
 *                    Ignored, and so not checked, in the other modes.
 * @param hop         Samples between frame starts, 1..n. 0 means n, which
 *                    gives non-overlapping frames; the default. A hop above
 *                    n would leave samples between frames that are never
 *                    kept, so it is refused.
 * @return Heap-allocated state, or NULL on invalid argument or OOM. A window
 *         whose coherent gain sum(w) is 0 -- Hann at n = 2, whose symmetric
 *         form is all zeros -- is refused too: every reading divides by it.
 *         So is an @p n or @p n * @p pad too large for its buffers to be
 *         sized, rather than wrapped to a small one.
 * @note Caller must call dp_psd_destroy() when done.
 *
 * @code
 * >>> from doppler.spectral import PSD
 * >>> w = PSD(n=1024, fs=1.0e6, window="kaiser", beta=8.0, mode="mean", hop=512)
 * >>> w.n, w.fs
 * (1024, 1000000.0)
 * >>> round(w.rbw / (w.fs / w.n), 3) == round(w.enbw, 3)
 * True
 * @endcode
 */
dp_psd_state_t *dp_psd_create_hop(size_t n, double fs, int window, float beta,
                                 size_t pad, double full_scale, size_t bits,
                                 int mode, double alpha, size_t hop);

/**
 * @brief Destroy a PSD instance and release all memory.
 * @param state  May be NULL (no-op).
 */
void dp_psd_destroy(dp_psd_state_t *state);

/**
 * @brief Discard the running average and any carried partial frame; the
 * next accumulate re-seeds the average and starts the stream again.
 * @param state  Must be non-NULL.
 */
void dp_psd_reset(dp_psd_state_t *state);

/**
 * @brief Window, FFT and fold complex baseband frames into the average.
 * Folds every frame the stream completes: frame k covers stream samples
 * k*hop through k*hop + n - 1 (hop = n unless dp_psd_create_hop() said otherwise).
 * A trailing partial frame is carried to the next call, not dropped: the
 * framer holds it. A single call of whole frames at hop = n averages what
 * dp_psd_accumulate always has.
 *
 * @param state  Must be non-NULL.
 * @param x      Complex baseband samples (cf32).
 * @param x_len  Number of samples in @p x.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import PSD
 * >>> n = 64
 * >>> w = PSD(n=n, fs=1.0, window="hann", mode="mean")
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
void dp_psd_accumulate(dp_psd_state_t *state, const float _Complex *x,
                      size_t x_len);

/**
 * @brief One frame's DC-centred two-sided power: window, zero-pad, FFT, |X|^2.
 *
 * The per-frame kernel dp_psd_accumulate() folds into its average, exposed
 * without the average. The result is the UN-NORMALISED power the averager
 * folds: no division by the window coherent gain squared, so a tone does not
 * read at its true level (dp_psd_frame_db() applies that division). Do not
 * compare this raw output with dp_psd_power_twosided(), which divides by
 * cg^2 on the double path. It stays the raw |X|^2 because that is what the
 * averager folds; a consumer that wants a frame against the dBFS reference
 * -- in linear units to average itself, or in dB -- takes
 * dp_psd_frame_linear() or dp_psd_frame_db(). Uses the kernel's scratch, so
 * it is not thread-safe on one kernel; a PSD's kernel is @c &psd->k, and this
 * does not touch that PSD's running average.
 *
 * @param k      A kernel dp_psd_kernel_init() filled.
 * @param x      Exactly @c k->n complex samples (cf32).
 * @param pwr    Output, @c k->nfft floats, bin k (centred) at index
 *               nfft/2 + k.
 */
void dp_psd_frame_power(dp_psd_kernel_t *k, const float _Complex *x,
                        float *pwr);

/**
 * @brief One frame in linear full-scale^2 units, against the estimator's own
 * reference.
 *
 * dp_psd_frame_power() divided by the window coherent gain squared and the
 * full-scale reference squared, cg^2 * full_scale^2: a full-scale tone on a
 * bin reads 1.0 whatever the window. dp_psd_frame_db() is 10*log10 of the same
 * quotient, with the -200 dB floor -- the two are read from one double, so
 * they cannot disagree about the reference. This is the frame a consumer that
 * averages for itself should take: the raw dp_psd_frame_power() sits
 * 20*log10(sum(w)) above it, 54.18 dB for Hann at 1024 points
 * (docs/design/spectrogram-measurements.md section 5.3). Does not touch a
 * PSD's running average; not thread-safe on one kernel.
 *
 * @param k      A kernel dp_psd_kernel_init() filled.
 * @param x      Exactly @c k->n complex samples (cf32).
 * @param out    Output, @c k->nfft floats, DC-centred.
 *
 * @code
 * dp_psd_kernel_t k;
 * if (dp_psd_kernel_init (&k, 8, DP_PSD_WINDOW_HANN, 0.0f, 1, 1.0) != DP_OK)
 *   return 1;
 * float _Complex x[8];
 * float lin[8];
 * for (int i = 0; i < 8; i++)
 *   x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
 * dp_psd_frame_linear (&k, x, lin);  // unit tone on bin 2, Hann
 * const int bad = fabsf (lin[4 + 2] - 1.0f) > 1e-4f; // bin 2 at nfft/2 + 2
 * dp_psd_kernel_free (&k);           // freed on either path
 * if (bad)
 *   return 1;
 * @endcode
 */
void dp_psd_frame_linear(dp_psd_kernel_t *k, const float _Complex *x,
                         float *out);

/**
 * @brief One frame in dBFS, against the estimator's own reference.
 *
 * dp_psd_frame_power() divided by the window coherent gain squared and the
 * full-scale reference, as 10*log10 with the -200 dB floor, so a full-scale
 * tone on a bin reads 0 dB whatever the window. The result equals
 * dp_psd_psd_db() after accumulating that one frame, bit for bit (pinned by
 * test_psd_core.c). Does not touch a PSD's running average.
 *
 * @param k      A kernel dp_psd_kernel_init() filled.
 * @param x      Exactly @c k->n complex samples (cf32).
 * @param out    Output, @c k->nfft floats, DC-centred.
 *
 * @code
 * dp_psd_kernel_t k;
 * if (dp_psd_kernel_init (&k, 8, DP_PSD_WINDOW_RECT, 0.0f, 1, 1.0) != DP_OK)
 *   return 1;
 * float _Complex x[8];
 * float db[8];
 * for (int i = 0; i < 8; i++)
 *   x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
 * dp_psd_frame_db (&k, x, db);       // unit tone on bin 2, rectangular
 * const int bad = fabsf (db[4 + 2]) > 1e-4f; // bin 2 is at nfft/2 + 2
 * dp_psd_kernel_free (&k);           // freed on either path
 * if (bad)
 *   return 1;
 * @endcode
 */
void dp_psd_frame_db(dp_psd_kernel_t *k, const float _Complex *x,
                     float *out);

/**
 * @brief Window, zero-pad, FFT and fold real frames into the average.
 * The real-input counterpart to dp_psd_accumulate(): each length-n frame is
 * windowed, zero-padded to nfft, transformed and folded as a DC-centred
 * two-sided power spectrum (a real frame is Hermitian, so the +k and -k bins
 * carry equal power).  Read the one-sided fold with dp_psd_power_onesided().
 *
 * This call stays on whole frames at hop = n: it folds floor(n_in / n) frames
 * and drops a trailing partial frame. It neither reads nor writes the carry of
 * the complex accumulate call, so mixing the two on one object mixes two
 * streams, and a hop below n is not used by this call.
 *
 * @param state  Must be non-NULL.
 * @param x      Real samples (f32).
 * @param x_len  Number of samples in @p x.
 */
void dp_psd_accumulate_real(dp_psd_state_t *state, const float *x, size_t x_len);

/** @brief Output capacity hint for dp_psd_power_twosided(); equals nfft. */
size_t dp_psd_power_twosided_max_out(dp_psd_state_t *state);

/**
 * @brief Averaged linear power, DC-centred two-sided (length nfft).
 * Coherent-gain normalised (`out[k]` = avg|`X[k]`|^2 / cg^2); full_scale is NOT
 * applied (callers that want a dBFS reference divide by full_scale^2).  This is
 * the raw spectral estimate the measurement kernels integrate over.  Returns 0
 * (and writes nothing) before any frame is accumulated.
 *
 * @param state  Must be non-NULL.
 * @param cap    Caller buffer capacity (must be >= nfft).
 * @param out    Destination, at least nfft float32 elements.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return min(nfft, max_out), or 0 if empty.
 */
size_t dp_psd_power_twosided(dp_psd_state_t *state, size_t cap, float *out,
                          size_t max_out);

/** @brief Output capacity hint for dp_psd_power_onesided(); equals nfft/2+1. */
size_t dp_psd_power_onesided_max_out(dp_psd_state_t *state);

/**
 * @brief Averaged linear power, one-sided (length nfft/2 + 1).
 * Folds the DC-centred two-sided estimate onto `[0, fs/2]`: the DC and Nyquist
 * bins are kept as-is, every interior bin is the sum of its +k and -k halves
 * (so a real-input tone reads 2*avg|`X[k]`|^2 / cg^2 there).  Coherent-gain
 * normalised; full_scale is NOT applied.  Returns 0 before any accumulate.
 *
 * @param state  Must be non-NULL.
 * @param cap    Caller buffer capacity (must be >= nfft/2 + 1).
 * @param out    Destination, at least nfft/2 + 1 float32 elements.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return min(nfft/2 + 1, max_out), or 0 if empty.
 */
size_t dp_psd_power_onesided(dp_psd_state_t *state, size_t cap, float *out,
                          size_t max_out);

/** @brief Output capacity hint for psd_db(); equals nfft. */
size_t dp_psd_psd_db_max_out(dp_psd_state_t *state);

/**
 * @brief Averaged power spectrum in dB, DC-centred.
 * Normalised by window coherent gain so a full-scale tone reads its true power
 * (a unit-amplitude tone peaks near 0 dB).  Returns 0 (Python None) before any
 * frame is accumulated.
 *
 * @param state  Must be non-NULL.
 * @param n      Caller buffer capacity (ignored; buffer is pre-sized to nfft).
 * @param out    Destination, at least nfft float32 elements.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return min(nfft, max_out), or 0 if empty.
 */
size_t dp_psd_psd_db(dp_psd_state_t *state, size_t n, float *out,
                  size_t max_out);

/** @brief Output capacity hint for psd_dbhz(); equals nfft. */
size_t dp_psd_psd_dbhz_max_out(dp_psd_state_t *state);

/**
 * @brief Averaged power spectral density in dB/Hz, DC-centred.
 * Normalised by ``fs * sum(w^2)`` (ENBW-aware), the standard one-sided-free PSD
 * scaling.  Differs from psd_db() by the constant ``10*log10(cg^2/(fs*s2))``.
 * Returns 0 (Python None) before any frame is accumulated.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import PSD
 * >>> w = PSD(n=32, fs=2.0, window="hann", mode="mean")
 * >>> w.accumulate(np.ones(32, dtype=np.complex64))
 * >>> a = w.psd_db(); b = w.psd_dbhz()
 * >>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant
 * True
 * @endcode
 */
size_t dp_psd_psd_dbhz(dp_psd_state_t *state, size_t n, float *out,
                    size_t max_out);

/** @brief Output capacity hint for band_power(); 0 (binding sizes from bands). */
size_t dp_psd_band_power_max_out(dp_psd_state_t *state);

/**
 * @brief Integrated power per band in dB.
 * @p bands is a flat array of `[lo0, hi0, lo1, hi1, ...]` band edges in Hz; the
 * output holds one dB value per band (n_bands = bands_len / 2).  Edges are
 * clamped to the analysed span; a band fully outside the span integrates to the
 * dB floor.  Returns 0 (Python None) when there is nothing to report: before
 * any frame is accumulated, or when @p bands holds no complete lo/hi pair.
 *
 * @param state      Must be non-NULL.
 * @param bands      Flat `[lo,hi,...]` band edges, Hz.
 * @param bands_len  Number of edge values (2 * n_bands).
 * @param out        Destination, at least n_bands float32 elements.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return The number of bands written, min(n_bands, max_out); 0 (Python
 *         None) when there is nothing to report.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import PSD
 * >>> w = PSD(n=64, fs=1.0, window="hann", mode="mean")
 * >>> w.accumulate(np.ones(64, dtype=np.complex64))
 * >>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
 * >>> pb.shape
 * (2,)
 * @endcode
 */
size_t dp_psd_band_power(dp_psd_state_t *state, const double *bands,
                        size_t bands_len, float *out, size_t max_out);

/**
 * @brief Total integrated power across all bands in dB.
 * @param state      Must be non-NULL.
 * @param bands      Flat `[lo,hi,...]` band edges, Hz.
 * @param bands_len  Number of edge values (2 * n_bands).
 * @return Total band power in dB (dB floor if empty).
 */
double dp_psd_total_band_power(dp_psd_state_t *state, const double *bands,
                              size_t bands_len);

/**
 * @brief Occupied bandwidth in Hz holding @p fraction of the total power.
 *
 * Calls obw_from_power on the averaged spectrum, so the search and its
 * domain are that function's and the two cannot disagree.
 *
 * @param state     Must be non-NULL.
 * @param fraction  Power fraction in the OPEN interval (0, 1), e.g. 0.99. 1 is
 *                  excluded: all of the power in float is every bin that
 *                  rounding residue reached, which has no stable answer.
 * @return Occupied bandwidth in Hz (0 if empty or no power); NaN when
 *         @p fraction is outside (0, 1), before any frame too, rather than a
 *         silently clamped answer.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import PSD
 * >>> w = PSD(n=64, fs=64.0, window="rect")
 * >>> w.accumulate(np.ones(64, dtype=np.complex64))  # all power at DC
 * >>> w.occupied_bw(0.99)                            # one bin, fs / 64
 * 1.0
 * >>> w.occupied_bw(1.0)                             # outside (0, 1)
 * nan
 * @endcode
 */
double dp_psd_occupied_bw(dp_psd_state_t *state, double fraction);

/**
 * @brief Noise-floor estimate: median of the averaged dB spectrum.
 * @return Median dB level (0 if empty).
 */
double dp_psd_noise_floor(dp_psd_state_t *state);

/**
 * @brief In-band SNR in dB: peak level in `[lo_hz, hi_hz]` minus the noise floor.
 * @param state  Must be non-NULL.
 * @param lo_hz  Band lower edge, Hz.
 * @param hi_hz  Band upper edge, Hz.
 * @return SNR in dB (0 if empty).
 */
double dp_psd_snr(dp_psd_state_t *state, double lo_hz, double hi_hz);

/**
 * @brief Spurious-free dynamic range in dB from the two strongest peaks.
 * @param state   Must be non-NULL.
 * @param min_db  Minimum peak level considered, dB.
 * @return Carrier-minus-highest-spur level in dB (0 if fewer than two peaks).
 */
double dp_psd_sfdr(dp_psd_state_t *state, float min_db);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * The blob is the acc_trace power averager, then the framer's snapshot of the
 * carried partial frame. Both children have fixed sizes, so the blob's size
 * depends on the config only. hop is a reject key: a blob of another hop is
 * refused, since the carried frame means something different at another hop.
 * Window, plan, scratch, n, nfft and hop are config, rebuilt by create. */
#define PSD_STATE_MAGIC DP_FOURCC ('P','S','D',' ')
#define PSD_STATE_VERSION 2u
size_t dp_psd_state_bytes (const dp_psd_state_t *state);
void dp_psd_get_state (const dp_psd_state_t *state, void *blob);
int dp_psd_set_state (dp_psd_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* PSD_CORE_H */
