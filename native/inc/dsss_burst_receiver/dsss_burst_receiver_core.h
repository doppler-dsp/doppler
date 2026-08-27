/**
 * @file dsss_burst_receiver_core.h
 * @brief DsssBurstReceiver — the burst chain composed in C.
 *
 * Composes the three certified burst objects behind one push(): acquisition
 * SEARCHES the stream, a refine stage recovers the exact preamble start, and
 * the demodulator produces the payload. It owns the hand-off between them --
 * the epoch, the fold, and the look-back reaching back to a burst start that
 * has already gone past -- which is the part every caller previously redid
 * by hand. See docs/design/dsss-burst-receiver.md.
 *
 * Lifecycle: create, then push() repeatedly, then destroy. There is no
 * step()/steps(): a burst is a frame, not a sample.
 *
 * @code
 * uint8_t acq[31], data[8], sync[13];
 * dsss_burst_receiver_state_t *rx = dsss_burst_receiver_create (
 *     acq, 31, data, 8, sync, 13, 4, 4, 1.0e6, 61,
 *     55.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
 * uint8_t bits[61];   // frame_syms per burst: sync | payload | CRC
 * size_t n = dsss_burst_receiver_push (rx, samples, n_samples, bits, 61);
 * // the bits are the FRAME as received; undoing it is a Frame's job
 * dsss_burst_receiver_destroy (rx);
 * @endcode
 */
#ifndef DSSS_BURST_RECEIVER_CORE_H
#define DSSS_BURST_RECEIVER_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "buffer/buffer.h"
#include "dp_state.h"

/**
 * @brief One completed burst's event, as `events()` hands it back.
 *
 * push() returns the PAYLOADS of every burst it completed, concatenated;
 * this is the parallel record for row `i` of that return. It exists because
 * a single push() can complete many bursts, and each one needs its own
 * event -- a single set of scalar read-backs would describe only the last
 * (docs/design/dsss-burst-receiver.md section 4: the record must be
 * sufficient on its own, for EVERY burst, not just the most recent).
 */
/**
 * @brief Detections collected from acquisition per batch.
 *
 * A BATCHING parameter, never a correctness one: push() loops until acq has
 * absorbed the whole chunk, so a smaller array means more iterations and
 * nothing else. Growing it to "be safe" would hide the fact that acq_push()
 * stops once its result array is full and abandons the rest of its input.
 */
#define DSSS_BR_HITS 16u

typedef struct
{
  uint64_t preamble_start; /**< Exact stream position of the preamble.     */
  double   doppler_hz_est; /**< Signed coarse Doppler, Hz.                 */
  double   doppler_res_hz; /**< Acquisition's native bin width, Hz.        */
  double   cn0_dbhz_est;   /**< C/N0 lower bound from the hit, dB-Hz.      */
  double   est_freq_hz;    /**< Demod's residual-frequency estimate.       */
  double   est_rate_hz;    /**< Demod's chirp-rate estimate.               */
  double   est_snr_db;     /**< Demod's post-decode SNR estimate.          */
  double   refine_margin;  /**< Runner-up period over the winner.          */
} dsss_br_event_t;

/**
 * @brief One detection between acquisition and demodulation.
 *
 * A hit cannot always be refined the moment it arrives -- the refine window
 * reaches BACKWARDS and forwards, so some of it may not have been pushed
 * yet -- and a detection dropped because its window was incomplete is a lost
 * burst. So a hit is queued here with the event fields acquisition supplied,
 * refined when its window is reachable, and demodulated when the burst has
 * fully arrived.
 */
typedef struct
{
  uint64_t anchor;     /**< Coarse code epoch from the hit (stream-absolute).*/
  uint64_t start;      /**< Refined preamble start; valid once `refined`.   */
  double   doppler_hz; /**< Signed coarse Doppler, Hz.                      */
  double   cn0_dbhz;   /**< C/N0 lower bound from the hit, dB-Hz.           */
  double   margin;     /**< Refine runner-up ratio; valid once `refined`.   */
  double   peak_mag;   /**< The hit's RAW CFAR peak. Two detections naming
                            the same preamble keep the stronger, so a weak
                            hit that merely arrived first cannot own the
                            slot a real burst needs (doppler#1004).
                            Deliberately not `test_stat`: that is
                            peak/noise_est, and the noise estimate is a mean
                            over the surface, so a BARE preamble -- which
                            raises no floor -- outscores a real burst whose
                            payload does. The raw peak measures what the
                            comparison actually means, how much preamble the
                            frame holds.                                   */
  int      refined;    /**< Non-zero once `start` is known.                 */
} dsss_br_pending_t;
#include "burst_acq/burst_acq_core.h"
#include "acq/acq_core.h"
#include "burst_demod/burst_demod_core.h"
#include "burst_despreader/burst_despreader_core.h"
#include "ppe/ppe_core.h"
#include "corr/corr_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "spectral/spectral_core.h"
#include "loop_filter/loop_filter_core.h"
#include "detection/detection_core.h"
#include "fft/fft_core.h"
#include "pn/pn_core.h"
#include "conv/conv_core.h"
#include "rs/rs_core.h"
#include "gold/gold_core.h"
#include "mpsk/mpsk_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DsssBurstReceiver state.
 *
 * Allocate with dsss_burst_receiver_create().
 */
