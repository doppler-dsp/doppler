/*
 * wfm_compose.c — multi-segment waveform composer (Phase B).
 *
 * A small state machine over a copied segment list. At any time the composer
 * is in one segment, in either its ON phase (pulling samples from a live
 * `synth`) or its OFF phase (emitting zeros). When OFF drains it advances to
 * the next segment; past the last segment it loops (repeat/continuous) or
 * finishes.
 */
#include "wfmgen/wfm_compose.h"

#include <math.h>
#include <stdlib.h>

enum
{
  PHASE_ON,
  PHASE_OFF,
  PHASE_DONE
};

struct wfm_compose_state
{
  wfm_segment_t *segs;
  size_t         n_segs;
  int            repeat;
  int            continuous;
  size_t         cur;   /* current segment index */
  int            phase; /* PHASE_ON / PHASE_OFF / PHASE_DONE */
  size_t         left;  /* samples remaining in the current phase */
  synth_state_t *syn;   /* active segment's synth (NULL outside ON) */
  float          gain;  /* 10^(level/20) for the active segment; 1.0 = no-op */
};

/* Start segment `cur` in its ON phase, creating its synth. On synth failure
 * the segment is skipped to OFF (a silent gap) so one bad segment can't wedge
 * the stream. */
static void
start_segment (wfm_compose_state_t *s)
{
  const wfm_segment_t *g = &s->segs[s->cur];
  s->gain
      = (float)pow (10.0, g->level / 20.0); /* source level → linear gain */
  s->syn = synth_create (g->type, g->fs, g->freq, g->snr, g->snr_mode, g->seed,
                         g->sps, g->pn_length, g->pn_poly, g->lfsr);
  if (s->syn && g->num_samples > 0)
    {
      s->phase = PHASE_ON;
      s->left  = g->num_samples;
    }
  else
    {
      if (s->syn)
        {
          synth_destroy (s->syn);
          s->syn = NULL;
        }
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
  s->segs = malloc (n_segs * sizeof (*s->segs));
  if (!s->segs)
    {
      free (s);
      return NULL;
    }
  for (size_t i = 0; i < n_segs; i++)
    s->segs[i] = segs[i];
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
              if (state->syn)
                {
                  synth_destroy (state->syn);
                  state->syn = NULL;
                }
              state->phase = PHASE_OFF;
              state->left  = state->segs[state->cur].off_samples;
              continue;
            }
          /* Pull the ON run as a block through the *same* synth_steps() the
           * wavegen CLI uses, so the composer and CLI are byte-identical by
           * construction. A per-sample synth_step() loop is only byte-equal to
           * synth_steps() when FP contraction is off: under -ffast-math the
           * scalar `sym*carrier + noise` contracts to FMAs on targets with a
           * fused multiply-add (arm64), while the block path rounds the
           * multiply and the noise-add separately. QPSK's irrational ±1/√2 leg
           * exposes the gap (±1/0 legs are immune), which diverged the macOS
           * composer from the CLI (#67). synth_steps() is chunk-invariant, so
           * batching here also leaves the Linux output bit-for-bit unchanged.
           */
          size_t k = max - i;
          if (k > state->left)
            k = state->left;
          synth_steps (state->syn, out + i, k);
          if (state->gain != 1.0f) /* apply source level (no-op at 0 dBFS) */
            for (size_t j = 0; j < k; j++)
              out[i + j] *= state->gain;
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
      if (state->syn)
        synth_destroy (state->syn);
      free (state->segs);
      free (state);
    }
}
