/**
 * @file async_dsss_pool_core.c
 * @brief AsyncDsssPool: N receivers, one controller. See
 *        async_dsss_pool_core.h for the rules and
 *        docs/design/async-dsss-parallel.md §4 for why.
 */
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What `threads` runs as: the online cores for 0, and never more workers
   than receivers, since push() hands out one receiver per task. */
static int
effective_threads (int threads, size_t n_receivers)
{
  long t = threads;
  if (t == 0)
    {
      t = dp_cpu_count ();
      if (t < 1)
        t = 1;
    }
  if ((size_t)t > n_receivers)
    t = (long)n_receivers;
  return (int)t;
}

/* Rebuild the workers for `threads`. One worker is serial: no helpers,
   and dp_pool_run() runs every task inline on the caller. */
static void
build_workers (dp_async_dsss_pool_state_t *s, int threads)
{
  dp_pool_destroy (s->workers);
  s->threads    = threads;
  const int eff = effective_threads (threads, s->n_receivers);
  s->workers    = eff > 1 ? dp_pool_create (eff) : NULL;
}

dp_async_dsss_pool_state_t *
dp_async_dsss_pool_create (
    const uint8_t *code, size_t code_len, size_t n_receivers, double chip_rate,
    double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
    double pd, double doppler_uncertainty, size_t segments, size_t sps,
    int differential, double refine_max_error_db,
    size_t refine_samples_per_symbol, double refine_design_margin_db,
    size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
    size_t refine_max_n_blocks, double carrier_freq_hz, double lost_confirm_s,
    bool reacquire, double code_only_s, double doppler_rate, int threads)
{
  if (n_receivers == 0 || n_receivers > ASYNC_DSSS_POOL_MAX_RECEIVERS
      || threads < 0)
    return NULL;
  /* Fixed sizes from validated arguments: abort-on-OOM, no unwind path. */
  dp_async_dsss_pool_state_t *s = dp_xcalloc (1, sizeof *s);
  s->n_receivers                = n_receivers;
  s->rx                         = dp_xcalloc (n_receivers, sizeof *s->rx);
  s->n_sym                      = dp_xcalloc (n_receivers, sizeof *s->n_sym);
  for (size_t i = 0; i < n_receivers; i++)
    {
      /* The receiver validates every parameter it takes; the first refusal
         refuses the pool. */
      s->rx[i] = dp_async_dsss_receiver_create (
          code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz, pfa, pd,
          doppler_uncertainty, segments, sps, differential,
          refine_max_error_db, refine_samples_per_symbol,
          refine_design_margin_db, refine_n_fft, refine_zero_pad,
          refine_sequential, refine_max_n_blocks, carrier_freq_hz,
          lost_confirm_s, reacquire, code_only_s, doppler_rate);
      if (!s->rx[i])
        {
          dp_async_dsss_pool_destroy (s);
          return NULL;
        }
      /* N engines each starting a pool of every core would oversubscribe
         N-fold: the parallelism is across receivers instead. */
      (void)dp_async_dsss_receiver_set_threads (s->rx[i], 1);
    }
  /* A call that completes a carried code period emits that period's
     symbols beyond its input's own (dp_async_dsss_receiver_steps()), and
     its input's own are at most one a sample (a symbol spans at least one
     chip of at least one sample). */
  s->sym_extra = (size_t)ceil ((double)code_len * symbol_rate / chip_rate) + 1;
  s->period    = code_len * spc;
  build_workers (s, threads);
  return s;
}

void
dp_async_dsss_pool_destroy (dp_async_dsss_pool_state_t *s)
{
  if (!s)
    return;
  dp_pool_destroy (s->workers);
  if (s->rx)
    for (size_t i = 0; i < s->n_receivers; i++)
      dp_async_dsss_receiver_destroy (s->rx[i]);
  free (s->rx);
  free (s->n_sym);
  free (s->sym_buf);
  free (s);
}

void
dp_async_dsss_pool_reset (dp_async_dsss_pool_state_t *s)
{
  for (size_t i = 0; i < s->n_receivers; i++)
    {
      dp_async_dsss_receiver_reset (s->rx[i]);
      (void)dp_async_dsss_receiver_set_exclusions (s->rx[i], NULL, 0);
      (void)dp_async_dsss_receiver_set_rank (s->rx[i], 0);
      s->n_sym[i] = 0;
    }
  s->samples            = 0;
  s->released_lost      = 0;
  s->released_duplicate = 0;
}

/* ── The controller ──────────────────────────────────────────────────── */