typedef struct {
  /* ── Configuration, copied at create() ──────────────────────────────── */
  uint8_t *acq_code;     /**< Preamble code, owned copy.                   */
  uint8_t *data_code;    /**< Payload spreading code, owned copy.          */
  uint8_t *sync;         /**< Frame sync word, owned copy.                 */
  size_t   acq_code_len; /**< Preamble code length, chips.                 */
  size_t   data_code_len;/**< Data code length, chips.                     */
  size_t   sync_len;     /**< Sync word length, symbols.                   */
  size_t   reps;         /**< Preamble code repetitions.                   */
  size_t   spc;          /**< Samples per chip.                            */
  double   chip_rate;    /**< Chip rate, Hz.                               */
  size_t   frame_syms;   /**< Symbols the frame occupies after the sync
                              word, and so bits per burst out of push().
                              What they MEAN is a frame description's
                              business, one layer up (doppler#1022).     */

  /* ── Derived geometry ───────────────────────────────────────────────── */
  size_t code_period; /**< One preamble repetition, in SAMPLES. The modulus
                           acq's code_phase is a residue of, so every epoch
                           ambiguity in this object is stated against it.  */
  size_t burst_len;   /**< Preamble + spread frame, in samples.           */

  /* ── The composed children (each certified separately) ──────────────── */
  burst_acq_state_t   *acq;   /**< Search stage.                           */
  burst_demod_state_t *demod; /**< Demod stage, re-seeded per burst.       */

  /* ── Look-back (docs/design/dsss-burst-receiver.md §7.1) ────────────── */
  dp_f32_t *hist;      /**< History ring. Double-mapped, so a window that
                            spans the wrap is ONE contiguous pointer. The
                            receiver keeps its own rather than borrowing
                            acq's, which consumes every frame it processes
                            and has therefore released what is still
                            needed.                                        */
  uint64_t samples_fed; /**< Stream position: total samples ever pushed.
                             What makes an epoch stream-ABSOLUTE, and the
                             reason preamble_start is a quantity only this
                             object can compute.                           */

  /* ── The DetectionEvent, describing the most recent completed burst ─── */
  uint64_t preamble_start; /**< Stream-absolute preamble start. Never late. */
  double   doppler_hz_est; /**< Signed coarse Doppler, Hz.                  */
  double   doppler_res_hz; /**< Width of that estimate.                     */
  double   cn0_dbhz_est;   /**< C/N0 lower bound, dB-Hz (saturating).       */
  double   est_freq_hz;    /**< Demod's own residual estimate, Hz.          */
  double   est_rate_hz;    /**< Demod's own chirp-rate estimate.            */
  double   est_snr_db;     /**< Demod's own post-decode SNR estimate.       */
  double   refine_margin;  /**< Winning preamble correlation over its
                                nearest whole-period competitor. Near 1
                                means the period was NOT resolved.         */

  /* ── Refine scratch (docs/design/dsss-burst-receiver.md §3.4) ───────── */
  float *ref_sign;   /**< One code period of +-1 chip signs, spc-expanded.
                          Real, so the per-period correlation is a signed
                          sum rather than a complex multiply.              */
  float _Complex *corr_buf; /**< Per-offset code-period correlations, reused
                                 across the candidate sweep so the sliding
                                 correlation is computed once and the
                                 non-coherent combine just indexes it.     */
  size_t refine_span;  /**< Candidate offsets searched, in samples:
                            `(k_lo + k_hi + reps) * code_period`, which is
                            `(4*reps + 4) * code_period` for the `k_lo`
                            chosen in create(). This doc claimed
                            `2*reps*code_period` until it was measured --
                            2.4x low at reps=5 -- so read `refine_span`
                            rather than the formula.

                            It is also the CALLER-facing minimum burst
                            spacing: two anchors closer than this are
                            coalesced as one preamble, so bursts packed
                            tighter are merged and the rest are lost.     */
  size_t corr_len;     /**< Entries in corr_buf.                            */
  size_t retain_span;  /**< Samples that must stay reachable: refine span +
                            one whole burst. Also the caller-facing minimum
                            TRAILING context -- a burst closer than this to
                            the end of what has been pushed is not emitted
                            until more samples arrive.                     */
  size_t chunk_max;    /**< Largest slice of one push processed at a time,
                            so any block size is accepted without the ring
                            overrunning its own retention.                 */

  /* ── Detections in flight ────────────────────────────────────────────
   * Only detections whose burst window has NOT yet arrived live here: every
   * one whose window HAS arrived is demodulated before push() returns, which
   * is what bounds retention (see dsss_br_trim). */
  dsss_br_pending_t *q;      /**< Detections, oldest first; `q_cap` long.   */
  size_t             q_cap;  /**< DERIVED, not a constant. Entries sit at
                                  least `refine_span` apart within
                                  `retain_span` of the head, so the count
                                  scales with burst_len/refine_span -- about
                                  1 at the C test geometry but 5.5x at a real
                                  link and 20x for a long payload. A fixed 8
                                  silently dropped the hit AND the rest of
                                  the batch on anything but the test's own
                                  geometry.                                 */
  size_t             q_head; /**< Index of the oldest entry.                */
  size_t             pending; /**< Detections held because their burst window
                                  has NOT fully arrived -- the caller-facing
                                  "there is not enough data yet" read-back.

                                  push() deliberately emits nothing for these:
                                  a burst is returned when it is complete, not
                                  when it is guessed at. Feed more samples and
                                  it comes out, bit-exact, wherever the split
                                  fell. What this exists for is the OTHER end:
                                  a caller closing a file or a socket while
                                  this is non-zero is discarding a burst that
                                  would have decoded, and every other
                                  read-back looks identical to "nothing was
                                  ever there" (`dropped` counts ring refusals,
                                  not this).                                */

  /* ── The completed bursts of the LAST push ───────────────────────────
   * Scratch, deliberately NOT serialized: it describes the most recent
   * push() only, so keeping it out of the blob is what lets state_bytes()
   * stay a pure function of configuration (finding F5). Grows on demand,
   * because the count scales with the caller's block size, not with any
   * configuration. */
  dsss_br_event_t *ev;     /**< One record per burst returned.             */
  size_t           ev_cap; /**< Allocated records.                          */
  float  *llr;     /**< The soft bits of every burst the last push
                        returned, concatenated: burst i starts at
                        i*frame_bits. Scratch, like `ev` -- it describes
                        one call and is never serialized.              */
  size_t  llr_cap; /**< Allocated floats.                               */
  size_t  llr_len; /**< Floats written by the last push.                */
  size_t  frame_bits; /**< The frame's length, from the description --
                           the stride of a row in `llr`.                */
  size_t           ev_len; /**< Records the last push() wrote.              */
  uint64_t suppress_until; /**< Detections below this stream position fall
                                inside a burst that has already DECODED, so
                                they are the payload firing against the
                                acquisition code rather than new bursts.
                                Armed only on a valid frame: arming it on
                                every detection let one spurious hit blind
                                the search for a whole burst and discard the
                                next real one (doppler#1004). Coalescing the
                                several frames of ONE preamble is a separate
                                job, done by `refine_span` proximity plus a
                                greatest-of tie-break.                     */
  size_t acq_blob_max; /**< Fixed upper bound on the acquisition child's
                            blob. `state_bytes()` must be a pure function of
                            CONFIGURATION -- jm's binding compares an incoming
                            blob's length against it -- yet both the retained
                            look-back and acq's own unconsumed ring vary with
                            the stream. Both are therefore written into
                            fixed-size regions with a length prefix.        */
  size_t k_lo; /**< Whole code periods searched BEFORE the anchor.        */
  size_t k_hi; /**< ...and after.                                         */

  /* ── Bookkeeping ────────────────────────────────────────────────────── */
  uint64_t dropped;  /**< Samples the ring refused. A LOST BURST each, not
                          a statistic -- lifetime, survives reset().       */
  uint64_t n_bursts; /**< Bursts demodulated, lifetime.                    */
/*<<property_struct_fields>>*/
} dsss_burst_receiver_state_t;

