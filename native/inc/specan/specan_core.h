/**
 * @file specan_core.h
 * @brief Specan — natural-parameter spectrum analyzer (DDC + averaging PSD).
 *
 * A streaming spectrum analyzer that speaks the *instrument* parameters an
 * operator already knows — center frequency, span, resolution bandwidth (RBW),
 * and reference level — instead of the DSP knobs (window length, Kaiser beta,
 * zero-pad factor) underneath them.  It is the C-first home for the mapping
 * that ``doppler.specan``'s engine used to hand-roll in Python.
 *
 * It composes the existing library, re-implementing nothing:
 *
 * ```
 * cf32 in (fs_in)  →  Ddc  (mix center→DC, decimate to fs_out = span·1.28)
 *                  →  PSD (window → zero-pad FFT → cg²-normalised power,
 *                            averaged over `navg` segments)
 *                  →  crop to the central ±span/2 display band
 *                  →  dB + ref offset  →  float display spectrum
 * ```
 *
 * - ::ddc_state_t is the tuner/decimator (LO mix + RateConverter cascade);
 *   retuning the center is a cheap, seamless LO phase change.
 * - ::psd_state_t is the one averaging-PSD core shared with the measurement
 *   suite; `navg = 1` gives a responsive single-periodogram frame, larger
 *   `navg` trades update rate for a smoother, lower-variance trace.
 *
 * The display band length and the bin→frequency map are fixed at create time:
 * bin `i` of the returned spectrum maps to
 * ``center + (i − disp_n/2)·fs_out/nfft`` Hz.  Peaks are intentionally NOT
 * computed here — compose ::find_peaks_f32 on the returned trace.
 *
 * Lifecycle: create → (execute / retune / reset)* → destroy.
 *
 * @code
 * // 200 kHz span, 500 Hz RBW around DC of a 2.048 MHz cf32 stream
 * specan_state_t *sa = specan_create(2.048e6, 200e3, 500.0, 0.0, 0.0,
 *                                    0.0, 1, 1);
 * float disp[8192];
 * size_t n = specan_execute(sa, iq, 65536, disp, 8192);  // 0 until a frame
 * specan_destroy(sa);
 * @endcode
 */
#ifndef SPECAN_CORE_H
#define SPECAN_CORE_H

