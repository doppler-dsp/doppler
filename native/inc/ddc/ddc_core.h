/**
 * @file ddc_core.h
 * @brief Digital Down-Converter — composes LO + RateConverter cascade.
 *
 * Two types:
 *
 *   Ddc   — complex (CF32) input.  Chain: LO mix → RateConverter.
 *   DdcR  — real (float32) input.  Chain: halfband R2C → LO mix → RateConverter.
 *
 * Both are streaming (variable block size per execute call).
 *
 * RateConverter selects the cheapest cascade (CIC + optional halfband +
 * polyphase resampler) for the requested rate at create time.  This
 * makes large-ratio decimation (e.g., 100:1) significantly cheaper than
 * a single polyphase stage.
 *
 * ### Ddc signal chain
 *
 * ```
 * CF32 in (fs_in)  →  LO mix  →  RateConverter  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  NCO normalised frequency (cycles/sample at fs_in).
 *             Set to -f_carrier to shift a carrier at f_carrier to DC.
 *
 * ### DdcR signal chain
 *
 * ```
 * float in (fs_in)  →  halfband R2C (2:1, embedded fs/4 shift)
 *                   →  LO mix at intermediate rate (fs_in/2)
 *                   →  RateConverter  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  Fine NCO frequency at the INTERMEDIATE rate (fs_in/2).
 *             To tune a real tone at f_carrier (input normalised) to DC:
 *             set norm_freq = -(2*f_carrier + 0.5).
 *             Total output rate: fs_out = rate * fs_in  (rate < 0.5).
 *
 * DdcR is approximately 2x cheaper than Ddc at equivalent total decimation
 * because the halfband R2C step has an fs/4 frequency shift baked in at
 * zero extra multiplications — the +/-1/0 coefficients multiply for free.
 *
 * ### Retuning vs. rebuilding
 *
 * - **Retune** (centre-frequency change): call ddc_set_norm_freq /
 *   ddcr_set_norm_freq.  Cheap — updates the LO phase increment without
 *   disturbing the resampler history.  Seamless across block boundaries.
 * - **Rate change** (span / decimation change): destroy and recreate the
 *   DDC for the new rate.
 *
 * ### Usage
 *
 * @code
 * // Complex DDC: shift a carrier at +0.1·fs to DC, decimate by 4
 * ddc_state_t *ddc = ddc_create(-0.1, 0.25);
 * float _Complex out[4096];
 * size_t n = ddc_execute(ddc, in, 1024, out, 4096);
 * ddc_destroy(ddc);
 *
 * // Real DDC: same carrier and decimation from a real ADC stream
 * // norm_freq at intermediate rate: -(2 * 0.1 + 0.5) = -0.7
 * ddcr_state_t *ddcr = ddcr_create(-0.7, 0.25);
 * size_t m = ddcr_execute(ddcr, real_in, 1024, out, 4096);
 * ddcr_destroy(ddcr);
 * @endcode
 */
#ifndef DDC_CORE_H
#define DDC_CORE_H

#include <complex.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* ================================================================== */
  /* Ddc — complex-input DDC                                            */
  /* ================================================================== */

  typedef struct ddc_state ddc_state_t;

  /**
   * @brief Create a complex-input Digital Down-Converter.
   * Allocates internal state for the LO and RateConverter cascade.
   * The RateConverter selects the cheapest multi-stage decimation chain
   * (CIC + optional halfband + polyphase resampler) for the given rate.
   *
   * @param norm_freq  LO frequency in cycles/sample at the input rate.
   *                   Set to -f_carrier to shift a carrier at f_carrier
   *                   to DC.  Any real value is accepted.
   * @param rate       Output rate / input rate.  Must be > 0.  Values
   *                   ≥ 1 are up-sampling; typical use is decimation
   *                   (0 < rate < 1).
   * @return Non-NULL on success, NULL on OOM or invalid args.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq
   * -0.1
   * >>> ddc.rate
   * 0.25
   * @endcode
   */
ddc_state_t *ddc_create(double norm_freq, double rate);

  /**
   * @brief Free all resources held by a DDC instance.
   * Releases the RateConverter and LO substructures, then the struct
   * itself.  Passing NULL is a no-op.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> ddc.destroy()   # releases C memory immediately
   * @endcode
   */
void ddc_destroy(ddc_state_t *state);

  /**
   * @brief Zero LO phase and resampler history.
   * After reset, the next execute call produces the same output as the
   * first execute after create — useful for reproducible block-by-block
   * processing or looped test fixtures.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> import numpy as np
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> x = np.ones(64, dtype=np.complex64)
   * >>> y1 = ddc.execute(x)
   * >>> ddc.reset()
   * >>> y2 = ddc.execute(x)
   * >>> bool(np.array_equal(y1, y2))
   * True
   * @endcode
   */
void ddc_reset(ddc_state_t *state);

  /**
   * @brief Return the current LO normalised frequency (cycles/sample).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq
   * -0.1
   * @endcode
   */
double ddc_get_norm_freq(const ddc_state_t *state);

  /**
   * @brief Retune the LO without resetting phase or resampler history.
   * Updates the NCO phase increment atomically so the carrier shift
   * changes seamlessly across block boundaries.  The resampler history
   * and LO phase accumulator are left intact, avoiding the transient
   * that a full reset would cause.
   *
   * @param state  Must be non-NULL.
   * @param val    New normalised frequency (cycles/sample at input rate).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq = -0.2
   * >>> ddc.norm_freq
   * -0.2
   * @endcode
   */
void ddc_set_norm_freq(ddc_state_t *state, double val);

  /**
   * @brief Return the configured output/input rate ratio (read-only).
   * The rate is fixed at create time; change it by destroying and
   * recreating the DDC with the new value.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> ddc.rate
   * 0.25
   * @endcode
   */
