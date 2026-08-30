/*
 * wfm_draw.h — the composer's deterministic ranged-draw hash (internal).
 *
 * One definition of the draw so every consumer agrees to the bit: the
 * streaming composer resolves each instance's ranged fields through these,
 * and wfm_compose_spans() (wfm_draw.c) replays them to report exact
 * rendered positions without rendering. Split from wfm_compose.c so the
 * SigMF sidecar (wfm_writer_core) can link the replay without dragging in
 * the whole composer + synth chain.
 */
#ifndef WFM_DRAW_H
#define WFM_DRAW_H

#include "wfm/wfm_compose.h" /* wfm_segment_t / wfm_source_t */

#include <stddef.h>
#include <stdint.h>

/* Draw a ranged field uniformly in [lo, hi]. The key folds the source seed,
 * the repeat epoch, the segment `repeats` instance, the segment and source
 * indices, and the field id, so every ranged field draws an independent yet
 * reproducible sequence across repeats and instances. Instance 0 contributes
 * nothing to the key — a repeats-less scene draws exactly as before. */
double wfm_draw_range (uint32_t seed, unsigned epoch, size_t inst, size_t seg,
                       size_t src, unsigned field, double lo, double hi);

/* Round a non-negative ranged draw to a sample count. */
size_t wfm_draw_samples (uint32_t seed, unsigned epoch, size_t inst,
                         size_t seg, unsigned field, size_t lo, size_t hi);

/* ── One resolution, every consumer ──────────────────────────────────────
 *
 * The two helpers below are the ONLY place a ranged field becomes a number.
 * They exist because the SigMF sidecar used to assemble one annotation from
 * two provenances -- its timing replayed through wfm_compose_spans(), its
 * frequency and SNR read straight off the source struct, which for a ranged
 * field still holds `lo`. The row was exact about WHEN and wrong about WHAT,
 * and the exact half is what stopped anyone looking at the other
 * (doppler#1086: 1224 Hz and 6.0 dB out, next to a sample-accurate start).
 *
 * So the renderer resolves through these and so does every report. A field
 * added to one is added to both by construction, rather than by a reviewer
 * noticing.
 */

/* This instance's segment timing. A fixed field passes its scalar through. */
typedef struct
{
  size_t on;    /* num_samples  */
  size_t off;   /* trailing gap */
  size_t delay; /* leading gap  */
} wfm_seg_draw_t;

void wfm_draw_segment (const wfm_segment_t *g, unsigned epoch, size_t inst,
                       size_t seg, wfm_seg_draw_t *out);

/* This instance's ranged SOURCE fields, for source `k` of segment `seg`. */
typedef struct
{
  double freq;
  double f_end;
  double snr;
  double level;
  double doppler;      /* ppm    */
  double doppler_rate; /* ppm/s  */
} wfm_src_draw_t;

void wfm_draw_source (const wfm_source_t *src, unsigned epoch, size_t inst,
                      size_t seg, size_t k, wfm_src_draw_t *out);

#endif /* WFM_DRAW_H */
