/*
 * wfm_compose.c — multi-segment waveform composer (Phase B + Phase 4a).
 *
 * A small state machine over a copied segment list. At any time the composer
 * is in one segment, in either its ON phase (summing its sources) or its OFF
 * phase (emitting zeros). When OFF drains it advances to the next segment;
 * past the last segment it loops (repeat/continuous) or finishes.
 *
 * A segment holds one or more sources summed at the same time. The 1-source
 * case is the original single-synth path, kept VERBATIM so its output stays
 * byte-identical (a bundled noisy synth owns a private RNG stream that cannot
 * be reproduced by summing a separate noise source). The N-source case renders
 * each source into a scratch buffer and accumulates with a single fixed-order
 * scale-then-add, so every face (CLI / Python) agrees bit-for-bit.
 */
#include "wfm/wfm_compose.h"
#include "wfm/wfm_dsp.h" /* wfm_rrc_taps / wfm_rrc_ntaps for pulse shaping */

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Free a segment's per-source bits patterns and then the sources array. */
static void
free_segment_sources (wfm_segment_t *seg)
{
  if (seg->sources)
    for (size_t k = 0; k < seg->n_sources; k++)
      free (seg->sources[k].bits);
  free (seg->sources);
  seg->sources = NULL;
}

enum
{
  PHASE_ON,
  PHASE_OFF,
  PHASE_DONE
};

/* N-source accumulate renders one source at a time into a fixed-size scratch
 * and adds it in. wfm_synth_steps() is chunk-invariant, so capping the
 * per-call chunk here does not change the output — and it keeps scratch a
 * fixed allocation regardless of the caller's `max` (the binding can pass
 * millions).
 */
#define SCRATCH_CAP 4096

struct wfm_compose_state
{
  wfm_segment_t *segs;
  size_t         n_segs;
  int            repeat;
  int            continuous;
  unsigned       epoch;      /* repeat counter — bumps each source's seed so
                                a looped stream is a fresh noise/PN
                                realization, not byte-identical repeats */
  size_t cur;                /* current segment index */
  int    phase;              /* PHASE_ON / PHASE_OFF / PHASE_DONE */
  size_t left;               /* samples remaining in the current phase */
  size_t cur_num;            /* this epoch's resolved on-time (ranged/fixed) */
  size_t cur_off;            /* this epoch's resolved off-time gap */
  wfm_synth_state_t **syn;   /* active segment's synths (one per source) */
  float              *gain;  /* parallel: 10^(level/20) per source */
  size_t              n_syn; /* live synth count while ON (0 otherwise) */
  size_t              syn_cap; /* capacity of syn/gain = max n_sources */
  float complex      *scratch; /* SCRATCH_CAP render buffer for N-source sum */
};

/* Destroy the active segment's synths (the syn[] array stays allocated). */
static void
stop_synths (wfm_compose_state_t *s)
{
  for (size_t k = 0; k < s->n_syn; k++)
    if (s->syn[k])
      {
        wfm_synth_destroy (s->syn[k]);
        s->syn[k] = NULL;
      }
  s->n_syn = 0;
}

/* Start segment `cur` in its ON phase, creating a synth per source. On any
 * synth failure the segment is skipped to OFF (a silent gap) so one bad
 * segment can't wedge the stream. */
/* Deterministic uniform double in [0,1) from a 64-bit key (splitmix64). The
 * same key always yields the same value, so a ranged scene replays
 * byte-for-byte from --from-file: the draw never consumes RNG state, it hashes
 * (seed, epoch, segment, source, field) afresh each time. */
static double
draw_u01 (uint64_t key)
{
  key += 0x9E3779B97F4A7C15ull;
  key = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ull;
  key = (key ^ (key >> 27)) * 0x94D049BB133111EBull;
  key ^= key >> 31;
  return (double)(key >> 11) * (1.0 / 9007199254740992.0); /* key/2^53 */
}

/* Draw a ranged field uniformly in [lo, hi]. The key folds the source seed,
 * the repeat epoch, the segment and source indices, and the field id, so every
 * ranged field draws an independent yet reproducible sequence across repeats.
 */
