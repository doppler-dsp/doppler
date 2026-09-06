/**
 * @file async_dsss_pool_core.c
 * @brief AsyncDsssPool -- the holder of the population (design section
 *        8.2). See async_dsss_pool_core.h for the lifecycle.
 */
#include "async_dsss_pool/async_dsss_pool_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The exclusion zone: a live row's code phase, within a chip, at ANY
   Doppler -- circular on the code. Not section 7.1's one row by one chip:
   that is the width of one emitter's main lobe on the surface, and at the
   pool's depth (D = 154, a 31.7 Hz row) a tracked emitter puts far more
   than its main lobe on the surface at its own code phase. Every block the
   searcher sums across its data is a smeared copy spread over the rows of
   its column (design section 12.7), at 45 dB-Hz still over the gate, and
   its argmax lands hundreds of Hz from the emitter block to block; with
   the zone one row wide each one seeded a fresh receiver onto the same
   emitter until the pool was full (section 12.14). The searcher's list
   carries no per-peak concentration to tell that copy from a whole
   emitter, and a strong emitter's coherent tile sidelobes at its own
   phase would read concentrated anyway; the code axis is the one key
   that holds. The cost is the resolution it gives up: a second emitter
   within a chip of a live one is not seen until the first leaves -- a
   pair the surface could not tell apart within a tile in any case
   (section 7.1), and the searcher's own twin rule already holds a
   same-phase peak at any tile as suspect. */
static int
in_zone (const async_dsss_pool_state_t *s, const async_dsss_pool_row_t *row,
         double chip_phase)
{
  double dc = fabs (chip_phase - row->chip_phase);
  double cl = (double)s->code_len;
  if (dc > cl / 2.0)
    dc = cl - dc;
  return dc <= 1.0;
}

/* One transition to the log (when attached) and to the count. The stamp
   is a stream position, never a time (section 8.1). */
static void
emit (async_dsss_pool_state_t *s, uint64_t sample, const char *label,
      size_t slot, int state, double doppler_hz, double chip_phase,
      double cn0_dbhz, const char *reason)
{
  s->events++;
  if (!s->log)
    return;
  (void)dp_event_log_field (s->log, "slot", (double)slot);
  (void)dp_event_log_field (s->log, "state", (double)state);
  (void)dp_event_log_field (s->log, "doppler_hz", doppler_hz);
  (void)dp_event_log_field (s->log, "chip_phase", chip_phase);
  (void)dp_event_log_field (s->log, "cn0_dbhz", cn0_dbhz);
  if (reason)
    (void)dp_event_log_field_str (s->log, "reason", reason);
  (void)dp_event_log_append (s->log, sample, label, 0, 0.0, 0.0);
}

/* The row's chip phase `samples` after the seed, on the nominal clock
   dilated by the seed's Doppler when the carrier is known: the phase a
   receiver still refining will have, and the key the zone is tested on. */
static double
advanced_phase (const async_dsss_pool_state_t *s, double chip_phase,
                double doppler_hz, double samples)
{
  double rate = 1.0;
  if (s->carrier_freq_hz > 0.0)
    rate += doppler_hz / s->carrier_freq_hz;
  return dp_fmod_pos (chip_phase + samples / (double)s->spc * rate,
                      (double)s->code_len);
}

static void
release (async_dsss_pool_state_t *s, size_t i, uint64_t sample,
         const async_dsss_receiver_status_t *st, const char *reason)
{
  async_dsss_pool_row_t *row = &s->rows[i];
  emit (s, sample, "released", i, st->state, st->doppler_hz, st->chip_phase,
        st->cn0_dbhz_est, reason);
  async_dsss_receiver_reset (s->rx[i]);
  memset (row, 0, sizeof *row);
  row->prev_state = ASYNC_DSSS_RX_IDLE;
}

