/**
 * @file mpsk_receiver_core.h
 * @brief Pulse-shaped M-PSK receiver: NDA-acquired carrier + Gardner timing.
 *
 * A complete per-sample inline modem for a continuous (unspread) M-PSK signal.
 * It composes the project's tracking primitives on one shared sample loop:
 *
 *   - @ref carrier_nda_state_t — per-sample carrier wipe-off with the integer
 *     NCO, plus a non-data-aided M-th-power discriminator on an I/Q arm
 *     integrate-and-dump (n dumps/symbol). This **acquires the carrier with no
 *     symbol timing and no data present** (cold start), which is what lets the
 *     receiver pull in before the matched filter / timing loop have settled.
 *   - a matched filter (@ref fir_state_t, owned) on the de-rotated stream:
 *     either an **integrate-and-dump boxcar** (`MPSK_RX_PULSE_IANDD`, default)
 *     or a **root-raised-cosine** (`MPSK_RX_PULSE_RRC`) for band-limited links.
 *   - @ref symsync_state_t — a carrier-blind Gardner symbol-timing loop on the
 *     matched-filter output, emitting one symbol per recovered symbol period.
 *
 * Carrier recovery follows the project rule: **predetection de-rotation**
 * (per-sample wipe, always) and **postdetection discrimination**. Two
 * discriminators steer one shared NCO:
 *   - **acquisition** — the NDA M-th-power error, n times/symbol, with no data
 *     or timing required (`tracking == 0`).
 *   - **tracking** — once `acq_to_track` is enabled and the loop has locked,
 *     a decision-directed error `e = Im(y·conj(â))/|y|` on the recovered
 *     symbols (lower jitter, naturally lower loop bandwidth at symbol rate).
 *     The same NCO/loop filter carries the frequency estimate across the
 *     switch.
 * The acquisition-to-tracking switch is **opt-in** (`acq_to_track`, default
 * off): by default the receiver stays in robust NDA tracking the whole time.
 *
 * The loop locks to one of M phases — an **M-fold ambiguity** on absolute phase.
 * Resolve it with differential demapping (`bits(..., differential=1)`) or a sync
 * word downstream. A DSSS-MPSK receiver is `Dll(segments) -> MpskReceiver`:
 * despread to symbol-rate soft chips, then this modem.
 *
 * Lifecycle: mpsk_receiver_create -> [steps / bits / reset]* -> _destroy.
 *
 * @code
 * // QPSK, 8 samples/symbol, I&D matched filter, NDA acquisition
 * mpsk_receiver_state_t *rx = mpsk_receiver_create(
 *     4, 8, 4, MPSK_RX_PULSE_IANDD, 0.35, 8,
 *     0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0);
 * float complex sym[256];
 * size_t k = mpsk_receiver_steps(rx, rx_in, rx_len, sym, 256);
 * double f = mpsk_receiver_get_norm_freq(rx);  // tracked residual carrier
 * mpsk_receiver_destroy(rx);
 * @endcode
 */
#ifndef MPSK_RECEIVER_CORE_H
#define MPSK_RECEIVER_CORE_H

