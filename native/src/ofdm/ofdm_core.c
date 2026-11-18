/**
 * @file ofdm_core.c
 * @brief The general OFDM modulator. The contract is in ofdm_core.h.
 */
#include "doppler/ofdm/ofdm_core.h"
#include "doppler/dp_hash64.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── small helpers ──────────────────────────────────────────────────────── */

static uint32_t
cls_useful (const ofdm_class_t *c)
{
  return c->useful ? c->useful : c->n_fft;
}

/* The cyclic prefix a run uses: its own override, or its class's. */
static uint32_t
run_cp (const dp_ofdm_state_t *o, const ofdm_run_t *r)
{
  return r->cyclic_prefix == OFDM_CP_DEFAULT
             ? o->cls[r->class_index].cyclic_prefix
             : r->cyclic_prefix;
}

static uint32_t
run_sym_len (const dp_ofdm_state_t *o, const ofdm_run_t *r)
{
  return run_cp (o, r) + cls_useful (&o->cls[r->class_index]);
}

/* Transform input position of subcarrier k. */
static uint32_t
bin_of (int32_t k, uint32_t n)
{
  return k < 0 ? (uint32_t)(k + (int32_t)n) : (uint32_t)k;
}

static int
fixed_applies (const ofdm_fixed_t *e, uint32_t phase)
{
  return e->phase == phase || e->phase == OFDM_ALL_PHASES;
}

/* Free what dp_ofdm_check() derived; the layout itself stays, and so does the
   held window tail, which is state and not derived. */
