/*
 * dp_tlm_capture_core.c — the boundary drain, owned.
 *
 * Consumer side only.  Nothing here runs on the producer thread and the emit
 * path (inline in dp_tlm/dp_tlm_core.h) is untouched: the whole mechanism is
 * one memcpy per block, on the caller's thread, at the moment the producer is
 * between blocks.  That placement is what makes the capture lossless — see
 * tlm_capture.h for the bound it rests on.
 *
 * The only thread here is the file writer, and it never touches the ring: it
 * consumes a staging buffer the boundary already filled.  So the ring keeps
 * exactly one producer and exactly one consumer at every instant, which is the
 * SPSC contract it has always had.
 */
#include "dp_tlm_capture/dp_tlm_capture_core.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Staging buffers hold several blocks so the writer gets useful-sized writes
 * rather than one syscall per block, but never so many that a wide context on
 * a big block turns into a surprise allocation.  Whatever the multiplier ends
 * up being, a stage always holds at least ONE block — that is what lets a
 * boundary drain in a single dp_tlm_read with no partial-fill bookkeeping. */
#define DP_TLM_CAP_STAGE_BLOCKS 8u
#define DP_TLM_CAP_STAGE_MAX (1u << 20) /* records, ~16 MiB per stage */

/* Memory-mode accumulator seed; grows geometrically from here. */
#define DP_TLM_CAP_ACC_MIN 16384u

struct dp_tlm_capture
{
  dp_tlm_t *tlm;
  size_t    block_samples;
  size_t    bound; /* records one block can emit; sizes everything */

  /* Ping-pong staging.  `active` is the producer-side buffer the boundary
     drains into; the other is either free or held by the writer. */
  dp_tlm_rec_t *stage[2];
  size_t        stage_n[2];
  size_t        stage_cap;
  int           active;

  /* Memory sink (path == NULL): one growing contiguous capture. */
  dp_tlm_rec_t *acc;
  size_t        acc_n;
  size_t        acc_cap;

  /* File sink + its writer thread. */
  FILE           *fp;
  char           *path;
  pthread_t       thread;
  pthread_mutex_t mu;
  pthread_cond_t  work_cv; /* -> writer: a buffer is pending   */
  pthread_cond_t  free_cv; /* -> boundary: the writer is idle  */
  int             pending; /* stage index awaiting write, else -1 */
  int             quit;
  int             werr; /* first writer error, sticky */

  /* Borrowed, not copied: the pipeline's clock is the SSOT for the time base,
     and it may be corrected by dp_sample_clock_track() after we open.  Read
     at close() so the sidecar records the corrected epoch, not a stale one. */
  const dp_sample_clock_t *clock;

  uint64_t dropped_at_open;
  uint64_t dropped;
  size_t   count;
  int      closed;
  int      verdict; /* close()'s result, replayed on a second call */
};

/* ── sinks ──────────────────────────────────────────────────────────────── */

/* Append `n` records from `src` to the in-memory capture. */
static int
acc_append (dp_tlm_capture_t *c, const dp_tlm_rec_t *src, size_t n)
{
  if (c->acc_n + n > c->acc_cap)
    {
      size_t cap = c->acc_cap ? c->acc_cap : DP_TLM_CAP_ACC_MIN;
      while (cap < c->acc_n + n)
        {
          if (cap > (size_t)-1 / 2)
            return DP_ERR_MEMORY;
          cap *= 2;
        }
      dp_tlm_rec_t *q = (dp_tlm_rec_t *)realloc (c->acc, cap * sizeof *q);
      if (!q)
        return DP_ERR_MEMORY;
      c->acc     = q;
      c->acc_cap = cap;
    }
  memcpy (c->acc + c->acc_n, src, n * sizeof *src);
  c->acc_n += n;
  return DP_OK;
}

/* Write one staging buffer out.  Runs on the writer thread in file mode and
   on the caller's thread in memory mode; either way it is the ONLY code that
   consumes a stage, so a stage has one owner at a time. */
