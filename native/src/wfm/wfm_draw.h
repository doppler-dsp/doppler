/**
 * @file wfm_draw.h
 * @brief The composer's deterministic ranged-draw hash (internal).
 *
 * One definition of the draw so every consumer agrees to the bit: the
 * streaming composer resolves each instance's ranged fields through these,
 * and dp_wfm_compose_spans() (wfm_draw.c) replays them to report exact
 * rendered positions without rendering. Split from wfm_compose.c so the
 * SigMF sidecar (wfm_writer_core) can link the replay without dragging in
 * the whole composer + synth chain.
 */
#ifndef WFM_DRAW_H
#define WFM_DRAW_H

#include "doppler/wfm/wfm_compose.h" /* wfm_segment_t / wfm_source_t */

#include <stddef.h>
#include <stdint.h>

/** Draw a ranged field uniformly in `[lo, hi]`. The key folds the source seed,
 * the repeat epoch, the segment `repeats` instance, the segment and source
 * indices, and the field id, so every ranged field draws an independent yet
 * reproducible sequence across repeats and instances. Instance 0 contributes
 * nothing to the key — a repeats-less scene draws exactly as before.
 *
 * Stateless: the same arguments always return the same value (a splitmix64
 * hash of the key, no RNG state consumed).
 *
 * @param seed   Source seed.
 * @param epoch  Repeat epoch (scene repetition index).
 * @param inst   Segment `repeats` instance index.
 * @param seg    Segment index.
 * @param src    Source index within the segment.
 * @param field  Field id (a WFM_RANGE_* bit).
 * @param lo     Lower bound of the range.
 * @param hi     Upper bound of the range.
 * @return `lo + (hi - lo) * u` with u uniform in `[0, 1)`, so the result is
 *         in `[lo, hi)`. */
double dp_wfm_draw_range (uint32_t seed, unsigned epoch, size_t inst,
                          size_t seg, size_t src, unsigned field, double lo,
                          double hi);

/** Round a non-negative ranged draw to a sample count: dp_wfm_draw_range()
 * with source index 0, rounded half up.
 *
 * @param seed   Seed (the segment's first source's seed for timing draws).
 * @param epoch  Repeat epoch.
 * @param inst   Segment `repeats` instance index.
 * @param seg    Segment index.
 * @param field  Field id (a WFM_RANGE_* bit).
 * @param lo     Lower bound, samples.
 * @param hi     Upper bound, samples.
 * @return A sample count in `[lo, hi]`. */
size_t dp__wfm_draw_samples (uint32_t seed, unsigned epoch, size_t inst,
                             size_t seg, unsigned field, size_t lo, size_t hi);

/* ── One resolution, every consumer ──────────────────────────────────────
 *
 * The two helpers below are the ONLY place a ranged field becomes a number.
 * They exist because the SigMF sidecar used to assemble one annotation from
 * two provenances -- its timing replayed through dp_wfm_compose_spans(), its
 * frequency and SNR read straight off the source struct, which for a ranged
 * field still holds `lo`. The row was exact about WHEN and wrong about WHAT,
 * and the exact half is what stopped anyone looking at the other
 * (doppler#1086: 1224 Hz and 6.0 dB out, next to a sample-accurate start).
 *
 * So the renderer resolves through these and so does every report. A field
 * added to one is added to both by construction, rather than by a reviewer
 * noticing.
 */

/** This instance's segment timing, in samples. A fixed field passes its
 *  scalar through; a ranged one holds this instance's draw. */
typedef struct
{
  size_t on;    /**< num_samples: samples the segment emits  */
  size_t off;   /**< off_samples: trailing gap, samples      */
  size_t delay; /**< delay_samples: leading gap, samples     */
} wfm_seg_draw_t;

/** Resolve segment @p g's timing for one instance. Timing draws key off the
 *  segment's first source's seed (1 when it has no sources), so every source
 *  of an instance shares one timeline.
 *
 *  @param g      Segment description.
 *  @param epoch  Repeat epoch.
 *  @param inst   Segment `repeats` instance index.
 *  @param seg    Segment index.
 *  @param[out] out    Resolved on / off / delay sample counts. */
void dp_wfm_draw_segment (const wfm_segment_t *g, unsigned epoch, size_t inst,
                          size_t seg, wfm_seg_draw_t *out);

/** This instance's ranged SOURCE fields, for source `k` of segment `seg`.
 *  A fixed field passes the source's scalar through. */
typedef struct
{
  double freq;         /**< carrier / sweep-start frequency, Hz        */
  double f_end;        /**< chirp end frequency, Hz                    */
  double snr;          /**< SNR, dB, in the source's own snr_mode      */
  double level;        /**< source power, dBFS                         */
  double doppler;      /**< clock Doppler, ppm                         */
  double doppler_rate; /**< clock Doppler ramp, ppm/s                  */
} wfm_src_draw_t;

/** Resolve source @p src's ranged fields for one instance, keyed by the
 *  source's own seed and index @p k.
 *
 *  @param src    Source description.
 *  @param epoch  Repeat epoch.
 *  @param inst   Segment `repeats` instance index.
 *  @param seg    Segment index.
 *  @param k      Source index within the segment.
 *  @param[out] out    Resolved field values. */
void dp_wfm_draw_source (const wfm_source_t *src, unsigned epoch, size_t inst,
                         size_t seg, size_t k, wfm_src_draw_t *out);

#endif /* WFM_DRAW_H */
