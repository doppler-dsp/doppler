/*
 * wfm_resolve.c — per-segment noise resolution (Phase 4b).
 *
 * A multi-source segment models one receiver, so it has ONE shared noise
 * floor. SNR lives on a source (self-referential; snr_mode's Eb/No needs that
 * source's bits/symbol & samples/symbol). This pass turns that into an
 * explicit additive noise source so the composer's accumulator (Phase 4a)
 * stays a dumb sum:
 *
 *   - an explicit `noise` source (WFM_SYNTH_NOISE) sets the floor at its
 * `level`;
 *   - otherwise the first signal source with snr < clean is the anchor, and
 * the floor is `level(anchor) − SNR_fs(anchor)`;
 *   - every signal source is then cleaned (snr → clean, so its synth makes no
 *     bundled AWGN) and, if it carried snr but is not the anchor, its level is
 *     set `snr` dB above the floor (the RFC's "place me N dB above" sugar);
 *   - a WFM_SYNTH_NOISE source at the floor is appended when none was
 * explicit.
 *
 * It is a NO-OP for a 1-source segment, which keeps the original bundled-synth
 * path byte-identical (a bundled noisy source's private RNG cannot be split).
 * It is idempotent: a resolved segment (clean signals + explicit noise)
 * resolves to itself, so `--record` → `--from-file` round-trips.
 */
#include "doppler/wfm/wfm_compose.h"

#include "doppler/wfm_synth/wfm_synth_core.h"

#include <math.h>
#include <stdlib.h>

/* SNR (dB) over fs from snr/snr_mode/sps/type — the SEGMENT-level view, whose
 * job is to pick the auto mode and work out the symbol span. The arithmetic
 * itself is wfm_synth_snr_over_fs() in wfm_synth_core.h, shared with the
 * generator so the resolved floor reproduces the bundled noise power exactly.
 *
 * That sharing used to be a comment claiming "single source of truth — no
 * drift" over a second copy of the formula. It was true only by inspection,
 * and only for the modes both copies implemented the same way. Public
 * (declared in wfm_compose.h) so the Plan stimulus engine recomputes
 * floor(snr) at an arbitrary swept SNR through the same call. */
double
dp_wfm_snr_over_fs (int snr_mode, int type, int sps, size_t sf,
                    double sym_span, double snr)
{
  int mode = snr_mode;
  /* auto: *psk/dsss → Es/No, tone/noise/pn/chirp/bits → fs. DSSS differs from
     the generator's own auto (which says fs) on purpose: only here is the
     spreading factor known, so only here can a data-symbol Es/N0 be referred
     to fs correctly. */
  if (mode == 0)
    mode = (type == WFM_SYNTH_BPSK || type == WFM_SYNTH_QPSK
            || type == WFM_SYNTH_DSSS)
               ? 3
               : 1;
  int nsps = (sps < 1) ? 1 : sps;
  /* dsss: Es spans one DATA symbol. For a BURST that is the outer data symbol
   * — sf chips × sps samples/chip. For a CONTINUOUS async stream the symbol
   * clock is independent of the code, so the span is fs/symbol_rate samples
   * (passed as @p sym_span, non-integer), NOT sf·sps — those coincide only in
   * the synchronous case continuous mode exists to avoid. The BPSK payload
   * makes ebno == esno in both. */
  double span;
  if (type == WFM_SYNTH_DSSS)
    span = (sym_span > 0.0) ? sym_span
                            : (double)nsps * (double)(sf < 1 ? 1 : sf);
  else
    span = (double)nsps;
  return wfm_synth_snr_over_fs (mode, wfm_synth_bps (type), span, snr);
}

/* Continuous-DSSS symbol span in samples (fs/symbol_rate), or 0 for a burst /
 * non-dsss source — the value @p sym_span above wants. */
static double
dsss_sym_span (const wfm_source_t *s, double fs)
{
  return (s->type == WFM_SYNTH_DSSS && s->symbol_rate > 0.0 && fs > 0.0)
             ? fs / s->symbol_rate
             : 0.0;
}

/* Convenience wrapper over the source's own fields (the resolve-time caller).
 */
static double
snr_over_fs (const wfm_source_t *s, double fs)
{
  return dp_wfm_snr_over_fs (s->snr_mode, s->type, s->sps, s->data_code.len,
                             dsss_sym_span (s, fs), s->snr);
}

