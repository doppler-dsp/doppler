/*
 * wfm_plan.c — component-cache stimulus engine (see wfm_plan.h).
 *
 * prepare() parses + resolves a scene, then renders each signal source's
 * ON-time contribution ONCE per segment (through the composer's own
 * wfm_compose_build_synth, so a cached render is byte-identical to a full
 * compose) and caches it at gain 1, clean (no AWGN). render()/at()
 * re-materialize a variation by walking every segment's repeat instances:
 * cheap re-weighted signal sum for the cached ON-time, plus a noise
 * synth spanning that instance's delay+on+off (rebuilt through the same
 * SSOT, so it is byte-identical to the composer's noise/gap handling)
 * added on top. Baseline (no overrides) reproduces wfm_compose to the bit.
 *
 * Two noise reconstruction modes, matching the composer's own "1-source
 * vs N-source" split (see wfm_compose.c's top-of-file comment):
 *   - SHARED: a multi-source segment's resolve-appended WFM_SYNTH_NOISE
 *     source. Its amplitude is a pure external multiply (segment_noise_gain,
 *     the anchor/floor math), independent of any per-signal-source gain
 *     override.
 *   - BUNDLED: a segment with exactly one source carrying its own real
 *     (non-clean) snr. Its AWGN is baked into synth creation (not a
 *     separable external multiply), and that source's own level/gains
 *     override multiplies the WHOLE synth output (signal AND noise), same
 *     as a real single continuously-alive synth in the composer.
 */
#include "wfm/wfm_plan.h"

#include "awgn/awgn_core.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_plan_dsp_hash.h" /* WFM_PLAN_DSP_HASH_U64 (configure-time) */
#include "wfm_synth/wfm_synth_core.h"

#include "cJSON.h"
#include "dp_parallel.h"
#include "wfm_draw.h"

#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Loaded cache buffers for the restore fast-path: cache_segment_signals()
 * reads from these (guarded per-source by length) instead of running the DSP.
 * NULL for wfm_plan_prepare() and for a fingerprint-mismatched restore. */
typedef struct
{
  size_t                 n;    /* number of source buffers in the blob */
  const size_t          *lens; /* [n] expected sample count per source */
  const float _Complex **bufs; /* [n] pointers into the blob's payload */
} plan_loaded_t;

typedef struct
{
  size_t   n_sig;   /* cached clean signal sources owned by this segment    */
  size_t   sig_off; /* offset into the plan's flat cache_sig[]/base_gain[]  */
  size_t   num_samples; /* on-time, fixed across every repeat instance      */
  size_t   repeats;     /* resolved: 0/1 both normalize to 1                */
  unsigned ranged;      /* subset of WFM_RANGE_OFF_SAMPLES|DELAY_SAMPLES    */
  size_t   off_lo, off_hi;
  size_t   delay_lo, delay_hi;
  int      gap_noise; /* 0 auto (gaps carry the floor), 1 off (hard zero) */
  uint32_t dseed;     /* default draw/noise seed == sources[0].seed       */

  int          has_noise; /* 1 -> a gap-spanning noise synth is built     */
  int          bundled;   /* 1 -> noise_src is the lone real-snr source   */
  wfm_source_t noise_src; /* deep-copied owned arrays (freed at destroy)  */
  /* SHARED-only anchor bookkeeping (segment_noise_gain); unused when
   * bundled, since a bundled source's amplitude is baked into creation. */
  int    explicit_floor;
  double floor_db;
  double anchor_level;
  int    anchor_type, anchor_mode, anchor_sps;
  size_t anchor_sf;
  double anchor_sym_span; /* continuous-dsss fs/symbol_rate; 0 = burst */
} wfm_plan_segment_t;

struct wfm_plan
{
  size_t              n_segs;
  wfm_plan_segment_t *segs;
  size_t              n_sig;     /* total signal sources, all segments  */
  float _Complex    **cache_sig; /* [n_sig] flat, gain 1, clean          */
  float              *base_gain; /* [n_sig] flat: 10^(level/20)          */
  double              fs;
  size_t              len;       /* worst-case total materialized length */
  char               *spec_json; /* owned copy, embedded by wfm_plan_save */
};

/* Free one source's owned arrays (mirrors wfm_compose.c's
 * free_segment_sources, but for a single embedded struct). */
