/*
 * wfm_plan.c — component-cache stimulus engine (see wfm_plan.h).
 *
 * prepare() parses + resolves a scene, then renders each signal source ONCE
 * (through the composer's own wfm_compose_build_synth, so a cached render is
 * byte-identical to a full compose) and caches it at gain 1. render()/at()
 * re-materialize a variation as the composer's identical accumulation kernel —
 * Σ g_k·[e^{jφ_k}·]cache_k, left-to-right in source order — then add the noise
 * floor last (rebuilt through the same SSOT with the requested seed, so it is
 * byte-identical to the composer's noise). Baseline (no overrides) reproduces
 * wfm_compose to the bit.
 */
#include "wfm/wfm_plan.h"

#include "awgn/awgn_core.h"
#include "wfm/wfm_compose.h"
#include "wfm_synth/wfm_synth_core.h"

#include "cJSON.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct wfm_plan
{
  size_t           len;       /* L: cached length in samples             */
  size_t           n_sig;     /* number of signal sources (excl. noise)  */
  float _Complex **cache_sig; /* [n_sig] buffers, each L, at gain 1      */
  float           *base_gain; /* [n_sig]: 10^(level_k/20) as resolved    */
  double           fs;        /* sample rate (for the noise rebuild)     */

  int          has_noise;      /* 1 → a trailing NOISE floor is present   */
  wfm_source_t noise_src;      /* the resolved NOISE source (for rebuild) */
  uint32_t     noise_seed;     /* == noise_src.seed (default for at())    */
  int          explicit_floor; /* 1 → floor is fixed (no snr anchor)      */
  double       floor_db;       /* base/explicit floor in dB               */
  /* anchor meta, so a swept SNR recomputes floor = level − snr_over_fs. */
  double anchor_level;
  int    anchor_type, anchor_mode, anchor_sps;
};

/* Accumulate one source into out: real·complex when φ==0 (exactly the
 * composer's expression), else the phase-rotated complex·complex transform. */
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

/* Noise gain 10^(floor/20): the resolved base floor for the baseline, or the
 * anchor-recomputed floor when a SNR is supplied (an explicit floor is fixed —
 * SNR has no anchor to move it). */
static float
noise_gain (const wfm_plan_t *p, double snr, int snr_given)
{
  if (!p->has_noise)
    return 0.0f;
  double fdb = p->floor_db;
  if (snr_given && !p->explicit_floor)
    fdb = p->anchor_level
          - wfm_snr_over_fs (p->anchor_mode, p->anchor_type, p->anchor_sps,
                             snr);
  return (float)pow (10.0, fdb / 20.0);
}

/* The shared kernel behind render() and at(): re-weighted signal sum + noise.
 * gains_db/phases/enable are per-source overrides (NULL → base gain / 0 phase
 * / all enabled). */
static size_t
materialize (const wfm_plan_t *p, const double *gains_db, const double *phases,
             const int *enable, double snr, int snr_given, uint64_t seed,
             float _Complex *out)
{
  size_t len = p->len;
  memset (out, 0, len * sizeof *out);
  for (size_t k = 0; k < p->n_sig; k++)
    {
      float g
          = gains_db ? (float)pow (10.0, gains_db[k] / 20.0) : p->base_gain[k];
      if (enable && !enable[k])
        g = 0.0f;
      double ph = phases ? phases[k] : 0.0;
      accumulate (out, len, g, ph, p->cache_sig[k]);
    }
  /* Noise floor last (the resolve-appended NOISE source is last), rebuilt via
   * the SSOT with the requested seed → byte-identical to the composer. */
  float gn = noise_gain (p, snr, snr_given);
  if (gn != 0.0f)
    {
      wfm_source_t ns = p->noise_src;
      ns.seed         = (uint32_t)seed;
      wfm_synth_state_t *syn
          = wfm_compose_build_synth (&ns, p->fs, len, ns.freq, ns.snr,
                                     ns.f_end, 0, WFM_SEED_ADVANCE_NONE);
      if (syn)
        {
          float _Complex *tmp = malloc (len * sizeof *tmp);
          if (tmp)
            {
              wfm_synth_steps (syn, tmp, len);
              for (size_t i = 0; i < len; i++)
                out[i] += gn * tmp[i];
              free (tmp);
            }
          wfm_synth_destroy (syn);
        }
    }
  return len;
}