#include "ddc/ddc_core.h"
#include "psd/psd_core.h"
#include <complex.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "acc_trace/acc_trace_core.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Specan state.  Allocate with specan_create().
   */
  typedef struct
  {
    ddc_state_t   *ddc;      /**< Tuner + decimator (mix to DC, resample).   */
    psd_state_t *psd;      /**< Averaging PSD at the decimated rate.       */
    float complex *scratch;  /**< Ddc output scratch, capacity scratch_cap.  */
    size_t scratch_cap;      /**< Elements allocated in @ref scratch.        */
    float complex *pend;     /**< Decimated samples awaiting a frame.        */
    size_t         pend_len; /**< Valid samples in @ref pend.                */
    size_t         pend_cap; /**< Elements allocated in @ref pend.           */
    float         *pwr;   /**< Two-sided linear power scratch, length nfft.*/
    double         fs_in; /**< Input sample rate, Hz.                     */
    double src_center;    /**< Source center frequency, Hz.              */
    double center;        /**< Display center frequency, Hz.             */
    double span;          /**< Display span, Hz.                         */
    double rbw;           /**< Requested resolution bandwidth, Hz.       */
    double offset_db;     /**< Additive dB offset on the display (dBm cal).*/
    double fs_out;        /**< Decimated rate, Hz (= span·1.28, ≤ fs_in).*/
    double beta;          /**< Kaiser beta realising @ref rbw.           */
    size_t n;             /**< Segment / window length (samples).        */
    size_t nfft;          /**< Zero-padded transform length.             */
    size_t navg;          /**< Segments averaged per emitted frame.      */
    size_t disp_n;        /**< Display band length (cropped bins).       */
    size_t disp_lo;       /**< First display bin in the DC-centred array.*/
  } specan_state_t;

  /**
   * @brief Create a natural-parameter spectrum analyzer.
   *
   * Derives the DSP from the instrument parameters: `fs_out = min(span·1.28,
   * fs)`, `n = next_pow2(ceil(fs_out/rbw))` (the coarse RBW knob), a Kaiser
   * `beta` solved so the window ENBW realises `rbw` (the fine knob),
   * `nfft = next_pow2(2·n)`, and the central display crop covering ±span/2.
   *
   * @param fs          Input sample rate (Hz).  Must be > 0.
   * @param span        Display span (Hz).  Must be > 0.
   * @param rbw         Resolution bandwidth (Hz).  Must be > 0.
   * @param src_center  Source center frequency (Hz); the input band is centred
   *                    here, so the analyzer mixes (center − src_center) to
   * DC.
   * @param center      Desired display center frequency (Hz).
   * @param offset_db   Additive dB offset on the display spectrum, applied on
   *                    top of dBFS (e.g. a dBm calibration the application
   *                    computes from a reference level).
   * @param full_scale  Amplitude that reads 0 dBFS (> 0).  Ignored if bits > 0.
   * @param bits        ADC depth: bits>0 sets the 0-dBFS reference to
   *                    2^(bits-1) in the shared PSD core (the single source of
   *                    truth for the dBFS reference).
   * @param window      Window index: 0 = Hann, 1 = Kaiser (RBW-trimmable).
   * @param navg        Segments averaged per emitted frame (>= 1).
   * @return Heap-allocated state, or NULL on invalid argument or OOM.
   * @note Caller must call specan_destroy() when done.  Argument order keeps
   * the required parameters (fs, span, rbw) first, matching the generated
   *       constructor's hoisting of jm `required` init params.
   *
   * @code
   * >>> from doppler.analyzer import Specan
   * >>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)
   * >>> sa.fs_out
   * 256000.0
   * >>> sa.nfft == 2 * sa.n
   * True
   * @endcode
   */
  specan_state_t *specan_create (double fs, double span, double rbw,
                                 double src_center, double center,
                                 double offset_db, double full_scale,
                                 size_t bits, int window, size_t navg);

  /**
   * @brief Destroy a Specan instance and release all memory.
   * @param state  May be NULL (no-op).
   */
  void specan_destroy (specan_state_t *state);

  /**
   * @brief Drop pending samples and the running average; LO/filter history
   * zero.
   * @param state  Must be non-NULL.
   */
  void specan_reset (specan_state_t *state);

  /** @brief Output capacity hint for specan_execute(); equals disp_n. */
  size_t specan_execute_max_out (specan_state_t *state);

  /**
   * @brief Mix, decimate, average and return one display spectrum, or nothing.
   *
   * Feeds @p x through the Ddc, buffers the decimated output, and once
   * `n·navg` decimated samples are available windows + FFTs + averages them
   * into a fresh frame, crops the central ±span/2 band and writes it in dB (+
   * ref_db). Returns 0 (writing nothing) until a frame is ready — the binding
   * maps that to Python ``None``.
   *
   * @param state    Must be non-NULL.
   * @param x        cf32 input block (C-only; the binding passes it).
   * @param x_len    Number of input samples (C-only).
   * @param out      Display-spectrum buffer, dB (C-only).
   * @param max_out  Capacity of @p out (C-only); >= disp_n is sufficient.
   * @return Display bins written (disp_n), or 0 if no frame is ready yet.
   *
   * @code
   * >>> from doppler.analyzer import Specan
   * >>> import numpy as np
   * >>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, navg=1)
   * >>> sa.execute(np.zeros(64, dtype=np.complex64)) is None  # too few samples
   * True
   * >>> frame = sa.execute(np.zeros(65536, dtype=np.complex64))
   * >>> frame.shape, frame.dtype
   * ((801,), dtype('float32'))
   *
   * @endcode
   */
  size_t specan_execute (specan_state_t *state, const float complex *x,
                         size_t x_len, float *out, size_t max_out);

  /**
   * @brief Retune the display center without rebuilding the chain.
   *
   * Updates the Ddc LO phase increment (seamless across blocks — no resampler
   * or window reset) and drops pending samples so the next frame reflects only
   * the new tuning.  Changing the span or RBW requires a destroy + create (the
   * decimation rate and window length change).
   *
   * @param state   Must be non-NULL.
   * @param center  New display center frequency (Hz).
   */
  void specan_retune (specan_state_t *state, double center);

#ifdef __cplusplus
}
#endif

#endif /* SPECAN_CORE_H */
