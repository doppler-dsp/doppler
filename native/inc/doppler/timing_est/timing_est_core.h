/**
 * @file timing_est_core.h
 * @brief Feedforward symbol-timing estimator: a block of matched-filtered
 *        samples in, one estimate of the symbol timing out, with no loop.
 *
 * The power of a matched-filtered data stream is periodic at the symbol
 * rate, with its maximum at the eye center. Over a block this object forms
 * the Fourier coefficient of that power at the symbol rate,
 *
 *     c = sum_n |x[n]|^2 exp(-j 2 pi phi[n]),    phi[n] = n / sps  (mod 1)
 *
 * and reads the timing from its phase and the estimate's quality from its
 * size (Oerder and Meyr, 1988). `phi` is the object's SYMBOL CLOCK: a
 * 32-bit phase word advanced once a sample by the NCO's own increment for
 * `1 / sps` cycles a sample, so `sps` may be any real number above 2.
 *
 * What it promises:
 *
 *  - **One estimate a block, in the block's own length.** There is no
 *    pull-in: the time an estimate takes is the block the caller hands it,
 *    whatever the timing was.
 *  - **The carrier does not enter.** Only `|x|^2` is used, so the estimate
 *    is the same for any carrier phase and for a frequency offset, which is
 *    what lets it run before carrier recovery. (An offset still costs the
 *    caller's matched filter amplitude; that loss is the filter's.)
 *  - **One symbol clock across calls.** The clock is not restarted between
 *    blocks, so consecutive estimates are comparable, and their difference
 *    is the symbol rate's error.
 *  - **Nothing is decided.** No threshold is applied: `quality` is
 *    reported and the caller judges it.
 *
 * It filters, resamples and decides nothing; the caller owns the matched
 * filter. The reasoning, the predicted variance and what is not yet known
 * are in docs/design/timing-estimator.md.
 *
 * **With a matched filter of its own.** dp_timing_est_create_matched()
 * takes the pulse and the input's samples a symbol and puts a matched
 * `RateConverter` ahead of the clock: its decimating cascade, then its
 * matched terminal stage at a fixed rate to 4 samples a symbol, which
 * nothing steers. dp_timing_est_estimate() then takes the samples as they
 * arrive, for any real number of them a symbol from 4 up. It skips the
 * converter's own start (dp_RateConverter_get_memory()) before the first
 * block counts, and every estimate says where the eye center sits on the
 * INPUT's timeline (`eye`), which is what lets a second converter on the
 * same samples be set to it.
 *
 * Lifecycle: create -> (estimate | accumulate.. finish | reset)* -> destroy
 *
 * Example:
 * @code
 * dp_timing_est_state_t *te = dp_timing_est_create (4.0);
 * float _Complex x[512] = { 0 };       // 128 symbols of your stream
 * dp_timing_estimate_t e = dp_timing_est_estimate (te, x, 512);
 * (void)e.tau;                         // eye center, symbols
 * dp_timing_est_destroy (te);
 * @endcode
 */
#ifndef DP_TIMING_EST_CORE_H
#define DP_TIMING_EST_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/nco/nco_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/resample/resample_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief State-blob identity (see dp_state.h). */
#define TIMING_EST_STATE_MAGIC DP_FOURCC ('T', 'M', 'G', 'E')
/** @brief State-blob layout version. */
#define TIMING_EST_STATE_VERSION 2u /* v2: the lead-in left, and a matched
                                      instance's converter */
/** @brief Samples a symbol the matched flavor's converter delivers, and
 *         its symbol clock runs at. */
#define TIMING_EST_MATCHED_SPS 4.0
/** @brief Arms in the matched flavor's terminal bank: the fixed-rate
 *         stage's timing resolution, 1 / 1024 of an output. */
#define TIMING_EST_MATCHED_PHASES 1024u

/**
 * @brief One feedforward symbol-timing estimate.
 */