double
dp_wfm_source_create_snr (const wfm_source_t *src, double fs, double snr,
                          int *snr_mode)
{
  *snr_mode = src->snr_mode;
  /* dp_wfm_synth_create() sees only sps, so a dsss data-symbol Es/N0 must be
   * pre-referred to fs here (the codes attach after create). So must a FRAMED
   * bpsk/qpsk/pn: its synth is created as BITS (dp_wfm_source_synth_type),
   * whose auto mode is fs and whose bits/symbol is 1, so handing it the
   * source's own figure would move the noise off the reference its TYPE
   * names -- the one dp_wfm_snr_over_fs resolves for the segment floor.
   * Clean sources pass through so the no-AWGN shortcut still applies. */
  if ((src->type == WFM_SYNTH_DSSS
       || dp_wfm_source_synth_type (src) != src->type)
      && snr < WFM_SYNTH_SNR_CLEAN)
    {
      snr = dp_wfm_snr_over_fs (src->snr_mode, src->type, src->sps,
                                src->data_code.len, dsss_sym_span (src, fs),
                                snr);
      *snr_mode = 1; /* fs */
    }
  return snr;
}

/* Where a multi-source segment's shared noise floor comes from: an explicit
 * noise source sets it (noise_idx; the anchor stays -1, so every snr source
 * is sugar), else the first snr-bearing signal source anchors it. Both -1
 * means the segment has no floor to resolve. The ONE place this is decided,
 * for dp_wfm_segment_snr_and_level() and dp_wfm_resolve_noise() alike. */
static void
floor_source (const wfm_segment_t *g, int *noise_idx, int *anchor)
{
  *noise_idx = -1;
  *anchor    = -1;
  for (size_t k = 0; k < g->n_sources; k++)
    if (g->sources[k].type == WFM_SYNTH_NOISE)
      {
        *noise_idx = (int)k;
        return;
      }
  for (size_t k = 0; k < g->n_sources; k++)
    if (g->sources[k].snr < WFM_SYNTH_SNR_CLEAN)
      {
        *anchor = (int)k;
        return;
      }
}

int
dp_wfm_segment_snr_and_level (const wfm_segment_t *g)
{
  if (g->n_sources <= 1)
    return -1; /* lone source: the bundled path, nothing is placed */
  int noise_idx, anchor;
  floor_source (g, &noise_idx, &anchor);
  if (noise_idx < 0 && anchor < 0)
    return -1; /* no floor: all clean, no AWGN */
  for (size_t k = 0; k < g->n_sources; k++)
    {
      const wfm_source_t *s = &g->sources[k];
      if (s->type == WFM_SYNTH_NOISE || s->snr >= WFM_SYNTH_SNR_CLEAN
          || (int)k == anchor)
        continue;
      if (s->level != 0.0)
        return (int)k; /* a non-anchor with both snr and a level */
    }
  return -1;
}

int
dp_wfm_resolve_noise (wfm_segment_t *segs, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      wfm_segment_t *g = &segs[i];
      if (g->n_sources <= 1)
        continue; /* lone source → bundled path, byte-identical */

      /* Over-specified is the same question dp_wfm_scene_error() asks, by the
       * same function, so the two cannot disagree about which scene it is. */
      if (dp_wfm_segment_snr_and_level (g) >= 0)
        return -1;

      int noise_idx, anchor;
      floor_source (g, &noise_idx, &anchor);
      double floor_db = 0.0;
      if (noise_idx >= 0)
        floor_db = g->sources[noise_idx].level;
      else if (anchor >= 0)
        floor_db = g->sources[anchor].level
                   - snr_over_fs (&g->sources[anchor], g->fs);
      else
        continue; /* all sources clean, no explicit noise → no AWGN */

      /* clean every signal source; sugar a non-anchor snr to a level. */
      for (size_t k = 0; k < g->n_sources; k++)
        {
          wfm_source_t *s = &g->sources[k];
          if (s->type == WFM_SYNTH_NOISE || s->snr >= WFM_SYNTH_SNR_CLEAN)
            continue;
          if ((int)k != anchor)
            s->level
                = floor_db + s->snr; /* place me `snr` dB above the floor */
          s->snr = (double)
              WFM_SYNTH_SNR_CLEAN; /* its synth makes no bundled AWGN */
        }

      /* append a WFM_SYNTH_NOISE source at the floor when none was explicit.
       */
      if (noise_idx < 0)
        {
          size_t        nn = g->n_sources + 1;
          wfm_source_t *more
              = realloc (g->sources, nn * sizeof (wfm_source_t));
          if (!more)
            return -1;
          g->sources               = more;
          g->sources[g->n_sources] = (wfm_source_t){
            .type  = WFM_SYNTH_NOISE,
            .seed  = (anchor >= 0) ? g->sources[anchor].seed : 1u,
            .level = floor_db,
          };
          g->n_sources = nn;
        }
    }
  return 0;
}
