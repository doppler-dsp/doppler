/**
 * @file dp_tlm_capture_core.h
 * @brief Lossless telemetry capture: sized by arithmetic, not by guesswork.
 *
 * The ring (dp_tlm/dp_tlm_core.h) drops on overrun so the DSP thread can
 * never stall — right for the emit path, useless as an answer to "did I get
 * everything?".  Every capture so far hand-rolled the same loop (`set_now`,
 * step, `read`, append, concatenate) and asked the user to pick a ring size,
 * which is a question nobody can answer: too small silently loses data, too
 * big wastes memory, and neither shows up until after the run.
 *
 * A capture retires the question.  It rests on one bound
 * (dp_tlm_block_bound()): **no probe emits more than once per input sample**,
 * so a block of @c N inputs emits at most `probe_count * N` records.  Size the
 * ring to that and drain it to empty at every block boundary, and the ring
 * *cannot* overflow — the producer never gets more than one block ahead of the
 * consumer, by construction.
 *
 * That is a proof, not a heuristic.  There is no polling interval to tune, no
 * scheduling assumption, no safety factor, and no background drain racing the
 * producer: the drain runs on the caller's thread, at the boundary, where the
 * producer is by definition quiescent.  It is also the *fastest* arrangement
 * available — one `memcpy` per block and nothing added to the emit path.
 *
 * @section cap_two_problems Two problems, kept separate
 * Losslessness is bought by the sizing above.  **Flat memory** on a long run
 * is a different problem, bought by handing drained blocks to a file: the
 * capture ping-pongs two staging buffers so a writer thread can be draining
 * one while the producer fills the other.  If the writer falls behind, the
 * *boundary* blocks.  That is backpressure — the capture waits, the data
 * survives.  Nothing is ever dropped to keep up.
 *
 * @section cap_boundary Where the boundary comes from
 * dp_tlm_set_now() delegates here whenever a capture is open.  Callers already
 * put it at the top of the block loop, before stepping, so an existing
 * `set_now / steps / read` loop becomes lossless by opening a capture and
 * changing nothing else.  Call dp_tlm_capture_block() directly if you stamp
 * the sample index some other way.
 *
 * @section cap_format On disk
 * The 16-byte ::dp_tlm_rec_t layout *is* the file — no framing, no header, no
 * version.  `np.fromfile(path, dtype=REC_DTYPE)` reads it, and so does a plain
 * `fread`.  A `<path>-meta` JSON sidecar carries what the records cannot: the
 * probe table, the counters, and the sample clock.
 *
 * @section cap_time The time base is borrowed, never re-declared
 * A record carries @c n and no time; time is `t0 + n / fs`.  That pair, and
 * that computation, are already ::dp_sample_clock_t — so a capture takes the
 * clock **by reference** rather than growing a private `fs`/`t0` of its own.
 * Two copies of a time base drift, and the one in the file is the copy nobody
 * can correct afterwards.  Passing NULL states "no time base", and the sidecar
 * then omits the keys rather than fabricating a plausible rate.
 *
 * @code
 *   rx_set_telemetry (rx, tlm, "rx", 1);   // probes first: they
 *                                          // set the bound
 *   dp_sample_clock_t clk;
 *   dp_sample_clock_init (&clk, 1e6, 1);
 *   dp_tlm_capture_t *cap =
 *     dp_tlm_capture_open (tlm, 256, "rx.tlm", &clk);
 *   for (size_t i = 0; i < n; i += 256)
 *     {
 *       dp_tlm_set_now (tlm, i);           // drains the block
 *                                          // just finished
 *       rx_steps (rx, x + i, 256, y);
 *     }
 *   int rc = dp_tlm_capture_close (cap);   // DP_OK == nothing lost
 *   dp_tlm_capture_destroy (cap);
 * @endcode
 */

#ifndef DP_TLM_CAPTURE_H
#define DP_TLM_CAPTURE_H

#include "dp_tlm/dp_tlm_core.h"
#include "timing/timing_core.h" /* dp_sample_clock_t — the ONE time base */

