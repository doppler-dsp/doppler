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
#include "wfm/wfm_compose.h"

#include "wfm_synth/wfm_synth_core.h"

#include <math.h>
#include <stdlib.h>

/* SNR (dB) over fs from snr/snr_mode/sps/type — mirrors the conversion in
 * wfm_synth_core.c so the resolved floor reproduces the bundled noise power
 * exactly. Public (declared in wfm_compose.h) so the Plan stimulus engine
 * recomputes floor(snr) at an arbitrary swept SNR using the identical formula
 * (single source of truth — no drift). */
double
wfm_snr_over_fs (int snr_mode, int type, int sps, size_t sf, double snr)
{
  int mode = snr_mode;
  if (mode == 0) /* auto: *psk/dsss → Es/No, tone/noise/pn/chirp/bits → fs */
    mode = (type == WFM_SYNTH_BPSK || type == WFM_SYNTH_QPSK
            || type == WFM_SYNTH_DSSS)
               ? 3
               : 1;
  int nsps = (sps < 1) ? 1 : sps;
  int bps  = (type == WFM_SYNTH_QPSK) ? 2 : 1;
  /* dsss: the symbol is the outer data symbol — sf chips × sps samples/chip
   * — so Es spans sf·sps samples. The BPSK payload makes ebno == esno. */
  double span = (type == WFM_SYNTH_DSSS)
                    ? (double)nsps * (double)(sf < 1 ? 1 : sf)
                    : (double)nsps;
  if (mode == 2) /* Eb/No */
    return snr + 10.0 * log10 ((double)bps) - 10.0 * log10 (span);
  if (mode == 3) /* Es/No */
    return snr - 10.0 * log10 (span);
  return snr; /* over fs */
}

/* Convenience wrapper over the source's own fields (the resolve-time caller).
 */
static double
snr_over_fs (const wfm_source_t *s)
{
  return wfm_snr_over_fs (s->snr_mode, s->type, s->sps, s->n_data_code,
                          s->snr);
}

double
wfm_source_create_snr (const wfm_source_t *src, double snr, int *snr_mode)
{
  *snr_mode = src->snr_mode;
  /* wfm_synth_create() sees only sps, so a dsss data-symbol Es/N0 must be
   * pre-referred to fs here (the codes attach after create). Clean sources
   * pass through so the no-AWGN shortcut still applies. */
  if (src->type == WFM_SYNTH_DSSS && snr < WFM_SYNTH_SNR_CLEAN)
    {
      snr       = wfm_snr_over_fs (src->snr_mode, src->type, src->sps,
                                   src->n_data_code, snr);
      *snr_mode = 1; /* fs */
    }
  return snr;
}

int
wfm_resolve_noise (wfm_segment_t *segs, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      wfm_segment_t *g = &segs[i];
      if (g->n_sources <= 1)
        continue; /* lone source → bundled path, byte-identical */

      /* explicit noise source sets the floor directly; else the first
       * snr-bearing signal source anchors it. */
      int    noise_idx  = -1;
      int    anchor     = -1;
      int    have_floor = 0;
      double floor_db   = 0.0;
      for (size_t k = 0; k < g->n_sources; k++)
        if (g->sources[k].type == WFM_SYNTH_NOISE)
          {
            noise_idx = (int)k;
            break;
          }
      if (noise_idx >= 0)
        {
          floor_db   = g->sources[noise_idx].level;
          have_floor = 1; /* anchor stays -1: every snr source is sugar */
        }
      else
        {
          for (size_t k = 0; k < g->n_sources; k++)
            if (g->sources[k].snr < WFM_SYNTH_SNR_CLEAN)
              {
                anchor   = (int)k;
                floor_db = g->sources[k].level - snr_over_fs (&g->sources[k]);
                have_floor = 1;
                break;
              }
        }
      if (!have_floor)
        continue; /* all sources clean, no explicit noise → no AWGN */

      /* clean every signal source; sugar a non-anchor snr to a level. */
      for (size_t k = 0; k < g->n_sources; k++)
        {
          wfm_source_t *s = &g->sources[k];
          if (s->type == WFM_SYNTH_NOISE || s->snr >= WFM_SYNTH_SNR_CLEAN)
            continue;
          if ((int)k != anchor)
            {
              if (s->level != 0.0)
                return -1; /* over-specified: both snr and level on a
                              non-anchor */
              s->level
                  = floor_db + s->snr; /* place me `snr` dB above the floor */
            }
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