typedef struct {
    double tau;     /**< Where the eye center sits on the estimator's symbol
                         clock, in symbols, in (-1/2, 1/2]: the clock's phase
                         at the instants the stream's power peaks. With a
                         symbol-rate error it is the value at the middle of
                         the block. For a pulse that is not band limited,
                         read at a whole number of samples a symbol, it is
                         off the eye center by where the samples fall in
                         the symbol: up to 0.011 symbols for a rectangle
                         at 4 (the validation report's finding
                         `whole_n_rect_sampling_phase`). 0 when the block
                         has no power.                                      */
    double rate;    /**< How far the eye center moves on the clock each
                         symbol of the clock, in symbols: for symbols that
                         last `sps * (1 + eps)` samples it is
                         `eps / (1 + eps)`. From how far `tau` moved since
                         the previous
                         block, so 0 until `has_rate` is 1, and right only
                         while the timing moves under half a symbol between
                         the two blocks' middles.                           */
    double quality; /**< `|c| / sum |x|^2`, in [0, 1]: how much of the
                         block's power is the symbol-rate line. Near 0 with
                         no signal, with no transitions, or with a clock
                         error that turns the line through a full cycle
                         inside the block. 0 when the block has no power.   */
    double power;   /**< Mean `|x|^2` over the block.                       */
    double symbols; /**< The block's length in symbols of the clock:
                         samples / sps.                                     */
    int has_rate;   /**< 1 when a previous block exists to take `rate`
                         against, else 0.                                   */
    double eye;     /**< Where an eye center sits on the INPUT's timeline:
                         the sample index, counted from create or reset
                         and not whole, of the eye center nearest the
                         block's middle. Other eye centers are whole
                         symbols (`sps` samples) either side. On a matched
                         instance the converter's group delay and the
                         lead-in it skipped are taken out, so the index is
                         into the samples the estimate was handed. 0 when
                         the block has no power.                            */
} dp_timing_estimate_t;

/**
 * @brief SymbolTimingEstimator state.
 *
 * Allocate with dp_timing_est_create() or dp_timing_est_create_matched().
 * `sps`, `clock_sps`, `inc`, `lead`, `delay` and the converter's plan are
 * config; the rest is running state and is in the blob.
 */
typedef struct {
    double   sps;       /**< Samples per symbol of the stream handed to
                             dp_timing_est_estimate().                      */
    double   clock_sps; /**< Samples per symbol the clock runs at: `sps`,
                             or TIMING_EST_MATCHED_SPS on a matched
                             instance, whose clock runs on its converter's
                             output.                                        */
    uint32_t inc;       /**< The clock's step a sample: the NCO's increment
                             for `1 / clock_sps` cycles a sample.           */
    uint32_t phase;     /**< The symbol clock: a 32-bit fraction of one
                             symbol, at the next sample.                    */
    uint64_t count;     /**< Samples taken since create or reset.           */
    /* the block in progress */
    double   c_re;      /**< Real part of the running coefficient.          */
    double   c_im;      /**< Imaginary part of the running coefficient.     */
    double   pwr;       /**< Running sum of `|x|^2`.                        */
    uint64_t blk_n;     /**< Samples in the block so far.                   */
    uint64_t blk_first; /**< `count` at the block's first sample.           */
    /* the previous block, for the rate */
    int      have_prev; /**< 1 once a block with power has finished.        */
    double   prev_tau;  /**< That block's `tau`.                            */
    double   prev_mid;  /**< That block's middle, in symbols of the clock
                             since create or reset.                        */
    /* the matched flavor */
    dp_RateConverter_state_t *mf; /**< The matched converter, owned; NULL
                             on a plain instance.                           */
    uint64_t lead;      /**< Converter outputs skipped after create or
                             reset: `ceil (memory * rate) + 1`, the
                             converter's own start. 0 on a plain instance. */
    uint64_t lead_left; /**< How many of those are still to skip.           */
    double   delay;     /**< The converter's group delay, in input
                             samples. 0 on a plain instance.                */
} dp_timing_est_state_t;

/**
 * @brief Create an estimator whose symbol clock runs at @p sps samples a
 *        symbol.
 *
 * The bound on @p sps is the line's own: the power is sampled @p sps times
 * a symbol and its symbol-rate line sits at `1 / sps` cycles a sample, which
 * is below the Nyquist rate only for `sps > 2`. How close to 2 a given pulse
 * allows is a property of the pulse, not of this object.
 *
 * @param sps  Samples per symbol of the stream handed to it; finite and
 *             above 2 (default: 4.0).
 * @return Heap-allocated state, or NULL when @p sps is not finite, is 2 or
 *         less, or allocation fails.
 * @note Caller must call dp_timing_est_destroy() when done.
 * @code
 * >>> from doppler.estimation import SymbolTimingEstimator
 * >>> te = SymbolTimingEstimator(sps=4.0)
 * >>> te.sps
 * 4.0
 *
 * @endcode
 */
