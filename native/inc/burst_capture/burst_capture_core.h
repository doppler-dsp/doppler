/**
 * @file burst_capture_core.h
 * @brief BurstCapture — acquisition's output turned into aligned bursts.
 *
 * Between a detector and whatever consumes a burst there is a stage nobody
 * owned: acquisition reports an END anchor and a code phase that is a lag
 * MODULO one code period, so it fixes the alignment WITHIN a repetition and
 * never says WHICH one. A burst has a frame that begins in one specific
 * repetition, so somebody has to resolve the period and reach BACK to a
 * start that has already gone past. This object is that somebody.
 *
 * It searches, refines, retains, and emits the burst's SAMPLES. It stops
 * there — demodulating, recording, or shipping a window elsewhere is the
 * caller's business. `DsssBurstReceiver` is this plus `BurstDemod`.
 *
 * It OWNS its acquisition engine rather than accepting someone else's
 * results, and that is a correctness choice rather than a convenience one:
 * `acq_result_t::samples_consumed` is stream-absolute only for an engine fed
 * continuously and never reset, in the caller's own sample coordinates. An
 * object taking foreign results would have to require that and could not
 * check it — and a violated assumption is not a slightly wrong window, it is
 * refine searching the wrong repetition, which returns noise rather than a
 * degraded frame. `push()` defining the coordinate system makes the
 * invariant internal. See docs/design/dsss-burst-receiver.md §11.
 *
 * Lifecycle: create, then push() repeatedly, then destroy. There is no
 * step()/steps(): a burst is a frame, not a sample.
 *
 * @code
 * uint8_t code[31];
 * for (size_t i = 0; i < 31; i++) code[i] = (uint8_t)(i & 1u);
 * burst_capture_state_t *cap = burst_capture_create (
 *     code, 31, 4096, 4, 4, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
 * float complex x[2048] = { 0 };
 * float complex win[4096];
 * size_t n = burst_capture_push (cap, x, 2048, win, 4096);
 * // n is a multiple of burst_len: burst i starts at i*burst_len
 * burst_capture_destroy (cap);
 * @endcode
 */
#ifndef BURST_CAPTURE_CORE_H
#define BURST_CAPTURE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "buffer/buffer.h"
#include "dp_state.h"
#include "burst_acq/burst_acq_core.h"
#include "acq/acq_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "fft/fft_core.h"
#include "detection/detection_core.h"
#include "pn/pn_core.h"

/**
 * @brief Detections collected from acquisition per batch.
 *
 * A BATCHING parameter, never a correctness one: push() loops until acq has
 * absorbed the whole chunk, so a smaller array means more iterations and
 * nothing else. Growing it to "be safe" would hide the fact that acq_push()
 * stops once its result array is full and abandons the rest of its input.
 */
#define BURST_CAPTURE_HITS 16u