/**
 * @brief Create a burst receiver: acquisition, refine and demodulation
 *        composed behind one push().
 *
 * Give it the waveform -- the two codes and the frame sync word -- plus the
 * geometry, and stream samples in. It searches blindly for a burst,
 * recovers the exact preamble start, and demodulates, publishing one
 * detection event per burst through the read-back fields.
 *
 * The look-back buffer is NOT a parameter. Its span is derived from the
 * geometry here (detection lag + refine search + the burst itself), because
 * every term is already known and a caller asked to size a history buffer
 * is a caller handed a way to lose bursts silently.
 *
 * @param acq_code  Preamble PN chips (0/1), length @p acq_code_len.
 * @param acq_code_len  Preamble code length, chips.
 * @param data_code  Payload spreading chips (0/1), @p data_code_len long.
 * @param data_code_len  Data code length, chips.
 * @param sync  Frame sync word (0/1 symbols), @p sync_len long.
 * @param sync_len  Sync word length, symbols.
 * @param reps  Preamble code repetitions (>= 1).
 * @param spc  Samples per chip (>= 1).
 * @param chip_rate  Chip rate in Hz (> 0).
 * @param frame_syms   Frame symbols per burst (>= 1) — what push()
 *                     returns, bit for bit.
 * @param cn0_dbhz  Carrier-to-noise density in dB-Hz (> 0), sizing the
 *                  acquisition search.
 * @param doppler_uncertainty  One-sided Doppler half-range, Hz.
 * @param pfa  Target false-alarm probability, in (0, 1).
 * @param pd  Target detection probability, in (0, 1).
 * @param carrier_hz  RF carrier (Hz) for code-Doppler; 0 = ignore.
 * @param max_rate  Chirp-rate search half-span (cycles/sample^2).
 * @param est_segments  Segments the feedforward estimator fits over.
 * @return Heap-allocated state, or NULL if any argument is invalid.
 * @note Caller must call dsss_burst_receiver_destroy() when done.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> acq = rng.integers(0, 2, 31).astype(np.uint8)
 * >>> dat = rng.integers(0, 2, 8).astype(np.uint8)
 * >>> syn = np.zeros(13, dtype=np.uint8)
 * >>> rx = DsssBurstReceiver(acq, dat, syn, reps=4, spc=4,
 * ...                        frame_syms=32)
 * >>> rx.n_bursts
 * 0
 *
 * @endcode
 */