static int
holds (int state)
{
  return state == ASYNC_DSSS_RX_REFINING || state == ASYNC_DSSS_RX_TRACKING;
}

/* Within one chip, circularly on the code: the receivers' own exclusion
   zone (dp_async_dsss_receiver_set_exclusions()), so a duplicate here is
   exactly a phase a search would have excluded. */
static int
same_phase (double a, double b, double code_len)
{
  double d = fabs (a - b);
  if (d > code_len / 2.0)
    d = code_len - d;
  return d <= 1.0;
}

/* Whether holder `i` keeps the signal over holder `j`: tracking over
   refining, then the longer in its state (the first to get there), then
   the higher C/N0 estimate, then the lower index. */
static int
keeps (const async_dsss_receiver_status_t *st, size_t i, size_t j)
{
  const int ti = st[i].state == ASYNC_DSSS_RX_TRACKING;
  const int tj = st[j].state == ASYNC_DSSS_RX_TRACKING;
  if (ti != tj)
    return ti;
  if (st[i].state_samples != st[j].state_samples)
    return st[i].state_samples > st[j].state_samples;
  if (st[i].cn0_dbhz_est != st[j].cn0_dbhz_est)
    return st[i].cn0_dbhz_est > st[j].cn0_dbhz_est;
  return i < j;
}

/* Header step 2. Serial, at each code-period crossing, and a function of
   the receivers' statuses alone, so it is the same at any thread count.
   Returns the holders it leaves. */
static size_t
control (dp_async_dsss_pool_state_t *s)
{
  const size_t                 n = s->n_receivers;
  async_dsss_receiver_status_t st[ASYNC_DSSS_POOL_MAX_RECEIVERS];
  const double                 code_len = (double)s->rx[0]->code_len;
  for (size_t i = 0; i < n; i++)
    {
      st[i] = dp_async_dsss_receiver_status (s->rx[i]);
      if (st[i].state == ASYNC_DSSS_RX_LOST)
        {
          dp_async_dsss_receiver_reset (s->rx[i]);
          st[i] = dp_async_dsss_receiver_status (s->rx[i]);
          s->released_lost++;
        }
    }
  /* Duplicates, pair by pair in index order. A loser is reset at once, so
     it holds nothing in the pairs after it. */
  for (size_t i = 0; i < n; i++)
    for (size_t j = i + 1; j < n && holds (st[i].state); j++)
      {
        if (!holds (st[j].state)
            || !same_phase (st[i].chip_phase, st[j].chip_phase, code_len))
          continue;
        const size_t loser = keeps (st, i, j) ? j : i;
        dp_async_dsss_receiver_reset (s->rx[loser]);
        st[loser] = dp_async_dsss_receiver_status (s->rx[loser]);
        s->released_duplicate++;
      }
  /* Exclusions: every other holder's phase. Ranks: searchers in index
     order; a holder's is 0, ready for its next search. */
  size_t holders = 0, rank = 0;
  for (size_t i = 0; i < n; i++)
    {
      double excl[ASYNC_DSSS_POOL_MAX_RECEIVERS];
      size_t n_excl = 0;
      for (size_t j = 0; j < n; j++)
        if (j != i && holds (st[j].state))
          excl[n_excl++] = st[j].chip_phase;
      (void)dp_async_dsss_receiver_set_exclusions (s->rx[i], excl, n_excl);
      if (holds (st[i].state))
        {
          holders++;
          (void)dp_async_dsss_receiver_set_rank (s->rx[i], 0);
        }
      else
        (void)dp_async_dsss_receiver_set_rank (s->rx[i], rank++);
    }
  return holders;
}

/* One receiver's share of a segment: the samples, appended to its own
   row. */
static void
feed_one (size_t i, void *ctx)
{
  dp_async_dsss_pool_state_t *s   = ctx;
  float _Complex             *row = s->sym_buf + i * s->sym_cap;
  s->n_sym[i] += dp_async_dsss_receiver_steps (s->rx[i], s->feed_x, s->feed_n,
                                               row + s->n_sym[i],
                                               s->sym_cap - s->n_sym[i]);
}

static size_t
count_holders (const dp_async_dsss_pool_state_t *s)
{
  size_t h = 0;
  for (size_t i = 0; i < s->n_receivers; i++)
    h += holds (s->rx[i]->state);
  return h;
}

