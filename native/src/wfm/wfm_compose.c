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
        free ((void *)seg->sources[k].payload.bits);
        free (seg->sources[k].symbols);
        free ((void *)seg->sources[k].acq_code.bits);
        free ((void *)seg->sources[k].data_code.bits);
        free ((void *)seg->sources[k].sync.bits);
        /* The carried description, on the same terms as the arrays above:
           the composer took its OWN copy so the caller's need not outlive
           it, so the copy is the composer's to release. Its fields' literal
           bits go first -- they hang off the description. */
        if (seg->sources[k].frame)
          {
            wfm_frame_desc_t *d = (wfm_frame_desc_t *)seg->sources[k].frame;
            for (unsigned f = 0; f < d->n_fields; f++)
              free ((void *)d->field[f].seq.bits);
            free (d);
            seg->sources[k].frame = NULL;
          }
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
  dst->payload.bits   = NULL;
  dst->symbols        = NULL;
  dst->acq_code.bits  = NULL;
  dst->data_code.bits = NULL;
  dst->sync.bits      = NULL;
  dst->frame          = NULL;
  /* A CARRIED description is borrowed by a source -- the caller's own must
     outlive it -- but the composer deliberately outlives its caller's
     buffers, which is what every dup_u8 below is for. So it takes its own
     copy of the description AND of each field's literal bits, and owns
     both. Without this a `--from-file` scene would be reading a description
     its parser had already freed. */
  if (src->frame)
    {
      wfm_frame_desc_t *d = dp_xmalloc (sizeof *d);
      *d                  = *src->frame;
      dst->frame          = d;
      /* Null every borrowed pointer FIRST, so a failure part-way leaves a
         description free_segment_sources() can walk without touching a
         buffer that belongs to the caller. */
      for (unsigned f = 0; f < d->n_fields; f++)
        d->field[f].seq.bits = NULL;
      for (unsigned f = 0; f < d->n_fields; f++)
        {
          const wfm_seq_t *q = &src->frame->field[f].seq;
          if (q->bits && q->len)
            {
              d->field[f].seq.bits = dup_u8 (q->bits, q->len);
              if (!d->field[f].seq.bits)
                return -1;
            }
        }
    }
  if (src->payload.bits && src->payload.len)
    {
      dst->payload.bits = dup_u8 (src->payload.bits, src->payload.len);
      if (!dst->payload.bits)
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
  if (src->acq_code.bits && src->acq_code.len)
    {
      dst->acq_code.bits = dup_u8 (src->acq_code.bits, src->acq_code.len);
      if (!dst->acq_code.bits)
        return -1;
    }
  if (src->data_code.bits && src->data_code.len)
    {
      dst->data_code.bits = dup_u8 (src->data_code.bits, src->data_code.len);
      if (!dst->data_code.bits)
        return -1;
    }
  if (src->sync.bits && src->sync.len)
    {
      dst->sync.bits = dup_u8 (src->sync.bits, src->sync.len);
      if (!dst->sync.bits)
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
  size_t         cur;          /* current segment index */
  int            phase;        /* PHASE_ON / PHASE_OFF / PHASE_DONE */
  size_t         left;         /* samples remaining in the current phase */
  size_t         cur_num;   /* this epoch's resolved on-time (ranged/fixed) */
  size_t         cur_off;   /* this epoch's resolved off-time gap */
  size_t         cur_delay; /* this epoch's resolved leading delay */
  wfm_render_t **rend;      /* active segment's renderers (one per source) */
  float         *gain;      /* parallel: 10^(level/20) per source */
  size_t         n_syn;     /* live renderer count while ON (0 otherwise) */
  size_t         syn_cap;   /* capacity of rend/gain = max n_sources */
  float complex *scratch;   /* SCRATCH_CAP render buffer for N-source sum */
  /* PERSIST channels, one slot per (segment, source), owned for the life of
     the scene. A source's identity is its position -- the composer has no
     other -- and that is exactly what has to survive the per-segment synth
     teardown for `doppler_rate` to mean anything across a multi-burst pass.
     NULL everywhere unless some source declares WFM_DOPPLER_PERSIST. */
  doppler_channel_state_t **pch;
  size_t                   *pch_off; /* first slot of segment i */
  size_t                    pch_n;   /* total slots */
};

/* Destroy the active segment's renderers (the rend[] array stays allocated).
 * A PERSIST source's channel is BORROWED, so it survives this by construction
 * -- that is the whole point of the borrow: this teardown is exactly the
 * event that used to restart the geometry at every segment boundary. */
static void
stop_synths (wfm_compose_state_t *s)
{
  for (size_t k = 0; k < s->n_syn; k++)
    if (s->rend[k])
      {
        wfm_render_destroy (s->rend[k]);
        s->rend[k] = NULL;
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
  double             snr_c = wfm_source_create_snr (src, fs, snr, &snr_mode);
  wfm_synth_state_t *syn
      = wfm_synth_create (src->type, fs, freq, snr_c, snr_mode, seed, src->sps,
                          src->pn_length, src->pn_poly, src->lfsr, f_end);
  if (!syn)
    return NULL;
  /* Pin a chirp's sweep to the on-time (no-op for non-chirp). */
  wfm_synth_set_chirp_span (syn, on_len);
  /* Attach a bits pattern / symbols stream / dsss burst (no-op otherwise).
     The bits attach goes through the frame path so a framed source emits
     `[preamble x reps | sync | payload | crc]` here exactly as it does on the
     standalone face — they share the bridge for that reason. */
  if (wfm_source_attach_frame (syn, src) != 0)
    {
      wfm_synth_destroy (syn);
      return NULL;
    }
  if (src->type == WFM_SYNTH_SYMBOLS && src->symbols)
    wfm_synth_set_symbols (syn, src->symbols, src->n_symbols);
  /* dsss burst OR continuous, via the one shared attach path (bridge) so the
   * standalone and composed faces cannot drift. */
  if (wfm_source_attach_dsss (syn, src, fs) != 0)
    {
      /* Invalid geometry: fail the build (the composer skips the segment to a
       * silent gap; a standalone Synth raises at first use). */
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

/* One source's renderer. `ch` is NULL unless the source declares Doppler, and
 * everything below it is then unused — which is what keeps a non-Doppler
 * scene on exactly its old code path. */
struct wfm_render
{
  wfm_synth_state_t       *syn;
  doppler_channel_state_t *ch;
  int                      ch_borrowed; /* a PERSIST channel the scene owns */
  float _Complex          *in;   /* one input block for the channel      */
  float _Complex          *hold; /* what the channel produced, undrained */
  size_t                   hold_cap;
  size_t                   hold_n;  /* valid samples in hold            */
  size_t                   hold_rd; /* how many of them are spent       */
};

/* Input block fed per refill. Not DOPPLER_CHANNEL_MAX_BLOCK: the holdover
 * buffer is sized from execute_max_out(), which assumes a FULL max block, so
 * a smaller feed keeps both allocations modest while still amortising the
 * per-call ramp setup. Any value is correct — the channel's accumulator
 * carries across calls, so the output does not depend on how the input was
 * chunked. */
#define RENDER_FEED 4096u

void
wfm_render_destroy (wfm_render_t *r)
{
  if (!r)
    return;
  if (r->syn)
    wfm_synth_destroy (r->syn);
  if (r->ch && !r->ch_borrowed)
    doppler_channel_destroy (r->ch);
  free (r->in);
  free (r->hold);
  free (r);
}

wfm_render_t *
wfm_compose_build_render (const wfm_source_t *src, double fs, size_t on_len,
                          double freq, double snr, double f_end,
                          double doppler, double doppler_rate, unsigned epoch,
                          int seed_advance, size_t instance,
                          doppler_channel_state_t *borrow)
{
  wfm_render_t *r = dp_xcalloc (1, sizeof *r);
  r->syn = wfm_compose_build_synth (src, fs, on_len, freq, snr, f_end, epoch,
                                    seed_advance, instance);
  if (!r->syn)
    {
      /* NOT an OOM path: build_synth also returns NULL for an invalid
         geometry, which the streaming composer turns into a silent gap. */
      free (r);
      return NULL;
    }
  /* No declared motion, no channel: the pull below is then a straight
     wfm_synth_steps and the scene is byte-identical to before Doppler
     existed. Both terms are checked because a pure rate ramp starting from
     zero offset is a legitimate pass. */
  if (!borrow && doppler == 0.0 && doppler_rate == 0.0)
    return r;

  if (borrow)
    {
      /* A PERSIST source: the scene owns this channel across segments, so
         the geometry carries rather than restarting when the synth is torn
         down at a boundary. */
      r->ch          = borrow;
      r->ch_borrowed = 1;
    }
  else
    r->ch
        = doppler_channel_create (fs, src->carrier_hz, doppler, doppler_rate);
  if (!r->ch)
    {
      wfm_render_destroy (r);
      return NULL;
    }
  r->hold_cap = doppler_channel_execute_max_out (r->ch);
  r->in       = dp_xmalloc (RENDER_FEED * sizeof *r->in);
  r->hold     = dp_xmalloc (r->hold_cap * sizeof *r->hold);
  return r;
}

/* Pull `n` samples, taking the synth's full output or only its AWGN.
 *
 * The channel RUNS EITHER WAY, and that is the point rather than an
 * incidental sharing of code: an emitter does not stop moving because its
 * burst ended. A pass is continuous, so during a gap the thing propagating
 * through the channel is the noise floor the segment already carries
 * (gh-409) — the geometry keeps advancing, and burst k+1 sees where the pass
 * actually got to instead of where it would be if time had stopped between
 * bursts. With the channel skipped over gaps, `doppler_rate` across a
 * multi-burst scene would silently mean "rate per unit of ON time".
 *
 * A consequence worth naming: the holdover is NOT flushed at a phase
 * boundary. Samples that entered the channel during the on-time can emerge
 * after it, which is the dilation being modelled, not leakage. */
static void
render_pull (wfm_render_t *r, float _Complex *dst, size_t n, int noise_only)
{
  if (!r->ch)
    {
      if (noise_only)
        wfm_synth_noise_steps (r->syn, dst, n);
      else
        wfm_synth_steps (r->syn, dst, n);
      return;
    }
  size_t done = 0;
  while (done < n)
    {
      if (r->hold_rd < r->hold_n)
        {
          size_t take = r->hold_n - r->hold_rd;
          if (take > n - done)
            take = n - done;
          memcpy (dst + done, r->hold + r->hold_rd, take * sizeof *dst);
          r->hold_rd += take;
          done += take;
          continue;
        }
      /* Refill. The output buffer is sized at execute_max_out() and NEVER
         at "just what is still wanted": doppler_channel_execute's loop is
         `off < x_len && n_out < max_out`, so a short buffer stops it early
         and the input it had not yet consumed is dropped on the floor --
         silently, and it is exactly the samples the holdover exists to
         keep. Re-queried each pass because the bound tracks the ramp. */
      r->hold_rd = 0;
      r->hold_n  = 0;
      /* Size the FEED to the buffer, rather than growing the buffer to the
         feed. This is the line that keeps doppler_channel_execute from
         stopping on `n_out < max_out` and DROPPING the input it had not
         consumed -- the one failure the whole holdover exists to avoid.

         execute_max_out() bounds the output of a full
         DOPPLER_CHANNEL_MAX_BLOCK input and tracks the ramp, so `fits`
         scales it down to the largest input whose output still fits what we
         own. Unconditional rather than a `bound > hold_cap` guard: with
         RENDER_FEED at a sixteenth of MAX_BLOCK the clamp is inert unless
         the time base compresses by more than 16x, so a guard would be a
         branch no test could reach -- while the arithmetic is exercised
         every call and is correct at any ramp. A shorter feed costs nothing:
         the channel's accumulator carries across calls, so chunking is
         invisible in the output. */
      size_t bound = doppler_channel_execute_max_out (r->ch);
      size_t fits
          = (size_t)((double)r->hold_cap * (double)DOPPLER_CHANNEL_MAX_BLOCK
                     / (double)bound);
      size_t feed = (fits < RENDER_FEED) ? fits : RENDER_FEED;
      if (feed == 0)
        feed = 1;
      if (noise_only)
        wfm_synth_noise_steps (r->syn, r->in, feed);
      else
        wfm_synth_steps (r->syn, r->in, feed);
      r->hold_n
          = doppler_channel_execute (r->ch, r->in, feed, r->hold, r->hold_cap);
    }
}

void
wfm_render_steps (wfm_render_t *r, float _Complex *dst, size_t n)
{
  render_pull (r, dst, n, 0);
}

void
wfm_render_noise_steps (wfm_render_t *r, float _Complex *dst, size_t n)
{
  render_pull (r, dst, n, 1);
}

static void
start_segment (wfm_compose_state_t *s)
{
  const wfm_segment_t *g = &s->segs[s->cur];
  /* Resolve this epoch's (possibly ranged) durations once, up front: ON uses
   * cur_num, the trailing gap uses cur_off. A fixed segment (ranged == 0) just
   * copies the scalars, so a non-ranged scene is byte-identical to before. */
  wfm_seg_draw_t d;
  wfm_draw_segment (g, s->epoch, s->instance, s->cur, &d);
  s->cur_num   = d.on;
  s->cur_off   = d.off;
  s->cur_delay = d.delay;
  int ok       = (s->cur_num > 0 && g->n_sources > 0);
  s->n_syn     = 0;
  for (size_t k = 0; k < g->n_sources && ok; k++)
    {
      const wfm_source_t *src = &g->sources[k];
      /* Draw this epoch's ranged source fields (freq/snr/level/f_end) through
         the SAME helper wfm_compose_draws() reports through, so what is
         rendered and what is reported cannot disagree -- see wfm_draw.h on
         the sidecar that assembled one row from two provenances. A fixed
         field passes its scalar through unchanged. */
      wfm_src_draw_t v;
      wfm_draw_source (src, s->epoch, s->instance, s->cur, k, &v);
      const double freq = v.freq, snr = v.snr, f_end = v.f_end;
      s->gain[k] = (float)pow (10.0, v.level / 20.0); /* level → gain */
      /* Construct the synth through the shared SSOT (wfm_compose_build_synth):
       * the identical create + chirp-span + bits/symbols/RRC + per-repeat
       * noise reseed sequence the Plan cache uses, so a cached per-source
       * render is byte-identical to this composed one. freq/snr/f_end are
       * already ranged-resolved above; epoch/seed_advance drive the per-repeat
       * seed. */
      /* A PERSIST source renders through the scene-owned channel in its
         slot, created on first use so a scene that never reaches a segment
         never pays for it. PER_INSTANCE passes NULL and the renderer makes
         its own, which dies with the instance -- the repeated-trial shape. */
      doppler_channel_state_t *borrow = NULL;
      if (src->doppler_lifetime == WFM_DOPPLER_PERSIST
          && (v.doppler != 0.0 || v.doppler_rate != 0.0) && s->pch)
        {
          size_t slot = s->pch_off[s->cur] + k;
          if (!s->pch[slot])
            s->pch[slot] = doppler_channel_create (g->fs, src->carrier_hz,
                                                   v.doppler, v.doppler_rate);
          borrow = s->pch[slot];
        }
      s->rend[k] = wfm_compose_build_render (
          src, g->fs, s->cur_num, freq, snr, f_end, v.doppler, v.doppler_rate,
          s->epoch, s->seed_advance, s->instance, borrow);
      if (!s->rend[k])
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
  /* Refuse a frame no source in this scene can carry, BEFORE anything is
     built. Deferring it to build time would reach wfm_compose_build_synth,
     whose NULL the streaming path turns into a silent gap — and a silent gap
     is how the frame fields came to be accepted and dropped in the first
     place. */
  for (size_t i = 0; i < n_segs; i++)
    for (size_t k = 0; k < segs[i].n_sources; k++)
      if (wfm_source_frame_error (&segs[i].sources[k]) != NULL)
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
      /* A lone dsss BURST's on-time is intrinsic — exactly one burst
       * (n_chips * sps samples) — so num_samples is derived here, on the
       * private copy, and any caller-supplied value (or range) is ignored:
       * every face resolves identically and --record emits the real span.
       * (A dsss source inside a multi-source sum keeps the segment's
       * explicit num_samples — the mix's span is the caller's call.)
       *
       * A CONTINUOUS dsss source (symbol_rate > 0) has NO intrinsic length —
       * the stream is endless and --count IS the span. It must be excluded
       * here, and not only because the derivation is meaningless:
       * wfm_frame_dsss_nchips() returns a nonzero (garbage) value for it
       * (n_bits payload * data_code.len + a spurious CRC), which would pass
       * the `if (nchips)` guard and silently overwrite the user's num_samples.
       */
      if (s->segs[i].n_sources == 1
          && s->segs[i].sources[0].type == WFM_SYNTH_DSSS
          && s->segs[i].sources[0].symbol_rate <= 0.0)
        {
          const wfm_source_t *d = &s->segs[i].sources[0];
          /* Through the source's own description, so a coding stage that
             lengthens the frame lengthens the segment by the SAME arithmetic
             the assembler uses -- a rate-1/2 inner code doubles both. */
          size_t nchips = wfm_source_dsss_nchips (d);
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
  s->rend    = calloc (max_src, sizeof (*s->rend));
  s->gain    = malloc (max_src * sizeof (*s->gain));
  s->scratch = malloc (SCRATCH_CAP * sizeof (*s->scratch));
  /* PERSIST slots, allocated only when a source actually asks for one: a
     scene with no persisting Doppler carries no extra state at all. The
     offsets are a prefix sum over segments, so slot (seg, k) is one add. */
  size_t persist = 0, slots = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      slots += s->segs[i].n_sources;
      for (size_t k = 0; k < s->segs[i].n_sources; k++)
        if (s->segs[i].sources[k].doppler_lifetime == WFM_DOPPLER_PERSIST)
          persist = 1;
    }
  if (persist)
    {
      s->pch     = dp_xcalloc (slots ? slots : 1, sizeof (*s->pch));
      s->pch_off = dp_xcalloc (n_segs, sizeof (*s->pch_off));
      s->pch_n   = slots;
      for (size_t i = 1; i < n_segs; i++)
        s->pch_off[i] = s->pch_off[i - 1] + s->segs[i - 1].n_sources;
    }
  if (!s->rend || !s->gain || !s->scratch)
    {
      for (size_t i = 0; i < n_segs; i++)
        free_segment_sources (&s->segs[i]);
      free (s->segs);
      free (s->rend);
      free (s->gain);
      free (s->scratch);
      free (s->pch);
      free (s->pch_off);
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
      wfm_render_noise_steps (s->rend[0], out, k);
      if (s->gain[0] != 1.0f)
        for (size_t j = 0; j < k; j++)
          out[j] *= s->gain[0];
      return;
    }
  wfm_render_noise_steps (s->rend[0], s->scratch, k);
  float g0 = s->gain[0];
  for (size_t j = 0; j < k; j++)
    out[j] = g0 * s->scratch[j];
  for (size_t sx = 1; sx < s->n_syn; sx++)
    {
      wfm_render_noise_steps (s->rend[sx], s->scratch, k);
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
              wfm_render_steps (state->rend[0], out + i, k);
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
              wfm_render_steps (state->rend[0], state->scratch, k);
              float g0 = state->gain[0];
              for (size_t j = 0; j < k; j++)
                out[i + j] = g0 * state->scratch[j];
              for (size_t sx = 1; sx < state->n_syn; sx++)
                {
                  wfm_render_steps (state->rend[sx], state->scratch, k);
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

int
wfm_compose_seed_advance (const wfm_compose_state_t *state)
{
  return state ? state->seed_advance : WFM_SEED_ADVANCE_NONE;
}

void
wfm_compose_destroy (wfm_compose_state_t *state)
{
  if (state)
    {
      stop_synths (state);
      /* The PERSIST channels outlive every renderer by design, so this is
         the only place that can free them. */
      for (size_t i = 0; i < state->pch_n; i++)
        if (state->pch[i])
          doppler_channel_destroy (state->pch[i]);
      for (size_t i = 0; i < state->n_segs; i++)
        free_segment_sources (&state->segs[i]);
      free (state->segs);
      free (state->rend);
      free (state->gain);
      free (state->scratch);
      free (state->pch);
      free (state->pch_off);
      free (state);
    }
}