#ifdef __cplusplus
extern "C" {
#endif

/* dp_tlm_capture_t and dp_tlm_capture_block() are declared in telemetry.h so
 * the inline dp_tlm_set_now() can delegate; the dependency runs one way. */

/**
 * @brief Opens a lossless capture over @p t and arms the boundary drain.
 *
 * Sizes the ring to `dp_tlm_block_bound (t, block_samples)`, so **attach every
 * probe first** — an object's `*_set_telemetry` is what registers them.  A
 * probe registered later is not a disaster: dp_tlm_capture_block() re-checks
 * the bound and grows before the larger block can cost a record.
 *
 * At most one capture per context; opening a second fails.
 *
 * @param t             Context to capture.  Must outlive the capture.
 * @param block_samples The LARGEST number of input samples processed between
 *                      two boundaries.  Not a buffer size to tune — the step
 *                      of the caller's own block loop.  Over-stating it costs
 *                      only memory; under-stating it is the one way to lose a
 *                      record, and dp_tlm_capture_close() reports it.
 * @param path          Output file.  NULL accumulates in memory instead, for
 *                      dp_tlm_capture_records().  Truncated if it exists.
 * @param clock         The pipeline's sample clock, borrowed for the sidecar's
 *                      time base.  Read at close(), so later `track()`
 *                      corrections to the epoch are picked up.  Must outlive
 *                      the capture.  NULL = no time base stated, and the
 *                      sidecar says so by omission.
 * @return New capture, or NULL on a NULL/zero @p t / @p block_samples, a
 *         context that already has a capture, an unopenable @p path, or
 *         allocation failure.
 */
dp_tlm_capture_t *dp_tlm_capture_open (dp_tlm_t *t, size_t block_samples,
                                       const char             *path,
                                       const dp_sample_clock_t *clock);

/**
 * @brief Block boundary: drains the ring to empty.
 *
 * Grows the ring first if probes appeared since the last boundary, which is
 * safe precisely here — the ring is about to be emptied and the producer is
 * between blocks.  Then copies everything available into the active staging
 * buffer, handing it to the sink and swapping when it can no longer hold
 * another block.
 *
 * **May block** in file mode, if the writer still holds the other buffer.
 * That wait is the backpressure that keeps the capture lossless; it happens
 * at the boundary, never inside the DSP loop.
 *
 * Usually reached through dp_tlm_set_now() rather than called directly.
 *
 * @return ::DP_OK, ::DP_ERR_INVALID on NULL / a closed capture, ::DP_ERR_MEMORY
 *         if a buffer could not grow, or ::DP_ERR_SEND if the writer failed.
 */
int dp_tlm_capture_block (dp_tlm_capture_t *c);

/**
 * @brief Final boundary, then flush, join, and write the sidecar.
 *
 * Sweeps the tail the last block left behind, drains the staging buffers,
 * joins the writer thread, closes the file and writes `<path>-meta`.
 * Idempotent: a second call is a no-op returning the first call's verdict.
 *
 * @return ::DP_OK when the capture is provably complete.  **::DP_ERR_INVALID
 *         when records were dropped** — the invariant makes that impossible,
 *         so a non-zero count means the contract was broken (a step longer
 *         than @c block_samples, or no boundary at all) and the capture has a
 *         hole in it.  A capture with a hole is not a smaller capture, it is a
 *         wrong one, so this fails loudly rather than returning quietly.
 *         ::DP_ERR_SEND on a write failure.
 */
int dp_tlm_capture_close (dp_tlm_capture_t *c);

/** @brief Records captured so far, across memory and file alike. */
size_t dp_tlm_capture_count (const dp_tlm_capture_t *c);

/**
 * @brief The accumulated records, contiguous and in emission order.
 *
 * Memory mode only (@c path was NULL) — in file mode the file *is* the
 * capture and this returns NULL.  Owned by the capture and invalidated by
 * dp_tlm_capture_destroy(); NULL when nothing was captured, so use
 * dp_tlm_capture_count() to tell empty from absent.
 */
const dp_tlm_rec_t *dp_tlm_capture_records (const dp_tlm_capture_t *c);

/**
 * @brief Records the ring dropped during this capture.
 *
 * Latched against the context's monotonic counter at open, so it reports this
 * capture rather than the context's lifetime.  Non-zero means a hole; see
 * dp_tlm_capture_close().
 */
uint64_t dp_tlm_capture_dropped (const dp_tlm_capture_t *c);

/** @brief Closes if still open, then frees.  NULL-safe. */
void dp_tlm_capture_destroy (dp_tlm_capture_t *c);

#ifdef __cplusplus
}
#endif

#endif /* DP_TLM_CAPTURE_H */