static double
draw_range (uint32_t seed, unsigned epoch, size_t seg, size_t src,
            unsigned field, double lo, double hi)
{
  uint64_t key = (uint64_t)seed * 0xD1B54A32D192ED03ull
                 ^ ((uint64_t)epoch << 32) ^ ((uint64_t)seg << 40)
                 ^ ((uint64_t)src << 16) ^ ((uint64_t)field << 8);
  return lo + (hi - lo) * draw_u01 (key);
}

/* Round a non-negative ranged draw to a sample count. */
static size_t
draw_samples (uint32_t seed, unsigned epoch, size_t seg, unsigned field,
              size_t lo, size_t hi)
{
  double v = draw_range (seed, epoch, seg, 0, field, (double)lo, (double)hi);
  return (size_t)(v + 0.5);
}

static void
start_segment (wfm_compose_state_t *s)
{
  const wfm_segment_t *g = &s->segs[s->cur];
  /* Resolve this epoch's (possibly ranged) durations once, up front: ON uses
   * cur_num, the trailing gap uses cur_off. A fixed segment (ranged == 0) just
   * copies the scalars, so a non-ranged scene is byte-identical to before. */
  uint32_t dseed = g->n_sources ? g->sources[0].seed : 1u;
  s->cur_num
      = (g->ranged & WFM_RANGE_NUM_SAMPLES)
            ? draw_samples (dseed, s->epoch, s->cur, WFM_RANGE_NUM_SAMPLES,
                            g->num_samples, g->num_samples_hi)
            : g->num_samples;
  s->cur_off
      = (g->ranged & WFM_RANGE_OFF_SAMPLES)
            ? draw_samples (dseed, s->epoch, s->cur, WFM_RANGE_OFF_SAMPLES,
                            g->off_samples, g->off_samples_hi)
            : g->off_samples;
  int ok   = (s->cur_num > 0 && g->n_sources > 0);
  s->n_syn = 0;
  for (size_t k = 0; k < g->n_sources && ok; k++)
    {
      const wfm_source_t *src = &g->sources[k];
      /* Draw this epoch's ranged source fields (freq/snr/level/f_end); a fixed
       * field passes its scalar through unchanged. */
      double freq = (src->ranged & WFM_RANGE_FREQ)
                        ? draw_range (src->seed, s->epoch, s->cur, k,
                                      WFM_RANGE_FREQ, src->freq, src->freq_hi)
                        : src->freq;
      double snr  = (src->ranged & WFM_RANGE_SNR)
                        ? draw_range (src->seed, s->epoch, s->cur, k,
                                      WFM_RANGE_SNR, src->snr, src->snr_hi)
                        : src->snr;
      double level
          = (src->ranged & WFM_RANGE_LEVEL)
                ? draw_range (src->seed, s->epoch, s->cur, k, WFM_RANGE_LEVEL,
                              src->level, src->level_hi)
                : src->level;
      double f_end
          = (src->ranged & WFM_RANGE_FEND)
                ? draw_range (src->seed, s->epoch, s->cur, k, WFM_RANGE_FEND,
                              src->f_end, src->f_end_hi)
                : src->f_end;
      s->gain[k] = (float)pow (10.0, level / 20.0); /* level → gain */
      /* Seed advances once per repeat (epoch), so a looped/continuous stream
       * yields a fresh realization each pass instead of identical bytes. For a
       * `bits` source (explicit pattern) this varies only the AWGN; for
       * pn/bpsk/qpsk it also advances the PN data sequence. */
      uint32_t seed = (uint32_t)(src->seed + s->epoch);
      s->syn[k] = wfm_synth_create (src->type, g->fs, freq, snr, src->snr_mode,
                                    seed, src->sps, src->pn_length,
                                    src->pn_poly, src->lfsr, f_end);
      if (!s->syn[k])
        ok = 0;
      else
        {
          /* Pin a chirp's sweep to the segment's on-time so it sweeps
           * f_start→f_end over exactly num_samples (no-op for non-chirp). */
          wfm_synth_set_chirp_span (s->syn[k], s->cur_num);
          /* Attach a bits source's pattern (no-op for other types). A NULL
           * pattern leaves the bits synth emitting 0, never crashing. */
          if (src->type == WFM_SYNTH_BITS && src->bits)
            wfm_synth_set_bits (s->syn[k], src->bits, src->n_bits,
                                src->modulation);
          /* RRC pulse shaping: compute the taps here (same wfm_rrc_taps() the
           * Python/standalone face uses) and attach them. set_rrc scales for
           * unit TX power and no-ops for non-modulated types, so the composer
           * and standalone faces are byte-identical. */
          if (src->pulse && src->sps > 0 && src->rrc_span > 0)
            {
              size_t nt   = wfm_rrc_ntaps (src->sps, src->rrc_span);
              float *taps = malloc (nt * sizeof (float));
              if (taps)
                {
                  wfm_rrc_taps (src->rrc_beta, src->sps, src->rrc_span, taps);
                  wfm_synth_set_rrc (s->syn[k], taps, nt);
                  free (taps);
                }
            }
          s->n_syn = k + 1; /* track for stop_synths on partial failure */
        }
    }
  if (ok)
    {
      s->phase = PHASE_ON;
      s->left  = s->cur_num;
    }
  else
    {
      stop_synths (s);
      s->phase = PHASE_OFF;
      s->left  = s->cur_off;
    }
}