double ddc_get_rate(const ddc_state_t *state);

  /**
   * @brief Mix and resample a block of CF32 samples.
   * Multiplies each input sample by the current LO phasor (advancing the
   * NCO phase per sample), then feeds the mixed block into the
   * RateConverter.  The resampler maintains history across calls, so
   * arbitrary block sizes produce contiguous output with no edge
   * artefacts.  Output length ≈ x_len * rate (varies by ±1 due to
   * polyphase indexing).
   *
   * @param state    Must be non-NULL.
   * @param x        CF32 input block; accepted as float32 (auto-cast).
   * @param x_len    Number of input samples (C-only, hidden from Python).
   * @param out      CF32 output buffer (C-only, hidden from Python).
   * @param max_out  Output buffer capacity (C-only, hidden from Python).
   * @return Number of output samples written (C-only).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> import numpy as np
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> t = np.arange(4096)
   * >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
   * >>> y = ddc.execute(x)
   * >>> y.shape
   * (1024,)
   * >>> y.dtype
   * dtype('complex64')
   * >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
   * 1.0
   * @endcode
   */
size_t ddc_execute(ddc_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

  /* ================================================================== */
  /* DdcR — real-input DDC (Architecture D2)                           */
  /* ================================================================== */

  typedef struct ddcr_state ddcr_state_t;

  /**
   * @brief Create a real-input Digital Down-Converter (Architecture D2).
   * The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) →
   * fine LO mix at the intermediate rate (fs_in/2) → RateConverter →
   * CF32 output.  The halfband stage uses ±1/0 coefficients (no
   * multiplications), making DDCR roughly 2× cheaper than DDC at the
   * same total decimation ratio.
   *
   * @param norm_freq  Fine NCO frequency at the intermediate rate
   *                   (fs_in/2, cycles/sample).  To tune a real tone at
   *                   normalised input frequency f_c to DC, set
   *                   norm_freq = -(2*f_c + 0.5).
   * @param rate       Total output/input rate.  Must be in (0, 0.5)
   *                   because the halfband pre-decimates by 2.
   * @return Non-NULL on success, NULL on OOM or invalid args.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq
   * -0.7
   * >>> ddcr.rate
   * 0.25
   * @endcode
   */
  ddcr_state_t *ddcr_create (double norm_freq, double rate);

  /**
   * @brief Free all resources held by a DDCR instance.
   * Releases the halfband, RateConverter, and LO substructures, then
   * the struct itself.  Passing NULL is a no-op.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> ddcr.close()   # releases C memory immediately
   * @endcode
   */
  void ddcr_destroy (ddcr_state_t *s);

  /**
   * @brief Zero halfband filter history, LO phase, and resampler history.
   * After reset, the next execute call reproduces the output of the
   * first call after create, enabling repeatable block-by-block tests.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> x = np.ones(64, dtype=np.float32)
   * >>> out = np.empty(64, dtype=np.complex64)
   * >>> y1 = ddcr.execute(x, out).copy()
   * >>> ddcr.reset()
   * >>> y2 = ddcr.execute(x, out)
   * >>> bool(np.array_equal(y1, y2))
   * True
   * @endcode
   */
  void ddcr_reset (ddcr_state_t *s);

  /**
   * @brief Return the current fine NCO normalised frequency at the
   * intermediate rate (fs_in/2, cycles/sample).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq
   * -0.7
   * @endcode
   */
  double ddcr_get_norm_freq (const ddcr_state_t *s);

  /**
   * @brief Retune the fine NCO without resetting halfband or resampler
   * history.  Updates the LO phase increment only; state is preserved
   * for seamless tuning across block boundaries.
   *
   * @param s         Must be non-NULL.
   * @param norm_freq New frequency at the intermediate rate (fs_in/2).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq = -0.5
   * >>> ddcr.norm_freq
   * -0.5
   * @endcode
   */
  void ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq);

  /**
   * @brief Return the total configured rate (fs_out / fs_in, read-only).
   * This is the end-to-end ratio from ADC input to CF32 output.  Change
   * it by destroying and recreating the DDCR.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> ddcr.rate
   * 0.25
   * @endcode
   */
  double ddcr_get_rate (const ddcr_state_t *s);

  /**
   * @brief Process a block of real float32 samples through the full
   * DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32.
   * The halfband decimates by 2 and applies a built-in +fs/4 frequency
   * shift; the fine NCO then completes the tuning.  State is maintained
   * across calls for contiguous streaming.  Output length ≈ n_in * rate
   * (±1 from polyphase indexing).  A real tone at input normalised
   * frequency f_c has amplitude 0.5 in the baseband output (one-sided
   * spectrum), consistent with analytic signal theory.
   *
   * @param s        Must be non-NULL.
   * @param in       Real float32 input block.
   * @param n_in     Number of input samples (C-only, hidden from Python).
   * @param out      CF32 output buffer (C-only, hidden from Python).
   * @param max_out  Output buffer capacity (C-only, hidden from Python).
   * @return Number of output samples written (C-only).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> t = np.arange(4096)
   * >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
   * >>> out = np.empty(len(x), dtype=np.complex64)
   * >>> y = ddcr.execute(x, out)
   * >>> y.shape
   * (1024,)
   * >>> y.dtype
   * dtype('complex64')
   * >>> round(float(abs(y[500])), 2)   # one-sided cosine amplitude ≈ 0.5
   * 0.5
   * @endcode
   */
  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

  /**
   * @brief Return the maximum output samples for one execute call.
   *
   * Returns 0, signalling the Python extension to fall back to
   * allocating n_in samples — always sufficient for a decimating DDC.
   */
size_t ddc_execute_max_out(ddc_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
