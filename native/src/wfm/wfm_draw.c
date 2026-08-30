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

void
wfm_draw_segment (const wfm_segment_t *g, unsigned epoch, size_t inst,
                  size_t seg, wfm_seg_draw_t *out)
{
  /* The timing draws key off the FIRST source's seed, so every source of an
     instance shares one timeline -- they are one emission, not several. */
  const uint32_t dseed = g->n_sources ? g->sources[0].seed : 1u;
  out->on
      = (g->ranged & WFM_RANGE_NUM_SAMPLES)
            ? wfm_draw_samples (dseed, epoch, inst, seg, WFM_RANGE_NUM_SAMPLES,
                                g->num_samples, g->num_samples_hi)
            : g->num_samples;
  out->off
      = (g->ranged & WFM_RANGE_OFF_SAMPLES)
            ? wfm_draw_samples (dseed, epoch, inst, seg, WFM_RANGE_OFF_SAMPLES,
                                g->off_samples, g->off_samples_hi)
            : g->off_samples;
  out->delay = (g->ranged & WFM_RANGE_DELAY_SAMPLES)
                   ? wfm_draw_samples (dseed, epoch, inst, seg,
                                       WFM_RANGE_DELAY_SAMPLES,
                                       g->delay_samples, g->delay_samples_hi)
                   : g->delay_samples;
}

void
wfm_draw_source (const wfm_source_t *src, unsigned epoch, size_t inst,
                 size_t seg, size_t k, wfm_src_draw_t *out)
{
  out->freq = (src->ranged & WFM_RANGE_FREQ)
                  ? wfm_draw_range (src->seed, epoch, inst, seg, k,
                                    WFM_RANGE_FREQ, src->freq, src->freq_hi)
                  : src->freq;
  out->snr  = (src->ranged & WFM_RANGE_SNR)
                  ? wfm_draw_range (src->seed, epoch, inst, seg, k,
                                    WFM_RANGE_SNR, src->snr, src->snr_hi)
                  : src->snr;
  out->level
      = (src->ranged & WFM_RANGE_LEVEL)
            ? wfm_draw_range (src->seed, epoch, inst, seg, k, WFM_RANGE_LEVEL,
                              src->level, src->level_hi)
            : src->level;
  out->f_end = (src->ranged & WFM_RANGE_FEND)
                   ? wfm_draw_range (src->seed, epoch, inst, seg, k,
                                     WFM_RANGE_FEND, src->f_end, src->f_end_hi)
                   : src->f_end;
}

size_t
wfm_compose_spans (const wfm_segment_t *segs, size_t n_segs, wfm_span_t *out,
                   size_t cap)
{
  size_t total = 0, pos = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g    = &segs[i];
      size_t               reps = g->repeats ? g->repeats : 1;
      for (size_t inst = 0; inst < reps; inst++)
        {
          /* Identical draw keys to the streaming composer (epoch 0): the
           * replayed spans are the rendered spans, sample for sample. */
          wfm_seg_draw_t d;
          wfm_draw_segment (g, 0, inst, i, &d);
          if (out && total < cap)
            out[total] = (wfm_span_t){ .seg      = i,
                                       .instance = inst,
                                       .start    = pos,
                                       .delay    = d.delay,
                                       .on       = d.on,
                                       .off      = d.off };
          total++;
          pos += d.delay + d.on + d.off;
        }
    }
  return total;
}

size_t
wfm_compose_draws (const wfm_segment_t *segs, size_t n_segs, wfm_draw_t *out,
                   size_t cap)
{
  size_t total = 0, pos = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g    = &segs[i];
      size_t               reps = g->repeats ? g->repeats : 1;
      for (size_t inst = 0; inst < reps; inst++)
        {
          wfm_seg_draw_t d;
          wfm_draw_segment (g, 0, inst, i, &d);
          /* One row per SOURCE: an annotation describes a source, and the
             sources of one instance share its timeline. */
          for (size_t k = 0; k < g->n_sources; k++)
            {
              wfm_src_draw_t v;
              wfm_draw_source (&g->sources[k], 0, inst, i, k, &v);
              if (out && total < cap)
                out[total] = (wfm_draw_t){ .seg      = i,
                                           .instance = inst,
                                           .src      = k,
                                           .start    = pos,
                                           .delay    = d.delay,
                                           .on       = d.on,
                                           .off      = d.off,
                                           .freq     = v.freq,
                                           .f_end    = v.f_end,
                                           .snr      = v.snr,
                                           .level    = v.level };
              total++;
            }
          pos += d.delay + d.on + d.off;
        }
    }
  return total;
}