/* Move to the next segment, looping or finishing at the end. */
static void
advance (wfm_compose_state_t *s)
{
  s->cur++;
  if (s->cur >= s->n_segs)
    {
      if (s->repeat || s->continuous)
        {
          s->cur = 0;
          s->epoch++; /* next pass: advance every source's seed */
        }
      else
        {
          s->phase = PHASE_DONE;
          return;
        }
    }
  start_segment (s);
}

wfm_compose_state_t *
wfm_compose_create (const wfm_segment_t *segs, size_t n_segs, int repeat,
                    int continuous)
{
  if (!segs || n_segs == 0)
    return NULL;
  wfm_compose_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->segs = calloc (n_segs, sizeof (*s->segs));
  if (!s->segs)
    {
      free (s);
      return NULL;
    }
  /* Deep-copy each segment's source list, including any bits pattern (so the
   * composer owns its own copy and the caller keeps theirs). */
  for (size_t i = 0; i < n_segs; i++)
    {
      s->segs[i] = segs[i]; /* scalar fields (the sources ptr is replaced) */
      size_t ns  = segs[i].n_sources;
      s->segs[i].sources = calloc (ns ? ns : 1, sizeof (wfm_source_t));
      if (!s->segs[i].sources)
        {
          for (size_t j = 0; j < i; j++)
            free_segment_sources (&s->segs[j]);
          free (s->segs);
          free (s);
          return NULL;
        }
      for (size_t k = 0; k < ns; k++)
        {
          s->segs[i].sources[k] = segs[i].sources[k]; /* scalars + bits ptr */
          const wfm_source_t *src = &segs[i].sources[k];
          if (src->bits && src->n_bits)
            {
              uint8_t *copy = malloc (src->n_bits);
              if (!copy)
                {
                  s->segs[i].sources[k].bits = NULL; /* don't free caller's */
                  for (size_t j = 0; j <= i; j++)
                    free_segment_sources (&s->segs[j]);
                  free (s->segs);
                  free (s);
                  return NULL;
                }
              memcpy (copy, src->bits, src->n_bits);
              s->segs[i].sources[k].bits = copy;
            }
        }
    }
  /* Resolve the per-segment noise model on the copy (may append a noise
   * source) — runs here so every face resolves identically. No-op at 1 src. */
  if (wfm_resolve_noise (s->segs, n_segs) != 0)
    {
      for (size_t i = 0; i < n_segs; i++)
        free_segment_sources (&s->segs[i]);
      free (s->segs);
      free (s);
      return NULL;
    }
  /* Widest (post-resolve) source list sizes the syn/gain arrays. */
  size_t max_src = 1;
  for (size_t i = 0; i < n_segs; i++)
    if (s->segs[i].n_sources > max_src)
      max_src = s->segs[i].n_sources;
  s->syn     = calloc (max_src, sizeof (*s->syn));
  s->gain    = malloc (max_src * sizeof (*s->gain));
  s->scratch = malloc (SCRATCH_CAP * sizeof (*s->scratch));
  if (!s->syn || !s->gain || !s->scratch)
    {
      for (size_t i = 0; i < n_segs; i++)
        free_segment_sources (&s->segs[i]);
      free (s->segs);
      free (s->syn);
      free (s->gain);
      free (s->scratch);
      free (s);
      return NULL;
    }
  s->syn_cap    = max_src;
  s->n_segs     = n_segs;
  s->repeat     = repeat;
  s->continuous = continuous != 0;
  s->cur        = 0;
  start_segment (s);
  return s;
}