static void
free_source_arrays (wfm_source_t *s)
{
  free (s->bits);
  free (s->symbols);
  free (s->acq_code);
  free (s->data_code);
  free (s->sync);
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

/* Replace dst's array pointers with owned copies of src's (dst's scalar
 * fields are assumed already struct-copied by the caller). Returns 0, or
 * -1 on allocation failure (dst is left safe to free either way). */
static int
copy_source_arrays (wfm_source_t *dst, const wfm_source_t *src)
{
  dst->bits      = NULL;
  dst->symbols   = NULL;
  dst->acq_code  = NULL;
  dst->data_code = NULL;
  dst->sync      = NULL;
  if (src->bits && src->n_bits)
    if (!(dst->bits = dup_u8 (src->bits, src->n_bits)))
      return -1;
  if (src->symbols && src->n_symbols)
    {
      size_t nbytes = src->n_symbols * sizeof *src->symbols;
      if (!(dst->symbols = malloc (nbytes)))
        return -1;
      memcpy (dst->symbols, src->symbols, nbytes);
    }
  if (src->acq_code && src->n_acq_code)
    if (!(dst->acq_code = dup_u8 (src->acq_code, src->n_acq_code)))
      return -1;
  if (src->data_code && src->n_data_code)
    if (!(dst->data_code = dup_u8 (src->data_code, src->n_data_code)))
      return -1;
  if (src->sync && src->n_sync)
    if (!(dst->sync = dup_u8 (src->sync, src->n_sync)))
      return -1;
  return 0;
}

/* Accumulate one source into out: real*complex when phi==0 (exactly the
 * composer's expression), else the phase-rotated complex*complex form. */
static void
accumulate (float _Complex *out, size_t len, float g, double phase,
            const float _Complex *cache)
{
  if (phase == 0.0)
    for (size_t i = 0; i < len; i++)
      out[i] += g * cache[i];
  else
    {
      float _Complex w = g * cexpf ((float _Complex) (I * phase));
      for (size_t i = 0; i < len; i++)
        out[i] += w * cache[i];
    }
}

/* SHARED-mode external multiply: 10^(floor/20), the resolved base floor or
 * the anchor-recomputed floor when an SNR override is given (an explicit
 * floor is fixed -- SNR has no anchor to move it). Bundled segments have no
 * separate multiplier (their amplitude is baked into synth creation via
 * segment_gap_snr below) so this always returns 1.0 for them. */
static float
segment_noise_gain (const wfm_plan_segment_t *ps, double snr, int snr_given)
{
  if (!ps->has_noise || ps->bundled)
    return 1.0f;
  double fdb = ps->floor_db;
  if (snr_given && !ps->explicit_floor)
    fdb = ps->anchor_level
          - wfm_snr_over_fs (ps->anchor_mode, ps->anchor_type, ps->anchor_sps,
                             ps->anchor_sf, ps->anchor_sym_span, snr);
  return (float)pow (10.0, fdb / 20.0);
}

/* Build this segment/instance's gap-spanning noise synth, or NULL if the
 * segment carries no noise. SHARED: noise_src.snr is fixed (unused by a
 * NOISE-type synth; amplitude comes entirely from segment_noise_gain's
 * external multiply). BUNDLED: an SNR override replaces the source's own
 * (already-resolved-units) snr directly, since its AWGN is baked into
 * creation, not a separable multiply. */
static wfm_synth_state_t *
build_gap_synth (const wfm_plan_segment_t *ps, double fs, double snr,
                 int snr_given, uint32_t seed, size_t instance)
{
  if (!ps->has_noise)
    return NULL;
  wfm_source_t ns = ps->noise_src;
  ns.seed         = seed;
  double use_snr  = ns.snr;
  if (ps->bundled && snr_given)
    use_snr = snr;
  return wfm_compose_build_synth (&ns, fs, ps->num_samples, ns.freq, use_snr,
                                  ns.f_end, 0, WFM_SEED_ADVANCE_NONE,
                                  instance);
}

/* Draw noise_steps(n) from syn and add gain*sample into out (used for both
 * the delay/off gaps and the always-on ON-time noise addition). */
static void
add_noise (wfm_synth_state_t *syn, float _Complex *out, size_t n, float gain)
{
  if (n == 0)
    return;
  float _Complex *tmp = malloc (n * sizeof *tmp);
  if (!tmp)
    return;
  wfm_synth_noise_steps (syn, tmp, n);
  for (size_t i = 0; i < n; i++)
    out[i] += gain * tmp[i];
  free (tmp);
}

/* The shared kernel behind render() and at(): walk every segment's repeat
 * instances, accumulating the cached ON-time signal plus a gap-spanning
 * noise synth (delay -> on -> off, matching the composer's own phase
 * sequencing so gap_noise/level semantics are byte-identical). Returns the
 * actual materialized length for this draw (<= wfm_plan_len(p), the
 * worst-case capacity `out` was sized to). */
static size_t
materialize (const wfm_plan_t *p, const double *gains_db, const double *phases,
             const int *enable, double snr, int snr_given, uint64_t seed,
             int seed_given, float _Complex *out)
{
  memset (out, 0, p->len * sizeof *out);
  size_t pos = 0;
  for (size_t si = 0; si < p->n_segs; si++)
    {
      const wfm_plan_segment_t *ps = &p->segs[si];
      uint32_t eff_seed            = seed_given ? (uint32_t)seed : ps->dseed;
      for (size_t inst = 0; inst < ps->repeats; inst++)
        {
          size_t dly = (ps->ranged & WFM_RANGE_DELAY_SAMPLES)
                           ? wfm_draw_samples (eff_seed, 0, inst, si,
                                               WFM_RANGE_DELAY_SAMPLES,
                                               ps->delay_lo, ps->delay_hi)
                           : ps->delay_lo;
          size_t off = (ps->ranged & WFM_RANGE_OFF_SAMPLES)
                           ? wfm_draw_samples (eff_seed, 0, inst, si,
                                               WFM_RANGE_OFF_SAMPLES,
                                               ps->off_lo, ps->off_hi)
                           : ps->off_lo;
          size_t on  = ps->num_samples;

          float ext_gain = 1.0f;
          if (ps->bundled)
            {
              size_t gi = ps->sig_off;
              ext_gain  = gains_db ? (float)pow (10.0, gains_db[gi] / 20.0)
                                   : p->base_gain[gi];
              if (enable && !enable[gi])
                ext_gain = 0.0f;
            }
          else
            ext_gain = segment_noise_gain (ps, snr, snr_given);

          wfm_synth_state_t *gsyn
              = build_gap_synth (ps, p->fs, snr, snr_given, eff_seed, inst);

          if (gsyn && !ps->gap_noise)
            add_noise (gsyn, out + pos, dly, ext_gain);
          pos += dly;

          for (size_t k = 0; k < ps->n_sig; k++)
            {
              size_t gi = ps->sig_off + k;
              float  g  = gains_db ? (float)pow (10.0, gains_db[gi] / 20.0)
                                   : p->base_gain[gi];
              if (enable && !enable[gi])
                g = 0.0f;
              double ph = phases ? phases[gi] : 0.0;
              accumulate (out + pos, on, g, ph, p->cache_sig[gi]);
            }
          if (gsyn) /* ON always carries noise, regardless of gap_noise */
            add_noise (gsyn, out + pos, on, ext_gain);
          pos += on;

          if (gsyn && !ps->gap_noise)
            add_noise (gsyn, out + pos, off, ext_gain);
          pos += off;

          if (gsyn)
            wfm_synth_destroy (gsyn);
        }
    }
  return pos;
}

/* Resolve one segment's noise configuration (SHARED / BUNDLED / none) into
 * *ps. Returns 0 on success, -1 on an out-of-scope condition. */
static int
resolve_segment_noise (wfm_plan_segment_t *ps, const wfm_segment_t *g)
{
  size_t n_noise   = 0;
  int    noise_idx = -1;
  for (size_t k = 0; k < g->n_sources; k++)
    if (g->sources[k].type == WFM_SYNTH_NOISE)
      {
        n_noise++;
        noise_idx = (int)k;
      }
  if (n_noise > 1 || (noise_idx >= 0 && (size_t)noise_idx != g->n_sources - 1))
    return -1; /* at most one noise source, and it must be trailing */

  ps->bundled   = (g->n_sources == 1 && n_noise == 0
                   && g->sources[0].snr < WFM_SYNTH_SNR_CLEAN);
  ps->has_noise = (n_noise > 0) || ps->bundled;

  if (n_noise > 0)
    {
      const wfm_source_t *nsrc = &g->sources[noise_idx];
      ps->noise_src            = *nsrc;
      if (copy_source_arrays (&ps->noise_src, nsrc) != 0)
        return -1;
      ps->explicit_floor = 1;
      ps->floor_db       = nsrc->level;
      for (size_t j = 0; j < g->n_sources; j++)
        if (g->sources[j].type != WFM_SYNTH_NOISE
            && g->sources[j].seed == nsrc->seed)
          {
            ps->explicit_floor = 0;
            ps->anchor_level   = g->sources[j].level;
            ps->anchor_type    = g->sources[j].type;
            ps->anchor_mode    = g->sources[j].snr_mode;
            ps->anchor_sps     = g->sources[j].sps;
            ps->anchor_sf      = g->sources[j].n_data_code;
            ps->anchor_sym_span
                = (g->sources[j].type == WFM_SYNTH_DSSS
                   && g->sources[j].symbol_rate > 0.0 && g->fs > 0.0)
                      ? g->fs / g->sources[j].symbol_rate
                      : 0.0;
            break;
          }
    }
  else if (ps->bundled)
    {
      ps->noise_src = g->sources[0];
      if (copy_source_arrays (&ps->noise_src, &g->sources[0]) != 0)
        return -1;
    }
  return 0;
}

/* One source's build: create its synth through the composer's SSOT, render
 * the clean ON-time into a fresh buffer, and store it at its own flat cache
 * slot. Each work item reads only shared read-only config (g, ps) and writes
 * only its own slot[w] — no cross-item state — so the sources of a segment can
 * be built concurrently and the result is bit-identical to the serial build
 * (a per-source AWGN seed is deterministic, and the sum is deferred to
 * render()/at() in fixed order). Failures set a shared flag; a partially
 * filled cache_sig[] is freed by wfm_plan_destroy (unset slots stay NULL from
 * calloc). */
typedef struct
{
  wfm_plan_t               *p;
  const wfm_segment_t      *g;
  const wfm_plan_segment_t *ps;
  const size_t             *src_idx; /* g->sources index for work item w    */
  const size_t             *slot;    /* flat cache_sig/base_gain index for w */
  atomic_int               *failed;  /* set once if any work item fails      */
  const plan_loaded_t      *loaded;  /* restore fast-path source (or NULL)   */
} cache_work_t;

static void
cache_build_one (size_t w, void *ctx)
{
  cache_work_t       *cw   = (cache_work_t *)ctx;
  const wfm_source_t *src  = &cw->g->sources[cw->src_idx[w]];
  size_t              slot = cw->slot[w];
  size_t              n    = cw->g->num_samples;

  /* base_gain is a pure function of the source level — always cheap. */
  cw->p->base_gain[slot] = (float)pow (10.0, src->level / 20.0);

  float _Complex *buf = malloc (n * sizeof *buf);
  if (!buf)
    {
      atomic_store (cw->failed, 1);
      return;
    }

  /* Fast path (restore): copy the cached render from a matching blob instead
   * of recomputing it. Guarded per-source by the expected length, so a
   * spec/buffer inconsistency silently falls through to the DSP. */
  const plan_loaded_t *ld = cw->loaded;
  if (ld && slot < ld->n && ld->lens[slot] == n)
    {
      memcpy (buf, ld->bufs[slot], n * sizeof *buf);
      cw->p->cache_sig[slot] = buf;
      return;
    }

  /* Slow path (prepare, or a mismatched restore): run the DSP. */
  double cache_snr = cw->ps->bundled ? WFM_SYNTH_SNR_CLEAN : src->snr;
  wfm_synth_state_t *syn
      = wfm_compose_build_synth (src, cw->g->fs, n, src->freq, cache_snr,
                                 src->f_end, 0, WFM_SEED_ADVANCE_NONE, 0);
  if (!syn)
    {
      free (buf);
      atomic_store (cw->failed, 1);
      return;
    }
  wfm_synth_steps (syn, buf, n);
  wfm_synth_destroy (syn);
  cw->p->cache_sig[slot] = buf;
}

/* Below this ON-time length the per-source build is too cheap for the thread
 * hand-off to pay for itself, so a multi-source segment stays serial. A single
 * non-noise source is always serial regardless (dp_parallel_for is a no-op
 * fan-out at n == 1). */
#define WFM_PLAN_PARALLEL_MIN_SAMPLES 4096u

/* Cache every clean signal source of segment g at gain 1, into
 * p->cache_sig[ps->sig_off .. +ps->n_sig). A bundled source's own real snr
 * is forced clean for this render (its noise is reconstructed separately,
 * per instance, at materialize time); a SHARED segment's non-noise sources
 * are already clean by the time wfm_resolve_noise ran. The per-source builds
 * fan out across cores (auto-sized to the online count, one worker per source
 * at most) — the win for a segment packed with many signals. Returns 0, or -1
 * on allocation/synth failure. */
static int
cache_segment_signals (wfm_plan_t *p, wfm_plan_segment_t *ps,
                       const wfm_segment_t *g, const plan_loaded_t *loaded)
{
  /* Flatten the non-noise sources to work items with their distinct cache
   * slots (serial, O(n_sources), no DSP) so the heavy build can fan out. */
  size_t *src_idx = malloc (g->n_sources * sizeof *src_idx);
  size_t *slot    = malloc (g->n_sources * sizeof *slot);
  if (!src_idx || !slot)
    {
      free (src_idx);
      free (slot);
      return -1;
    }
  size_t nw = 0;
  size_t si = ps->sig_off;
  for (size_t k = 0; k < g->n_sources; k++)
    {
      if (g->sources[k].type == WFM_SYNTH_NOISE)
        continue;
      src_idx[nw] = k;
      slot[nw]    = si;
      nw++;
      si++;
    }

  atomic_int failed;
  atomic_init (&failed, 0);
  cache_work_t cw = { p, g, ps, src_idx, slot, &failed, loaded };
  int          max_threads
      = (nw > 1 && g->num_samples >= WFM_PLAN_PARALLEL_MIN_SAMPLES) ? 0 : 1;
  dp_parallel_for (nw, cache_build_one, &cw, max_threads);

  free (src_idx);
  free (slot);
  return atomic_load (&failed) ? -1 : 0;
}

/* The shared engine behind prepare() and restore(): parse + validate the spec,
 * build every segment descriptor, and fill each source's cache — from @p
 * loaded (the restore fast-path) where the fingerprint and length match, else
 * via the DSP. `loaded == NULL` is a plain prepare(). */
static wfm_plan_t *
plan_build (const char *spec_json, const plan_loaded_t *loaded)
{
  if (!spec_json)
    return NULL;
  wfm_compose_state_t *cs = wfm_compose_from_json (spec_json);
  if (!cs)
    return NULL;

  size_t               n_segs;
  int                  repeat, cont;
  const wfm_segment_t *segs
      = wfm_compose_segments (cs, &n_segs, &repeat, &cont);

  wfm_plan_t *p = NULL;
  if (!segs || n_segs == 0 || repeat || cont)
    goto done; /* unbounded repeat/continuous: no fixed capacity (gh-410) */

  /* Pass 1: per-segment scope validation + total signal-source count. */
  size_t total_sig = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g = &segs[i];
      if (g->num_samples == 0 || g->n_sources == 0)
        goto done;
      if (g->ranged & WFM_RANGE_NUM_SAMPLES)
        goto done; /* a ranged on-time would invalidate the fixed cache */
      if (g->fs != segs[0].fs)
        goto done; /* one global sample rate across the whole scene */
      for (size_t k = 0; k < g->n_sources; k++)
        if (g->sources[k].ranged)
          goto done; /* a ranged source is ambiguous for a static cache */
      size_t n_noise = 0;
      for (size_t k = 0; k < g->n_sources; k++)
        if (g->sources[k].type == WFM_SYNTH_NOISE)
          n_noise++;
      total_sig += g->n_sources - n_noise;
    }
  if (total_sig == 0)
    goto done;

  p = calloc (1, sizeof *p);
  if (!p)
    goto done;
  p->n_segs    = n_segs;
  p->fs        = segs[0].fs;
  p->segs      = calloc (n_segs, sizeof *p->segs);
  p->n_sig     = total_sig;
  p->cache_sig = calloc (total_sig, sizeof *p->cache_sig);
  p->base_gain = calloc (total_sig, sizeof *p->base_gain);
  p->spec_json = strdup (spec_json); /* embedded verbatim by wfm_plan_save */
  if (!p->segs || !p->cache_sig || !p->base_gain || !p->spec_json)
    {
      wfm_plan_destroy (p);
      p = NULL;
      goto done;
    }

  /* Pass 2: build each segment descriptor, cache its clean signal(s), and
   * accumulate the worst-case (ranged-hi) total materialized length. */
  size_t sig_off   = 0;
  size_t worst_len = 0;
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g  = &segs[i];
      wfm_plan_segment_t  *ps = &p->segs[i];

      size_t n_noise = 0;
      for (size_t k = 0; k < g->n_sources; k++)
        if (g->sources[k].type == WFM_SYNTH_NOISE)
          n_noise++;

      ps->num_samples = g->num_samples;
      ps->repeats     = g->repeats ? g->repeats : 1;
      ps->dseed       = g->n_sources ? g->sources[0].seed : 1u;
      ps->ranged
          = g->ranged & (WFM_RANGE_OFF_SAMPLES | WFM_RANGE_DELAY_SAMPLES);
      ps->off_lo    = g->off_samples;
      ps->off_hi    = g->off_samples_hi;
      ps->delay_lo  = g->delay_samples;
      ps->delay_hi  = g->delay_samples_hi;
      ps->gap_noise = g->gap_noise;
      ps->sig_off   = sig_off;
      ps->n_sig     = g->n_sources - n_noise;

      if (resolve_segment_noise (ps, g) != 0
          || cache_segment_signals (p, ps, g, loaded) != 0)
        {
          wfm_plan_destroy (p);
          p = NULL;
          goto done;
        }

      sig_off += ps->n_sig;

      size_t off_worst
          = (ps->ranged & WFM_RANGE_OFF_SAMPLES) ? ps->off_hi : ps->off_lo;
      size_t delay_worst = (ps->ranged & WFM_RANGE_DELAY_SAMPLES)
                               ? ps->delay_hi
                               : ps->delay_lo;
      worst_len += ps->repeats * (delay_worst + ps->num_samples + off_worst);
    }
  p->len = worst_len;

