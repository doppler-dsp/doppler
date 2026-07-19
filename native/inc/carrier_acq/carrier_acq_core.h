/**
 * @file carrier_acq_core.h
 * @brief CarrierAcquisition — PSDMF residual-carrier frequency refinement.
 *
 * Runs AFTER Acquisition's own coarse Doppler search, as a one-shot
 * matched-filter refinement stage: feed the already-despread symbol-rate
 * stream via steps(), and once enough non-coherent looks have
 * accumulated to cross a Pfa/Pd-driven detection threshold, ready
 * becomes true and residual_hz holds the sub-bin-refined residual
 * carrier estimate, Hz.
 *
 * Composes existing primitives rather than reimplementing them:
 *
 *   - psd_state_t: FFT + window + zero-pad + non-coherent power
 *     averaging (Welch's method) -- the entire "measure the average
 *     power spectrum of what's coming in" half of the algorithm.
 *   - detector_state_t: FFT-based circular correlation of the averaged
 *     power spectrum against a known template (the average PSD shape
 *     of a random rectangular-pulse BPSK symbol stream by default, or a
 *     caller-supplied override for a different pulse/modulation) plus a
 *     noise-referenced test statistic and argmax lag.
 *   - detection_core's det_n_noncoh()/det_threshold(): det_n_noncoh
 *     (the same chi-square statistic Acquisition's own auto-config
 *     uses) drives the precomputed fixed-dwell/give-up cap; det_threshold
 *     (sqrt(-2*ln(pfa))) is reused as the tail-quantile stand-in inside
 *     the per-block CFAR ratio threshold below.
 *
 * The per-block CFAR ratio threshold is NOT det_threshold_noncoherent()
 * (that statistic -- a classic complex-correlator peak/noise envelope
 * ratio -- does not transfer to this object's real statistic, a
 * power-spectrum-vs-known-template correlation; confirmed via Monte
 * Carlo, ~5x too conservative). _ratio_threshold() (carrier_acq_core.c)
 * instead uses the derived H0 model for this specific statistic (an
 * exact Gamma-sum mean/variance for the averaged, template-correlated
 * periodogram) plus ONE empirically-calibrated tail-inflation constant
 * (kappa, standing in for the argmax-over-nfft-correlated-lags extreme-
 * value quantile a full closed form hasn't cleanly reduced to yet --
 * see FINISHING_PLAN.md's CarrierAcquisition section / the
 * derive_carrier_acq_statistic.py derivation for the full story, and
 * revisit/refine kappa when time allows).
 *
 * Only the default template generator (a sinc^2 shape, DC-centred to
 * match psd_power_twosided()'s own bin order) and the 3-point parabolic
 * sub-bin peak refinement (read directly off detector_state_t's own
 * out_buf -- no second correlation pass needed) are new leaf code.
 *
 * Lifecycle: create -> steps()* -> (ready ? residual_hz : keep feeding)
 *            -> reset()/destroy
 *
 * @code
 * carrier_acq_state_t *ca = carrier_acq_create(
 *     4.092e6, 100e3, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9, 2.0, true,
 *     100000);
 * while (!ca->ready && ca->n_blocks < ca->max_n_blocks)
 *     carrier_acq_steps(ca, block, block_len);
 * double hz = ca->residual_hz; // valid only when ca->ready
 * carrier_acq_destroy(ca);
 * @endcode
 */
#ifndef CARRIER_ACQ_CORE_H
#define CARRIER_ACQ_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "spectral/spectral_core.h"
#include "corr/corr_core.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CarrierAcquisition state.
 *
 * Allocate with carrier_acq_create().
 */
typedef struct {
    /* Composed children (owned, by pointer). */
    psd_state_t      *psd; /**< FFT + window + non-coherent power avg.  */
    detector_state_t *det; /**< Correlate averaged power vs. template.  */

    /* Scratch, not state -- sized nfft/psd->n, allocated once at create
     * (no allocation in the steps() hot path). */
    float         *pwr_buf;    /**< psd_power_twosided() output, nfft.  */
    float complex *power_buf;  /**< pwr_buf packed complex (imag=0).    */
    float complex *carry_buf;  /**< Raw-input carry, capacity psd->n.   */
    size_t         carry_len;  /**< Valid samples in carry_buf, 0..n-1. */

    /* Config, fixed at construction (restored by create(), not the
     * blob -- validated against the blob's own copy in set_state). */
    double sample_rate_hz;
    double pfa;
    bool   sequential;
    double s_t;   /**< sum(template) -- ratio-threshold calibration.   */
    double s_t2;  /**< sum(template^2) -- ratio-threshold calibration. */

    /* Public (property-backed) running/result fields. */
    bool   ready;
    double residual_hz;
    size_t n_blocks;
    size_t dwell_target;  /**< Non-sequential mode's fixed wait count.   */
    size_t max_n_blocks;  /**< Sequential mode's OWN give-up cap --      *
                            *  deliberately independent of dwell_target  *
                            *  (see carrier_acq_create()'s own doc).     */
    size_t nfft;
} carrier_acq_state_t;