size_t
wfm_compose_execute (wfm_compose_state_t *state, float complex *out,
                     size_t max)
{
  size_t i = 0;
  while (i < max)
    {
      if (state->phase == PHASE_DONE)
        break;
      if (state->phase == PHASE_ON)
        {
          if (state->left == 0)
            {
              /* ON drained → trailing off-time gap, then advance. */
              stop_synths (state);
              state->phase = PHASE_OFF;
              state->left  = state->cur_off;
              continue;
            }
          size_t k = max - i;
          if (k > state->left)
            k = state->left;

          if (state->n_syn == 1)
            {
              /* ── 1 source: the original single-synth path, VERBATIM ──
               * Pull the ON run as a block through the *same*
               * wfm_synth_steps() the wavegen CLI uses, so composer and CLI
               * are byte-identical by construction. (Under -ffast-math a
               * per-sample wfm_synth_step() loop contracts `sym*carrier +
               * noise` to an FMA on arm64 while the block path rounds
               * separately — QPSK's ±1/√2 leg exposed that as #67;
               * wfm_synth_steps() is chunk-invariant, so block size is free.)
               * The Phase-3 level gain is a post-multiply here (no-op at 0
               * dB). */
              wfm_synth_steps (state->syn[0], out + i, k);
              if (state->gain[0] != 1.0f)
                for (size_t j = 0; j < k; j++)
                  out[i + j] *= state->gain[0];
            }
          else
            {
              /* ── N sources: one fixed-order scale-then-add into scratch ──
               * Source 0 initialises out; 1..n-1 add. Same source order, same
               * sample order on every face (one routine, one shared object).
               */
              if (k > (size_t)SCRATCH_CAP)
                k = SCRATCH_CAP;
              wfm_synth_steps (state->syn[0], state->scratch, k);
              float g0 = state->gain[0];
              for (size_t j = 0; j < k; j++)
                out[i + j] = g0 * state->scratch[j];
              for (size_t sx = 1; sx < state->n_syn; sx++)
                {
                  wfm_synth_steps (state->syn[sx], state->scratch, k);
                  float gs = state->gain[sx];
                  for (size_t j = 0; j < k; j++)
                    out[i + j] += gs * state->scratch[j];
                }
            }
          i += k;
          state->left -= k;
        }
      else
        { /* PHASE_OFF */
          if (state->left == 0)
            {
              advance (state);
              continue;
            }
          out[i++] = 0.0f + 0.0f * I;
          state->left--;
        }
    }
  return i;
}

const wfm_segment_t *
wfm_compose_segments (const wfm_compose_state_t *state, size_t *n_out,
                      int *repeat, int *continuous)
{
  if (n_out)
    *n_out = state->n_segs;
  if (repeat)
    *repeat = state->repeat;
  if (continuous)
    *continuous = state->continuous;
  return state->segs;
}

void
wfm_compose_destroy (wfm_compose_state_t *state)
{
  if (state)
    {
      stop_synths (state);
      for (size_t i = 0; i < state->n_segs; i++)
        free_segment_sources (&state->segs[i]);
      free (state->segs);
      free (state->syn);
      free (state->gain);
      free (state->scratch);
      free (state);
    }
}