static void
free_derived (dp_ofdm_state_t *o)
{
  for (uint32_t i = 0; i < o->n_cls; i++)
    {
      free (o->cls[i].n_data);
      free (o->cls[i].rise);
      o->cls[i].n_data = NULL;
      o->cls[i].rise   = NULL;
    }
  for (uint32_t i = 0; i < o->n_plans; i++)
    dp_fft_destroy (o->plans[i]);
  free (o->plans);
  free (o->plan_of);
  free (o->grid);
  free (o->xform);
  free (o->taken);
  o->plans   = NULL;
  o->plan_of = NULL;
  o->grid    = NULL;
  o->xform   = NULL;
  o->taken   = NULL;
  o->n_plans = 0;
  o->sealed  = 0;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

dp_ofdm_state_t *
dp_ofdm_create (void)
{
  return dp_xcalloc (1, sizeof (dp_ofdm_state_t));
}

void
dp_ofdm_destroy (dp_ofdm_state_t *o)
{
  if (!o)
    return;
  free_derived (o);
  free (o->tail);
  for (uint32_t i = 0; i < o->n_cls; i++)
    {
      free (o->cls[i].subcarrier_map);
      free (o->cls[i].subcarrier_gain);
      free (o->cls[i].fixed);
      free (o->cls[i].polarity);
    }
  free (o->cls);
  free (o->runs);
  free (o);
}

void
dp_ofdm_reset (dp_ofdm_state_t *o)
{
  o->run_index  = 0;
  o->sym_in_run = 0;
  o->tail_len   = 0;
  if (o->tail)
    memset (o->tail, 0, (size_t)o->max_window * sizeof *o->tail);
}

/* Adding to a sealed layout re-opens it: the derived tables are dropped and
   the next dp_ofdm_check() rebuilds them over the longer layout. The position
   and the held tail stay, so a signal can be extended while it is rendered. */
static void
reopen (dp_ofdm_state_t *o)
{
  if (o->sealed)
    free_derived (o);
}

/* ── building the layout ────────────────────────────────────────────────── */

int
dp_ofdm_add_class (dp_ofdm_state_t *o, uint32_t n_fft,
                   const uint8_t *subcarrier_map, size_t subcarrier_map_len,
                   uint32_t useful, uint32_t cyclic_prefix, uint32_t window,
                   double scale, uint32_t n_phase)
{
  if (!o || n_fft < 2 || n_fft > OFDM_N_FFT_MAX || subcarrier_map_len != n_fft
      || !subcarrier_map || n_phase == 0 || n_phase > OFDM_N_PHASE_MAX
      || o->n_cls >= OFDM_COUNT_MAX || !(scale == scale) /* NaN */)
    return -1;
  const uint32_t u = useful ? useful : n_fft;
  if (cyclic_prefix > u || window > cyclic_prefix + u)
    return -1;
  for (uint32_t i = 0; i < n_fft; i++)
    if (subcarrier_map[i] > OFDM_DATA)
      return -1;

  reopen (o);
  if (o->n_cls == o->cap_cls)
    {
      uint32_t      cap = o->cap_cls ? 2 * o->cap_cls : 4;
      ofdm_class_t *p   = dp_reallocn (o->cls, cap, sizeof *p);
      if (!p)
        return -1;
      o->cls     = p;
      o->cap_cls = cap;
    }
  ofdm_class_t c;
  memset (&c, 0, sizeof c);
  c.n_fft          = n_fft;
  c.useful         = useful;
  c.cyclic_prefix  = cyclic_prefix;
  c.window         = window;
  c.scale          = scale;
  c.n_phase        = n_phase;
  c.subcarrier_map = dp_xmalloc (n_fft); /* n_fft is bounded above */
  memcpy (c.subcarrier_map, subcarrier_map, n_fft);

  o->cls[o->n_cls] = c;
  if (window > o->max_window)
    o->max_window = window;
  return (int)o->n_cls++;
}

int
dp_ofdm_add_subcarrier_gain (dp_ofdm_state_t *o, uint32_t class_index,
                             const float _Complex *gain, size_t gain_len)
{
  if (!o || class_index >= o->n_cls || !gain)
    return -1;
  ofdm_class_t *c = &o->cls[class_index];
  if (gain_len != c->n_fft || c->subcarrier_gain)
    return -1;
  reopen (o);
  c->subcarrier_gain = dp_mallocn (gain_len, sizeof *gain);
  if (!c->subcarrier_gain)
    return -1;
  memcpy (c->subcarrier_gain, gain, gain_len * sizeof *gain);
  return 0;
}

int
dp_ofdm_add_polarity (dp_ofdm_state_t *o, uint32_t class_index,
                      const float _Complex *polarity, size_t polarity_len)
{
  if (!o || class_index >= o->n_cls || !polarity)
    return -1;
  ofdm_class_t *c = &o->cls[class_index];
  if (polarity_len == 0 || polarity_len > OFDM_COUNT_MAX || c->polarity)
    return -1;
  reopen (o);
  c->polarity = dp_mallocn (polarity_len, sizeof *polarity);
  if (!c->polarity)
    return -1;
  memcpy (c->polarity, polarity, polarity_len * sizeof *polarity);
  c->n_polarity = (uint32_t)polarity_len;
  return 0;
}

int
dp_ofdm_add_fixed (dp_ofdm_state_t *o, uint32_t class_index, uint32_t phase,
                   int32_t k, double _Complex value)
{
  if (!o || class_index >= o->n_cls)
    return -1;
  ofdm_class_t *c = &o->cls[class_index];
  if ((phase != OFDM_ALL_PHASES && phase >= c->n_phase)
      || k < -(int32_t)(c->n_fft / 2)
      || k > (int32_t)(c->n_fft - c->n_fft / 2) - 1
      || c->n_fixed >= OFDM_COUNT_MAX)
    return -1;
  reopen (o);
  if (c->n_fixed == c->fixed_cap)
    {
      uint32_t      cap = c->fixed_cap ? 2 * c->fixed_cap : 8;
      ofdm_fixed_t *p   = dp_reallocn (c->fixed, cap, sizeof *p);
      if (!p)
        return -1;
      c->fixed     = p;
      c->fixed_cap = cap;
    }
  /* Kept sorted by (phase, k): insert after the entries that precede it. */
  uint32_t at = c->n_fixed;
  while (at > 0)
    {
      const ofdm_fixed_t *q = &c->fixed[at - 1];
      if (q->phase < phase || (q->phase == phase && q->k <= k))
        break;
      at--;
    }
  memmove (&c->fixed[at + 1], &c->fixed[at],
           (size_t)(c->n_fixed - at) * sizeof *c->fixed);
  c->fixed[at].phase = phase;
  c->fixed[at].k     = k;
  c->fixed[at].value = (float _Complex)value;
  c->n_fixed++;
  return 0;
}

int
dp_ofdm_add_run (dp_ofdm_state_t *o, uint32_t class_index, uint32_t count,
                 uint32_t cyclic_prefix, uint32_t polarity_offset,
                 uint32_t first_phase)
{
  if (!o || class_index >= o->n_cls || count == 0
      || o->n_runs >= OFDM_COUNT_MAX)
    return -1;
  const ofdm_class_t *c = &o->cls[class_index];
  if (first_phase >= c->n_phase)
    return -1;
  if (cyclic_prefix != OFDM_CP_DEFAULT && cyclic_prefix > cls_useful (c))
    return -1;
  reopen (o);
  if (o->n_runs == o->cap_runs)
    {
      uint32_t    cap = o->cap_runs ? 2 * o->cap_runs : 4;
      ofdm_run_t *p   = dp_reallocn (o->runs, cap, sizeof *p);
      if (!p)
        return -1;
      o->runs     = p;
      o->cap_runs = cap;
    }
  ofdm_run_t *r      = &o->runs[o->n_runs];
  r->class_index     = class_index;
  r->count           = count;
  r->cyclic_prefix   = cyclic_prefix;
  r->polarity_offset = polarity_offset;
  r->first_phase     = first_phase;
  return (int)o->n_runs++;
}

/* ── sealing ────────────────────────────────────────────────────────────── */

int
dp_ofdm_check_why (dp_ofdm_state_t *o, const char **why)
{
  /* The sentence for the condition that failed; every `return -1` and
     `goto out` below sets it first, and the one left standing at `out` is
     the allocation (or transform plan) that could not be built. */
  const char *w = "memory or a transform plan could not be built";
  if (!o)
    {
      if (why)
        *why = "there is no modulator";
      return -1;
    }
  if (o->sealed)
    return 0;
  if (o->n_runs == 0)
    {
      if (why)
        *why = "the layout has no run";
      return -1;
    }

  /* A window longer than an OFDM symbol of its class cannot be a transition
     inside it. Checked per run, because the prefix is per run. */
  for (uint32_t i = 0; i < o->n_runs; i++)
    {
      const ofdm_run_t *r = &o->runs[i];
      if (o->cls[r->class_index].window > run_sym_len (o, r))
        {
          if (why)
            *why = "a run's window is longer than an OFDM symbol of its class";
          return -1;
        }
    }

  uint32_t n_max = 0;
  for (uint32_t i = 0; i < o->n_cls; i++)
    if (o->cls[i].n_fft > n_max)
      n_max = o->cls[i].n_fft;

  int rc        = -1;
  o->plans      = dp_xcalloc (o->n_cls, sizeof *o->plans);
  o->plan_of    = dp_xcalloc (o->n_cls, sizeof *o->plan_of);
  o->grid       = dp_mallocn (n_max, sizeof *o->grid);
  o->xform      = dp_mallocn (n_max, sizeof *o->xform);
  o->taken      = dp_xmalloc (n_max);
  uint8_t *mark = dp_xmalloc (n_max);
  if (!o->grid || !o->xform)
    goto out;
  if (o->tail_cap < o->max_window || !o->tail)
    {
      /* grown in place: the held tail is kept, the new slots are zero */
      float _Complex *t
          = dp_xcalloc (o->max_window ? o->max_window : 1, sizeof *t);
      if (o->tail)
        memcpy (t, o->tail, (size_t)o->tail_cap * sizeof *t);
      free (o->tail);
      o->tail     = t;
      o->tail_cap = o->max_window;
    }

  for (uint32_t i = 0; i < o->n_cls; i++)
    {
      ofdm_class_t  *c = &o->cls[i];
      const uint32_t n = c->n_fft;

      /* Data points per pattern phase: the data subcarriers, less those a
         fixed entry takes. Two entries on one subcarrier in one phase would
         make the grid ambiguous, so they are refused here. */
      c->n_data = dp_mallocn (c->n_phase, sizeof *c->n_data);
      if (!c->n_data)
        goto out;
      uint32_t n_map = 0;
      for (uint32_t j = 0; j < n; j++)
        n_map += c->subcarrier_map[j];
      for (uint32_t p = 0; p < c->n_phase; p++)
        {
          memset (mark, 0, n);
          uint32_t displaced = 0;
          for (uint32_t e = 0; e < c->n_fixed; e++)
            {
              const ofdm_fixed_t *f = &c->fixed[e];
              if (!fixed_applies (f, p))
                continue;
              const uint32_t idx = (uint32_t)(f->k + (int32_t)(n / 2));
              if (mark[idx])
                {
                  w = "two fixed entries are on one subcarrier in one phase";
                  goto out;
                }
              mark[idx] = 1;
              displaced += c->subcarrier_map[idx];
            }
          c->n_data[p] = n_map - displaced;
        }

      /* The raised-cosine rise; the fall is its complement. */
      if (c->window)
        {
          c->rise = dp_mallocn (c->window, sizeof *c->rise);
          if (!c->rise)
            goto out;
          for (uint32_t j = 0; j < c->window; j++)
            {
              const double s
                  = sin (M_PI * ((double)j + 0.5) / (2.0 * c->window));
              c->rise[j] = (float)(s * s);
            }
        }

      /* One inverse transform per distinct length. */
      uint32_t found = o->n_plans;
      for (uint32_t j = 0; j < i; j++)
        if (o->cls[j].n_fft == n)
          {
            found = o->plan_of[j];
            break;
          }
      if (found == o->n_plans)
        {
          o->plans[o->n_plans] = dp_fft_create (n, +1, 1);
          if (!o->plans[o->n_plans])
            goto out;
          o->n_plans++;
        }
      o->plan_of[i] = found;
    }
  o->sealed = 1;
  rc        = 0;
out:
  free (mark);
  if (rc != 0)
    {
      free_derived (o);
      if (why)
        *why = w;
    }
  return rc;
}

int
dp_ofdm_check (dp_ofdm_state_t *o)
{
  return dp_ofdm_check_why (o, NULL);
}

/* ── walking the layout ─────────────────────────────────────────────────── */

/* The window used at the boundary after the symbol at (ri, si): the next
   symbol's class's, or, after the last symbol, its own. */
static const ofdm_class_t *
boundary_class (const dp_ofdm_state_t *o, uint32_t ri, uint32_t si)
{
  const ofdm_run_t *r = &o->runs[ri];
  if (si + 1 < r->count)
    return &o->cls[r->class_index];
  if (ri + 1 < o->n_runs)
    return &o->cls[o->runs[ri + 1].class_index];
  return &o->cls[r->class_index];
}

static uint32_t
points_of (const dp_ofdm_state_t *o, uint32_t ri, uint32_t si)
{
  const ofdm_run_t   *r = &o->runs[ri];
  const ofdm_class_t *c = &o->cls[r->class_index];
  return c->n_data[(r->first_phase + si) % c->n_phase];
}

static void
advance (const dp_ofdm_state_t *o, uint32_t *ri, uint32_t *si)
{
  if (*si + 1 < o->runs[*ri].count)
    (*si)++;
  else
    {
      (*ri)++;
      *si = 0;
    }
}

/* Samples a call with n_in points writes. A refusal (it ends inside an OFDM
   symbol, or runs past the layout) returns 0 and sets *why, which is left as
   the caller set it otherwise: 0 samples is also a valid answer, for no
   points at the end of the layout, or when the next symbol needs some.
   *n_syms is the OFDM symbols. */
static size_t
walk (const dp_ofdm_state_t *o, size_t n_in, size_t *n_syms, const char **why)
{
  uint32_t ri = o->run_index, si = o->sym_in_run;
  size_t   left = n_in, total = 0, syms = 0;
  while (ri < o->n_runs)
    {
      const size_t need = points_of (o, ri, si);
      if (need > left)
        {
          if (left > 0)
            {
              if (why)
                *why = "the points end inside an OFDM symbol: pass a "
                       "count that ends where one does (see "
                       "points_remaining and modulate_max_out)";
              return 0;
            }
          break;
        }
      left -= need;
      total += run_sym_len (o, &o->runs[ri]);
      syms++;
      advance (o, &ri, &si);
    }
  if (left > 0)
    {
      if (why)
        *why = "more points than the layout has left";
      return 0;
    }
  if (n_syms)
    *n_syms = syms;
  return total;
}

size_t
dp_ofdm_points_remaining (dp_ofdm_state_t *o)
{
  if (!o || dp_ofdm_check (o) != 0)
    return 0;
  uint32_t ri = o->run_index, si = o->sym_in_run;
  size_t   total = 0;
  while (ri < o->n_runs)
    {
      total += points_of (o, ri, si);
      advance (o, &ri, &si);
    }
  return total;
}

size_t
dp_ofdm_modulate_max_out (dp_ofdm_state_t *o, size_t n_in)
{
  if (!o || dp_ofdm_check (o) != 0)
    return 0;
  return walk (o, n_in, NULL, NULL);
}

size_t
dp_ofdm_modulate_why_max_out (dp_ofdm_state_t *o, size_t n_in)
{
  return dp_ofdm_modulate_max_out (o, n_in);
}

/* ── one OFDM symbol ────────────────────────────────────────────────────── */

/* Render the symbol at (ri, si) into out, consuming its points, and leave the
   window tail for the symbol after it. Returns the points consumed. */
static size_t
render_symbol (dp_ofdm_state_t *o, uint32_t ri, uint32_t si,
               const float _Complex *pts, float _Complex *out)
{
  const ofdm_run_t   *r = &o->runs[ri];
  const ofdm_class_t *c = &o->cls[r->class_index];
  const uint32_t      n = c->n_fft, u = cls_useful (c), cp = run_cp (o, r);
  const uint32_t      len   = cp + u;
  const uint32_t      phase = (r->first_phase + si) % c->n_phase;
  const float _Complex pv
      = c->n_polarity ? c->polarity[(r->polarity_offset + si) % c->n_polarity]
                      : 1.0f;

  memset (o->grid, 0, (size_t)n * sizeof *o->grid);
  memset (o->taken, 0, n);
  for (uint32_t e = 0; e < c->n_fixed; e++)
    {
      const ofdm_fixed_t *f = &c->fixed[e];
      if (!fixed_applies (f, phase))
        continue;
      o->grid[bin_of (f->k, n)]                     = f->value * pv;
      o->taken[(uint32_t)(f->k + (int32_t)(n / 2))] = 1;
    }
  size_t used = 0;
  for (uint32_t idx = 0; idx < n; idx++)
    if (c->subcarrier_map[idx] && !o->taken[idx])
      o->grid[bin_of ((int32_t)idx - (int32_t)(n / 2), n)] = pts[used++];
  if (c->subcarrier_gain)
    for (uint32_t idx = 0; idx < n; idx++)
      o->grid[bin_of ((int32_t)idx - (int32_t)(n / 2), n)]
          *= c->subcarrier_gain[idx];

  dp_fft_execute_cf32 (o->plans[o->plan_of[r->class_index]], o->grid, n,
                       o->xform, n);

  const float sc = (float)c->scale;
  for (uint32_t i = 0; i < len; i++)
    {
      const uint32_t b = i < cp ? u - cp + i : i - cp;
      float _Complex v = o->xform[b % n] * sc;
      if (i < c->window)
        v *= c->rise[i];
      if (i < o->tail_len)
        v += o->tail[i];
      out[i] = v;
    }

  /* The continuation of this symbol, faded out over the next boundary. */
  const ofdm_class_t *nc = boundary_class (o, ri, si);
  for (uint32_t j = 0; j < nc->window; j++)
    o->tail[j] = o->xform[(u + j) % n] * sc * (1.0f - nc->rise[j]);
  o->tail_len = nc->window;
  return used;
}

int64_t
dp_ofdm_modulate_why (dp_ofdm_state_t *o, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out,
                      const char **why)
{
  const char *w = NULL; /* set only by a refusal; *why is written only then */
  if (dp_ofdm_check_why (o, &w) != 0)
    {
      if (why)
        *why = w;
      return -1;
    }
  size_t       n_syms = 0;
  const size_t total  = walk (o, n_in, &n_syms, &w);
  if (w)
    {
      if (why)
        *why = w;
      return -1;
    }
  if (max_out < total)
    {
      if (why)
        *why = "the output buffer is shorter than the samples this call "
               "writes (see modulate_max_out)";
      return -1;
    }
  if (total == 0)
    return 0;

  size_t pi = 0, po = 0;
  for (size_t s = 0; s < n_syms; s++)
    {
      pi += render_symbol (o, o->run_index, o->sym_in_run, in + pi, out + po);
      po += run_sym_len (o, &o->runs[o->run_index]);
      advance (o, &o->run_index, &o->sym_in_run);
    }
  return (int64_t)po;
}

size_t
dp_ofdm_modulate (dp_ofdm_state_t *o, const float _Complex *in, size_t n_in,
                  float _Complex *out, size_t max_out)
{
  const int64_t n = dp_ofdm_modulate_why (o, in, n_in, out, max_out, NULL);
  return n < 0 ? 0 : (size_t)n;
}

size_t
dp_ofdm_finish_max_out (dp_ofdm_state_t *o, size_t n)
{
  if (!o)
    return 0;
  return o->tail_len < n ? o->tail_len : n;
}

size_t
dp_ofdm_finish (dp_ofdm_state_t *o, size_t n, float _Complex *out,
                size_t max_out)
{
  if (!o || !o->tail_len)
    return 0;
  size_t k = o->tail_len < n ? o->tail_len : n;
  if (k > max_out)
    k = max_out;
  memcpy (out, o->tail, k * sizeof *out);
  o->tail_len = 0;
  memset (o->tail, 0, (size_t)o->max_window * sizeof *o->tail);
  return k;
}

/* ── state ──────────────────────────────────────────────────────────────── */

typedef struct
{
  uint64_t layout_hash;
  uint32_t run_index;
  uint32_t sym_in_run;
  uint32_t tail_len;
  uint32_t max_window;
} ofdm_extra_t;

static uint64_t
hash_u32 (uint64_t h, uint32_t v)
{
  return dp_hash64 (h, &v, sizeof v);
}

/* The layout as bytes, canonically: the reject key. Pointer-free, so the same
   layout built on another machine hashes alike. */
static uint64_t
layout_hash (const dp_ofdm_state_t *o)
{
  uint64_t h = DP_HASH64_INIT;
  h          = hash_u32 (h, o->n_cls);
  for (uint32_t i = 0; i < o->n_cls; i++)
    {
      const ofdm_class_t *c = &o->cls[i];
      h                     = hash_u32 (h, c->n_fft);
      h                     = hash_u32 (h, c->useful);
      h                     = hash_u32 (h, c->cyclic_prefix);
      h                     = hash_u32 (h, c->window);
      h                     = dp_hash64 (h, &c->scale, sizeof c->scale);
      h                     = hash_u32 (h, c->n_phase);
      h                     = dp_hash64 (h, c->subcarrier_map, c->n_fft);
      h                     = hash_u32 (h, c->subcarrier_gain ? 1u : 0u);
      if (c->subcarrier_gain)
        h = dp_hash64 (h, c->subcarrier_gain,
                       (size_t)c->n_fft * sizeof *c->subcarrier_gain);
      h = hash_u32 (h, c->n_fixed);
      for (uint32_t e = 0; e < c->n_fixed; e++)
        {
          h = hash_u32 (h, c->fixed[e].phase);
          h = hash_u32 (h, (uint32_t)c->fixed[e].k);
          h = dp_hash64 (h, &c->fixed[e].value, sizeof c->fixed[e].value);
        }
      h = hash_u32 (h, c->n_polarity);
      if (c->n_polarity)
        h = dp_hash64 (h, c->polarity,
                       (size_t)c->n_polarity * sizeof *c->polarity);
    }
  h = hash_u32 (h, o->n_runs);
  for (uint32_t i = 0; i < o->n_runs; i++)
    {
      h = hash_u32 (h, o->runs[i].class_index);
      h = hash_u32 (h, o->runs[i].count);
      h = hash_u32 (h, o->runs[i].cyclic_prefix);
      h = hash_u32 (h, o->runs[i].polarity_offset);
      h = hash_u32 (h, o->runs[i].first_phase);
    }
  return h;
}

size_t
dp_ofdm_state_bytes (const dp_ofdm_state_t *o)
{
  return sizeof (dp_state_hdr_t) + sizeof (ofdm_extra_t)
         + (size_t)o->max_window * sizeof (float _Complex);
}

void
dp_ofdm_get_state (const dp_ofdm_state_t *o, void *blob)
{
  DP_GET_OPEN (OFDM_STATE_MAGIC, OFDM_STATE_VERSION, dp_ofdm_state_bytes (o));
  ofdm_extra_t x;
  memset (&x, 0, sizeof x);
  x.layout_hash = layout_hash (o);
  x.run_index   = o->run_index;
  x.sym_in_run  = o->sym_in_run;
  x.tail_len    = o->tail_len;
  x.max_window  = o->max_window;
  dp_w_bytes (&_w, &x, sizeof x);
  /* Always max_window slots, so the size is a function of the layout; the
     slots past tail_len are zero. */
  for (uint32_t j = 0; j < o->max_window; j++)
    {
      const float _Complex v = (o->tail && j < o->tail_len) ? o->tail[j] : 0;
      dp_w_cf32 (&_w, &v, 1);
    }
}

int
dp_ofdm_set_state (dp_ofdm_state_t *o, const void *blob)
{
  /* Sealing is idempotent and checks the layout; a layout that cannot seal
     has no position to restore into. */
  if (dp_ofdm_check (o) != 0)
    return DP_ERR_INVALID;
  DP_SET_OPEN (OFDM_STATE_MAGIC, OFDM_STATE_VERSION, dp_ofdm_state_bytes (o));

  ofdm_extra_t x;
  dp_r_bytes (&_r, &x, sizeof x);
  if (x.layout_hash != layout_hash (o) || x.max_window != o->max_window)
    return DP_ERR_INVALID;

  /* The position must name an OFDM symbol (or the end), and the held tail
     must be the one that position implies: the window of the boundary class,
     or nothing (before the first symbol, or after finish()). */
  uint32_t want = 0;
  if (x.run_index > o->n_runs)
    return DP_ERR_INVALID;
  if (x.run_index == o->n_runs)
    {
      if (x.sym_in_run != 0)
        return DP_ERR_INVALID;
      if (o->n_runs)
        {
          const ofdm_run_t *last = &o->runs[o->n_runs - 1];
          want                   = o->cls[last->class_index].window;
        }
    }
  else
    {
      if (x.sym_in_run >= o->runs[x.run_index].count)
        return DP_ERR_INVALID;
      if (x.run_index || x.sym_in_run)
        {
          /* The symbol before the position. */
          uint32_t pr = x.run_index, ps = x.sym_in_run;
          if (ps)
            ps--;
          else
            {
              pr--;
              ps = o->runs[pr].count - 1;
            }
          want = boundary_class (o, pr, ps)->window;
        }
    }
  if (x.tail_len != 0 && x.tail_len != want)
    return DP_ERR_INVALID;

  float _Complex tmp_tail[1];
  float _Complex *tail
      = o->max_window ? dp_mallocn (o->max_window, sizeof *tail) : tmp_tail;
  if (!tail)
    return DP_ERR_INVALID;
  /* This read cannot overrun: the envelope check above fixed the blob at
     exactly dp_ofdm_state_bytes() (header + extra + max_window slots), the
     extra was read first, and x.max_window == o->max_window was just
     checked, so the slots are exactly what is left. */
  dp_r_cf32 (&_r, tail, o->max_window);
  o->run_index  = x.run_index;
  o->sym_in_run = x.sym_in_run;
  o->tail_len   = x.tail_len;
  if (o->max_window)
    {
      memcpy (o->tail, tail, (size_t)o->max_window * sizeof *tail);
      free (tail);
    }
  return DP_OK;
}