dsss_burst_receiver_state_t *dsss_burst_receiver_create(const uint8_t *acq_code, size_t acq_code_len, const uint8_t *data_code, size_t data_code_len, const uint8_t *sync, size_t sync_len, size_t reps, size_t spc, double chip_rate, size_t frame_syms, double cn0_dbhz, double doppler_uncertainty, double pfa, double pd, double carrier_hz, double max_rate, size_t est_segments);

/**
 * @brief Destroy a dsss_burst_receiver instance and release all memory.
 * @param state  May be NULL.
 */
void dsss_burst_receiver_destroy(dsss_burst_receiver_state_t *state);

/**
 * @brief Return to searching: drop the history and clear every read-back.
 *
 * Resets the embedded acquisition, discards the retained look-back, and
 * clears all the event fields, so a fresh stream cannot inherit the
 * previous burst's verdict. The lifetime counters (`n_bursts`, `dropped`)
 * deliberately survive -- a reset that zeroed them could hide that this
 * receiver had already lost samples. Construction parameters are untouched.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> rx = DsssBurstReceiver(
 * ...     rng.integers(0, 2, 31).astype(np.uint8),
 * ...     rng.integers(0, 2, 8).astype(np.uint8),
 * ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
 * >>> _ = rx.push(np.zeros(1024, dtype=np.complex64))
 * >>> rx.reset()
 *
 * @endcode
 */
void dsss_burst_receiver_reset(dsss_burst_receiver_state_t *state);









/**
 * @brief Max bits push() can write for an input of @p x_len samples.
 *
 * push() returns EVERY burst it completed, so the bound scales with the
 * input: distinct bursts cannot overlap, so they are at least `burst_len`
 * apart, and a push of @p x_len samples can complete at most
 * `x_len/burst_len + 1` of them -- plus every detection already queued from
 * an earlier call, which is `q_cap`.
 *
 * @param state  Must be non-NULL.
 * @param x_len  Number of input samples the caller is about to push.
 * @return `(x_len/burst_len + 1 + q_cap) * frame_syms`.
 */
