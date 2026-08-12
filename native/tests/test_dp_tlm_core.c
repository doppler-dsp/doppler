/*
 * C-level tests for the telemetry taps (dp_tlm/dp_tlm_core.h).
 *
 * Covers the full contract PR-able without any instrumented object:
 * lifecycle, the probe registry (idempotence, capacity, lookup), the
 * detached no-op, decimation phasing, `now` stamping, ring wraparound and
 * overrun accounting (dropped vs emitted reconcile), the non-blocking
 * drain, the SPSC hand-off across real threads, and the
 * DP_DEFINE_POD_STATE_TLM serialization rule (attachment zeroed in blobs,
 * preserved across restore) exercised on a local mock object.
 */
#include "dp_state.h"
#include "dp_test.h"
#include "dp_tlm/dp_tlm_core.h"

#include <pthread.h>
#include <stdio.h>

/* ── mock instrumented object for the POD_STATE_TLM rule ────────────────── */

typedef struct
{
  dp_tlm_t *ctx;
  int32_t   id_x;
  int32_t   _pad;
} mock_tlm_t;

typedef struct
{
  double     phase; /* pretend running state */
  uint64_t   count;
  mock_tlm_t tlm; /* live attachment; zeroed in blobs */
} mock_state_t;

#define MOCK_STATE_MAGIC DP_FOURCC ('M', 'O', 'C', 'K')
#define MOCK_STATE_VERSION 1u

DP_DEFINE_POD_STATE_TLM (mock, mock_state_t, MOCK_STATE_MAGIC,
                         MOCK_STATE_VERSION, tlm)

/* ── producer thread for the SPSC smoke test ────────────────────────────── */

typedef struct
{
  dp_tlm_t *t;
  int       id;
  int       n_events;
} producer_arg_t;

static void *
producer_main (void *arg)
{
  producer_arg_t *pa = (producer_arg_t *)arg;
  for (int i = 0; i < pa->n_events; i++)
    {
      dp_tlm_set_now (pa->t, (uint64_t)i);
      dp_tlm_emit (pa->t, pa->id, (double)i);
    }
  return NULL;
}