async_dsss_pool_state_t *
async_dsss_pool_create (
    const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate,
    size_t spc, int m, double cn0_dbhz, double pfa, double pd,
    double doppler_uncertainty, size_t code_only_epochs, double doppler_rate,
    size_t max_peaks, size_t n_slots, int threads, double carrier_freq_hz,
    double lost_confirm_s, double max_emitter_on_time_secs, size_t segments,
    size_t sps, int differential, double refine_max_error_db,
    size_t refine_samples_per_symbol, double refine_design_margin_db,
    size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
    size_t refine_max_n_blocks)
{
  if (!code || code_len == 0 || !(chip_rate > 0.0) || !(symbol_rate > 0.0)
      || spc == 0 || n_slots == 0 || max_peaks == 0
      || !(carrier_freq_hz >= 0.0) || !(lost_confirm_s >= 0.0)
      || !(max_emitter_on_time_secs >= 0.0))
    return NULL;
  /* Fixed sizes from validated arguments: abort-on-OOM, no unwind path. */
  async_dsss_pool_state_t *s = dp_xcalloc (1, sizeof *s);
  s->code                    = dp_xmalloc (code_len);
  memcpy (s->code, code, code_len);
  s->code_len        = code_len;
  s->spc             = spc;
  s->chip_rate       = chip_rate;
  s->symbol_rate     = symbol_rate;
  s->fs              = chip_rate * (double)spc;
  s->carrier_freq_hz = carrier_freq_hz;
  s->max_on_samples  = (uint64_t)llround (max_emitter_on_time_secs * s->fs);
  s->max_peaks       = max_peaks;
  s->threads         = threads;
  s->n_slots         = n_slots;

  /* The searcher: continuous, with the depth the window buys, the list,
     the threads, and the carrier its hand-off and its blocks need. */
  s->acq = acq_create_continuous (s->code, code_len, spc, chip_rate,
                                  symbol_rate, cn0_dbhz, doppler_uncertainty,
                                  pfa, pd, 0, code_only_epochs, doppler_rate);
  if (!s->acq || acq_set_max_peaks (s->acq, max_peaks) != DP_OK
      || acq_set_threads (s->acq, threads) != DP_OK
      || (carrier_freq_hz > 0.0
          && acq_set_carrier_freq_hz (s->acq, carrier_freq_hz) != DP_OK))
    {
      async_dsss_pool_destroy (s);
      return NULL;
    }
  s->rx    = dp_xcalloc (n_slots, sizeof *s->rx);
  s->rows  = dp_xcalloc (n_slots, sizeof *s->rows);
  s->n_sym = dp_xcalloc (n_slots, sizeof *s->n_sym);
  s->hits  = dp_xcalloc (max_peaks, sizeof *s->hits);
  for (size_t i = 0; i < n_slots; i++)
    {
      s->rx[i] = async_dsss_receiver_create_handoff (
          s->code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz, pfa, pd,
          segments, sps, differential, refine_max_error_db,
          refine_samples_per_symbol, refine_design_margin_db, refine_n_fft,
          refine_zero_pad, refine_sequential, refine_max_n_blocks,
          carrier_freq_hz, lost_confirm_s);
      if (!s->rx[i])
        {
          async_dsss_pool_destroy (s);
          return NULL;
        }
      s->rows[i].prev_state = ASYNC_DSSS_RX_IDLE;
    }
  /* One thread is serial: no helpers, and dp_pool_run() runs the body
     inline -- the same code path, bit-identical. */
  s->pool = threads == 1 ? NULL : dp_pool_create (threads);
  return s;
}

void
async_dsss_pool_destroy (async_dsss_pool_state_t *s)
{
  if (!s)
    return;
  dp_pool_destroy (s->pool);
  if (s->rx)
    for (size_t i = 0; i < s->n_slots; i++)
      async_dsss_receiver_destroy (s->rx[i]);
  acq_destroy (s->acq);
  free (s->rx);
  free (s->rows);
  free (s->n_sym);
  free (s->hits);
  free (s->sym_buf);
  free (s->code);
  free (s);
}