size_t
dp_async_dsss_pool_push (dp_async_dsss_pool_state_t *s,
                         const float _Complex *x, size_t x_len)
{
  /* Each segment can emit its own samples' symbols plus one carried code
     period's (sym_extra), and a push is at most x_len / period + 2
     segments. */
  const size_t need = x_len + (x_len / s->period + 2) * s->sym_extra;
  if (need > s->sym_cap)
    {
      free (s->sym_buf);
      s->sym_buf = dp_xmalloc (s->n_receivers * need * sizeof *s->sym_buf);
      s->sym_cap = need;
    }
  for (size_t i = 0; i < s->n_receivers; i++)
    s->n_sym[i] = 0;
  /* Cut the block at the code-period boundaries of the shared clock, and
     run the controller at each one: where it acts is a stream position,
     not a call boundary, so any blocking of one stream gives the same
     result. */
  size_t pos = 0;
  while (pos < x_len)
    {
      const size_t to_edge = s->period - (size_t)(s->samples % s->period);
      const size_t take    = x_len - pos < to_edge ? x_len - pos : to_edge;
      s->feed_x            = x + pos;
      s->feed_n            = take;
      dp_pool_run (s->workers, s->n_receivers, feed_one, s);
      pos += take;
      s->samples += take;
      if (s->samples % s->period == 0)
        (void)control (s);
    }
  s->feed_x = NULL;
  return count_holders (s);
}

async_dsss_receiver_status_t
dp_async_dsss_pool_status (const dp_async_dsss_pool_state_t *s,
                           size_t                            receiver)
{
  if (receiver >= s->n_receivers)
    {
      async_dsss_receiver_status_t none = { 0 };
      none.state                        = ASYNC_DSSS_POOL_NO_RECEIVER;
      return none;
    }
  return dp_async_dsss_receiver_status (s->rx[receiver]);
}

size_t
dp_async_dsss_pool_symbols_max_out (const dp_async_dsss_pool_state_t *s)
{
  size_t most = 0;
  for (size_t i = 0; i < s->n_receivers; i++)
    if (s->n_sym[i] > most)
      most = s->n_sym[i];
  return most;
}

size_t
dp_async_dsss_pool_symbols (const dp_async_dsss_pool_state_t *s,
                            size_t receiver, float _Complex *out,
                            size_t max_out)
{
  if (receiver >= s->n_receivers)
    return 0;
  size_t n = s->n_sym[receiver];
  if (n > max_out)
    n = max_out;
  if (n)
    memcpy (out, s->sym_buf + receiver * s->sym_cap, n * sizeof *out);
  return n;
}

int
dp_async_dsss_pool_set_threads (dp_async_dsss_pool_state_t *s, int n)
{
  /* An attached context has one producer only while one worker runs. */
  if (n < 0 || (s->tlm && effective_threads (n, s->n_receivers) != 1))
    return DP_ERR_INVALID;
  build_workers (s, n);
  return DP_OK;
}

int
dp_async_dsss_pool_get_threads (const dp_async_dsss_pool_state_t *s)
{
  return effective_threads (s->threads, s->n_receivers);
}

size_t
dp_async_dsss_pool_get_coherent_bins (const dp_async_dsss_pool_state_t *s)
{
  return dp_async_dsss_receiver_get_coherent_bins (s->rx[0]);
}

/* Every receiver off any context. */
static void
tlm_detach (dp_async_dsss_pool_state_t *s)
{
  for (size_t i = 0; i < s->n_receivers; i++)
    (void)dp_async_dsss_receiver_set_telemetry (s->rx[i], NULL, NULL, 1);
  s->tlm = NULL;
}

/* Receiver i onto `tlm` as "<p>.r<i>", every one; on a refusal what
   registered stays registered and the caller detaches. */
static int
tlm_attach (dp_async_dsss_pool_state_t *s, dp_tlm_t *tlm, const char *p,
            uint32_t decim)
{
  for (size_t i = 0; i < s->n_receivers; i++)
    {
      char sfx[24], name[DP_TLM_NAME_MAX];
      (void)snprintf (sfx, sizeof sfx, "r%zu", i);
      if (dp_tlm_name_join (name, p, sfx) != DP_OK
          || dp_async_dsss_receiver_set_telemetry (s->rx[i], tlm, name, decim)
                 != DP_OK)
        return DP_ERR_INVALID;
    }
  (void)snprintf (s->tlm_prefix, sizeof s->tlm_prefix, "%s", p);
  s->tlm_decim = decim;
  s->tlm       = tlm;
  return DP_OK;
}