int
main (void)
{

  /* ── create/destroy: invalid sizes rejected, NULL-safe destroy ───── */
  {
    DP_CHECK (dp_tlm_create (0) == NULL);
    DP_CHECK (dp_tlm_create (3) == NULL); /* not a power of two */
    dp_tlm_destroy (NULL);                /* must not crash */

    dp_tlm_t *t = dp_tlm_create (256);
    DP_CHECK (t != NULL);
    /* Sub-page requests round up (buffer.h semantics); capacity is
     * authoritative and stays a power of two. */
    DP_CHECK (dp_tlm_capacity (t) >= 256);
    DP_CHECK ((dp_tlm_capacity (t) & (dp_tlm_capacity (t) - 1)) == 0);
    dp_tlm_destroy (t);
  }

  /* ── registry: register, idempotent re-register, lookup, names ───── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    int       a = dp_tlm_probe (t, "agc.gain_db", 1);
    int       b = dp_tlm_probe (t, "sync.e", 4);
    DP_CHECK (a == 0 && b == 1);
    DP_CHECK (dp_tlm_probe_count (t) == 2);

    /* Same name: same id, decim updated, no new entry. */
    int a2 = dp_tlm_probe (t, "agc.gain_db", 8);
    DP_CHECK (a2 == a);
    DP_CHECK (dp_tlm_probe_count (t) == 2);
    DP_CHECK (t->probes[a].decim == 8);

    DP_CHECK (dp_tlm_probe_id (t, "sync.e") == b);
    DP_CHECK (dp_tlm_probe_id (t, "nope") == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_probe_name (t, b) != NULL);
    DP_CHECK (dp_tlm_probe_name (t, 99) == NULL);

    /* Invalid registrations. */
    DP_CHECK (dp_tlm_probe (t, NULL, 1) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_probe (t, "x", 0) == DP_ERR_INVALID);
    char long_name[DP_TLM_NAME_MAX + 8];
    for (int i = 0; i < DP_TLM_NAME_MAX + 4; i++)
      long_name[i] = 'a';
    long_name[DP_TLM_NAME_MAX + 4] = '\0';
    DP_CHECK (dp_tlm_probe (t, long_name, 1) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }

  /* ── registry: table-full rejected ───────────────────────────────── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    char      name[DP_TLM_NAME_MAX];
    for (int i = 0; i < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (name, sizeof (name), "p%d", i);
        DP_CHECK (dp_tlm_probe (t, name, 1) == i);
      }
    DP_CHECK (dp_tlm_probe (t, "one_too_many", 1) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }

  /* ── detached emit is a no-op (the disabled path) ────────────────── */
  {
    dp_tlm_emit (NULL, 0, 1.0); /* must not crash */
    dp_tlm_set_now (NULL, 42);  /* NULL-safe */
    DP_TLM (NULL, 0, 2.0);      /* macro form */
  }

  /* ── an id outside probes[] is ignored, not indexed ──────────────── */
  {
    /* probes[] is a fixed DP_TLM_MAX_PROBES array, so a negative or
       past-the-end id used to index out of bounds and write through the
       result.  Reachable from any binding that passes the caller's id
       straight through: Telemetry.emit(1000000, 1.0) segfaulted the
       interpreter.  Each of these must be a silent no-op.

       The bound is the ARRAY's, not the registry's: an in-range but
       unregistered id still emits (decim 0 never suppresses), because
       checking n_probes here costs ~16% of the decimated path and the hot
       loop's caller holds an id dp_tlm_probe() gave it.  Rejecting THAT is
       the binding's job, where the id is untrusted. */
    dp_tlm_t *t  = dp_tlm_create (256);
    int       id = dp_tlm_probe (t, "x", 1);

    dp_tlm_emit (t, -1, 2.0);                /* negative */
    dp_tlm_emit (t, -1000000, 3.0);          /* very negative */
    dp_tlm_emit (t, DP_TLM_MAX_PROBES, 4.0); /* one past the array */
    dp_tlm_emit (t, 1000000, 5.0);           /* far past it */

    dp_tlm_rec_t recs[8];
    DP_CHECK (dp_tlm_read (t, 8, recs, 8) == 0);
    DP_CHECK (dp_tlm_stats (t).emitted == 0);

    /* The registered id still works, so the guard is not simply off. */
    dp_tlm_emit (t, id, 6.0);
    DP_CHECK (dp_tlm_read (t, 8, recs, 8) == 1);
    DP_CHECK (recs[0].value == 6.0f);
    dp_tlm_destroy (t);
  }

  /* ── emit + read round-trip, now stamping, value narrowing ───────── */
  {
    dp_tlm_t *t  = dp_tlm_create (256);
    int       id = dp_tlm_probe (t, "x", 1);

    dp_tlm_set_now (t, 1000);
    dp_tlm_emit (t, id, 1.5);
    dp_tlm_set_now (t, 2000);
    dp_tlm_emit (t, id, -3.25);

    dp_tlm_rec_t recs[8];
    DP_CHECK (dp_tlm_read (t, 8, recs, 8) == 2);
    DP_CHECK (recs[0].n == 1000 && recs[0].value == 1.5f);
    DP_CHECK (recs[1].n == 2000 && recs[1].value == -3.25f);
    DP_CHECK (recs[0].probe == (uint16_t)id && recs[0].flags == 0);
    DP_CHECK (dp_tlm_emitted (t, id) == 2);

    /* Drained: next read is empty, non-blocking. */
    DP_CHECK (dp_tlm_read (t, 8, recs, 8) == 0);
    dp_tlm_destroy (t);
  }

  /* ── decimation: first event emits, then every decim-th ──────────── */
  {
    dp_tlm_t *t  = dp_tlm_create (256);
    int       id = dp_tlm_probe (t, "x", 3);
    for (int i = 0; i < 10; i++) /* events 0..9 */
      dp_tlm_emit (t, id, (double)i);

    dp_tlm_rec_t recs[8];
    size_t       n = dp_tlm_read (t, 8, recs, 8);
    DP_CHECK (n == 4); /* events 0, 3, 6, 9 */
    DP_CHECK (recs[0].value == 0.0f && recs[1].value == 3.0f
              && recs[2].value == 6.0f && recs[3].value == 9.0f);
    dp_tlm_destroy (t);
  }

  /* ── partial read + wraparound ordering ──────────────────────────── */
  {
    dp_tlm_t *t   = dp_tlm_create (256);
    size_t    cap = dp_tlm_capacity (t);
    int       id  = dp_tlm_probe (t, "x", 1);

    /* Walk head/tail around the ring in chunks so writes straddle the
     * wrap point; every record must come back in order. */
    dp_tlm_rec_t recs[64];
    uint64_t     next_expected = 0;
    uint64_t     produced      = 0;
    while (produced < (uint64_t)cap * 2 + 17)
      {
        for (int i = 0; i < 48; i++)
          {
            dp_tlm_set_now (t, produced);
            dp_tlm_emit (t, id, (double)(produced & 0xffff));
            produced++;
          }
        size_t n;
        while ((n = dp_tlm_read (t, 31, recs, 31)) > 0) /* odd partial size */
          for (size_t i = 0; i < n; i++)
            {
              DP_CHECK (recs[i].n == next_expected);
              next_expected++;
            }
      }
    DP_CHECK (next_expected == produced);
    DP_CHECK (dp_tlm_dropped (t) == 0);
    dp_tlm_destroy (t);
  }

  /* ── overrun: drops counted, emitted reconciles ──────────────────── */
  {
    dp_tlm_t *t   = dp_tlm_create (256);
    size_t    cap = dp_tlm_capacity (t);
    int       id  = dp_tlm_probe (t, "x", 1);

    size_t total = cap + 100; /* 100 more events than the ring holds */
    for (size_t i = 0; i < total; i++)
      dp_tlm_emit (t, id, (double)i);

    DP_CHECK (dp_tlm_dropped (t) == 100);
    DP_CHECK (dp_tlm_emitted (t, id) == (uint64_t)cap);

    /* The ring holds the FIRST cap records (lossy producer drops new
     * data on overrun, never overwrites old). */
    dp_tlm_rec_t recs[64];
    uint64_t     seen = 0;
    size_t       n;
    while ((n = dp_tlm_read (t, 64, recs, 64)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          DP_CHECK (recs[i].value == (float)seen + (float)i);
        seen += n;
      }
    DP_CHECK (seen == (uint64_t)cap);
    dp_tlm_destroy (t);
  }

  /* ── SPSC smoke: producer thread vs consumer thread ──────────────── */
  {
    dp_tlm_t      *t  = dp_tlm_create (1 << 12);
    int            id = dp_tlm_probe (t, "x", 1);
    producer_arg_t pa = { t, id, 100000 };
    pthread_t      th;
    pthread_create (&th, NULL, producer_main, &pa);

    /* Drain concurrently while the producer runs (this is the actual
     * cross-thread hand-off under test), then join and drain the tail.
     * Every record must arrive in strictly increasing `n` order per the
     * ring's release/acquire contract; drops are allowed and accounted. */
    dp_tlm_rec_t recs[256];
    uint64_t     got     = 0;
    uint64_t     last_n  = 0;
    int          ordered = 1;
#define DRAIN()                                                               \
  do                                                                          \
    {                                                                         \
      size_t _n;                                                              \
      while ((_n = dp_tlm_read (t, 256, recs, 256)) > 0)                      \
        for (size_t _i = 0; _i < _n; _i++)                                    \
          {                                                                   \
            if (got && recs[_i].n <= last_n)                                  \
              ordered = 0;                                                    \
            last_n = recs[_i].n;                                              \
            got++;                                                            \
          }                                                                   \
    }                                                                         \
  while (0)
    for (int spin = 0; spin < 1000000 && got < (uint64_t)pa.n_events; spin++)
      DRAIN ();
    pthread_join (th, NULL);
    DRAIN (); /* tail: everything still in the ring after the join */
#undef DRAIN
    DP_CHECK (ordered);
    DP_CHECK (got + dp_tlm_dropped (t) == (uint64_t)pa.n_events);
    dp_tlm_destroy (t);
  }

  /* ── DP_DEFINE_POD_STATE_TLM: blob deterministic, attachment live ── */
  {
    dp_tlm_t    *t = dp_tlm_create (256);
    mock_state_t a = { 0.5, 7, { t, 3, 0 } };

    /* Blob must not depend on the attachment: attached vs detached
     * instances with identical running state serialize identically. */
    mock_state_t detached = a;
    detached.tlm.ctx      = NULL;
    detached.tlm.id_x     = 0;
    uint8_t blob_a[sizeof (dp_state_hdr_t) + sizeof (mock_state_t)];
    uint8_t blob_b[sizeof (blob_a)];
    DP_CHECK (mock_state_bytes (&a) == sizeof (blob_a));
    mock_get_state (&a, blob_a);
    mock_get_state (&detached, blob_b);
    DP_CHECK (memcmp (blob_a, blob_b, sizeof (blob_a)) == 0);

    /* Restore into an attached instance: running state comes from the
     * blob, the receiver's live attachment survives. */
    dp_tlm_t    *t2 = dp_tlm_create (256);
    mock_state_t b  = { 9.9, 1, { t2, 5, 0 } };
    DP_CHECK (mock_set_state (&b, blob_a) == DP_OK);
    DP_CHECK (b.phase == 0.5 && b.count == 7);
    DP_CHECK (b.tlm.ctx == t2 && b.tlm.id_x == 5);

    /* Envelope reject still applies. */
    blob_a[0] ^= 0xff;
    DP_CHECK (mock_set_state (&b, blob_a) == DP_ERR_INVALID);
    dp_tlm_destroy (t2);
    dp_tlm_destroy (t);
  }

  /* ── block_bound: the number the whole lossless story rests on ────────── */
  {
    dp_tlm_t *t = dp_tlm_create (1 << 12);

    /* No probes registered means no bound to state -- not "zero records". */
    DP_CHECK (dp_tlm_block_bound (t, 256) == 0);

    DP_CHECK (dp_tlm_probe (t, "a", 1) == 0);
    DP_CHECK (dp_tlm_block_bound (t, 256) == 256);
    DP_CHECK (dp_tlm_probe (t, "b", 1) == 1);
    DP_CHECK (dp_tlm_block_bound (t, 256) == 512);

    /* Decimation must NOT shrink the bound. Probes registered together share
       a phase, so a decim-D probe still emits its whole burst on the same
       event -- decim thins the SERIES, never the worst-case block. Dividing
       here is the tempting bug that would silently under-size the ring. */
    DP_CHECK (dp_tlm_probe (t, "b", 8) == 1);
    DP_CHECK (dp_tlm_block_bound (t, 256) == 512);

    /* Degenerate inputs say "no bound" rather than a plausible-looking 0. */
    DP_CHECK (dp_tlm_block_bound (t, 0) == 0);
    DP_CHECK (dp_tlm_block_bound (NULL, 256) == 0);

    /* Saturate rather than wrap: a wrapped product would size the ring SMALL,
       the one failure this function exists to make impossible. */
    DP_CHECK (dp_tlm_block_bound (t, (size_t)-1) == (size_t)-1);

    /* Ids are slots, and probe_id_at is the accessor that says so. */
    DP_CHECK (dp_tlm_probe_id_at (t, 0) == 0);
    DP_CHECK (dp_tlm_probe_id_at (t, 1) == 1);
    DP_CHECK (dp_tlm_probe_id_at (t, 2) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_probe_id_at (NULL, 0) == DP_ERR_INVALID);

    dp_tlm_destroy (t);
  }

  /* ── avail / resize: the consumer's snapshot and the boundary's grow ──── */
  {
    dp_tlm_t *t  = dp_tlm_create (1 << 12);
    int       id = dp_tlm_probe (t, "x", 1);
    DP_CHECK (dp_tlm_avail (t) == 0);
    DP_CHECK (dp_tlm_avail (NULL) == 0);
    for (int i = 0; i < 5; i++)
      dp_tlm_emit (t, id, (double)i);
    DP_CHECK (dp_tlm_avail (t) == 5);

    /* avail() must not consume: reading it twice reads the same 5. */
    DP_CHECK (dp_tlm_avail (t) == 5);
    dp_tlm_rec_t got[8];
    DP_CHECK (dp_tlm_read (t, 8, got, 8) == 5);
    DP_CHECK (dp_tlm_avail (t) == 0);

    /* Already big enough -> no-op, and cheap enough to call every boundary. */
    size_t cap0 = dp_tlm_capacity (t);
    DP_CHECK (dp_tlm_resize (t, cap0 / 2) == DP_OK);
    DP_CHECK (dp_tlm_capacity (t) == cap0);

    /* Growth rounds a non-power-of-two request UP; buffer.h demands pow2. */
    DP_CHECK (dp_tlm_resize (t, cap0 + 1) == DP_OK);
    DP_CHECK (dp_tlm_capacity (t) >= cap0 + 1);
    size_t cap1 = dp_tlm_capacity (t);
    DP_CHECK ((cap1 & (cap1 - 1)) == 0);

    DP_CHECK (dp_tlm_resize (NULL, 16) == DP_ERR_INVALID);

    /* The fresh ring is usable, and the probe registry survived the swap. */
    dp_tlm_emit (t, id, 42.0);
    DP_CHECK (dp_tlm_read (t, 8, got, 8) == 1 && got[0].value == 42.0f);
    dp_tlm_destroy (t);
  }

  /* ── stats: one snapshot, taken together ──────────────────────────────── */
  {
    dp_tlm_t *t = dp_tlm_create (1 << 12);
    int       a = dp_tlm_probe (t, "a", 1);
    int       b = dp_tlm_probe (t, "b", 1);
    for (int i = 0; i < 3; i++)
      dp_tlm_emit (t, a, 1.0);
    dp_tlm_emit (t, b, 2.0);

    dp_tlm_stats_t s = dp_tlm_stats (t);
    DP_CHECK (s.probes == 2);
    DP_CHECK (s.capacity == dp_tlm_capacity (t));
    DP_CHECK (s.dropped == 0);
    /* emitted is the SUM across probes, not any single probe's count -- 4,
       not 3 and not 1. */
    DP_CHECK (s.emitted == 4);

    dp_tlm_stats_t z = dp_tlm_stats (NULL);
    DP_CHECK (z.probes == 0 && z.capacity == 0 && z.emitted == 0
              && z.dropped == 0);
    dp_tlm_destroy (t);
  }

  /* ── set_decim: retune, never register. A typo here used to create a probe
     nothing emits to and report success (it was Python composing probe() over
     a lookup); the C form refuses instead. ─────────────────────────────────*/
  {
    dp_tlm_t *t = dp_tlm_create (1 << 12);
    DP_CHECK (dp_tlm_probe (t, "sd", 4) == 0);
    DP_CHECK (dp_tlm_set_decim (t, "sd", 2) == DP_OK);

    /* It really retuned, and re-primed the phase so the next event emits. */
    dp_tlm_rec_t buf[8];
    for (int i = 0; i < 4; i++)
      dp_tlm_emit (t, 0, (double)i);
    DP_CHECK (dp_tlm_read (t, 8, buf, 8) == 2); /* decim 2 over 4 events */

    /* An unknown name is an ERROR, not a silent registration. */
    DP_CHECK (dp_tlm_set_decim (t, "typo", 2) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_probe_count (t) == 1);
    DP_CHECK (dp_tlm_set_decim (t, "sd", 0) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_set_decim (NULL, "sd", 2) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }

  /* ── read(): the buffer is the limit, and reading is destructive ─────── */
  {
    dp_tlm_t    *t  = dp_tlm_create (1 << 12);
    int          id = dp_tlm_probe (t, "rw", 1);
    dp_tlm_rec_t buf[64];
    for (int i = 0; i < 10; i++)
      dp_tlm_emit (t, id, (double)i);

    DP_CHECK (dp_tlm_read_max_out (t) == 10);
    DP_CHECK (dp_tlm_read_max_out (NULL) == 0);

    /* A short buffer takes what fits and leaves the rest -- it does not
       truncate the ring. */
    DP_CHECK (dp_tlm_read (t, 4, buf, 4) == 4);
    DP_CHECK (buf[0].value == 0.0f && buf[3].value == 3.0f);
    DP_CHECK (dp_tlm_avail (t) == 6);

    /* An ample buffer takes the remainder, and no more than exists. */
    DP_CHECK (dp_tlm_read (t, 64, buf, 64) == 6);
    DP_CHECK (buf[0].value == 4.0f && buf[5].value == 9.0f);
    DP_CHECK (dp_tlm_avail (t) == 0);
    DP_CHECK (dp_tlm_read (t, 64, buf, 64) == 0);
    dp_tlm_destroy (t);
  }

  /* ── read() NULL-safety: the one accessor that used to dereference ────── */
  {
    dp_tlm_rec_t out[4];
    DP_CHECK (dp_tlm_read (NULL, 4, out, 4) == 0);
    dp_tlm_t *t = dp_tlm_create (1 << 12);
    DP_CHECK (dp_tlm_read (t, 4, NULL, 4) == 0);
    dp_tlm_destroy (t);
  }

  /* ── demux: counts size what the fill writes ──────────────────────────── */
  {
    /* Interleaved on purpose: a per-probe filter and a one-pass demux agree
     * on contiguous input, so only interleaving can tell them apart. */
    dp_tlm_rec_t recs[6]
        = { { 10, 1.0f, 0, 0 }, { 11, 2.0f, 1, 0 }, { 12, 3.0f, 0, 0 },
            { 13, 4.0f, 2, 0 }, { 14, 5.0f, 1, 0 }, { 15, 6.0f, 0, 0 } };
    size_t counts[3];

    dp_tlm_demux_counts (recs, 6, counts, 3);
    DP_CHECK (counts[0] == 3 && counts[1] == 2 && counts[2] == 1);

    float     v0[3], v1[2], v2[1];
    uint64_t  n0[3], n1[2], n2[1];
    float    *values[3] = { v0, v1, v2 };
    uint64_t *index[3]  = { n0, n1, n2 };
    size_t    caps[3]   = { 3, 2, 1 };

    dp_tlm_demux (recs, 6, values, index, caps, 3);
    /* FIFO order preserved within each probe, values paired to their n. */
    DP_CHECK (v0[0] == 1.0f && v0[1] == 3.0f && v0[2] == 6.0f);
    DP_CHECK (n0[0] == 10 && n0[1] == 12 && n0[2] == 15);
    DP_CHECK (v1[0] == 2.0f && v1[1] == 5.0f);
    DP_CHECK (n1[0] == 11 && n1[1] == 14);
    DP_CHECK (v2[0] == 4.0f && n2[0] == 13);

    /* index = NULL is values-only, not a crash. */
    float  v0b[3], v1b[2], v2b[1];
    float *vonly[3] = { v0b, v1b, v2b };
    dp_tlm_demux (recs, 6, vonly, NULL, caps, 3);
    DP_CHECK (v0b[0] == 1.0f && v0b[2] == 6.0f);

    /* A stale (too-small) count truncates rather than overrunning: cap
     * probe 0 at one record and the second and third must not be written. */
    float  guard[3]    = { -1.0f, -1.0f, -1.0f };
    float *one[3]      = { guard, NULL, NULL };
    size_t smallcap[3] = { 1, 0, 0 };
    dp_tlm_demux (recs, 6, one, NULL, smallcap, 3);
    DP_CHECK (guard[0] == 1.0f && guard[1] == -1.0f && guard[2] == -1.0f);

    /* Ids past the caller's table belong to another context: skipped, and
     * they must not shift anyone else's placement. */
    dp_tlm_rec_t alien[3]
        = { { 20, 7.0f, 0, 0 }, { 21, 8.0f, 9, 0 }, { 22, 9.0f, 0, 0 } };
    size_t c1[1];
    dp_tlm_demux_counts (alien, 3, c1, 1);
    DP_CHECK (c1[0] == 2);
    float  va[2]     = { -1.0f, -1.0f };
    float *valien[1] = { va };
    size_t capa[1]   = { 2 };
    dp_tlm_demux (alien, 3, valien, NULL, capa, 1);
    DP_CHECK (va[0] == 7.0f && va[1] == 9.0f);

    /* NULL-safety on both halves. */
    dp_tlm_demux_counts (NULL, 4, counts, 3);
    DP_CHECK (counts[0] == 0 && counts[1] == 0 && counts[2] == 0);
    dp_tlm_demux_counts (recs, 6, NULL, 3);
    dp_tlm_demux (NULL, 6, values, index, caps, 3);
    dp_tlm_demux (recs, 6, values, index, NULL, 3);
  }

  DP_TEST_END ("test_dp_tlm_core");
}