/** @brief State blob magic — a wrong blob is rejected, not reinterpreted. */
#define BURST_CAPTURE_STATE_MAGIC DP_FOURCC ('B', 'C', 'A', 'P')
/** @brief State blob layout version. */
#define BURST_CAPTURE_STATE_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One captured burst's event, as `events()` hands it back.
 *
 * push() returns the SAMPLES of every burst it completed, concatenated; this
 * is the parallel record for row `i` of that return. It exists because a
 * single push() can complete many bursts and each one needs its own event --
 * a single set of scalar read-backs would describe only the last
 * (docs/design/dsss-burst-receiver.md §4: the record must be sufficient on
 * its own, for EVERY burst, not just the most recent).
 *
 * Everything here is acquisition's or refine's. A consumer's own estimates
 * (a demodulator's residual frequency, its post-decode SNR) belong to the
 * consumer's record, not to this one.
 */
typedef struct
{
  uint64_t preamble_start; /**< Exact stream position of the preamble.     */
  double   doppler_hz_est; /**< Signed coarse Doppler, Hz.                 */
  double   doppler_res_hz; /**< Acquisition's native bin width, Hz.        */
  double   cn0_dbhz_est;   /**< C/N0 lower bound from the hit, dB-Hz.      */
  double   refine_margin;  /**< Runner-up period over the winner.          */
} burst_capture_event_t;

/**
 * @brief One raw detection, as the search reported it.
 *
 * What `detections()` hands back: everything acquisition found in the last
 * push, BEFORE the claim rule merged anything and before the suppression
 * window dropped anything. `events()` is the other end of the same pipe --
 * the bursts that survived all of that and whose windows arrived.
 *
 * Both exist because they answer different questions and arrive at different
 * times. A detection is available the moment a frame clears threshold; a
 * burst is not available until its LAST sample has, which is `retain_span`
 * later and never for a burst that was cut off. A caller watching a band for
 * activity wants the first; a caller decoding wants the second; a bank doing
 * both would otherwise have to run two acquisition engines over one stream.
 *
 * The epoch is stream-absolute, which `acq_result_t::code_phase` is not --
 * that is a lag modulo one code period, and making it absolute is the first
 * thing this object does with a hit.
 */
typedef struct
{
  uint64_t epoch;      /**< Stream-absolute code epoch of the hit.         */
  double   doppler_hz; /**< Signed coarse Doppler, folded, Hz.            */
  double   cn0_dbhz;   /**< C/N0 lower bound from the hit, dB-Hz.         */
  double   test_stat;  /**< The CFAR gating statistic, peak over noise.   */
  double   peak_mag;   /**< Raw CFAR peak magnitude.                      */
} burst_capture_detection_t;

/**
 * @brief One detection between acquisition and emission.
 *
 * A hit cannot always be refined the moment it arrives -- the refine window
 * reaches BACKWARDS and forwards, so some of it may not have been pushed
 * yet -- and a detection dropped because its window was incomplete is a lost
 * burst. So a hit is queued here with the event fields acquisition supplied,
 * refined when its window is reachable, and emitted when the burst has fully
 * arrived.
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
} burst_capture_pending_t;

/**
 * @brief BurstCapture state.
 *
 * Allocate with burst_capture_create().
 */
typedef struct
{
  /* ── Configuration, copied at create() ──────────────────────────────── */
  uint8_t *acq_code;     /**< Preamble code, owned copy.                   */
  size_t   acq_code_len; /**< Preamble code length, chips.                 */
  size_t   reps;         /**< Preamble code repetitions.                   */
  size_t   spc;          /**< Samples per chip.                            */
  double   chip_rate;    /**< Chip rate, Hz.                               */

  /* ── Derived geometry ───────────────────────────────────────────────── */
  size_t code_period; /**< One preamble repetition, in SAMPLES. The modulus
                           acq's code_phase is a residue of, so every epoch
                           ambiguity in this object is stated against it.  */
  size_t burst_len;   /**< Samples in one emitted window. Acquisition has
                           no notion of this -- acq_create_burst() takes
                           search parameters only -- which is exactly why it
                           is a parameter HERE: for a capture, the burst
                           length is what gets captured.                   */

  /* ── The composed child ─────────────────────────────────────────────── */
  burst_acq_state_t *acq; /**< Search stage, certified separately.         */

  /* ── Look-back (docs/design/dsss-burst-receiver.md §7.1) ────────────── */
  dp_f32_t *hist;      /**< History ring. Double-mapped, so a window that
                            spans the wrap is ONE contiguous pointer. This
                            object keeps its own rather than borrowing
                            acq's, which consumes every frame it processes
                            and has therefore released what is still
                            needed.                                        */
  uint64_t samples_fed; /**< Stream position: total samples ever pushed.
                             What makes an epoch stream-ABSOLUTE, and the
                             reason preamble_start is a quantity only this
                             object can compute.                           */

  /* ── The event describing the most recent window emitted ────────────── */
  uint64_t preamble_start; /**< Stream-absolute preamble start. Never late. */
  double   doppler_hz_est; /**< Signed coarse Doppler, Hz.                  */
  double   doppler_res_hz; /**< Width of that estimate.                     */
  double   cn0_dbhz_est;   /**< C/N0 lower bound, dB-Hz (saturating).       */
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
                            `(k_lo + k_hi + reps) * code_period`. Read it
                            rather than restating the formula -- the design
                            doc's own prose for it was 2.4x low at reps=5
                            until it was measured.

                            The merge test compares two resolved code
                            epochs -- burst START against burst START -- so
                            it bounds start-to-start separation, NOT the
                            dead air between bursts (doppler#1085). The gap
                            actually required is NOT
                            `max(0, refine_span - burst_len)` either: swept,
                            a pair needs about two code periods of dead air,
                            against the 32 samples that formula gives at the
                            test geometry (doppler#1172).                 */
  size_t corr_len;     /**< Entries in corr_buf.                            */
  size_t min_gap;      /**< Dead air a caller must leave BETWEEN bursts, in
                            samples -- edge to edge, not start to start.

                            DERIVED, and the derivation is the point. A
                            detection's anchor is the code epoch of whichever
                            frame detected, and acquisition's framing is not
                            aligned to the preamble, so the last frame that
                            can detect sits up to `reps * code_period` past
                            the true start (the detection lag,
                            docs/design/dsss-burst-receiver.md §7.1). CLAIM
                            merges two anchors closer than `refine_span`, so
                            with the first burst detected LATE and the second
                            EARLY the pair survives only when

                              gap >= refine_span + reps*P - burst_len

                            ZERO is a real answer -- a burst longer than
                            `refine_span + reps*P` needs no gap for the claim
                            rule's sake. It does not mean zero is wise: a
                            zero gap is a continuous stream rather than a
                            burst link (the design's own non-goal), and it
                            measures 88% at a geometry where this reads 0.

                            The prose this replaces said
                            `max(0, refine_span - burst_len)` and was short by
                            the whole detection-lag term -- 32 samples against
                            528 at the C suite's geometry (doppler#1172).   */
  size_t retain_span;  /**< Samples that must stay reachable: refine span +
                            one whole burst. Also the caller-facing minimum
                            TRAILING context -- a burst closer than this to
                            the end of what has been pushed is not emitted
                            until more samples arrive.                     */
  size_t chunk_max;    /**< Largest slice of one push processed at a time,
                            so any block size is accepted without the ring
                            overrunning its own retention.                 */
  size_t k_lo;         /**< Whole code periods searched BEFORE the anchor.  */
  size_t k_hi;         /**< ...and after.                                   */

  /* ── Detections in flight ────────────────────────────────────────────
   * Only detections whose burst window has NOT yet arrived live here: every
   * one whose window HAS arrived is emitted before push() returns, which is
   * what bounds retention (see the trim rule in the implementation). */
  burst_capture_pending_t *q; /**< Detections, oldest first; `q_cap` long.  */
  size_t q_cap;   /**< DERIVED, not a constant. Entries sit at least
                       `refine_span` apart within `retain_span` of the head,
                       so the count scales with burst_len/refine_span --
                       about 1 at a short-burst test geometry but 5.5x at a
                       real link. A fixed 8 silently dropped the hit AND the
                       rest of the batch on anything else.                 */
  size_t q_head;  /**< Index of the oldest entry.                          */
  size_t pending; /**< Detections held because their burst window has NOT
                       fully arrived -- the caller-facing "there is not
                       enough data yet" read-back. push() deliberately emits
                       nothing for these: a window is returned when it is
                       complete, not when it is guessed at. What it exists
                       for is the other end -- a caller closing a file while
                       this is non-zero is discarding a burst that would
                       have been captured.                                 */

  /* ── The windows of the LAST push ────────────────────────────────────
   * Scratch, deliberately NOT serialized: it describes the most recent
   * push() only, so keeping it out of the blob is what lets state_bytes()
   * stay a pure function of configuration.
   *
   * The windows are COPIED here rather than left in the ring. A window in
   * the ring is a borrow whose lifetime the retention rule would have to
   * extend across the whole call, and one push can complete several bursts
   * -- so the ring would have to hold every one of them at once, which its
   * derived capacity does not promise. The cost is one memcpy per BURST,
   * not per sample, which is a different order of magnitude from the copy
   * §6.1 weighs (that one is the whole stream). It is also what lets a C
   * consumer borrow a window through burst_capture_window() and hand it
   * onward with no further copy. */
  burst_capture_detection_t *det;     /**< Raw hits of the LAST push -- what
                                           the SEARCH found, before the claim
                                           rule and the suppression window.  */
  size_t                     det_cap; /**< Allocated records.               */
  size_t                     det_len; /**< Records the last push wrote.     */
  float _Complex *win;     /**< Emitted windows, burst_len apart.          */
  size_t          win_cap; /**< Allocated samples.                          */
  burst_capture_event_t *ev; /**< One record per window returned.           */
  size_t ev_cap;           /**< Allocated records.                          */
  size_t ev_len;           /**< Records the last push() wrote.              */

  uint64_t suppress_until; /**< Detections below this stream position fall
                                inside a burst already EMITTED, so they are
                                the payload firing against the acquisition
                                code rather than new bursts. Armed when a
                                window is emitted -- refine resolved a start
                                here and this object handed out the whole
                                span, which is the fact it owns. Arming it
                                on every DETECTION instead let one spurious
                                hit blind the search for a whole burst and
                                discard the next real one (doppler#1004).
                                Coalescing the several frames of ONE
                                preamble is a separate job, done by
                                `refine_span` proximity plus a greatest-of
                                tie-break.                                 */
  size_t acq_blob_max;     /**< Fixed upper bound on the acquisition child's
                                blob. state_bytes() must be a pure function
                                of CONFIGURATION -- jm's binding compares an
                                incoming blob's length against it -- yet both
                                the retained look-back and acq's own
                                unconsumed ring vary with the stream. Both
                                are therefore written into fixed-size regions
                                with a length prefix.                      */

  /* ── Persistence (docs/design/burst-capture.md §9) ───────────────────── */
  int backed;   /**< Non-zero when the ring's pages are a FILE's. Fixed at
                     create(), so state_bytes() stays a pure function of
                     configuration -- a backed blob and an in-RAM one are
                     different sizes on purpose, and neither restores into
                     the other.                                            */
  int recovered; /**< Non-zero when create() found the backing file already
                      holding a ring of exactly this geometry, so its
                      samples ARE the look-back. Zero when the file was
                      created or resized, which zeroes it -- and then a blob
                      claiming retained history has nothing to reach back
                      into, which set_state() refuses rather than resuming
                      into silence.                                        */

  /* ── Diagnostics ────────────────────────────────────────────────────
   * Mirrored from the engine at create() rather than read through it on
   * demand, because jm's declared warning needs a bare bool field on THIS
   * struct -- the reason the sibling BurstAcquisition's copy of the same
   * warning has to be a hand-patch in its fragment (see the note at the top
   * of objects/burst_acq.toml). */
  int underpowered; /**< The search cannot meet the requested pd at this
                         cn0_dbhz and geometry. It still builds a
                         best-effort grid, so the symptom is bursts that are
                         never captured rather than an error.             */

  /* ── Bookkeeping ────────────────────────────────────────────────────── */
  uint64_t dropped;  /**< Samples the ring refused. A LOST BURST each, not
                          a statistic -- lifetime, survives reset().       */
  uint64_t n_bursts; /**< Windows emitted, lifetime.                       */
/*<<property_struct_fields>>*/
} burst_capture_state_t;

/**
 * @brief Create a burst capture: acquisition, refine and retention behind
 *        one push().
 *
 * Give it the preamble code and the geometry, say how long a burst is, and
 * stream samples in. It searches blindly, recovers the exact preamble start,
 * and hands back the burst's samples once they have all arrived.
 *
 * The look-back buffer is NOT a parameter. Its span is derived from the
 * geometry here (detection lag + refine search + the burst itself), because
 * every term is already known and a caller asked to size a history buffer is
 * a caller handed a way to lose bursts silently.
 *
 * @param acq_code      Preamble PN chips (0/1), length @p acq_code_len.
 * @param acq_code_len  Preamble code length, chips.
 * @param burst_len     Samples in one burst -- what gets captured.
 * @param reps          Preamble code repetitions.
 * @param spc           Samples per chip.
 * @param chip_rate     Chip rate, Hz.
 * @param cn0_dbhz      C/N0 the search is sized for, dB-Hz.
 * @param doppler_uncertainty  Doppler search half-range, Hz (0 = native).
 * @param pfa           Target false-alarm probability, in (0, 1).
 * @param pd            Target detection probability, in (0, 1).
 * @param noise_mode    CFAR reference: 0=mean, 1=median, 2=min, 3=max.
 * @return Heap state, or NULL if any parameter is out of range.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> cap.burst_len
 * 512
 * >>> cap.retain_span == cap.refine_span + cap.burst_len
 * True
 * @endcode
 */
burst_capture_state_t *burst_capture_create (const uint8_t *acq_code,
                                             size_t acq_code_len,
                                             size_t burst_len, size_t reps,
                                             size_t spc, double chip_rate,
                                             double cn0_dbhz,
                                             double doppler_uncertainty,
                                             double pfa, double pd,
                                             int noise_mode);

/**
 * @brief Create a capture whose look-back lives in a FILE.
 *
 * Same object, same behaviour, one difference in where the history ring's
 * pages come from: they are a `MAP_SHARED` mapping of @p path, so the ring's
 * samples ARE the file's contents. There is no copy and no separate flush
 * path — the kernel writes the pages back, and `get_state()` forces the point
 * so a checkpoint and its history agree.
 *
 * Two things follow, and they are the reason to reach for this constructor:
 *
 * - **The blob stops carrying the look-back.** For an in-RAM capture the
 *   retained history IS the blob (measured: 2.57 MB at a 1029-symbol frame,
 *   16.68 MB at 8029). Backed, `state_bytes()` is a few hundred bytes plus
 *   the acquisition child, because the samples are already durable and the
 *   blob only has to name where in the ring they sit.
 * - **The history outlives the process.** Point a new capture at the same
 *   path and the samples are there; restore the blob and it reaches back
 *   across the restart into a burst that began before it.
 *
 * The file is created if absent and truncated to the ring's byte size, which
 * zeroes it. An existing file of exactly that size is adopted as it stands.
 * Because the capacity rounds up to a page, that size is
 * `capacity * sizeof(float complex)` — do not compute it from `burst_len`.
 *
 * A blob from a backed capture does NOT restore into an in-RAM one, or the
 * reverse: `state_bytes()` differs, so jm's length check rejects it. That is
 * the intent — they are different configurations, and silently accepting one
 * for the other would resume a capture whose history was somewhere else.
 *
 * @param path          File to back the ring with; not NULL and not empty.
 * @param acq_code      Preamble PN chips (0/1), length @p acq_code_len.
 * @param acq_code_len  Preamble code length, chips.
 * @param burst_len     Samples in one burst -- what gets captured.
 * @param reps          Preamble code repetitions.
 * @param spc           Samples per chip.
 * @param chip_rate     Chip rate, Hz.
 * @param cn0_dbhz      C/N0 the search is sized for, dB-Hz.
 * @param doppler_uncertainty  Doppler search half-range, Hz (0 = native).
 * @param pfa           Target false-alarm probability, in (0, 1).
 * @param pd            Target detection probability, in (0, 1).
 * @param noise_mode    CFAR reference: 0=mean, 1=median, 2=min, 3=max.
 * @return Heap state, or NULL if a parameter is out of range or the file
 *         could not be opened, sized or mapped.
 *
 * @code
 * >>> import numpy as np, tempfile, os
 * >>> from doppler.dsss import BurstCapture, PersistentBurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
 * >>> cap = PersistentBurstCapture(path, code, burst_len=512,
 * ...                             reps=4, spc=2)
 * >>> ram = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
 * >>> # the look-back is in the file, so the blob stops carrying it
 * >>> ram.state_bytes() - cap.state_bytes() == ram.retain_span * 8
 * True
 * >>> os.path.getsize(path) > 0
 * True
 * @endcode
 */
burst_capture_state_t *
burst_capture_create_backed (const char *path, const uint8_t *acq_code,
                             size_t acq_code_len, size_t burst_len,
                             size_t reps, size_t spc, double chip_rate,
                             double cn0_dbhz, double doppler_uncertainty,
                             double pfa, double pd, int noise_mode);

/** @brief Release a capture and everything it owns. NULL-safe. */
void burst_capture_destroy (burst_capture_state_t *state);

/**
 * @brief Return to the searching state.
 *
 * Resets the embedded acquisition, rewinds the history ring, clears every
 * queued detection and every read-back. Construction parameters are
 * untouched; `dropped` deliberately survives, because a lost burst stays
 * lost.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> cap.push(np.zeros(4096, dtype=np.complex64)).size
 * 0
 * >>> cap.reset()
 * >>> cap.pending
 * 0
 * @endcode
 */
void burst_capture_reset (burst_capture_state_t *state);

/**
 * @brief Upper bound on samples push() can return for @p x_len input.
 *
 * Distinct bursts cannot overlap, so `x_len` samples complete at most
 * `x_len/burst_len + 1` of them, plus whatever is already queued.
 */
size_t burst_capture_push_max_out (burst_capture_state_t *state,
                                   size_t x_len);

/**
 * @brief Stream samples; get back every burst whose window has arrived.
 *
 * Windows are concatenated: burst `i` occupies `burst_len` samples starting
 * at `i*burst_len`, and events() returns the matching record for each. Every
 * sample of @p x is consumed. An empty return is normal -- it means no burst
 * completed in this call.
 *
 * @param state    Capture.
 * @param x        Input samples, @p x_len long.
 * @param x_len    Samples in @p x.
 * @param out      Written with the completed windows; may be NULL to drop.
 * @param max_out  Capacity of @p out, in samples.
 * @return Samples written -- always a multiple of `burst_len`.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
 * >>> win.size % cap.burst_len        # whole windows, never a partial
 * 0
 * >>> win.size                        # silence, so no burst completed
 * 0
 * @endcode
 */
size_t burst_capture_push (burst_capture_state_t *state,
                           const float complex *x, size_t x_len,
                           float complex *out, size_t max_out);

/** @brief Raw detections available from the last push(). @p n is ignored. */
size_t burst_capture_detections_max_out (burst_capture_state_t *state,
                                         size_t n);

/**
 * @brief Every hit the search made in the last push(), unfiltered.
 *
 * BEFORE the claim rule and the suppression window: several rows can name one
 * preamble, and a row can be a false alarm. That is the point -- this is what
 * acquisition FOUND, and `events()` is what survived. Valid until the next
 * push(), reset() or set_state().
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
 * >>> # what the search found, against what became a burst
 * >>> len(cap.detections()) >= len(cap.events())
 * True
 * @endcode
 */
size_t burst_capture_detections (burst_capture_state_t *state, size_t n,
                                 burst_capture_detection_t *out,
                                 size_t max_out);

/** @brief Records available from the last push(). @p n is ignored. */
size_t burst_capture_events_max_out (burst_capture_state_t *state, size_t n);

/**
 * @brief The event record for each burst the last push() returned.
 *
 * Row `i` describes the window at `i*burst_len`. Valid until the next
 * push(), reset() or set_state().
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
 * >>> len(cap.events()) == win.size // cap.burst_len
 * True
 * @endcode
 */
size_t burst_capture_events (burst_capture_state_t *state, size_t n,
                             burst_capture_event_t *out, size_t max_out);

/**
 * @brief Windows the last push() completed.
 *
 * The C consumer's face, and the reason a composing object pays no second
 * copy: burst_capture_window() borrows straight out of the scratch that
 * push() filled.
 */
size_t burst_capture_ready (const burst_capture_state_t *state);

/**
 * @brief Borrow window @p i of the last push(), or NULL if out of range.
 *
 * Contiguous, `burst_len` samples, valid until the next push(), reset() or
 * set_state(). The caller must not free it.
 */
const float complex *burst_capture_window (const burst_capture_state_t *state,
                                           size_t i);

/** @brief Borrow event @p i of the last push(), or NULL if out of range. */
const burst_capture_event_t *
burst_capture_event_at (const burst_capture_state_t *state, size_t i);

/**
 * @brief Pin the embedded acquisition's search grid directly.
 *
 * The escape hatch for a caller who wants a specific (doppler_bins,
 * n_noncoh). Forwards to the engine unchanged.
 *
 * @return DP_OK, or DP_ERR_INVALID if the engine refused the grid.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstCapture
 * >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
 * >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
 * >>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only
 * @endcode
 */
int burst_capture_configure_search_raw (burst_capture_state_t *state,
                                        size_t doppler_bins,
                                        size_t n_noncoh);

/* ── Serializable state — the elastic / pure-transducer face ──────────── */

/* ── The search this capture will do, as numbers ──────────────────────
 *
 * A capture is only as good as the search under it, and a caller sizing a
 * link needs to see that search rather than infer it. These forward the
 * engine's own figures: what a detection must clear, how deep the sizer
 * went, and how wide in Doppler and code phase it will look.
 *
 * They are read-backs, not knobs -- every one is derived at create() from
 * the parameters above, and `configure_search_raw()` is the one call that
 * moves them. */

/** @brief Dead air a caller must leave between bursts, edge to edge. */
size_t burst_capture_get_min_gap (const burst_capture_state_t *state);

/** @brief Coherent detection gate; in force when `n_noncoh == 1`. */
double burst_capture_get_eta (const burst_capture_state_t *state);
/** @brief Non-coherent gate; in force when `n_noncoh > 1` (the usual case). */
double burst_capture_get_eta_nc (const burst_capture_state_t *state);
/** @brief Correlation kept, worst case, by a burst landing between bins. */
double burst_capture_get_straddle_loss (const burst_capture_state_t *state);
/** @brief Detection probability the sized grid actually predicts. */
double burst_capture_get_pd_predicted (const burst_capture_state_t *state);
/** @brief Doppler hypotheses searched (the coherent depth). */
size_t burst_capture_get_doppler_bins (const burst_capture_state_t *state);
/** @brief Non-coherent looks combined per decision. */
size_t burst_capture_get_n_noncoh (const burst_capture_state_t *state);
/** @brief Code-phase hypotheses per Doppler row. */
size_t burst_capture_get_code_bins (const burst_capture_state_t *state);
/** @brief Unambiguous Doppler half-range, Hz (+/- this). */
double burst_capture_get_doppler_span_hz (const burst_capture_state_t *state);

/** @brief Bytes one blob occupies: a pure function of CONFIGURATION. */
size_t burst_capture_state_bytes (const burst_capture_state_t *state);
/** @brief Serialize into @p blob, which must be state_bytes() long. */
void burst_capture_get_state (const burst_capture_state_t *state, void *blob);
/** @brief Restore from @p blob. @return DP_OK or DP_ERR_INVALID. */
int burst_capture_set_state (burst_capture_state_t *state, const void *blob);

uint64_t burst_capture_get_preamble_start(const burst_capture_state_t *state);
double burst_capture_get_doppler_hz_est(const burst_capture_state_t *state);
double burst_capture_get_doppler_res_hz(const burst_capture_state_t *state);
double burst_capture_get_cn0_dbhz_est(const burst_capture_state_t *state);
double burst_capture_get_refine_margin(const burst_capture_state_t *state);
size_t burst_capture_get_pending(const burst_capture_state_t *state);
uint64_t burst_capture_get_dropped(const burst_capture_state_t *state);
uint64_t burst_capture_get_n_bursts(const burst_capture_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* BURST_CAPTURE_CORE_H */