wfm_plan_t *
wfm_plan_prepare (const char *spec_json)
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

  /* v1 scope: a single finite, non-continuous, non-repeating segment. */
  wfm_plan_t *p = NULL;
  if (!segs || n_segs != 1 || repeat || cont)
    goto done;
  const wfm_segment_t *g = &segs[0];
  if (g->off_samples != 0 || g->num_samples == 0 || g->ranged)
    goto done; /* no OFF gap / ranged segment in v1 */
  for (size_t k = 0; k < g->n_sources; k++)
    if (g->sources[k].ranged)
      goto done; /* a ranged source is ambiguous for a static Plan */

  /* Count signal sources, locate the (at most one, trailing) noise source. */
  size_t n_noise   = 0;
  int    noise_idx = -1;
  for (size_t k = 0; k < g->n_sources; k++)
    if (g->sources[k].type == WFM_SYNTH_NOISE)
      {
        n_noise++;
        noise_idx = (int)k;
      }
  if (n_noise > 1 || (noise_idx >= 0 && (size_t)noise_idx != g->n_sources - 1))
    goto done; /* v1: at most one noise source, and it must be last */
  size_t n_sig = g->n_sources - n_noise;
  if (n_sig == 0)
    goto done;
  /* A lone bundled noisy source is not separable (its private RNG is fused).
   */
  if (g->n_sources == 1 && g->sources[0].type != WFM_SYNTH_NOISE
      && g->sources[0].snr < WFM_SYNTH_SNR_CLEAN)
    goto done;

  p = calloc (1, sizeof *p);
  if (!p)
    goto done;
  p->len       = g->num_samples;
  p->n_sig     = n_sig;
  p->fs        = g->fs;
  p->cache_sig = calloc (n_sig, sizeof *p->cache_sig);
  p->base_gain = calloc (n_sig, sizeof *p->base_gain);
  if (!p->cache_sig || !p->base_gain)
    {
      wfm_plan_destroy (p);
      p = NULL;
      goto done;
    }

  size_t si = 0;
  for (size_t k = 0; k < g->n_sources; k++)
    {
      const wfm_source_t *src = &g->sources[k];
      if (src->type == WFM_SYNTH_NOISE)
        {
          p->has_noise  = 1;
          p->noise_src  = *src;
          p->noise_seed = src->seed;
          p->floor_db   = src->level;
          /* The anchor is the signal source whose seed the floor inherited
           * (wfm_resolve_noise sets noise.seed = anchor.seed). Found → SNR is
           * anchor-relative; not found → an explicit fixed floor. */
          p->explicit_floor = 1;
          for (size_t j = 0; j < g->n_sources; j++)
            if (g->sources[j].type != WFM_SYNTH_NOISE
                && g->sources[j].seed == src->seed)
              {
                p->explicit_floor = 0;
                p->anchor_level   = g->sources[j].level;
                p->anchor_type    = g->sources[j].type;
                p->anchor_mode    = g->sources[j].snr_mode;
                p->anchor_sps     = g->sources[j].sps;
                break;
              }
          continue;
        }
      /* Signal source: render once through the SSOT and cache at gain 1. */
      wfm_synth_state_t *syn
          = wfm_compose_build_synth (src, g->fs, p->len, src->freq, src->snr,
                                     src->f_end, 0, WFM_SEED_ADVANCE_NONE);
      float _Complex *buf = syn ? malloc (p->len * sizeof *buf) : NULL;
      if (!syn || !buf)
        {
          free (buf);
          if (syn)
            wfm_synth_destroy (syn);
          wfm_plan_destroy (p);
          p = NULL;
          goto done;
        }
      wfm_synth_steps (syn, buf, p->len);
      wfm_synth_destroy (syn);
      p->cache_sig[si] = buf;
      p->base_gain[si] = (float)pow (10.0, src->level / 20.0);
      si++;
    }

done:
  wfm_compose_destroy (cs);
  return p;
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
  return (p && p->has_noise) ? p->noise_seed : 0;
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
  int      snr_given = 0;
  uint64_t seed      = p->noise_seed;

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
          seed_d = sd->valuedouble;
          seed   = (uint64_t)seed_d;
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

  size_t n = materialize (p, gains, phases, enable, snr, snr_given, seed, out);
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
  return materialize (p, NULL, NULL, NULL, snr, 1, seed, out);
}

void
wfm_plan_destroy (wfm_plan_t *p)
{
  if (!p)
    return;
  if (p->cache_sig)
    for (size_t k = 0; k < p->n_sig; k++)
      free (p->cache_sig[k]);
  free (p->cache_sig);
  free (p->base_gain);
  free (p);
}