int
dp_async_dsss_pool_set_telemetry (dp_async_dsss_pool_state_t *s, dp_tlm_t *tlm,
                                  const char *prefix, uint32_t decim)
{
  if (!tlm)
    {
      tlm_detach (s);
      return DP_OK;
    }
  const char *p = prefix ? prefix : "pool";
  if (decim == 0 || strlen (p) >= DP_TLM_NAME_MAX
      || effective_threads (s->threads, s->n_receivers) != 1)
    return DP_ERR_INVALID;
  /* Keep the request this replaces, to put back on any refusal: the dry
     run below moves every receiver off whatever context it was on. */
  dp_tlm_t *const old_tlm   = s->tlm;
  const uint32_t  old_decim = s->tlm_decim;
  char            old_prefix[DP_TLM_NAME_MAX];
  memcpy (old_prefix, s->tlm_prefix, sizeof old_prefix);
  /* Every name must fit before ANY registers on the caller's context: the
     attach runs first on a scratch context, which then says whether the
     caller's table has room for the names it lacks. */
  dp_tlm_t *scratch = dp_tlm_create (1);
  int       fits    = 0;
  if (scratch)
    {
      fits = tlm_attach (s, scratch, p, decim) == DP_OK
             && dp_tlm_has_room_for (tlm, scratch);
      tlm_detach (s);
      dp_tlm_destroy (scratch);
    }
  if (fits)
    {
      if (tlm_attach (s, tlm, p, decim) == DP_OK)
        return DP_OK;
      tlm_detach (s); /* whole or nothing */
    }
  /* Refused: restore the previous request. Its names are already in its
     context, so re-registering them takes no slot and cannot fail. */
  if (old_tlm)
    (void)tlm_attach (s, old_tlm, old_prefix, old_decim);
  return DP_ERR_INVALID;
}

dp_async_dsss_receiver_state_t *
dp_async_dsss_pool_receiver (const dp_async_dsss_pool_state_t *s,
                             size_t                            receiver)
{
  return receiver < s->n_receivers ? s->rx[receiver] : NULL;
}

/* ── Serializable state ──────────────────────────────────────────────── */

typedef struct
{
  uint64_t n_receivers;
  int64_t  threads;
  uint64_t samples;
  uint64_t released_lost;
  uint64_t released_duplicate;
} async_dsss_pool_extra_t;

size_t
dp_async_dsss_pool_state_bytes (const dp_async_dsss_pool_state_t *s)
{
  size_t n = sizeof (dp_state_hdr_t) + sizeof (async_dsss_pool_extra_t);
  for (size_t i = 0; i < s->n_receivers; i++)
    n += dp_async_dsss_receiver_state_bytes (s->rx[i]);
  return n;
}

void
dp_async_dsss_pool_get_state (const dp_async_dsss_pool_state_t *s, void *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_POOL_STATE_MAGIC, ASYNC_DSSS_POOL_STATE_VERSION,
               dp_async_dsss_pool_state_bytes (s));
  async_dsss_pool_extra_t extra = {
    .n_receivers        = (uint64_t)s->n_receivers,
    .threads            = (int64_t)s->threads,
    .samples            = s->samples,
    .released_lost      = s->released_lost,
    .released_duplicate = s->released_duplicate,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  for (size_t i = 0; i < s->n_receivers; i++)
    DP_W_CHILD (&_w, dp_async_dsss_receiver, s->rx[i]);
}

int
dp_async_dsss_pool_set_state (dp_async_dsss_pool_state_t *s, const void *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_POOL_STATE_MAGIC, ASYNC_DSSS_POOL_STATE_VERSION,
               dp_async_dsss_pool_state_bytes (s));
  /* Zeroed: dp_r_bytes leaves its destination alone on an errored reader,
     and the checks below read it before the error is looked at. */
  async_dsss_pool_extra_t extra = { 0 };
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.n_receivers != (uint64_t)s->n_receivers || extra.threads < 0
      || extra.threads > INT_MAX
      || (s->tlm
          && effective_threads ((int)extra.threads, s->n_receivers) != 1))
    return DP_ERR_INVALID;
  for (size_t i = 0; i < s->n_receivers; i++)
    DP_R_CHILD (&_r, dp_async_dsss_receiver, s->rx[i]);
  if ((int)extra.threads != s->threads)
    build_workers (s, (int)extra.threads);
  s->samples            = extra.samples;
  s->released_lost      = extra.released_lost;
  s->released_duplicate = extra.released_duplicate;
  for (size_t i = 0; i < s->n_receivers; i++)
    s->n_sym[i] = 0; /* the last push's symbols do not travel */
  return DP_OK;
}
