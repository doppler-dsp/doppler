/*
 * timing_core.h — sample-clock pacing + timestamping.
 *
 * A ``dp_sample_clock_t`` models an ideal sample clock running at ``fs`` Hz.
 * It does two things off one shared, drift-free timeline anchored when the
 * clock is created or reset:
 *
 *   - PACE a producer so each block leaves at its real-time deadline
 *     ``epoch + n/fs`` — mimicking a hardware sample clock feeding a DAC.
 *   - STAMP a block with the ideal wall-clock time of its samples
 *     (``epoch_real + n/fs``), for reproducible capture metadata.
 *
 * Because both derive from the cumulative sample count ``n`` against a fixed
 * epoch — not from summed per-block sleeps — the schedule is drift-free: an
 * over- or under-sleep on one block is corrected on the next. The residual
 * error is per-block jitter bounded by the OS scheduler, which averages out.
 *
 * Pacing is POSIX only (``clock_gettime`` + an absolute-deadline sleep); the
 * build guards this translation unit out on Windows, mirroring the stream
 * sink.
 */
#ifndef DP_TIMING_CORE_H
#define DP_TIMING_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** Sample-clock state. Treat the fields as read-only from the outside;
   *  mutate only through the functions below. */
  typedef struct
  {
    double   fs;            /**< sample rate (Hz). */
    uint64_t epoch_mono_ns; /**< CLOCK_MONOTONIC baseline for pacing. */
    uint64_t epoch_real_ns; /**< CLOCK_REALTIME baseline for stamping. */
    uint64_t n;             /**< cumulative samples advanced. */
    uint64_t underruns;     /**< pace() calls that arrived past deadline. */
    uint64_t max_late_ns;   /**< worst lateness observed (ns). */
    int      resync;        /**< nonzero: pace() re-anchors on underrun. */
    int      has_anchor; /**< nonzero once track() has adopted a real observed
                               (timestamp, n) pair; init()/reset() start at 0 --
                               the receive-side counterpart of the TX-side
                               epoch capture, so the first track() call always
                               adopts rather than tolerance-gating against an
                               arbitrary construction-time guess. */
  } dp_sample_clock_t;

  /** Current monotonic clock in ns (CLOCK_MONOTONIC) — for pacing. */
  uint64_t dp_mono_ns (void);

  /** Current wall-clock in ns since the UNIX epoch (CLOCK_REALTIME). */
  uint64_t dp_real_ns (void);

  /** Initialise @p c for sample rate @p fs (Hz), capturing both epochs now.
   *  If @p resync is nonzero, pace() re-anchors the timeline to "now"
   *  whenever it falls behind (absorbing the slip) instead of keeping the
   *  absolute schedule. */
  void dp_sample_clock_init (dp_sample_clock_t *c, double fs, int resync);

  /** Advance by @p count samples and sleep until that block's deadline
   *  (``epoch + n/fs``). Returns the slack in seconds measured before
   *  sleeping: ``>= 0`` means early (and it slept that long); ``< 0`` means
   *  it arrived late — an underrun, which is counted (and the epoch
   *  re-anchored when ``resync`` is set), with no sleep. */
  double dp_sample_clock_pace (dp_sample_clock_t *c, size_t count);

  /** Ideal wall-clock timestamp (ns since the UNIX epoch) of the next sample
   *  to be produced — sample index ``n``. Call it before pace() to tag the
   *  block you are about to emit, or after to tag the following block.
   *  Equivalent to ``dp_sample_clock_stamp_at(c, c->n)``. */
  uint64_t dp_sample_clock_stamp (const dp_sample_clock_t *c);

  /** Ideal wall-clock timestamp (ns since the UNIX epoch) of an ARBITRARY
   *  sample index @p n — past, present, or future, not just the clock's own
   *  live position. The receive-side counterpart of dp_sample_clock_stamp():
   *  a block emitting several per-record outputs from one buffered input
   *  (e.g. several detections spanning different epochs from one streamed
   *  message) stamps each at its own historical sample offset instead of
   *  reusing the whole buffer's single arrival time. */
  uint64_t dp_sample_clock_stamp_at (const dp_sample_clock_t *c, uint64_t n);

  /** Reconcile @p c's epoch_real_ns against one OBSERVED (timestamp, sample
   *  index) pair read off an incoming stream header — the receive-side dual
   *  of pace()'s resync: instead of sleeping toward a deadline, this adopts
   *  or corrects the epoch from ground truth the sender already stamped.
   *
   *  The FIRST call always adopts @p observed_timestamp_ns as the epoch
   *  (``has_anchor`` starts false — a fresh clock has no real observation
   *  yet, so there is nothing to compare against). Every later call only
   *  re-anchors if the discrepancy between the observation and what the
   *  clock's current model predicts exceeds @p tolerance_ns (same
   *  step-correction semantics as pace()'s own resync, applied to tracking
   *  instead of sleeping) — this corrects accumulated epoch OFFSET only, it
   *  does not model sample-rate SKEW, exactly like pace()'s resync.
   *
   *  Rejects (no-op, returns 0) any observation with @p n_at_observation
   *  less than the clock's current @c n outright: a stale, out-of-order, or
   *  redelivered header must never walk the epoch backward. Never treat two
   *  reconciled observations as literal replay-safe state — always resync
   *  from an ARRIVING message, not a cached one.
   *
   *  @return Nonzero if this call adopted or re-anchored the epoch; 0 if it
   *  was accepted as already consistent, or rejected as stale. */
  int dp_sample_clock_track (dp_sample_clock_t *c,
                             uint64_t           observed_timestamp_ns,
                             uint64_t n_at_observation, uint64_t tolerance_ns);

  /** Re-capture both epochs and zero the counters — a fresh clock at n=0. */
  void dp_sample_clock_reset (dp_sample_clock_t *c);

  /** Re-anchor the pacing epoch to "now" without clearing ``n`` or counters,
   *  dropping any accumulated lateness so future blocks pace forward from the
   *  present. (pace() does this automatically when ``resync`` is set.) */
  void dp_sample_clock_resync (dp_sample_clock_t *c);

  /* A stats snapshot for the generated `SampleClock` handle (jm
   * kind="handle"): the decoded-getter face wants one call that fills an
   * out-struct, so this copies the (public) clock struct out. The handle
   * constructs init-in-place via dp_sample_clock_init above (jm#320
   * `init_fn`); jm owns that malloc/free. */
  void dp_sample_clock_stats (const dp_sample_clock_t *c,
                              dp_sample_clock_t       *out);

  /** Heap-allocate and initialise a clock for sample rate @p fs (Hz); see
   *  dp_sample_clock_init for @p resync. Returns NULL on allocation failure.
   *  This is the opaque-handle constructor the generated realtime composer
   *  stream drives (`Composer.stream(realtime=fs)`): it owns a `void *clk`
   *  created here and freed by dp_sample_clock_destroy. */
  dp_sample_clock_t *dp_sample_clock_create (double fs, int resync);

  /** Free a clock from dp_sample_clock_create (NULL-safe). */
  void dp_sample_clock_destroy (dp_sample_clock_t *c);

#ifdef __cplusplus
}
#endif

#endif /* DP_TIMING_CORE_H */