done:
  wfm_compose_destroy (cs);
  return p;
}

wfm_plan_t *
wfm_plan_prepare (const char *spec_json)
{
  return plan_build (spec_json, NULL);
}

size_t
wfm_plan_len (const wfm_plan_t *p)
{
  return p ? p->len : 0;
}

size_t
wfm_plan_n_sources (const wfm_plan_t *p)
{
  return p ? p->n_sig : 0;
}

uint64_t
wfm_plan_anchor_seed (const wfm_plan_t *p)
{
  if (!p)
    return 0;
  for (size_t i = 0; i < p->n_segs; i++)
    if (p->segs[i].has_noise)
      return p->segs[i].noise_src.seed;
  return 0;
}

/* Read an optional per-source double array (gains/phases) from the override
 * object into `dst` (length n_sig). Returns 1 if the key was present and had
 * the right length (dst filled), 0 otherwise (dst untouched → use defaults).
 */
static int
read_dbl_array (const cJSON *root, const char *key, double *dst, size_t n)
{
  const cJSON *a = cJSON_GetObjectItemCaseSensitive (root, key);
  if (!cJSON_IsArray (a) || (size_t)cJSON_GetArraySize (a) != n)
    return 0;
  size_t       i = 0;
  const cJSON *e;
  cJSON_ArrayForEach (e, a)
  {
    if (!cJSON_IsNumber (e))
      return 0;
    dst[i++] = e->valuedouble;
  }
  return 1;
}

