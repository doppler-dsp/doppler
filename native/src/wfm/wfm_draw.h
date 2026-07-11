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

#include <stddef.h>
#include <stdint.h>

/* Draw a ranged field uniformly in [lo, hi]. The key folds the source seed,
 * the repeat epoch, the segment `repeats` instance, the segment and source
 * indices, and the field id, so every ranged field draws an independent yet
 * reproducible sequence across repeats and instances. Instance 0 contributes
 * nothing to the key — a repeats-less scene draws exactly as before. */
double wfm_draw_range(uint32_t seed, unsigned epoch, size_t inst, size_t seg,
                      size_t src, unsigned field, double lo, double hi);

/* Round a non-negative ranged draw to a sample count. */
size_t wfm_draw_samples(uint32_t seed, unsigned epoch, size_t inst,
                        size_t seg, unsigned field, size_t lo, size_t hi);

#endif /* WFM_DRAW_H */
