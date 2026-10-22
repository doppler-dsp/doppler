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
  s->carry     = dp_xcalloc (s->period, sizeof *s->carry);
  /* The ghost rule's clock is the controller's: one step a code period. */
  s->lattice_hz   = chip_rate / (2.0 * (double)code_len);
  s->ghost_window = (size_t)llround (ASYNC_DSSS_POOL_GHOST_WINDOW_S * chip_rate
                                     / (double)code_len);
  if (s->ghost_window < 2)
    s->ghost_window = 2;
  s->car_hist
      = dp_xcalloc (n_receivers * s->ghost_window, sizeof *s->car_hist);
  s->cl_hist  = dp_xcalloc (n_receivers * s->ghost_window, sizeof *s->cl_hist);
  s->hist_len = dp_xcalloc (n_receivers, sizeof *s->hist_len);
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
  free (s->carry);
  free (s->car_hist);
  free (s->cl_hist);
  free (s->hist_len);
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
  s->carry_len          = 0;
  s->released_lost      = 0;
  s->released_duplicate = 0;
  s->released_ghost     = 0;
  s->hist_pos           = 0;
  memset (s->car_hist, 0,
          s->n_receivers * s->ghost_window * sizeof *s->car_hist);
  memset (s->cl_hist, 0,
          s->n_receivers * s->ghost_window * sizeof *s->cl_hist);
  memset (s->hist_len, 0, s->n_receivers * sizeof *s->hist_len);
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

/* Receiver i's history starts again: every entry back to 0, so a ring
   holds nothing but its last hist_len steps (the blob is a function of the
   state). */
static void
hist_clear (dp_async_dsss_pool_state_t *s, size_t i)
{
  const size_t w = s->ghost_window;
  memset (s->car_hist + i * w, 0, w * sizeof *s->car_hist);
  memset (s->cl_hist + i * w, 0, w * sizeof *s->cl_hist);
  s->hist_len[i] = 0;
}

/* This step's carrier and code-lock flag into a tracking receiver's ring;
   any other state clears it, so a ring is always one unbroken track. */
static void
hist_record (dp_async_dsss_pool_state_t *s, size_t i,
             const async_dsss_receiver_status_t *st)
{
  const size_t w = s->ghost_window;
  if (st->state != ASYNC_DSSS_RX_TRACKING)
    {
      if (s->hist_len[i])
        hist_clear (s, i);
      return;
    }
  s->car_hist[i * w + s->hist_pos] = st->doppler_hz;
  s->cl_hist[i * w + s->hist_pos]  = (uint8_t)(st->code_locked != 0);
  if (s->hist_len[i] < w)
    s->hist_len[i]++;
}

/* Test 2 of the ghost rule: receiver j held code lock in fewer than
   ASYNC_DSSS_POOL_GHOST_LOCK_FRAC of the window's steps. A sidelobe is
   not the code's triangle, so the Dll's lock detector rarely sees one. */
static int
code_lock_low (const dp_async_dsss_pool_state_t *s, size_t j)
{
  const size_t   w  = s->ghost_window;
  const uint8_t *cl = s->cl_hist + j * w;
  size_t         n  = 0;
  for (size_t b = 0; b < w; b++)
    n += cl[b];
  return (double)n < ASYNC_DSSS_POOL_GHOST_LOCK_FRAC * (double)w;
}

/* The stillness limit on the carrier delta's standard deviation, Hz. */
static int
still (double sd)
{
  return sd < ASYNC_DSSS_POOL_GHOST_STILL_HZ;
}

/* Test 1 of the ghost rule: j's carrier moves in lockstep with i's, a
   whole nonzero number of lattice steps away. A sidelobe of i's code sits
   on its parent's carrier offset by k / (2 T_E), so the delta is still
   and on the lattice; two emitters' carriers move apart. Both rings are
   full, and every ring is written at the same slots, so slot b of one is
   the same step as slot b of the other. */
static int
carrier_lockstep (const dp_async_dsss_pool_state_t *s, size_t i, size_t j)
{
  const size_t  w  = s->ghost_window;
  const double *fi = s->car_hist + i * w, *fj = s->car_hist + j * w;
  double        m = 0.0;
  for (size_t b = 0; b < w; b++)
    m += fj[b] - fi[b];
  m /= (double)w;
  const double k = (double)llround (m / s->lattice_hz);
  if (k == 0.0 || fabs (m - k * s->lattice_hz) >= ASYNC_DSSS_POOL_GHOST_EPS_HZ)
    return 0;
  double v = 0.0; /* two passes: the delta is ~1500 Hz, its spread ~5 */
  for (size_t b = 0; b < w; b++)
    {
      const double e = fj[b] - fi[b] - m;
      v += e * e;
    }
  return still (sqrt (v / (double)w));
}