size_t
wfm_plan_render (const wfm_plan_t *p, const char *overrides_json,
                 float _Complex *out)
{
  if (!p)
    return 0;
  double   snr = 0.0, seed_d = 0.0;
  int      snr_given = 0, seed_given = 0;
  uint64_t seed = 0;

  double *gains = NULL, *phases = NULL;
  int    *enable = NULL;
  cJSON  *root   = (overrides_json && *overrides_json)
                       ? cJSON_Parse (overrides_json)
                       : NULL;
  if (root)
    {
      const cJSON *s = cJSON_GetObjectItemCaseSensitive (root, "snr");
      if (cJSON_IsNumber (s))
        {
          snr       = s->valuedouble;
          snr_given = 1;
        }
      const cJSON *sd = cJSON_GetObjectItemCaseSensitive (root, "seed");
      if (cJSON_IsNumber (sd))
        {
          seed_d     = sd->valuedouble;
          seed       = (uint64_t)seed_d;
          seed_given = 1;
        }
      double *gbuf = malloc (p->n_sig * sizeof *gbuf);
      double *pbuf = malloc (p->n_sig * sizeof *pbuf);
      if (gbuf && read_dbl_array (root, "gains", gbuf, p->n_sig))
        gains = gbuf;
      else
        free (gbuf);
      if (pbuf && read_dbl_array (root, "phases", pbuf, p->n_sig))
        phases = pbuf;
      else
        free (pbuf);
      const cJSON *en = cJSON_GetObjectItemCaseSensitive (root, "enable");
      if (cJSON_IsArray (en) && (size_t)cJSON_GetArraySize (en) == p->n_sig)
        {
          int *ebuf = malloc (p->n_sig * sizeof *ebuf);
          if (ebuf)
            {
              size_t       i = 0;
              const cJSON *e;
              cJSON_ArrayForEach (e, en) ebuf[i++]
                  = cJSON_IsTrue (e)
                    || (cJSON_IsNumber (e) && e->valuedouble != 0.0);
              enable = ebuf;
            }
        }
    }

  size_t n = materialize (p, gains, phases, enable, snr, snr_given, seed,
                          seed_given, out);
  free (gains);
  free (phases);
  free (enable);
  if (root)
    cJSON_Delete (root);
  return n;
}

