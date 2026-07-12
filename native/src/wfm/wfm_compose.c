/*
 * wfm_compose.c — multi-segment waveform composer (Phase B + Phase 4a).
 *
 * A small state machine over a copied segment list. At any time the composer
 * is in one segment, in its DELAY phase (leading gap), ON phase (summing its
 * sources), or OFF phase (trailing gap). Gaps carry the segment's noise
 * floor by default (gh-409): each source's additive-AWGN term keeps running
 * — the same stream that noises the on-time — while the signal stops, so a
 * clean scene's gaps stay exact zeros and gap_noise=off forces zeros. When
 * OFF drains it advances to the next segment (or `repeats` instance); past
 * the last segment it loops (repeat/continuous) or finishes.
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
#include "wfm_draw.h"    /* the shared ranged-draw hash (one definition) */

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Free a segment's per-source owned arrays and then the sources array. */
static void
free_segment_sources (wfm_segment_t *seg)
{
  if (seg->sources)
    for (size_t k = 0; k < seg->n_sources; k++)
      {
        free (seg->sources[k].bits);
        free (seg->sources[k].symbols);
        free (seg->sources[k].acq_code);
        free (seg->sources[k].data_code);
        free (seg->sources[k].sync);
      }
  free (seg->sources);
  seg->sources = NULL;
}

/* malloc+memcpy an owned byte array (NULL for an empty one). */
static uint8_t *
dup_u8 (const uint8_t *src, size_t n)
{
  if (!src || !n)
    return NULL;
  uint8_t *copy = malloc (n);
  if (copy)
    memcpy (copy, src, n);
  return copy;
}

/* Replace dst's array pointers (struct-assigned from the caller's source)
 * with owned copies. On failure every pointer is already owned-or-NULL, so
 * free_segment_sources() on the partially-built list stays safe (it never
 * frees a caller's buffer). Returns 0, or -1 on allocation failure. */
static int
copy_source_arrays (wfm_source_t *dst, const wfm_source_t *src)
{
  dst->bits      = NULL;
  dst->symbols   = NULL;
  dst->acq_code  = NULL;
  dst->data_code = NULL;
  dst->sync      = NULL;
  if (src->bits && src->n_bits)
    {
      dst->bits = dup_u8 (src->bits, src->n_bits);
      if (!dst->bits)
        return -1;
    }
  if (src->symbols && src->n_symbols)
    {
      size_t nbytes = src->n_symbols * sizeof *src->symbols;
      dst->symbols  = malloc (nbytes);
      if (!dst->symbols)
        return -1;
      memcpy (dst->symbols, src->symbols, nbytes);
    }
  if (src->acq_code && src->n_acq_code)
    {
      dst->acq_code = dup_u8 (src->acq_code, src->n_acq_code);
      if (!dst->acq_code)
        return -1;
    }
  if (src->data_code && src->n_data_code)
    {
      dst->data_code = dup_u8 (src->data_code, src->n_data_code);
      if (!dst->data_code)
        return -1;
    }
  if (src->sync && src->n_sync)
    {
      dst->sync = dup_u8 (src->sync, src->n_sync);
      if (!dst->sync)
        return -1;
    }
  return 0;
}