void
async_dsss_pool_reset (async_dsss_pool_state_t *s)
{
  acq_reset (s->acq);
  for (size_t i = 0; i < s->n_slots; i++)
    {
      async_dsss_receiver_reset (s->rx[i]);
      memset (&s->rows[i], 0, sizeof s->rows[i]);
      s->rows[i].prev_state = ASYNC_DSSS_RX_IDLE;
      s->n_sym[i]           = 0;
    }
  s->n_assigned       = 0;
  s->dropped          = 0;
  s->events           = 0;
  s->samples_consumed = 0;
}

/* The fan's body: one receiver over the block, into its own row of the
   symbol buffer. Reads the shared block, writes only its slot. */
static void
feed_one (size_t i, void *ctx)
{
  async_dsss_pool_state_t *s = ctx;
  s->n_sym[i]                = async_dsss_receiver_steps (
      s->rx[i], s->feed_x, s->feed_n, s->sym_buf + i * s->sym_cap, s->sym_cap);
}

size_t
async_dsss_pool_push (async_dsss_pool_state_t *s, const float _Complex *x,
                      size_t x_len)
{
  const uint64_t start = s->samples_consumed;
  const uint64_t end   = start + x_len;

  /* The symbol buffer follows the largest block seen: a receiver emits at
     most one symbol per symbol period plus a settling burst, and the bound
     is generous rather than exact. Nothing allocates once a size has been
     seen. */
  size_t need = (size_t)ceil ((double)x_len * s->symbol_rate / s->fs) * 2 + 64;
  if (need > s->sym_cap)
    {
      s->sym_buf
          = dp_xrealloc (s->sym_buf, s->n_slots * need * sizeof *s->sym_buf);
      s->sym_cap = need;
    }

  /* 1. The searcher. */
  size_t nh = acq_push (s->acq, x, x_len, s->hits, s->max_peaks);

  /* 2. The table: a tracking receiver's live coordinates -- each only
     while its own lock flag says the loop holds them: the carrier's
     Doppler under `locked`, the Dll's phase under `code_locked`. An
     unlocked loop free-runs (loop 1 wandered 800 Hz in a second on a
     hand-over past its pull-in, doppler#1261), and a zone keyed on it
     would let the searcher's next hit on the same emitter look new. With a
     flag down the row keeps its last locked value; a receiver not yet
     tracking has the seed, its phase advanced to this block's start on
     the dilated clock. */
  for (size_t i = 0; i < s->n_slots; i++)
    {
      async_dsss_pool_row_t *row = &s->rows[i];
      if (!row->assigned)
        continue;
      async_dsss_receiver_status_t st = async_dsss_receiver_status (s->rx[i]);
      if (st.state == ASYNC_DSSS_RX_TRACKING)
        {
          if (st.locked)
            row->doppler_hz = st.doppler_hz;
          if (st.code_locked)
            row->chip_phase = st.chip_phase;
        }
      else
        {
          row->doppler_hz = row->seed_doppler_hz;
          row->chip_phase
              = advanced_phase (s, row->seed_chip_phase, row->seed_doppler_hz,
                                (double)(start - row->seed_sample));
        }
    }

  /* 3. The hits: a live row's own -- at its code phase, whatever the
     Doppler -- are dropped silently; the rest seed a free slot or are
     counted dropped. A hit's phase is at its dwell's end,
     inside this block; referred to the block's start, since the receiver
     is fed the whole block. */
  for (size_t h = 0; h < nh; h++)
    {
      acq_handoff_t ho;
      acq_build_handoff (s->acq, &s->hits[h], s->code_len, s->spc, &ho);
      double offset = (double)(s->hits[h].samples_consumed - start);
      double phase
          = advanced_phase (s, ho.chip_phase, ho.doppler_hz_est, -offset);
      int own = 0;
      for (size_t i = 0; i < s->n_slots && !own; i++)
        own = s->rows[i].assigned && in_zone (s, &s->rows[i], phase);
      if (own)
        continue;
      size_t free_slot = s->n_slots;
      for (size_t i = 0; i < s->n_slots; i++)
        if (!s->rows[i].assigned
            && async_dsss_receiver_seed (s->rx[i], phase, ho.doppler_hz_est,
                                         ho.cn0_dbhz_est)
                   == DP_OK)
          {
            free_slot = i;
            break;
          }
      if (free_slot == s->n_slots)
        {
          s->dropped++;
          emit (s, s->hits[h].samples_consumed, "dropped", s->n_slots,
                ASYNC_DSSS_RX_IDLE, ho.doppler_hz_est, phase, ho.cn0_dbhz_est,
                NULL);
          continue;
        }
      async_dsss_pool_row_t *row = &s->rows[free_slot];
      row->assigned              = 1;
      row->seed_sample           = start;
      row->seed_chip_phase       = phase;
      row->seed_doppler_hz       = ho.doppler_hz_est;
      row->seed_cn0_dbhz         = ho.cn0_dbhz_est;
      row->doppler_hz            = ho.doppler_hz_est;
      row->chip_phase            = phase;
      row->prev_state            = ASYNC_DSSS_RX_REFINING;
      row->prev_code = row->prev_sym = 0;
      s->n_assigned++;
      emit (s, s->hits[h].samples_consumed, "seeded", free_slot,
            ASYNC_DSSS_RX_REFINING, ho.doppler_hz_est, phase, ho.cn0_dbhz_est,
            NULL);
    }

  /* 4. Every receiver, across the threads. */
  s->feed_x = x;
  s->feed_n = x_len;
  dp_pool_run (s->pool, s->n_slots, feed_one, s);
  s->samples_consumed = end;

  /* 5. The transitions, and the releases. */
  for (size_t i = 0; i < s->n_slots; i++)
    {
      async_dsss_pool_row_t *row = &s->rows[i];
      if (!row->assigned)
        continue;
      async_dsss_receiver_status_t st = async_dsss_receiver_status (s->rx[i]);
      if (st.state == ASYNC_DSSS_RX_TRACKING
          && row->prev_state != ASYNC_DSSS_RX_TRACKING)
        emit (s, end, "tracking", i, st.state, st.doppler_hz, st.chip_phase,
              st.cn0_dbhz_est, NULL);
      /* A degrade is one flag down while tracking: logged on the edge
         from both up, not on every block it lasts. */
      if (st.state == ASYNC_DSSS_RX_TRACKING && row->prev_code && row->prev_sym
          && (st.code_locked != st.locked))
        emit (s, end, "degrade", i, st.state, st.doppler_hz, st.chip_phase,
              st.cn0_dbhz_est, NULL);
      if (st.state == ASYNC_DSSS_RX_LOST)
        {
          if (row->prev_state != ASYNC_DSSS_RX_LOST)
            emit (s, end, "lost", i, st.state, st.doppler_hz, st.chip_phase,
                  st.cn0_dbhz_est, NULL);
          release (s, i, end, &st, "lost");
          s->n_assigned--;
          continue;
        }
      if (s->max_on_samples && end - row->seed_sample > s->max_on_samples)
        {
          release (s, i, end, &st, "on_time");
          s->n_assigned--;
          continue;
        }
      row->prev_state = st.state;
      row->prev_code  = st.code_locked;
      row->prev_sym   = st.locked;
    }
  return s->n_assigned;
}

