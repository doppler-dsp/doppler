/*
 * wfm_draw.c — deterministic ranged draws + the span replayer.
 *
 * The draw never consumes RNG state: it hashes (seed, epoch, instance,
 * segment, source, field) afresh each time (splitmix64), so a ranged scene
 * replays byte-for-byte from --from-file and wfm_compose_spans() can report
 * the exact rendered instance timeline without rendering anything.
 */
#include "wfm_draw.h"

#include "wfm/wfm_compose.h"

/* Deterministic uniform double in [0,1) from a 64-bit key (splitmix64). */
static double
draw_u01 (uint64_t key)
{
  key += 0x9E3779B97F4A7C15ull;
  key = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ull;
  key = (key ^ (key >> 27)) * 0x94D049BB133111EBull;
  key ^= key >> 31;
  return (double)(key >> 11) * (1.0 / 9007199254740992.0); /* key/2^53 */
}

double
wfm_draw_range (uint32_t seed, unsigned epoch, size_t inst, size_t seg,
                size_t src, unsigned field, double lo, double hi)
{
  uint64_t key = (uint64_t)seed * 0xD1B54A32D192ED03ull
                 ^ ((uint64_t)epoch << 32) ^ ((uint64_t)inst << 48)
                 ^ ((uint64_t)seg << 40) ^ ((uint64_t)src << 16)
                 ^ ((uint64_t)field << 8);
  return lo + (hi - lo) * draw_u01 (key);
}

size_t
wfm_draw_samples (uint32_t seed, unsigned epoch, size_t inst, size_t seg,
                  unsigned field, size_t lo, size_t hi)
{
  double v = wfm_draw_range (seed, epoch, inst, seg, 0, field, (double)lo,
                             (double)hi);
  return (size_t)(v + 0.5);
}

size_t
wfm_compose_spans (const wfm_segment_t *segs, size_t n_segs, wfm_span_t *out,
                   size_t cap)
{
  size_t total = 0, pos = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g     = &segs[i];
      size_t               reps  = g->repeats ? g->repeats : 1;
      uint32_t             dseed = g->n_sources ? g->sources[0].seed : 1u;
      for (size_t inst = 0; inst < reps; inst++)
        {
          /* Identical draw keys to the streaming composer (epoch 0): the
           * replayed spans are the rendered spans, sample for sample. */
          size_t on = (g->ranged & WFM_RANGE_NUM_SAMPLES)
                          ? wfm_draw_samples (dseed, 0, inst, i,
                                              WFM_RANGE_NUM_SAMPLES,
                                              g->num_samples,
                                              g->num_samples_hi)
                          : g->num_samples;
          size_t off = (g->ranged & WFM_RANGE_OFF_SAMPLES)
                           ? wfm_draw_samples (dseed, 0, inst, i,
                                               WFM_RANGE_OFF_SAMPLES,
                                               g->off_samples,
                                               g->off_samples_hi)
                           : g->off_samples;
          size_t dly = (g->ranged & WFM_RANGE_DELAY_SAMPLES)
                           ? wfm_draw_samples (dseed, 0, inst, i,
                                               WFM_RANGE_DELAY_SAMPLES,
                                               g->delay_samples,
                                               g->delay_samples_hi)
                           : g->delay_samples;
          if (out && total < cap)
            out[total] = (wfm_span_t){ .seg      = i,
                                       .instance = inst,
                                       .start    = pos,
                                       .delay    = dly,
                                       .on       = on,
                                       .off      = off };
          total++;
          pos += dly + on + off;
        }
    }
  return total;
}