int
dp_async_dsss_pool_ghost_of (const dp_async_dsss_pool_state_t *s, size_t i,
                             size_t j)
{
  const size_t w = s->ghost_window;
  if (i >= s->n_receivers || j >= s->n_receivers || i == j
      || s->hist_len[i] < w || s->hist_len[j] < w)
    return 0;
  return code_lock_low (s, j) && carrier_lockstep (s, i, j);
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
      hist_record (s, i, &st[i]);
    }
  s->hist_pos = (s->hist_pos + 1) % s->ghost_window;
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
        hist_clear (s, loser);
        s->released_duplicate++;
      }
  /* Ghosts: j, a whole window tracking, is a Doppler-offset sidelobe of a
     stronger holder i -- the carrier test against i AND its own code-lock
     test, both over the same window (doppler#2529). The code-lock test is
     j's alone, so it goes first and screens nearly every pair out. */
  for (size_t j = 0; j < n; j++)
    {
      if (s->hist_len[j] < s->ghost_window || !code_lock_low (s, j))
        continue;
      for (size_t i = 0; i < n; i++)
        if (st[i].cn0_dbhz_est > st[j].cn0_dbhz_est
            && dp_async_dsss_pool_ghost_of (s, i, j))
          {
            dp_async_dsss_receiver_reset (s->rx[j]);
            st[j] = dp_async_dsss_receiver_status (s->rx[j]);
            hist_clear (s, j);
            s->released_ghost++;
            break;
          }
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

/* One whole code period to every receiver, then the controller. */
static void
feed_period (dp_async_dsss_pool_state_t *s, const float _Complex *x)
{
  s->feed_x = x;
  s->feed_n = s->period;
  dp_pool_run (s->workers, s->n_receivers, feed_one, s);
  s->feed_x = NULL;
  (void)control (s);
}

size_t
dp_async_dsss_pool_push (dp_async_dsss_pool_state_t *s,
                         const float _Complex *x, size_t x_len)
{
  /* A period's call emits that period's symbols plus at most one carried
     period's (sym_extra), and a push feeds at most x_len / period + 1
     periods. */
  const size_t need = (x_len / s->period + 2) * (s->period + s->sym_extra);
  if (need > s->sym_cap)
    {
      free (s->sym_buf);
      s->sym_buf = dp_xmalloc (s->n_receivers * need * sizeof *s->sym_buf);
      s->sym_cap = need;
    }
  for (size_t i = 0; i < s->n_receivers; i++)
    s->n_sym[i] = 0;
  s->samples += x_len;
  /* Every receiver call is exactly one period on the shared grid, so where
     each call starts and ends -- and where a receiver reads its release
     clock, once per call -- is a position in the stream. The carry first,
     then whole periods straight from x, then the rest into the carry. */
  size_t pos = 0;
  if (s->carry_len)
    {
      const size_t want = s->period - s->carry_len;
      const size_t take = x_len < want ? x_len : want;
      memcpy (s->carry + s->carry_len, x, take * sizeof *x);
      s->carry_len += take;
      pos = take;
      if (s->carry_len < s->period)
        return count_holders (s);
      feed_period (s, s->carry);
      s->carry_len = 0;
    }
  for (; x_len - pos >= s->period; pos += s->period)
    feed_period (s, x + pos);
  s->carry_len = x_len - pos;
  /* The tail, and zeros past it: the blob writes the whole buffer. */
  memcpy (s->carry, x + pos, s->carry_len * sizeof *x);
  memset (s->carry + s->carry_len, 0,
          (s->period - s->carry_len) * sizeof *s->carry);
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
  uint64_t carry_len;
  uint64_t released_ghost;
  uint64_t hist_pos;
} async_dsss_pool_extra_t;

/* The ghost rule's history in the blob: each ring's length, then every
   carrier ring, then every code-lock ring, whole. */
static size_t
hist_bytes (const dp_async_dsss_pool_state_t *s)
{
  const size_t cells = s->n_receivers * s->ghost_window;
  return s->n_receivers * sizeof (uint64_t) + cells * sizeof *s->car_hist
         + cells * sizeof *s->cl_hist;
}

/* Whether ring slot b is one of a ring's last `len` steps, the head at
   `pos` (the slot the next step writes). */
static int
in_span (size_t b, size_t pos, size_t len, size_t w)
{
  return (pos + w - 1 - b) % w < len;
}

size_t
dp_async_dsss_pool_state_bytes (const dp_async_dsss_pool_state_t *s)
{
  size_t n = sizeof (dp_state_hdr_t) + sizeof (async_dsss_pool_extra_t)
             + s->period * sizeof *s->carry + hist_bytes (s);
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
    .carry_len          = (uint64_t)s->carry_len,
    .released_ghost     = s->released_ghost,
    .hist_pos           = (uint64_t)s->hist_pos,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  /* The whole buffer, its unused tail zeros (create, reset and every
     refill keep it so), so the blob is a function of the state alone. */
  dp_w_bytes (&_w, s->carry, s->period * sizeof *s->carry);
  /* The rings too: every entry outside a ring's span is 0 (hist_clear). */
  for (size_t i = 0; i < s->n_receivers; i++)
    {
      const uint64_t len = (uint64_t)s->hist_len[i];
      dp_w_bytes (&_w, &len, sizeof len);
    }
  const size_t cells = s->n_receivers * s->ghost_window;
  dp_w_bytes (&_w, s->car_hist, cells * sizeof *s->car_hist);
  dp_w_bytes (&_w, s->cl_hist, cells * sizeof *s->cl_hist);
  for (size_t i = 0; i < s->n_receivers; i++)
    DP_W_CHILD (&_w, dp_async_dsss_receiver, s->rx[i]);
}

/* The decode, child by child into the live receivers: a refusal part way
   leaves the earlier children restored, which the wrapper below undoes. */
static int
decode_state (dp_async_dsss_pool_state_t *s, const void *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_POOL_STATE_MAGIC, ASYNC_DSSS_POOL_STATE_VERSION,
               dp_async_dsss_pool_state_bytes (s));
  /* Zeroed: dp_r_bytes leaves its destination alone on an errored reader,
     and the checks below read it before the error is looked at. */
  async_dsss_pool_extra_t extra = { 0 };
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.n_receivers != (uint64_t)s->n_receivers || extra.threads < 0
      || extra.threads > INT_MAX || extra.carry_len >= (uint64_t)s->period
      || (s->tlm
          && effective_threads ((int)extra.threads, s->n_receivers) != 1))
    return DP_ERR_INVALID;
  const float _Complex *carry
      = dp_r_reserve (&_r, s->period * sizeof *s->carry);
  if (!carry)
    return DP_ERR_INVALID;
  /* Only what push() can leave: zeros past the carried samples. */
  for (size_t k = (size_t)extra.carry_len; k < s->period; k++)
    if (carry[k] != 0)
      return DP_ERR_INVALID;
  /* The rings, likewise only what control() can leave: a length up to the
     window, flags of 0 or 1, and zeros outside each ring's span. */
  const size_t n = s->n_receivers, w = s->ghost_window;
  if (extra.hist_pos >= (uint64_t)w)
    return DP_ERR_INVALID;
  const uint64_t *lens = dp_r_reserve (&_r, n * sizeof *lens);
  const double   *car  = dp_r_reserve (&_r, n * w * sizeof *car);
  const uint8_t  *cl   = dp_r_reserve (&_r, n * w * sizeof *cl);
  if (!lens || !car || !cl)
    return DP_ERR_INVALID;
  uint64_t len[ASYNC_DSSS_POOL_MAX_RECEIVERS];
  memcpy (len, lens, n * sizeof *len); /* the blob need not be aligned */
  for (size_t i = 0; i < n; i++)
    {
      if (len[i] > (uint64_t)w)
        return DP_ERR_INVALID;
      for (size_t b = 0; b < w; b++)
        {
          double f;
          memcpy (&f, car + i * w + b, sizeof f);
          const uint8_t c = cl[i * w + b];
          if (in_span (b, (size_t)extra.hist_pos, (size_t)len[i], w)
                  ? !isfinite (f) || c > 1
                  : f != 0.0 || c != 0)
            return DP_ERR_INVALID;
        }
    }
  for (size_t i = 0; i < s->n_receivers; i++)
    DP_R_CHILD (&_r, dp_async_dsss_receiver, s->rx[i]);
  /* A ring is one unbroken track, so it is held only by a tracking
     receiver: checked on the receivers just restored. */
  for (size_t i = 0; i < n; i++)
    if (len[i] && s->rx[i]->state != ASYNC_DSSS_RX_TRACKING)
      return DP_ERR_INVALID;
  if ((int)extra.threads != s->threads)
    build_workers (s, (int)extra.threads);
  memcpy (s->carry, carry, s->period * sizeof *s->carry);
  s->carry_len          = (size_t)extra.carry_len;
  s->samples            = extra.samples;
  s->released_lost      = extra.released_lost;
  s->released_duplicate = extra.released_duplicate;
  s->released_ghost     = extra.released_ghost;
  s->hist_pos           = (size_t)extra.hist_pos;
  memcpy (s->car_hist, car, n * w * sizeof *s->car_hist);
  memcpy (s->cl_hist, cl, n * w * sizeof *s->cl_hist);
  for (size_t i = 0; i < s->n_receivers; i++)
    {
      s->hist_len[i] = (size_t)len[i];
      s->n_sym[i]    = 0; /* the last push's symbols do not travel */
    }
  return DP_OK;
}

/* Whole or not at all: a snapshot of the state as it is, put back if the
   blob is refused anywhere -- the receivers decode in place, so a refusal
   at receiver k would otherwise leave 0..k-1 restored from the blob. */
int
dp_async_dsss_pool_set_state (dp_async_dsss_pool_state_t *s, const void *blob)
{
  const size_t bytes = dp_async_dsss_pool_state_bytes (s);
  void        *snap  = dp_xmalloc (bytes);
  dp_async_dsss_pool_get_state (s, snap);
  const int rc = decode_state (s, blob);
  if (rc != DP_OK)
    (void)decode_state (s, snap); /* its own state: cannot be refused */
  free (snap);
  return rc;
}