static int
sink_write (dp_tlm_capture_t *c, const dp_tlm_rec_t *src, size_t n)
{
  if (n == 0)
    return DP_OK;
  if (!c->fp)
    return acc_append (c, src, n);
  return fwrite (src, sizeof *src, n, c->fp) == n ? DP_OK : DP_ERR_SEND;
}

static void *
writer_thread (void *arg)
{
  dp_tlm_capture_t *c = (dp_tlm_capture_t *)arg;
  for (;;)
    {
      pthread_mutex_lock (&c->mu);
      while (c->pending < 0 && !c->quit)
        pthread_cond_wait (&c->work_cv, &c->mu);
      if (c->pending < 0)
        { /* quit with nothing outstanding */
          pthread_mutex_unlock (&c->mu);
          return NULL;
        }
      int    idx = c->pending;
      size_t n   = c->stage_n[idx];
      pthread_mutex_unlock (&c->mu);

      int rc = sink_write (c, c->stage[idx], n);

      pthread_mutex_lock (&c->mu);
      c->stage_n[idx] = 0;
      c->pending      = -1;
      if (rc != DP_OK && c->werr == DP_OK)
        c->werr = rc;
      pthread_cond_signal (&c->free_cv);
      pthread_mutex_unlock (&c->mu);
    }
}

/* Hand the active stage to the sink and switch to the other one.
 *
 * In file mode this WAITS for the writer to release the other buffer, and
 * that wait is the whole backpressure story: when the disk cannot keep up the
 * block boundary stalls, the producer stalls with it, and not one record is
 * lost.  Blocking here is safe because the producer is between blocks. */
static int
swap_stage (dp_tlm_capture_t *c)
{
  if (c->stage_n[c->active] == 0)
    return DP_OK;

  if (!c->fp)
    { /* memory mode: no thread, no handoff — append and reuse the buffer */
      int rc = sink_write (c, c->stage[c->active], c->stage_n[c->active]);
      c->stage_n[c->active] = 0;
      return rc;
    }

  pthread_mutex_lock (&c->mu);
  while (c->pending >= 0 && c->werr == DP_OK)
    pthread_cond_wait (&c->free_cv, &c->mu);
  int rc = c->werr;
  if (rc == DP_OK)
    {
      c->pending = c->active;
      c->active ^= 1;
      pthread_cond_signal (&c->work_cv);
    }
  pthread_mutex_unlock (&c->mu);
  return rc;
}

/* ── sizing ─────────────────────────────────────────────────────────────── */

static size_t
stage_cap_for (size_t bound)
{
  size_t mult = DP_TLM_CAP_STAGE_BLOCKS;
  while (mult > 1 && bound > DP_TLM_CAP_STAGE_MAX / mult)
    mult /= 2;
  return bound * mult;
}

/* Grow the ring and both staging buffers to accommodate `bound` records per
   block.  Called at open, and again at any boundary where probes have been
   registered since — safe there because the ring is about to be drained and
   the producer is quiescent. */
static int
resize_for (dp_tlm_capture_t *c, size_t bound)
{
  size_t want = stage_cap_for (bound);
  if (want > c->stage_cap)
    {
      /* The writer may be holding stage[1-active]; wait it out rather than
         realloc under its feet.
         NOT gated by a test, and said plainly rather than implied: removing
         this wait leaves the suite green, because firing it needs a probe
         registered mid-run AND the writer still inside fwrite at that exact
         instant, which cannot be scheduled deterministically. It stays as
         defensive code. Normally `pending` is already -1 at a boundary and
         this returns without blocking. */
      if (c->fp)
        {
          pthread_mutex_lock (&c->mu);
          while (c->pending >= 0 && c->werr == DP_OK)
            pthread_cond_wait (&c->free_cv, &c->mu);
          pthread_mutex_unlock (&c->mu);
        }
      for (int i = 0; i < 2; i++)
        {
          dp_tlm_rec_t *q
              = (dp_tlm_rec_t *)realloc (c->stage[i], want * sizeof *q);
          if (!q)
            return DP_ERR_MEMORY;
          c->stage[i] = q;
        }
      c->stage_cap = want;
    }
  if (dp_tlm_resize (c->tlm, bound) != DP_OK)
    return DP_ERR_MEMORY;
  c->bound = bound;
  return DP_OK;
}

