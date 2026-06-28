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
  int            seed_advance; /* per-repeat seed policy (wfm_seed_advance_t):
                                  NONE = byte-identical; NOISE = advance only the
                                  AWGN seed (signal fixed); ALL = advance the
                                  whole seed (code+data+noise) */
  unsigned            epoch;   /* repeat counter (0 on the first pass) */
  size_t              cur;     /* current segment index */
  int                 phase;   /* PHASE_ON / PHASE_OFF / PHASE_DONE */
  size_t              left;    /* samples remaining in the current phase */
  wfm_synth_state_t **syn;     /* active segment's synths (one per source) */
  float              *gain;    /* parallel: 10^(level/20) per source */
  size_t              n_syn;   /* live synth count while ON (0 otherwise) */
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
static void
start_segment (wfm_compose_state_t *s)
{
  const wfm_segment_t *g  = &s->segs[s->cur];
  int                  ok = (g->num_samples > 0 && g->n_sources > 0);
  s->n_syn                = 0;
  for (size_t k = 0; k < g->n_sources && ok; k++)
    {
      const wfm_source_t *src = &g->sources[k];
      s->gain[k] = (float)pow (10.0, src->level / 20.0); /* level → gain */
      /* seed_advance == ALL bumps the whole seed by the repeat epoch, so the
       * PN LFSR (code+data) *and* the AWGN both advance → a new realization
       * every pass; NONE/NOISE create from the fixed seed (NOISE reseeds noise
       * below). epoch == 0 (first pass) is always the unmodified seed. */
      uint32_t seed = src->seed;
      if (s->seed_advance == WFM_SEED_ADVANCE_ALL && s->epoch)
        seed = (uint32_t)(src->seed + s->epoch);
      s->syn[k] = wfm_synth_create (
          src->type, g->fs, src->freq, src->snr, src->snr_mode, seed, src->sps,
          src->pn_length, src->pn_poly, src->lfsr, src->f_end);
      if (!s->syn[k])
        ok = 0;
      else
        {
          /* Pin a chirp's sweep to the segment's on-time so it sweeps
           * f_start→f_end over exactly num_samples (no-op for non-chirp). */
          wfm_synth_set_chirp_span (s->syn[k], g->num_samples);
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
          /* Fresh noise per repeat (NOISE mode): advance ONLY the AWGN seed by
           * the repeat epoch, leaving the signal (LO / PN code / data / pulse)
           * bit-identical — so a fixed preamble/code re-acquires every burst.
           * (ALL already advanced the whole seed at create; NONE does
           * nothing.)
           */
          if (s->seed_advance == WFM_SEED_ADVANCE_NOISE && s->epoch)
            wfm_synth_reseed_noise (s->syn[k],
                                    (uint32_t)(src->seed + s->epoch));
          s->n_syn = k + 1; /* track for stop_synths on partial failure */
        }
    }
  if (ok)
    {
      s->phase = PHASE_ON;
      s->left  = g->num_samples;
    }
  else
    {
      stop_synths (s);
      s->phase = PHASE_OFF;
      s->left  = g->off_samples;
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
              state->left  = state->segs[state->cur].off_samples;
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
wfm_compose_set_seed_advance (wfm_compose_state_t *state, int mode)
{
  if (state && mode >= WFM_SEED_ADVANCE_NONE && mode <= WFM_SEED_ADVANCE_ALL)
    state->seed_advance = mode;
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