dp_timing_est_state_t *dp_timing_est_create(double sps);

/**
 * @brief Create an estimator with a matched filter of its own, for a
 *        stream of @p sps samples a symbol that nothing has filtered.
 *
 * Builds one matched `RateConverter` for the rate
 * `TIMING_EST_MATCHED_SPS / sps`, droop compensation on, and runs the
 * symbol clock on its output at TIMING_EST_MATCHED_SPS samples a symbol.
 * The converter's terminal stage is the matched filter and runs at that
 * fixed rate: nothing steers it. dp_timing_est_estimate() then takes the
 * unfiltered samples, and its record's `symbols` counts the converter
 * outputs the block gave, so a block that ends inside the lead-in reports
 * fewer symbols than it was handed (none, while the lead-in lasts).
 *
 * @param sps    Samples per symbol of the input; finite, and
 *               TIMING_EST_MATCHED_SPS or more (the terminal stage does
 *               not interpolate).
 * @param pulse  RC_PULSE_IANDD (rectangle) or RC_PULSE_RRC.
 * @param beta   Root-raised-cosine roll-off, 0 to 1; ignored for the
 *               rectangle.
 * @param span   One-sided root-raised-cosine span in symbols, 1 or more;
 *               ignored for the rectangle.
 * @return Heap-allocated state, or NULL when @p sps is not finite or is
 *         below TIMING_EST_MATCHED_SPS, the pulse is neither, @p beta or
 *         @p span is out of range, or allocation fails.
 * @note Caller must call dp_timing_est_destroy() when done.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.estimation import MatchedSymbolTimingEstimator
 * >>> n = np.arange(4096)
 * >>> # +1 -1 +1 ... through a root-raised cosine at 8 samples a symbol,
 * >>> # with a symbol instant at sample 3 and every 8 after it
 * >>> x = (0.25 * np.cos(np.pi * (n - 3.0) / 8.0)).astype(np.complex64)
 * >>> te = MatchedSymbolTimingEstimator(sps=8.0, pulse="rrc")
 * >>> e = te.estimate(x)
 * >>> bool(abs(e.eye % 8.0 - 3.0) < 0.1), e.symbols
 * (True, 490.25)
 * @endcode
 */
dp_timing_est_state_t *dp_timing_est_create_matched(double sps, int pulse, double beta, size_t span);

/**
 * @brief Destroy a timing_est instance and release all memory.
 * @param state  May be NULL.
 */
void dp_timing_est_destroy(dp_timing_est_state_t *state);

/**
 * @brief Return the symbol clock to zero and forget the previous block and
 *        any block in progress; keep `sps`.
 *
 * The next estimate is then the first again: its `tau` is on a clock that
 * starts at the next sample, and it carries no `rate`.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> import numpy as np
 * >>> from doppler.estimation import SymbolTimingEstimator
 * >>> n = np.arange(1024)
 * >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * n / 4)).astype(np.complex64)
 * >>> te = SymbolTimingEstimator(sps=4.0)
 * >>> te.estimate(x).has_rate, te.estimate(x).has_rate
 * (0, 1)
 * >>> te.reset()
 * >>> te.estimate(x).has_rate
 * 0
 *
 * @endcode
 */
void dp_timing_est_reset(dp_timing_est_state_t *state);

/**
 * @brief Add one sample's power to the block in progress and advance the
 *        symbol clock by one sample.
 *
 * The form a composing object uses when it has the power but not a block of
 * samples (a matched bank read at several timing hypotheses a symbol hands
 * its `|y|^2` here, one hypothesis a call). dp_timing_est_estimate() is this
 * in a loop followed by dp_timing_est_finish().
 *
 * @param state  Must be non-NULL.
 * @param power  `|x|^2` of the sample; not checked.
 */