async_dsss_pool_slot_t
async_dsss_pool_status (async_dsss_pool_state_t *s, size_t slot)
{
  async_dsss_pool_slot_t r;
  memset (&r, 0, sizeof r);
  r.slot  = slot;
  r.state = -1;
  if (slot >= s->n_slots)
    return r;
  const async_dsss_pool_row_t *row = &s->rows[slot];
  async_dsss_receiver_status_t st  = async_dsss_receiver_status (s->rx[slot]);
  r.assigned                       = row->assigned;
  r.state                          = st.state;
  r.seed_sample                    = row->seed_sample;
  r.seed_chip_phase                = row->seed_chip_phase;
  r.seed_doppler_hz                = row->seed_doppler_hz;
  r.seed_cn0_dbhz                  = row->seed_cn0_dbhz;
  r.doppler_hz                     = st.doppler_hz;
  r.chip_phase                     = st.chip_phase;
  r.code_rate                      = st.code_rate;
  r.cn0_dbhz_est                   = st.cn0_dbhz_est;
  r.code_locked                    = st.code_locked;
  r.locked                         = st.locked;
  r.lock_metric                    = st.lock_metric;
  r.state_samples                  = st.state_samples;
  r.both_down_samples              = st.both_down_samples;
  r.assigned_samples
      = row->assigned ? s->samples_consumed - row->seed_sample : 0;
  return r;
}