size_t dsss_burst_receiver_push_max_out(dsss_burst_receiver_state_t *state, size_t x_len);

/**
 * @brief Stream samples; return the payload of EVERY burst that completed.
 *
 * Retains @p x in the history ring and feeds the embedded acquisition.
 * When a detection fires, the refine stage correlates the whole preamble to
 * recover the exact preamble start -- the quantity acquisition structurally
 * cannot report, its code phase being a lag modulo one code period -- and
 * the burst is demodulated once its last sample has arrived.
 *
 * EVERY SAMPLE OF @p x IS CONSUMED, and every burst that completes is
 * returned by the call that completed it. Payloads are concatenated, so
 * burst `i` occupies `out` from `i*frame_syms`, and
 * `events()` returns the matching record for each. Returning 0 is normal,
 * not an error: it means no burst completed in this call.
 *
 * This is the contract doppler#1008 broke. push() used to return at most one
 * burst per call AND abandon the rest of its input to do it, so a block
 * carrying several bursts lost all but the first -- measured at 6/6 decoded
 * with 333-sample blocks against 1/6 with one large one. The history ring is
 * a contiguous window over the stream and is never reset between bursts, so
 * a payload whose tail falls outside one call is completed by a later one.
 *
 * @param state  Must be non-NULL.
 * @param x  Input samples (cf32), @p x_len long.
 * @param x_len  Number of input samples.
 * @param out  Payload bits, caller-owned, @p max_out long.
 * @param max_out  Capacity of @p out; see push_max_out().
 * @return Bits written to @p out -- `n_bursts_returned * frame_syms`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> rx = DsssBurstReceiver(
 * ...     rng.integers(0, 2, 31).astype(np.uint8),
 * ...     rng.integers(0, 2, 8).astype(np.uint8),
 * ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
 * >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
 * >>> bits.size            # silence carries no burst
 * 0
 *
 * @endcode
 */
size_t dsss_burst_receiver_push(dsss_burst_receiver_state_t *state, const float complex *x, size_t x_len, uint8_t *out, size_t max_out);

/**
 * @brief Max records events() writes: one per burst the last push() returned.
 *
 * @param state  Must be non-NULL.
 * @return The number of bursts the most recent push() completed.
 */
size_t dsss_burst_receiver_events_max_out(dsss_burst_receiver_state_t *state);

/**
 * @brief The SOFT bits of every burst the last push() returned.
 *
 * `crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and the
 * demodulator used to compute it, slice it to one bit and free it. A hard
 * decision throws away roughly 2 dB of the coding gain a soft-input decoder
 * exists to deliver (`mpsk_soft_demap`'s own docstring), which is what makes
 * a coded burst worth coding.
 *
 * Concatenated the same way push()'s payloads are, one row of `frame_bits`
 * per burst: burst @c i starts at `i * frame_bits`, in the order events()
 * reports. The convention
 * is `mpsk_soft_demap`'s — positive means bit 0, so `L < 0` reproduces
 * exactly the bits push() returned. Spans the WHOLE frame rather than the
 * payload alone, because a code covers what its description says it covers.
 *
 * Valid until the next push(), reset() or set_state(); deliberately not
 * serialized, for the same reason events() is not: it describes one call.
 *
 * @param state    Receiver handle.
 * @param n        Ignored — the count is the last push's, not a request.
 * @param out      Receives the LLRs.
 * @param max_out  Capacity of @p out; see llrs_max_out().
 * @return LLRs written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> rx = DsssBurstReceiver(
 * ...     rng.integers(0, 2, 31).astype(np.uint8),
 * ...     rng.integers(0, 2, 8).astype(np.uint8),
 * ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
 * >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
 * >>> len(bits), len(rx.llrs(rx.llrs_max_out(1)))   # nothing decoded
 * (0, 0)
 *
 * @endcode
 */
size_t dsss_burst_receiver_llrs(dsss_burst_receiver_state_t *state, size_t n, float *out, size_t max_out);

/**
 * @brief Max LLRs llrs() writes: frame bits x the bursts the last push
 *        returned.
 *
 * @param state  Receiver handle.
 * @param n      Ignored, as in llrs().
 */
size_t dsss_burst_receiver_llrs_max_out(dsss_burst_receiver_state_t *state, size_t n);