/**
 * @brief Create a carrier_acq instance.
 *
 * @param sample_rate_hz  Sample rate of the input stream, Hz (required).
 * @param symbol_rate_hz  Symbol rate, Hz -- builds the default template
 *                        (required).
 * @param resolution_hz   Desired FFT frequency resolution, Hz. <= 0.0 is
 *                         a sentinel meaning "auto": symbol_rate_hz/10.0.
 * @param zero_pad        PSD zero-pad factor (>= 1); see psd_core.h.
 * @param window          Enum index; 0=hann, 1=kaiser, 2=blackman-harris.
 * @param beta            Kaiser beta (ignored for hann/blackman-harris).
 * @param psd_template     Known PSD-shape template override, length
 *                        must equal nfft = next_pow2(round(sample_rate_hz
 *                        /resolution_hz) * zero_pad); NULL/length-0 means
 *                        "not supplied" -- the default rectangular-pulse
 *                        sinc^2 template (from symbol_rate_hz) is used.
 * @param psd_template_len Length of @p psd_template (0 if not supplied).
 * @param pfa             Target per-test false-alarm probability.
 * @param pd              Target detection probability.
 * @param design_snr      Assumed per-sample amplitude SNR used ONLY to
 *                        precompute dwell_target via det_n_noncoh(); not
 *                        a live measurement. An optimistic guess only
 *                        affects NON-sequential mode (which trusts this
 *                        one-shot wait count outright) -- sequential
 *                        mode's own give-up bound is max_n_blocks, not
 *                        dwell_target, precisely so a wrong design_snr
 *                        can't stop it from trying more blocks once real
 *                        data shows it needs to.
 * @param sequential      True: test for a detection after EVERY block
 *                        (the per-block CFAR ratio threshold -- see
 *                        _ratio_threshold() in carrier_acq_core.c --
 *                        tightens as more looks accumulate), stopping
 *                        the moment one fires or max_n_blocks is
 *                        reached. False: accumulate silently and test
 *                        once, at dwell_target.
 * @param max_n_blocks    Sequential mode's own give-up cap (ignored by
 *                        non-sequential mode, which stops at
 *                        dwell_target instead) -- deliberately a
 *                        SEPARATE, generous bound from dwell_target;
 *                        capping sequential mode at design_snr's own
 *                        point estimate would defeat the reason to test
 *                        every block in the first place.
 * @return Heap-allocated state, or NULL on invalid argument or
 *         allocation failure.
 * @note Caller must call carrier_acq_destroy() when done.
 */
carrier_acq_state_t *carrier_acq_create(
    double sample_rate_hz, double symbol_rate_hz, double resolution_hz,
    size_t zero_pad, int window, float beta, const float *psd_template,
    size_t psd_template_len, double pfa, double pd, double design_snr,
    bool sequential, size_t max_n_blocks);

/**
 * @brief Destroy a carrier_acq instance and release all memory.
 * @param state  May be NULL.
 */
void carrier_acq_destroy(carrier_acq_state_t *state);

/**
 * @brief Reset to the post-create state: discard the running PSD
 * average and detection state; n_blocks/ready/residual_hz return to
 * their initial values. Config (psd/detector/dwell_target) is
 * untouched.
 * @param state  Must be non-NULL.
 */
void carrier_acq_reset(carrier_acq_state_t *state);

/**
 * @brief Fold raw complex samples into the running PSD average and test
 * for a detection. Accepts any chunk size across repeated calls -- a
 * partial trailing block is carried to the next call. A no-op once
 * ready is true or the give-up cap (max_n_blocks in sequential mode,
 * dwell_target otherwise) has been reached.
 *
 * @param state  Must be non-NULL.
 * @param x      Raw complex input samples (cf32).
 * @param x_len  Number of samples in @p x.
 */
void carrier_acq_steps(carrier_acq_state_t *state, const float complex *x,
                       size_t x_len);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Composition: psd + detector children (self-contained sub-blobs) + own
 * running fields (n_blocks/ready/residual_hz/carry_len) + the carry
 * buffer (fixed psd->n capacity, only the first carry_len samples
 * meaningful). nfft/dwell_target/max_n_blocks are config -- validated
 * against the live instance, not restored from the blob (a resumed
 * instance must be
 * constructed with the same sample_rate_hz/symbol_rate_hz/psd_template/
 * pfa/pd/design_snr as the one that produced the blob -- the same class
 * of precondition dsss_receiver's own segments/sps/n check documents;
 * neither corr_core nor detector_core hash-verify their own reference
 * spectra either). */
#define CARRIER_ACQ_STATE_MAGIC DP_FOURCC('C', 'A', 'Q', 'R')
#define CARRIER_ACQ_STATE_VERSION 1u
size_t carrier_acq_state_bytes(const carrier_acq_state_t *state);
void   carrier_acq_get_state(const carrier_acq_state_t *state, void *blob);
int    carrier_acq_set_state(carrier_acq_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CARRIER_ACQ_CORE_H */