/* ── sidecar ────────────────────────────────────────────────────────────── */

/* Minimal JSON string escaping.  Probe names are ASCII identifiers by
   construction, but a sidecar that can emit invalid JSON for an unexpected
   name is a worse bug than a few lines of escaping. */
static void
json_str (FILE *f, const char *s)
{
  fputc ('"', f);
  for (; *s; s++)
    {
      unsigned char ch = (unsigned char)*s;
      if (ch == '"' || ch == '\\')
        fprintf (f, "\\%c", ch);
      else if (ch < 0x20)
        fprintf (f, "\\u%04x", ch);
      else
        fputc ((int)ch, f);
    }
  fputc ('"', f);
}

/* Write "<path>-meta": everything the raw records cannot carry.  The dtype is
   in there deliberately — with it the file is self-describing, and reading a
   capture needs no doppler code at all. */
static int
write_sidecar (const dp_tlm_capture_t *c)
{
  size_t n    = strlen (c->path) + 6;
  char  *meta = (char *)malloc (n);
  if (!meta)
    return DP_ERR_MEMORY;
  snprintf (meta, n, "%s-meta", c->path);
  FILE *f = fopen (meta, "wb");
  free (meta);
  if (!f)
    return DP_ERR_SEND;

  dp_tlm_stats_t st = dp_tlm_stats (c->tlm);
  fprintf (f, "{\n  \"records\": %zu,\n", c->count);
  fprintf (f, "  \"dropped\": %llu,\n", (unsigned long long)c->dropped);
  fprintf (f, "  \"capacity\": %zu,\n", st.capacity);
  fprintf (f, "  \"block_samples\": %zu,\n", c->block_samples);
  /* The time base, read from the clock NOW so a dp_sample_clock_track()
     correction that landed mid-capture is what gets recorded.  An absent or
     unstated value is OMITTED, never written as a plausible-looking zero:
     "no rate given" and "exactly 0 Hz" must not be the same bytes. */
  if (c->clock)
    {
      if (c->clock->fs != 0.0)
        fprintf (f, "  \"fs\": %.17g,\n", c->clock->fs);
      /* Gated on has_anchor, NOT on the epoch being non-zero. dp_sample_clock
         _init() captures CLOCK_REALTIME at construction, so an unanchored
         clock always has a plausible-looking epoch -- and it is "now", which
         for a replayed 2019 capture is worse than no timestamp because it
         looks authoritative. Only an epoch adopted from ground truth via
         dp_sample_clock_track() is worth writing down. */
      if (c->clock->has_anchor)
        fprintf (f, "  \"epoch_real_ns\": %llu,\n",
                 (unsigned long long)c->clock->epoch_real_ns);
    }
  fprintf (f, "  \"dtype\": [[\"n\", \"<u8\"], [\"value\", \"<f4\"],"
              " [\"probe\", \"<u2\"], [\"flags\", \"<u2\"]],\n");
  fprintf (f, "  \"probes\": {");
  size_t np = dp_tlm_probe_count (c->tlm);
  for (size_t i = 0; i < np; i++)
    {
      fprintf (f, "%s\n    ", i ? "," : "");
      json_str (f, dp_tlm_probe_name (c->tlm, (int)i));
      fprintf (f, ": %d", dp_tlm_probe_id_at (c->tlm, i));
    }
  fprintf (f, "%s}\n}\n", np ? "\n  " : "");
  return fclose (f) == 0 ? DP_OK : DP_ERR_SEND;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

dp_tlm_capture_t *
dp_tlm_capture_open (dp_tlm_t *t, size_t block_samples, const char *path,
                     const dp_sample_clock_t *clock)
{
  if (!t || block_samples == 0 || t->capture)
    return NULL;
  size_t bound = dp_tlm_block_bound (t, block_samples);
  if (bound == 0)
    return NULL; /* no probes attached: nothing to capture, and no bound */

  dp_tlm_capture_t *c = (dp_tlm_capture_t *)calloc (1, sizeof *c);
  if (!c)
    return NULL;
  c->tlm           = t;
  c->block_samples = block_samples;
  c->pending       = -1;
  c->clock         = clock;

  if (path)
    {
      c->path = (char *)malloc (strlen (path) + 1);
      if (!c->path)
        goto fail;
      strcpy (c->path, path);
      c->fp = fopen (path, "wb");
      if (!c->fp)
        goto fail;
      pthread_mutex_init (&c->mu, NULL);
      pthread_cond_init (&c->work_cv, NULL);
      pthread_cond_init (&c->free_cv, NULL);
    }

  if (resize_for (c, bound) != DP_OK)
    goto fail_threaded;
  if (c->fp && pthread_create (&c->thread, NULL, writer_thread, c) != 0)
    goto fail_threaded;

  /* Latch the monotonic drop count so `dropped` reports THIS capture. */
  c->dropped_at_open = dp_tlm_dropped (t);
  t->capture         = c;
  /* Register the drain by pointer; see dp_tlm_t.capture_drain. */
  t->capture_drain = dp_tlm_capture_block;
  return c;

fail_threaded:
  if (c->fp)
    {
      pthread_cond_destroy (&c->free_cv);
      pthread_cond_destroy (&c->work_cv);
      pthread_mutex_destroy (&c->mu);
    }
fail:
  if (c->fp)
    fclose (c->fp);
  free (c->stage[0]);
  free (c->stage[1]);
  free (c->path);
  free (c);
  return NULL;
}

/* One boundary pass: drain the ring dry, then re-size for the next block.
 *
 * The loop, rather than a single read, is what makes an over-long step
 * survivable: whatever the ring holds ends up staged, swapping buffers as
 * often as it takes.  The bound guarantees ONE iteration suffices in the
 * contract-honouring case.
 *
 * The grow comes AFTER the drain, not before, and that ordering is load
 * bearing: dp_tlm_resize() replaces the ring outright, so growing first would
 * throw away exactly the records this boundary exists to collect. */
static int
drain (dp_tlm_capture_t *c)
{
  for (;;)
    {
      size_t room = c->stage_cap - c->stage_n[c->active];
      if (room == 0)
        {
          int rc = swap_stage (c);
          if (rc != DP_OK)
            return rc;
          continue;
        }
      size_t got = dp_tlm_read (
          c->tlm, room, c->stage[c->active] + c->stage_n[c->active], room);
      if (got == 0)
        break;
      c->stage_n[c->active] += got;
      c->count += got;
    }

  /* Probes registered since the last boundary raise the bound; catch up now,
     with the ring drained and the producer between blocks — the one moment a
     resize is safe. */
  size_t bound = dp_tlm_block_bound (c->tlm, c->block_samples);
  if (bound > c->bound)
    {
      int rc = resize_for (c, bound);
      if (rc != DP_OK)
        return rc;
    }

  /* Hand off early enough that the next block always finds a full block's
     worth of room — the property the single-read fast path relies on. */
  if (c->stage_cap - c->stage_n[c->active] < c->bound)
    return swap_stage (c);
  return DP_OK;
}

int
dp_tlm_capture_block (dp_tlm_capture_t *c)
{
  if (!c || c->closed)
    return DP_ERR_INVALID;
  return drain (c);
}

int
dp_tlm_capture_close (dp_tlm_capture_t *c)
{
  if (!c)
    return DP_ERR_INVALID;
  if (c->closed)
    return c->verdict;

  int rc = drain (c); /* the tail the last block left behind */

  if (c->fp)
    {
      /* Retire the writer, then finish the active buffer on this thread —
         legal because the thread is joined, so there is still exactly one
         consumer of a stage at every instant. */
      pthread_mutex_lock (&c->mu);
      while (c->pending >= 0 && c->werr == DP_OK)
        pthread_cond_wait (&c->free_cv, &c->mu);
      c->quit = 1;
      pthread_cond_signal (&c->work_cv);
      pthread_mutex_unlock (&c->mu);
      pthread_join (c->thread, NULL);
      if (rc == DP_OK)
        rc = c->werr;
    }
  int wrc = sink_write (c, c->stage[c->active], c->stage_n[c->active]);
  c->stage_n[c->active] = 0;
  if (rc == DP_OK)
    rc = wrc;

  c->dropped = dp_tlm_dropped (c->tlm) - c->dropped_at_open;

  if (c->fp)
    {
      if (fclose (c->fp) != 0 && rc == DP_OK)
        rc = DP_ERR_SEND;
      c->fp   = NULL;
      int src = write_sidecar (c);
      if (rc == DP_OK)
        rc = src;
    }

  /* Stop delegating before anything else can call in through set_now. */
  if (c->tlm->capture == c)
    {
      c->tlm->capture       = NULL;
      c->tlm->capture_drain = NULL;
    }
  c->closed = 1;

  /* A hole is a wrong capture, not a small one — say so. */
  if (rc == DP_OK && c->dropped != 0)
    rc = DP_ERR_INVALID;
  c->verdict = rc;
  return rc;
}

dp_tlm_capture_t *
dp_tlm_capture_open_memory (dp_tlm_t *t, size_t block_samples,
                            const dp_sample_clock_t *clock)
{
  /* Memory mode IS open() with no path — a separate entry point rather than a
     separate implementation, so the two flavours cannot acquire different
     sizing or latching behaviour. */
  return dp_tlm_capture_open (t, block_samples, NULL, clock);
}

size_t
dp_tlm_capture_count (const dp_tlm_capture_t *c)
{
  return c ? c->count : 0;
}

const dp_tlm_rec_t *
dp_tlm_capture_records (const dp_tlm_capture_t *c)
{
  return (c && c->acc_n) ? c->acc : NULL;
}

size_t
dp_tlm_capture_read_max_out (const dp_tlm_capture_t *c)
{
  return c ? c->acc_n : 0;
}

size_t
dp_tlm_capture_read (const dp_tlm_capture_t *c, size_t n, dp_tlm_rec_t *out,
                     size_t max_out)
{
  if (!c || !out || !c->acc_n)
    return 0;
  /* Clamped to the smaller of the request and the destination, exactly as
     dp_tlm_read() does — n == 0 means "everything", so it is the request that
     drops out of the min, not the capacity. */
  size_t want = (n == 0 || n > c->acc_n) ? c->acc_n : n;
  if (want > max_out)
    want = max_out;
  memcpy (out, c->acc, want * sizeof *out);
  return want;
}

uint64_t
dp_tlm_capture_dropped (const dp_tlm_capture_t *c)
{
  return c ? c->dropped : 0;
}

dp_tlm_t *
dp_tlm_capture_context (const dp_tlm_capture_t *c)
{
  return c ? c->tlm : NULL;
}

int
dp_tlm_capture_destroy (dp_tlm_capture_t *c)
{
  if (!c)
    return DP_OK;
  /* The verdict is REPORTED, not discarded: a `with` block's exit and a
     garbage collection both arrive here, and those are exactly the paths a
     hole would otherwise slip out through. Freeing happens either way — the
     return value carries the bad news, it does not withhold the cleanup. */
  int rc = c->closed ? c->verdict : dp_tlm_capture_close (c);
  if (c->path)
    {
      pthread_cond_destroy (&c->free_cv);
      pthread_cond_destroy (&c->work_cv);
      pthread_mutex_destroy (&c->mu);
    }
  free (c->stage[0]);
  free (c->stage[1]);
  free (c->acc);
  free (c->path);
  free (c);
  return rc;
}
