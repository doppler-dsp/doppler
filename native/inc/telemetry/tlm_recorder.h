/**
 * @file tlm_recorder.h
 * @brief Lossless capture: computed sizing, ping-pong staging, flush to file.
 *
 * @section rec_why Why this exists
 * The ring drops on overrun. That is correct for the *ring* — `dp_tlm_emit`
 * runs in the DSP hot loop, so it must never block and never allocate, and a
 * bounded lock-free buffer with a drop counter is the only structure that
 * honours both. But "correct for the ring" became "the caller's problem" in
 * practice: every consumer hand-rolled a drain loop, guessed a ring size, and
 * discovered loss (if ever) from a counter nobody read.
 *
 * A capture with a hole is not a smaller capture, it is a wrong one. So the
 * recorder makes loss structurally impossible rather than merely counted, and
 * takes the sizing decision away from the caller entirely.
 *
 * @section rec_how The three pieces
 *
 * **1. The size is COMPUTED, not guessed.** A probe emits at most once per
 * event and events are at most one per input sample, so one block's worst
 * case is bounded and knowable:
 *
 *     records_per_block <= n_probes * block_samples / min_decim
 *
 * Both terms are in hand: probes are registered before the producer starts
 * (the registry is setup-time by contract), and `block_samples` is the block
 * the caller is already stepping with. The ring is sized to that ceiling, so
 * it cannot overflow *within* a block.
 *
 * **2. The drain happens at the BLOCK BOUNDARY, on the producer thread.**
 * That is the one instant the producer is quiescent by construction — between
 * `steps()` calls — so a synchronous drain there needs the ring to hold one
 * block, never a whole capture. A background thread cannot offer this: it is
 * best-effort by nature, and a producer emitting in a tight loop outruns any
 * consumer, which is how the earlier threaded draft lost records.
 *
 * **3. Staging PING-PONGS, and the boundary may wait.** `tick()` drains the
 * ring into one half of a staging pair and hands the full half to the flusher
 * thread, which writes it out while the producer fills the other. If the
 * flusher is still busy when the next swap comes, `tick()` WAITS for it.
 * Waiting at a block boundary is legal — it is not the hot loop — and it is
 * what converts "drop" into "back-pressure". The emit path is untouched and
 * still never blocks.
 *
 * @section rec_contract Contract
 *   - `tick()` runs on the PRODUCER thread, once per block, after stepping.
 *   - The recorder is the ring's only consumer; do not also call
 *     `dp_tlm_read()` on the same context while one is attached.
 *   - Probes must all be registered before `create()`, since the sizing
 *     depends on the count (this is already the registry's contract).
 *
 * @code
 *   dp_tlm_recorder_t *r =
 *       dp_tlm_recorder_create (tlm, "cap.tlm16", 256);  // block = 256
 *   for (size_t i = 0; i < n; i += 256) {
 *     dp_tlm_set_now (tlm, i);
 *     obj_steps (obj, x + i, 256);
 *     dp_tlm_recorder_tick (r);        // drain + hand off; never drops
 *   }
 *   dp_tlm_recorder_finish (r);        // final flush, close, join
 * @endcode
 */

#ifndef DP_TLM_RECORDER_H
#define DP_TLM_RECORDER_H

#include "telemetry/telemetry.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque recorder: computed ring, ping-pong staging, flusher thread. */
typedef struct dp_tlm_recorder dp_tlm_recorder_t;

/**
 * @brief Records one block can produce, at worst, for @p t.
 *
 * `n_probes * block_samples / min_decim`, where `min_decim` is the smallest
 * decimation across the registered probes (the fastest emitter sets the
 * bound). Exposed because it is the number that replaces the caller's guess —
 * a caller who wants to know what a capture will cost can ask.
 *
 * @param t             Context, with every probe already registered.
 * @param block_samples Samples per step() call.
 * @return Upper bound in records; 0 if @p t is NULL or has no probes.
 */
size_t dp_tlm_block_bound (const dp_tlm_t *t, size_t block_samples);

/**
 * @brief Creates a recorder and RESIZES the context's ring to fit one block.
 *
 * The ring is replaced with one sized from dp_tlm_block_bound(), which is
 * safe here and only here: no producer is running yet, so nothing is mid-write.
 * Whatever size the context was created with is irrelevant afterwards — that
 * is the point, the caller stops having to pick one.
 *
 * @param t             Context. Must outlive the recorder. All probes
 *                      registered.
 * @param path          Where to flush records, or NULL to keep the capture
 *                      in memory only (it still never drops; it just grows).
 * @param block_samples Samples per step() call — the term the bound needs.
 * @return New recorder, or NULL on bad arguments / allocation / open failure.
 */
dp_tlm_recorder_t *dp_tlm_recorder_create (dp_tlm_t *t, const char *path,
                                           size_t block_samples);

/**
 * @brief Drains one block and hands it to the flusher. Producer thread.
 *
 * Call once per block, after stepping. Blocks only if the flusher has not
 * finished the previous half — back-pressure at a boundary, never in the
 * emit path.
 *
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL / a write error.
 */
int dp_tlm_recorder_tick (dp_tlm_recorder_t *r);

/**
 * @brief Final drain, flush and close. Joins the flusher.
 *
 * Sweeps anything the last tick left, writes it, and closes the file. After
 * this the capture on disk is complete.
 *
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL / a write error.
 */
int dp_tlm_recorder_finish (dp_tlm_recorder_t *r);

/** @brief Records captured so far (flushed + staged). */
uint64_t dp_tlm_recorder_count (const dp_tlm_recorder_t *r);

/**
 * @brief Records lost. Zero by construction — non-zero is a BUG here.
 *
 * Kept as an assertion surface rather than an expected outcome: the sizing
 * and the boundary back-pressure between them mean a drop cannot happen in
 * the supported usage, so a caller seeing one has found a defect, not a
 * tuning problem.
 */
uint64_t dp_tlm_recorder_dropped (const dp_tlm_recorder_t *r);

/** @brief Ring capacity the recorder computed, in records. */
size_t dp_tlm_recorder_ring_records (const dp_tlm_recorder_t *r);

/** @brief Finishes if needed, then frees. NULL-safe. */
void dp_tlm_recorder_destroy (dp_tlm_recorder_t *r);

#ifdef __cplusplus
}
#endif

#endif /* DP_TLM_RECORDER_H */