/**
 * @brief The event record for each burst the last push() returned.
 *
 * Row `i` describes the payload at `out[i*frame_syms ...]` of that push.
 * A single push can complete many bursts and each needs its own event, so
 * these are a list rather than the scalar read-backs -- those still exist
 * and still describe the LAST burst, but they cannot speak for the others.
 *
 * Valid until the next push(), reset() or set_state(). Deliberately not
 * serialized: it describes one call, and keeping it out of the blob is what
 * holds state_bytes() to a pure function of configuration.
 *
 * @param state  Must be non-NULL.
 * @param n  Ignored. The record count is whatever the last push() produced,
 *           not something a caller chooses; this parameter exists because
 *           every variable-output method carries one, and the binding uses
 *           it only as a floor on the buffer it allocates.
 * @param out  Records, caller-owned, @p max_out long.
 * @param max_out  Capacity of @p out; see events_max_out().
 * @return Records written to @p out -- `min(events_max_out(), max_out)`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> rx = DsssBurstReceiver(
 * ...     rng.integers(0, 2, 31).astype(np.uint8),
 * ...     rng.integers(0, 2, 8).astype(np.uint8),
 * ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
 * >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
 * >>> len(rx.events()) == bits.size // 32   # one record per payload
 * True
 *
 * @endcode
 */
size_t dsss_burst_receiver_events(dsss_burst_receiver_state_t *state, size_t n, dsss_br_event_t *out, size_t max_out);
/**
 * @brief Pin the acquisition search grid, bypassing the auto-sizing.
 *
 * The escape hatch for a caller who wants a specific (doppler_bins,
 * n_noncoh) rather than the grid the cn0_dbhz/pfa/pd sizing chooses.
 * Forwards to the embedded engine unchanged.
 *
 * @param state  Must be non-NULL.
 * @param doppler_bins  Coherent depth to pin, in `[1, reps]`.
 * @param n_noncoh  Non-coherent looks to combine.
 * @return 0 on success, non-zero if the grid is out of range.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import DsssBurstReceiver
 * >>> rng = np.random.default_rng(0)
 * >>> rx = DsssBurstReceiver(
 * ...     rng.integers(0, 2, 31).astype(np.uint8),
 * ...     rng.integers(0, 2, 8).astype(np.uint8),
 * ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
 * >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=1)
 *
 * @endcode
 */
int dsss_burst_receiver_configure_search_raw(dsss_burst_receiver_state_t *state, size_t doppler_bins, size_t n_noncoh);
uint64_t dsss_burst_receiver_get_preamble_start(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_doppler_hz_est(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_doppler_res_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_cn0_dbhz_est(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_freq_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_rate_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_snr_db(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_refine_margin(const dsss_burst_receiver_state_t *state);
size_t dsss_burst_receiver_get_pending(const dsss_burst_receiver_state_t *state);
uint64_t dsss_burst_receiver_get_dropped(const dsss_burst_receiver_state_t *state);
uint64_t dsss_burst_receiver_get_n_bursts(const dsss_burst_receiver_state_t *state);

/* ── Serializable state — the elastic / pure-transducer face ──────────────
 *
 * The composition's checkpoint boundary is BETWEEN bursts, which is what
 * makes burst_demod's deliberate statelessness cost nothing: a burst
 * completes inside one demod() call or is lost (its own validation report
 * certifies that as correct), so there is no mid-demod position to save.
 * What must travel is this object's own stream bookkeeping, the retained
 * look-back the next burst may still need, and the acquisition engine's
 * own state -- delegated to its triplet, never re-packed here.
 */

/** @brief Per-object envelope tag: "DBRX" (DsssBurstReceiver). */
#define DSSS_BURST_RECEIVER_STATE_MAGIC DP_FOURCC('D', 'B', 'R', 'X')
#define DSSS_BURST_RECEIVER_STATE_VERSION 4u

/** @brief Byte size of @p state's blob (envelope + payload + child). */
size_t dsss_burst_receiver_state_bytes(const dsss_burst_receiver_state_t *state);

/**
 * @brief Serialize @p state's cross-call state into @p blob (caller-owned,
 *        dsss_burst_receiver_state_bytes() long).
 */
void dsss_burst_receiver_get_state(const dsss_burst_receiver_state_t *state, void *blob);

/**
 * @brief Restore cross-call state from @p blob (replacing it).
 * @return DP_OK, or DP_ERR_INVALID if the envelope or any child rejects.
 */
int dsss_burst_receiver_set_state(dsss_burst_receiver_state_t *state, const void *blob);
#ifdef __cplusplus
}
#endif

#endif /* DSSS_BURST_RECEIVER_CORE_H */