JM_FORCEINLINE void
dp_timing_est_accumulate (dp_timing_est_state_t *state, double power)
{
  if (state->blk_n == 0)
    state->blk_first = state->count;
  double th = 6.283185307179586476925 * nco_word_to_norm (state->phase);
  state->c_re += power * cos (th);
  state->c_im -= power * sin (th);
  state->pwr += power;
  state->phase += state->inc;
  state->count++;
  state->blk_n++;
}

/**
 * @brief Close the block in progress and return its estimate.
 *
 * A block with no samples returns a record of zeros and changes nothing. A
 * block with no power returns `tau` 0 and `quality` 0 and is not kept as
 * the previous block, so the next `rate` is taken against the last block
 * that had power.
 *
 * @param state  Must be non-NULL.
 * @return The estimate for the samples accumulated since the last finish.
 */
dp_timing_estimate_t dp_timing_est_finish(dp_timing_est_state_t *state);

/**
 * @brief Add samples to the block in progress without closing it.
 *
 * The streaming half of dp_timing_est_estimate(), which is this followed by
 * dp_timing_est_finish(). A composing object that takes its input a sample
 * or a chunk at a time pushes here and closes the block when it has the
 * length it wants: `blk_n` counts the clock's samples in the block, `sps`
 * a symbol on a plain instance and TIMING_EST_MATCHED_SPS on a matched one.
 * On a matched instance the samples go through the converter and its
 * lead-in is skipped, as in dp_timing_est_estimate().
 *
 * @param state  Must be non-NULL.
 * @param in     Samples, `sps` a symbol.
 * @param n_in   Number of samples; 0 does nothing.
 */
void dp_timing_est_push(dp_timing_est_state_t *state, const float _Complex *in, size_t n_in);

/** @brief Bytes in the blob dp_timing_est_get_state() writes.
 *  @param state  Must be non-NULL.
 *  @return The blob's size: the envelope and the running fields. */
size_t dp_timing_est_state_bytes(const dp_timing_est_state_t *state);

/** @brief Write the running state into @p blob: the symbol clock, the
 *         sample count, the block in progress and the previous block.
 *  @param state  Must be non-NULL.
 *  @param blob   At least dp_timing_est_state_bytes() bytes. */
void dp_timing_est_get_state(const dp_timing_est_state_t *state, void *blob);

/** @brief Restore the running state from @p blob.
 *  @param state  Must be non-NULL; created with any `sps` (the blob carries
 *                no config, so the resumed clock runs at this object's).
 *  @param blob   A blob dp_timing_est_get_state() wrote.
 *  @return DP_OK, or DP_ERR_INVALID for a blob of another kind, version,
 *          size or byte order, or one whose fields are not a state this
 *          object reaches (a non-finite sum, a negative power, a previous
 *          `tau` outside (-1/2, 1/2], a block longer than the count); the
 *          state is then unchanged. */
int dp_timing_est_set_state(dp_timing_est_state_t *state, const void *blob);

/**
 * @brief Estimate the symbol timing from one block of matched-filtered
 *        samples.
 *
 * The block is the whole call: its length sets the estimate's variance and
 * the time the estimate takes. Consecutive calls stay on one symbol clock,
 * and from the second call that had power the result carries the symbol
 * rate's error.
 *
 * @param state  Must be non-NULL.
 * @param in     Matched-filtered samples, `sps` a symbol.
 * @param n_in   Number of samples; 0 returns a record of zeros.
 * @return The block's estimate.
 * @code
 * >>> import numpy as np
 * >>> from doppler.estimation import SymbolTimingEstimator
 * >>> n = np.arange(4 * 256)
 * >>> # power peaking a quarter symbol into each symbol, on any carrier
 * >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))
 * >>> x = (x * np.exp(1j * 0.3 * n)).astype(np.complex64)
 * >>> e = SymbolTimingEstimator(sps=4.0).estimate(x)
 * >>> round(e.tau, 3), round(e.quality, 3), e.symbols, e.has_rate
 * (0.25, 0.25, 256.0, 0)
 *
 * @endcode
 */
dp_timing_estimate_t dp_timing_est_estimate(dp_timing_est_state_t *state, const float _Complex *in, size_t n_in);
#ifdef __cplusplus
}
#endif

#endif /* DP_TIMING_EST_CORE_H */