size_t
async_dsss_pool_symbols_max_out (async_dsss_pool_state_t *s)
{
  return s->sym_cap;
}

size_t
async_dsss_pool_symbols (async_dsss_pool_state_t *s, size_t slot,
                         float _Complex *out, size_t max_out)
{
  if (slot >= s->n_slots || !s->sym_buf)
    return 0;
  size_t n = s->n_sym[slot];
  if (n > max_out)
    n = max_out;
  memcpy (out, s->sym_buf + slot * s->sym_cap, n * sizeof *out);
  return n;
}

int
async_dsss_pool_set_event_log (async_dsss_pool_state_t *s, dp_event_log_t *log)
{
  s->log = log;
  return DP_OK;
}

/* ── Serializable state ──────────────────────────────────────────────── */

typedef struct
{
  uint64_t n_slots;
  uint64_t n_assigned;
  uint64_t dropped;
  uint64_t events;
  uint64_t samples_consumed;
} async_dsss_pool_extra_t;

size_t
async_dsss_pool_state_bytes (const async_dsss_pool_state_t *s)
{
  size_t n = sizeof (dp_state_hdr_t) + sizeof (async_dsss_pool_extra_t)
             + s->n_slots * sizeof (async_dsss_pool_row_t)
             + acq_state_bytes (s->acq);
  for (size_t i = 0; i < s->n_slots; i++)
    n += async_dsss_receiver_state_bytes (s->rx[i]);
  return n;
}

void
async_dsss_pool_get_state (const async_dsss_pool_state_t *s, void *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_POOL_STATE_MAGIC, ASYNC_DSSS_POOL_STATE_VERSION,
               async_dsss_pool_state_bytes (s));
  async_dsss_pool_extra_t extra = {
    .n_slots          = (uint64_t)s->n_slots,
    .n_assigned       = (uint64_t)s->n_assigned,
    .dropped          = s->dropped,
    .events           = s->events,
    .samples_consumed = s->samples_consumed,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  dp_w_bytes (&_w, s->rows, s->n_slots * sizeof *s->rows);
  DP_W_CHILD (&_w, acq, s->acq);
  for (size_t i = 0; i < s->n_slots; i++)
    DP_W_CHILD (&_w, async_dsss_receiver, s->rx[i]);
}

int
async_dsss_pool_set_state (async_dsss_pool_state_t *s, const void *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_POOL_STATE_MAGIC, ASYNC_DSSS_POOL_STATE_VERSION,
               async_dsss_pool_state_bytes (s));
  async_dsss_pool_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.n_slots != (uint64_t)s->n_slots
      || extra.n_assigned > (uint64_t)s->n_slots)
    return DP_ERR_INVALID;
  dp_r_bytes (&_r, s->rows, s->n_slots * sizeof *s->rows);
  DP_R_CHILD (&_r, acq, s->acq);
  for (size_t i = 0; i < s->n_slots; i++)
    DP_R_CHILD (&_r, async_dsss_receiver, s->rx[i]);
  s->n_assigned       = (size_t)extra.n_assigned;
  s->dropped          = extra.dropped;
  s->events           = extra.events;
  s->samples_consumed = extra.samples_consumed;
  for (size_t i = 0; i < s->n_slots; i++)
    s->n_sym[i] = 0; /* the last push's symbols do not travel */
  return DP_OK;
}