size_t
wfm_plan_at (const wfm_plan_t *p, double snr, uint64_t seed,
             float _Complex *out)
{
  if (!p)
    return 0;
  return materialize (p, NULL, NULL, NULL, snr, 1, seed, 1, out);
}

void
wfm_plan_destroy (wfm_plan_t *p)
{
  if (!p)
    return;
  if (p->segs)
    for (size_t i = 0; i < p->n_segs; i++)
      if (p->segs[i].has_noise)
        free_source_arrays (&p->segs[i].noise_src);
  free (p->segs);
  if (p->cache_sig)
    for (size_t k = 0; k < p->n_sig; k++)
      free (p->cache_sig[k]);
  free (p->cache_sig);
  free (p->base_gain);
  free (p->spec_json);
  free (p);
}

/* ── save / restore ──────────────────────────────────────────────────────────
 *
 * Blob layout (native-endian POD, foreign-endian rejected on read):
 *
 *   [0]  magic[4]      = 'P','L','N','0'
 *   [4]  u16 version   = WFM_PLAN_BLOB_VERSION
 *   [6]  u8  endian    = 1 little / 0 big (host, checked on restore)
 *   [7]  u8  reserved  = 0
 *   [8]  u64 dsp_hash  = WFM_PLAN_DSP_HASH_U64 at save time
 *   [16] u64 spec_len  = strlen(spec_json)         (no NUL stored)
 *   [24] spec bytes    [spec_len]
 *        u64 n_sig
 *        per source (n_sig of them): u64 num_samples; cf32 data[num_samples]
 *
 * The spec fully reconstructs the Plan's structure; only the cached sample
 * buffers are payload. On restore the fingerprint gates whether those buffers
 * are trusted (loaded) or rebuilt from the spec via the DSP.
 */