enum
{
  PHASE_DELAY, /* leading gap (delay_samples) — noise floor, no signal */
  PHASE_ON,
  PHASE_OFF, /* trailing gap (off_samples) — noise floor, no signal */
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
  unsigned epoch;              /* repeat counter (0 on the first pass) — drives
                                  per-repeat seed advance + ranged-field draws */
  size_t instance;             /* current segment's repeats counter (0-based) —
                                  folds into ranged draws + the AWGN reseed */
  size_t cur;                  /* current segment index */
  int    phase;                /* PHASE_ON / PHASE_OFF / PHASE_DONE */
  size_t left;                 /* samples remaining in the current phase */
  size_t cur_num;            /* this epoch's resolved on-time (ranged/fixed) */
  size_t cur_off;            /* this epoch's resolved off-time gap */
  size_t cur_delay;          /* this epoch's resolved leading delay */
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

/* Construct + configure the synth for one resolved source: create + chirp-span
 * pin + bits/symbols/RRC attach + per-repeat NOISE reseed. THE single
 * synth-construction path — the streaming composer (start_segment) and the
 * Plan stimulus cache (wfm_plan_prepare) both call it, so a cached per-source
 * render is byte-identical to the composed one. freq/snr/f_end are passed
 * already ranged-resolved by the caller; on_len pins a chirp's sweep to the
 * on-time; epoch/seed_advance drive the per-repeat seed policy (epoch 0 → the
 * unmodified seed); a non-zero `repeats` instance always freshens the AWGN
 * (signal fixed). Returns NULL only on synth-create failure. */
wfm_synth_state_t *
wfm_compose_build_synth (const wfm_source_t *src, double fs, size_t on_len,
                         double freq, double snr, double f_end, unsigned epoch,
                         int seed_advance, size_t instance)
{
  /* seed_advance == ALL bumps the whole seed by the repeat epoch (PN LFSR +
   * AWGN both advance); NONE/NOISE create from the fixed seed. */
  uint32_t seed = src->seed;
  if (seed_advance == WFM_SEED_ADVANCE_ALL && epoch)
    seed = (uint32_t)(src->seed + epoch);
  /* A dsss data-symbol Es/N0 is referred to fs before create (the codes
   * attach below, after create resolves the noise); identity otherwise. */
  int                snr_mode = 0;
  double             snr_c    = wfm_source_create_snr (src, snr, &snr_mode);
  wfm_synth_state_t *syn
      = wfm_synth_create (src->type, fs, freq, snr_c, snr_mode, seed, src->sps,
                          src->pn_length, src->pn_poly, src->lfsr, f_end);
  if (!syn)
    return NULL;
  /* Pin a chirp's sweep to the on-time (no-op for non-chirp). */
  wfm_synth_set_chirp_span (syn, on_len);
  /* Attach a bits pattern / symbols stream / dsss burst (no-op otherwise). */
  if (src->type == WFM_SYNTH_BITS && src->bits)
    wfm_synth_set_bits (syn, src->bits, src->n_bits, src->modulation);
  if (src->type == WFM_SYNTH_SYMBOLS && src->symbols)
    wfm_synth_set_symbols (syn, src->symbols, src->n_symbols);
  if (src->type == WFM_SYNTH_DSSS
      && wfm_synth_set_dsss (syn, src->acq_code, src->n_acq_code,
                             src->acq_reps, src->data_code, src->n_data_code,
                             src->sync, src->n_sync, src->bits, src->n_bits,
                             src->crc)
             != 0)
    {
      /* Invalid burst geometry: fail the build (the composer skips the
       * segment to a silent gap; a standalone Synth raises at first use). */
      wfm_synth_destroy (syn);
      return NULL;
    }
  /* RRC pulse shaping (same wfm_rrc_taps() the standalone face uses; set_rrc
   * scales for unit TX power and no-ops for non-modulated types). */
  if (src->pulse && src->sps > 0 && src->rrc_span > 0)
    {
      size_t nt   = wfm_rrc_ntaps (src->sps, src->rrc_span);
      float *taps = malloc (nt * sizeof (float));
      if (taps)
        {
          wfm_rrc_taps (src->rrc_beta, src->sps, src->rrc_span, taps);
          wfm_synth_set_rrc (syn, taps, nt);
          free (taps);
        }
    }
  /* Fresh noise per repeat (NOISE mode) and per `repeats` instance: advance
   * ONLY the AWGN seed, leaving the signal bit-identical. (ALL already
   * advanced the whole seed per epoch; NONE nothing.) A non-zero instance
   * always freshens the noise — two instances of one burst declaration must
   * never share an AWGN realization — while instance 0 keeps the historical
   * seeds exactly (byte-compat). The golden-ratio fold keeps distinct
   * (epoch, instance) pairs from colliding on nearby seeds. */
  if ((seed_advance == WFM_SEED_ADVANCE_NOISE && epoch) || instance)
    {
      uint32_t nseed = (seed_advance == WFM_SEED_ADVANCE_NOISE)
                           ? (uint32_t)(src->seed + epoch)
                           : seed;
      nseed ^= (uint32_t)(instance * 0x9E3779B9u);
      wfm_synth_reseed_noise (syn, nseed);
    }
  return syn;
}

static void
start_segment (wfm_compose_state_t *s)
{
  const wfm_segment_t *g = &s->segs[s->cur];
  /* Resolve this epoch's (possibly ranged) durations once, up front: ON uses
   * cur_num, the trailing gap uses cur_off. A fixed segment (ranged == 0) just
   * copies the scalars, so a non-ranged scene is byte-identical to before. */
  uint32_t dseed = g->n_sources ? g->sources[0].seed : 1u;
  s->cur_num   = (g->ranged & WFM_RANGE_NUM_SAMPLES)
                     ? wfm_draw_samples (dseed, s->epoch, s->instance, s->cur,
                                         WFM_RANGE_NUM_SAMPLES, g->num_samples,
                                         g->num_samples_hi)
                     : g->num_samples;
  s->cur_off   = (g->ranged & WFM_RANGE_OFF_SAMPLES)
                     ? wfm_draw_samples (dseed, s->epoch, s->instance, s->cur,
                                         WFM_RANGE_OFF_SAMPLES, g->off_samples,
                                         g->off_samples_hi)
                     : g->off_samples;
  s->cur_delay = (g->ranged & WFM_RANGE_DELAY_SAMPLES)
                     ? wfm_draw_samples (dseed, s->epoch, s->instance, s->cur,
                                         WFM_RANGE_DELAY_SAMPLES,
                                         g->delay_samples, g->delay_samples_hi)
                     : g->delay_samples;
  int ok       = (s->cur_num > 0 && g->n_sources > 0);
  s->n_syn     = 0;
  for (size_t k = 0; k < g->n_sources && ok; k++)
    {
      const wfm_source_t *src = &g->sources[k];
      /* Draw this epoch's ranged source fields (freq/snr/level/f_end); a fixed
       * field passes its scalar through unchanged. */
      double freq
          = (src->ranged & WFM_RANGE_FREQ)
                ? wfm_draw_range (src->seed, s->epoch, s->instance, s->cur, k,
                                  WFM_RANGE_FREQ, src->freq, src->freq_hi)
                : src->freq;
      double snr
          = (src->ranged & WFM_RANGE_SNR)
                ? wfm_draw_range (src->seed, s->epoch, s->instance, s->cur, k,
                                  WFM_RANGE_SNR, src->snr, src->snr_hi)
                : src->snr;
      double level
          = (src->ranged & WFM_RANGE_LEVEL)
                ? wfm_draw_range (src->seed, s->epoch, s->instance, s->cur, k,
                                  WFM_RANGE_LEVEL, src->level, src->level_hi)
                : src->level;
      double f_end
          = (src->ranged & WFM_RANGE_FEND)
                ? wfm_draw_range (src->seed, s->epoch, s->instance, s->cur, k,
                                  WFM_RANGE_FEND, src->f_end, src->f_end_hi)
                : src->f_end;
      s->gain[k] = (float)pow (10.0, level / 20.0); /* level → gain */
      /* Construct the synth through the shared SSOT (wfm_compose_build_synth):
       * the identical create + chirp-span + bits/symbols/RRC + per-repeat
       * noise reseed sequence the Plan cache uses, so a cached per-source
       * render is byte-identical to this composed one. freq/snr/f_end are
       * already ranged-resolved above; epoch/seed_advance drive the per-repeat
       * seed. */
      s->syn[k]
          = wfm_compose_build_synth (src, g->fs, s->cur_num, freq, snr, f_end,
                                     s->epoch, s->seed_advance, s->instance);
      if (!s->syn[k])
        ok = 0;
      else
        s->n_syn = k + 1; /* track for stop_synths on partial failure */
    }
  if (ok)
    {
      /* The synths stay alive through DELAY, ON, and OFF (advance() tears
       * them down), so the leading and trailing gaps can carry each
       * source's noise term as a seamless continuation. */
      s->phase = s->cur_delay ? PHASE_DELAY : PHASE_ON;
      s->left  = s->cur_delay ? s->cur_delay : s->cur_num;
    }
  else
    {
      /* Failed segment: no live synths — the whole delay + gap span is
       * silence (a bad segment degrades quietly, never wedges the stream).
       */
      stop_synths (s);
      s->phase = PHASE_OFF;
      s->left  = s->cur_delay + s->cur_off;
    }
}

/* Move to the next segment, looping or finishing at the end. A `repeats=N`
 * segment first re-enters itself N times (fresh instance: new ranged draws,
 * fresh AWGN, fixed signal); 0 and 1 both mean a single instance. The
 * outgoing instance's synths die here — they lived through its trailing gap
 * so the gap could carry their noise floor. */
static void
advance (wfm_compose_state_t *s)
{
  stop_synths (s);
  const wfm_segment_t *g    = &s->segs[s->cur];
  size_t               reps = g->repeats ? g->repeats : 1;
  s->instance++;
  if (s->instance < reps)
    {
      start_segment (s);
      return;
    }
  s->instance = 0;
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
          s->segs[i].sources[k] = segs[i].sources[k]; /* scalar fields */
          if (copy_source_arrays (&s->segs[i].sources[k], &segs[i].sources[k])
              != 0)
            {
              for (size_t j = 0; j <= i; j++)
                free_segment_sources (&s->segs[j]);
              free (s->segs);
              free (s);
              return NULL;
            }
        }
      /* A lone dsss source's on-time is intrinsic — exactly one burst
       * (n_chips * sps samples) — so num_samples is derived here, on the
       * private copy, and any caller-supplied value (or range) is ignored:
       * every face resolves identically and --record emits the real span.
       * (A dsss source inside a multi-source sum keeps the segment's
       * explicit num_samples — the mix's span is the caller's call.) */
      if (s->segs[i].n_sources == 1
          && s->segs[i].sources[0].type == WFM_SYNTH_DSSS)
        {
          const wfm_source_t *d = &s->segs[i].sources[0];
          size_t nchips = wfm_frame_dsss_nchips (d->n_acq_code, d->acq_reps,
                                                 d->n_data_code, d->n_sync,
                                                 d->n_bits, d->crc);
          if (nchips)
            {
              int sps                   = (d->sps < 1) ? 1 : d->sps;
              s->segs[i].num_samples    = nchips * (size_t)sps;
              s->segs[i].num_samples_hi = 0;
              s->segs[i].ranged &= ~(unsigned)WFM_RANGE_NUM_SAMPLES;
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

/* Render k gap samples (leading delay or trailing off-time) at out. The
 * gap carries the segment's noise floor: every live source contributes only
 * its additive-AWGN term, at its gain — the same streams that noise the
 * on-time, continued (gap_noise=auto). With no live synths (failed segment)
 * or gap_noise=off the gap is exact zeros, as before. Mirrors the ON path's
 * 1-source / N-source split so face parity holds sample-for-sample. */
static void
render_gap (wfm_compose_state_t *s, float complex *out, size_t k)
{
  const wfm_segment_t *g = &s->segs[s->cur];
  if (s->n_syn == 0 || g->gap_noise)
    {
      memset (out, 0, k * sizeof *out);
      return;
    }
  if (s->n_syn == 1)
    {
      wfm_synth_noise_steps (s->syn[0], out, k);
      if (s->gain[0] != 1.0f)
        for (size_t j = 0; j < k; j++)
          out[j] *= s->gain[0];
      return;
    }
  wfm_synth_noise_steps (s->syn[0], s->scratch, k);
  float g0 = s->gain[0];
  for (size_t j = 0; j < k; j++)
    out[j] = g0 * s->scratch[j];
  for (size_t sx = 1; sx < s->n_syn; sx++)
    {
      wfm_synth_noise_steps (s->syn[sx], s->scratch, k);
      float gs = s->gain[sx];
      for (size_t j = 0; j < k; j++)
        out[j] += gs * s->scratch[j];
    }
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
              /* ON drained → trailing off-time gap (synths stay alive so
               * the gap carries their noise floor), then advance. */
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
        { /* PHASE_DELAY (leading) or PHASE_OFF (trailing) gap */
          if (state->left == 0)
            {
              if (state->phase == PHASE_DELAY)
                {
                  /* Delay drained → the burst itself. */
                  state->phase = PHASE_ON;
                  state->left  = state->cur_num;
                }
              else
                advance (state);
              continue;
            }
          size_t k = max - i;
          if (k > state->left)
            k = state->left;
          if (k > (size_t)SCRATCH_CAP && state->n_syn > 1)
            k = SCRATCH_CAP; /* N-source gap accumulates via scratch */
          render_gap (state, out + i, k);
          i += k;
          state->left -= k;
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