#include "carrier_nda/carrier_nda_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "fir/fir_core.h"
#include "jm_perf.h"
#include "mpsk/mpsk_core.h"
#include "symsync/symsync_core.h"
#include <complex.h>
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include "farrow/farrow_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Matched-filter pulse shape selector. */
  enum
  {
    MPSK_RX_PULSE_IANDD = 0, /**< integrate-and-dump boxcar (rectangular). */
    MPSK_RX_PULSE_RRC = 1    /**< root-raised-cosine (band-limited link).  */
  };

  /**
   * @brief M-PSK receiver state.
   *
   * Allocate with mpsk_receiver_create(). Embeds the carrier loop (`car`) and
   * symbol-timing loop (`sync`) by value and owns the matched-filter FIR; the
   * carrier NCO and lock metric live in `car`. Treat all fields as internal
   * (use the getters); they are exposed for the inline sample loop.
   */
  typedef struct
  {
    carrier_nda_state_t car;  /**< carrier loop: wipe NCO + arm I/D + NDA.   */
    symsync_state_t     sync; /**< Gardner symbol-timing loop (by value).    */
    fir_state_t        *mf;   /**< matched filter on the de-rotated stream.  */
    float              *mf_taps;      /**< owned real MF taps.               */
    int                 m;           /**< constellation order M (2, 4, 8).   */
    size_t              sps;         /**< samples per symbol.                */
    int                 n;           /**< arm dumps per symbol.              */
    int                 pulse;       /**< MPSK_RX_PULSE_IANDD / _RRC.        */
    double              rrc_beta;    /**< RRC roll-off (pulse == RRC).       */
    int                 rrc_span;    /**< RRC one-sided span, symbols.       */
    int                 acq_to_track; /**< opt-in NDA->decision switch.      */
    double              lock_thresh; /**< acq-to-track lock-metric threshold.*/
    size_t              warmup_syms; /**< symbols before the switch allowed. */
    int                 tracking;    /**< 0 = NDA acquire, 1 = decision.     */
    size_t              sym_count;   /**< symbols emitted (warmup counter).  */
    int                 differential;  /**< bits(): differential demap.      */
    int                 have_prev_idx; /**< differential: prev_idx valid.    */
    unsigned            prev_idx;      /**< differential: prev sliced index. */
    float complex       sym_rot;       /**< exp(j*phi0): NDA-grid -> slicer.  */
  } mpsk_receiver_state_t;

  /**
   * @brief Create an M-PSK receiver.
   *
   * @param m              Constellation order M, 2/4/8 (default 4 = QPSK).
   * @param sps            Samples per symbol (default 8).
   * @param n              Carrier arm dumps per symbol (default 4; sps % n == 0).
   * @param pulse          Matched-filter shape (default MPSK_RX_PULSE_IANDD).
   * @param rrc_beta       RRC roll-off in [0, 1] (default 0.35; RRC only).
   * @param rrc_span       RRC one-sided span in symbols (default 8; RRC only).
   * @param bn_carrier     Carrier loop noise bandwidth (default 0.01).
   * @param zeta           Damping factor for both loops (default 0.707).
   * @param bn_timing      Symbol-timing loop noise bandwidth (default 0.01).
   * @param acq_to_track   Enable NDA->decision-directed tracking (default 0).
   * @param lock_thresh    Lock metric required to switch to tracking
   *                        (default 0.5).
   * @param init_norm_freq Seed carrier frequency, cycles/sample (default 0.0).
   * @param warmup_syms    Symbols before the acq-to-track switch is allowed
   *                        (default 100).
   * @param differential   bits(): differential (rotation-invariant) demap
   *                        (default 0 = coherent).
   * @return Heap-allocated state, or NULL on invalid args / allocation failure.
   * @note Caller must call mpsk_receiver_destroy() when done.
   */
  mpsk_receiver_state_t *mpsk_receiver_create (
      int m, size_t sps, int n, int pulse, double rrc_beta, int rrc_span,
      double bn_carrier, double zeta, double bn_timing, int acq_to_track,
      double lock_thresh, double init_norm_freq, size_t warmup_syms,
      int differential);

  /**
   * @brief Destroy an M-PSK receiver and release all memory.
   * @param state  May be NULL.
   */
  void mpsk_receiver_destroy (mpsk_receiver_state_t *state);

  /**
   * @brief Re-seed the carrier/timing loops to their create-time state.
   * @param state  Must be non-NULL.
   */
  void mpsk_receiver_reset (mpsk_receiver_state_t *state);

  size_t mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a cf32 block and emit the recovered symbols.
   *
   * Runs the per-sample loop (carrier wipe-off + NDA arm + matched filter +
   * Gardner timing) over @p x and writes one cf32 symbol per recovered symbol
   * period. Fewer outputs than inputs (~ x_len / sps). Read norm_freq for the
   * tracked carrier and lock for the carrier lock metric.
   *
   * @param state    Receiver state.  Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written.
   */
  size_t mpsk_receiver_steps (mpsk_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  size_t mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a cf32 block and emit hard Gray-coded bits.
   *
   * Like mpsk_receiver_steps(), but each recovered symbol is sliced to its
   * nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With the
   * differential option set at create time, the Gray label is taken from the
   * phase *difference* between consecutive symbols (rotation-invariant — it
   * resolves the M-fold carrier ambiguity), else from the absolute (coherent)
   * decision.
   *
   * @param state    Receiver state.  Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output bytes (0/1); caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of bits written.
   */
  size_t mpsk_receiver_bits (mpsk_receiver_state_t *state,
                             const float complex *x, size_t x_len,
                             uint8_t *out, size_t max_out);

  double mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state);
  void   mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val);
  double mpsk_receiver_get_lock (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_m (const mpsk_receiver_state_t *state);
  size_t mpsk_receiver_get_sps (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_n (const mpsk_receiver_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: carrier_nda + symsync + matched-filter children +
 * running tracking/handover state; MF taps restored by create. */
#define MPSK_RECEIVER_STATE_MAGIC DP_FOURCC ('M','P','S','K')
#define MPSK_RECEIVER_STATE_VERSION 1u
size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *state);
void mpsk_receiver_get_state (const mpsk_receiver_state_t *state, void *blob);
int mpsk_receiver_set_state (mpsk_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_CORE_H */