#define WFM_PLAN_BLOB_MAGIC0 'P'
#define WFM_PLAN_BLOB_MAGIC1 'L'
#define WFM_PLAN_BLOB_MAGIC2 'N'
#define WFM_PLAN_BLOB_MAGIC3 '0'
#define WFM_PLAN_BLOB_VERSION 1u

static int
host_is_little (void)
{
  uint16_t one = 1u;
  return *(const uint8_t *)&one == 1u;
}

size_t
wfm_plan_save_bytes (const wfm_plan_t *p)
{
  if (!p)
    return 0;
  size_t n = 24;              /* fixed header */
  n += strlen (p->spec_json); /* spec, no NUL */
  n += sizeof (uint64_t);     /* n_sig */
  for (size_t k = 0; k < p->n_sig; k++)
    {
      /* each source's cached length == its segment's num_samples */
      size_t len = 0;
      for (size_t s = 0; s < p->n_segs; s++)
        if (k >= p->segs[s].sig_off
            && k < p->segs[s].sig_off + p->segs[s].n_sig)
          {
            len = p->segs[s].num_samples;
            break;
          }
      n += sizeof (uint64_t) + len * sizeof (float _Complex);
    }
  return n;
}

/* Length of the cache buffer at flat source index k (its segment's on-time).
 */
static size_t
sig_len (const wfm_plan_t *p, size_t k)
{
  for (size_t s = 0; s < p->n_segs; s++)
    if (k >= p->segs[s].sig_off && k < p->segs[s].sig_off + p->segs[s].n_sig)
      return p->segs[s].num_samples;
  return 0;
}

size_t
wfm_plan_save (const wfm_plan_t *p, void *blob)
{
  if (!p || !blob)
    return 0;
  uint8_t *const start   = (uint8_t *)blob;
  uint8_t       *q       = start;
  size_t         splen   = strlen (p->spec_json);
  uint64_t       hash    = (uint64_t)WFM_PLAN_DSP_HASH_U64;
  uint64_t       n_sig   = (uint64_t)p->n_sig;
  uint64_t       splen64 = (uint64_t)splen;

  q[0]         = WFM_PLAN_BLOB_MAGIC0;
  q[1]         = WFM_PLAN_BLOB_MAGIC1;
  q[2]         = WFM_PLAN_BLOB_MAGIC2;
  q[3]         = WFM_PLAN_BLOB_MAGIC3;
  uint16_t ver = WFM_PLAN_BLOB_VERSION;
  memcpy (q + 4, &ver, 2);
  q[6] = (uint8_t)(host_is_little () ? 1 : 0);
  q[7] = 0;
  memcpy (q + 8, &hash, 8);
  memcpy (q + 16, &splen64, 8);
  q += 24;
  memcpy (q, p->spec_json, splen);
  q += splen;
  memcpy (q, &n_sig, 8);
  q += 8;
  for (size_t k = 0; k < p->n_sig; k++)
    {
      uint64_t len = (uint64_t)sig_len (p, k);
      memcpy (q, &len, 8);
      q += 8;
      memcpy (q, p->cache_sig[k], (size_t)len * sizeof (float _Complex));
      q += (size_t)len * sizeof (float _Complex);
    }
  return (size_t)(q - start);
}

wfm_plan_t *
wfm_plan_restore (const void *blob, size_t n)
{
  if (!blob || n < 24)
    return NULL;
  const uint8_t *q = (const uint8_t *)blob;
  if (q[0] != WFM_PLAN_BLOB_MAGIC0 || q[1] != WFM_PLAN_BLOB_MAGIC1
      || q[2] != WFM_PLAN_BLOB_MAGIC2 || q[3] != WFM_PLAN_BLOB_MAGIC3)
    return NULL;
  uint16_t ver;
  memcpy (&ver, q + 4, 2);
  if (ver != WFM_PLAN_BLOB_VERSION)
    return NULL;
  if (q[6] != (uint8_t)(host_is_little () ? 1 : 0))
    return NULL; /* foreign endian — the buffers are not host-readable */
  uint64_t hash, splen64;
  memcpy (&hash, q + 8, 8);
  memcpy (&splen64, q + 16, 8);
  size_t splen = (size_t)splen64;
  if (24 + splen + 8 > n)
    return NULL;

  /* Embedded spec is NUL-terminated into a scratch copy for the parser. */
  char *spec = malloc (splen + 1);
  if (!spec)
    return NULL;
  memcpy (spec, q + 24, splen);
  spec[splen] = '\0';

  const uint8_t *r = q + 24 + splen;
  uint64_t       n_sig;
  memcpy (&n_sig, r, 8);
  r += 8;

  /* Parse the payload buffer table (lengths + pointers into the blob). Bounds
   * are validated against `n` so a truncated/tampered blob is rejected rather
   * than read out of range; on any doubt we still fall back to a DSP rebuild.
   */
  plan_loaded_t          loaded    = { 0 };
  plan_loaded_t         *use       = NULL;
  size_t                *lens      = NULL;
  const float _Complex **bufs      = NULL;
  int                    fp_ok     = (hash == (uint64_t)WFM_PLAN_DSP_HASH_U64);
  int                    layout_ok = 1;
  if (n_sig > 0 && n_sig < (SIZE_MAX / sizeof (size_t)))
    {
      lens = (size_t *)malloc ((size_t)n_sig * sizeof *lens);
      bufs = (const float _Complex **)malloc ((size_t)n_sig * sizeof *bufs);
      if (!lens || !bufs)
        {
          free (lens);
          free (bufs);
          free (spec);
          return NULL;
        }
      for (uint64_t k = 0; k < n_sig; k++)
        {
          if ((size_t)(r - q) + 8 > n)
            {
              layout_ok = 0;
              break;
            }
          uint64_t len;
          memcpy (&len, r, 8);
          r += 8;
          size_t nbytes = (size_t)len * sizeof (float _Complex);
          if ((size_t)(r - q) + nbytes > n)
            {
              layout_ok = 0;
              break;
            }
          lens[k] = (size_t)len;
          bufs[k] = (const float _Complex *)r;
          r += nbytes;
        }
    }

  /* Trust the buffers only when the DSP fingerprint AND the payload layout are
   * intact; otherwise rebuild from the (authoritative) embedded spec. */
  if (fp_ok && layout_ok && n_sig > 0)
    {
      loaded.n    = (size_t)n_sig;
      loaded.lens = lens;
      loaded.bufs = bufs;
      use         = &loaded;
    }

  wfm_plan_t *p = plan_build (spec, use);
  free (lens);
  free (bufs);
  free (spec);
  return p;
}

int
wfm_plan_dump (const wfm_plan_t *p, const char *path)
{
  if (!p || !path)
    return -1;
  size_t   n    = wfm_plan_save_bytes (p);
  uint8_t *blob = malloc (n);
  if (!blob)
    return -1;
  wfm_plan_save (p, blob);
  FILE *f = fopen (path, "wb");
  if (!f)
    {
      free (blob);
      return -1;
    }
  size_t wrote = fwrite (blob, 1, n, f);
  free (blob);
  /* Always close exactly once; success needs both a full write and a clean
     close (fclose is where a full-disk error finally surfaces). */
  return (fclose (f) == 0 && wrote == n) ? 0 : -1;
}

wfm_plan_t *
wfm_plan_load (const char *path)
{
  if (!path)
    return NULL;
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  if (fseek (f, 0, SEEK_END) != 0)
    {
      fclose (f);
      return NULL;
    }
  long sz = ftell (f);
  if (sz < 0 || fseek (f, 0, SEEK_SET) != 0)
    {
      fclose (f);
      return NULL;
    }
  uint8_t *blob = malloc ((size_t)sz);
  if (!blob)
    {
      fclose (f);
      return NULL;
    }
  size_t got = fread (blob, 1, (size_t)sz, f);
  fclose (f);
  wfm_plan_t *p = (got == (size_t)sz) ? wfm_plan_restore (blob, got) : NULL;
  free (blob);
  return p;
}
