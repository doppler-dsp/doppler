/*
 * C-level tests for the telemetry taps (telemetry/telemetry.h).
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
#include "telemetry/telemetry.h"

#include <pthread.h>
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

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
  int _fails = 0;

  /* ── create/destroy: invalid sizes rejected, NULL-safe destroy ───── */
  {
    CHECK (dp_tlm_create (0) == NULL);
    CHECK (dp_tlm_create (3) == NULL); /* not a power of two */
    dp_tlm_destroy (NULL);             /* must not crash */

    dp_tlm_t *t = dp_tlm_create (256);
    CHECK (t != NULL);
    /* Sub-page requests round up (buffer.h semantics); capacity is
     * authoritative and stays a power of two. */
    CHECK (dp_tlm_capacity (t) >= 256);
    CHECK ((dp_tlm_capacity (t) & (dp_tlm_capacity (t) - 1)) == 0);
    dp_tlm_destroy (t);
  }

  /* ── registry: register, idempotent re-register, lookup, names ───── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    int       a = dp_tlm_probe (t, "agc.gain_db", 1);
    int       b = dp_tlm_probe (t, "sync.e", 4);
    CHECK (a == 0 && b == 1);
    CHECK (dp_tlm_probe_count (t) == 2);

    /* Same name: same id, decim updated, no new entry. */
    int a2 = dp_tlm_probe (t, "agc.gain_db", 8);
    CHECK (a2 == a);
    CHECK (dp_tlm_probe_count (t) == 2);
    CHECK (t->probes[a].decim == 8);

    CHECK (dp_tlm_lookup (t, "sync.e") == b);
    CHECK (dp_tlm_lookup (t, "nope") == DP_ERR_INVALID);
    CHECK (dp_tlm_probe_name (t, b) != NULL);
    CHECK (dp_tlm_probe_name (t, 99) == NULL);

    /* Invalid registrations. */
    CHECK (dp_tlm_probe (t, NULL, 1) == DP_ERR_INVALID);
    CHECK (dp_tlm_probe (t, "x", 0) == DP_ERR_INVALID);
    char long_name[DP_TLM_NAME_MAX + 8];
    for (int i = 0; i < DP_TLM_NAME_MAX + 4; i++)
      long_name[i] = 'a';
    long_name[DP_TLM_NAME_MAX + 4] = '\0';
    CHECK (dp_tlm_probe (t, long_name, 1) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }

  /* ── registry: table-full rejected ───────────────────────────────── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    char      name[DP_TLM_NAME_MAX];
    for (int i = 0; i < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (name, sizeof (name), "p%d", i);
        CHECK (dp_tlm_probe (t, name, 1) == i);
      }
    CHECK (dp_tlm_probe (t, "one_too_many", 1) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }

  /* ── detached emit is a no-op (the disabled path) ────────────────── */
  {
    dp_tlm_emit (NULL, 0, 1.0); /* must not crash */
    dp_tlm_set_now (NULL, 42);  /* NULL-safe */
    DP_TLM (NULL, 0, 2.0);      /* macro form */
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
    CHECK (dp_tlm_read (t, recs, 8) == 2);
    CHECK (recs[0].n == 1000 && recs[0].value == 1.5f);
    CHECK (recs[1].n == 2000 && recs[1].value == -3.25f);
    CHECK (recs[0].probe == (uint16_t)id && recs[0].flags == 0);
    CHECK (dp_tlm_emitted (t, id) == 2);

    /* Drained: next read is empty, non-blocking. */
    CHECK (dp_tlm_read (t, recs, 8) == 0);
    dp_tlm_destroy (t);
  }

  /* ── decimation: first event emits, then every decim-th ──────────── */
  {
    dp_tlm_t *t  = dp_tlm_create (256);
    int       id = dp_tlm_probe (t, "x", 3);
    for (int i = 0; i < 10; i++) /* events 0..9 */
      dp_tlm_emit (t, id, (double)i);

    dp_tlm_rec_t recs[8];
    size_t       n = dp_tlm_read (t, recs, 8);
    CHECK (n == 4); /* events 0, 3, 6, 9 */
    CHECK (recs[0].value == 0.0f && recs[1].value == 3.0f
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
        while ((n = dp_tlm_read (t, recs, 31)) > 0) /* odd partial size */
          for (size_t i = 0; i < n; i++)
            {
              CHECK (recs[i].n == next_expected);
              next_expected++;
            }
      }
    CHECK (next_expected == produced);
    CHECK (dp_tlm_dropped (t) == 0);
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

    CHECK (dp_tlm_dropped (t) == 100);
    CHECK (dp_tlm_emitted (t, id) == (uint64_t)cap);

    /* The ring holds the FIRST cap records (lossy producer drops new
     * data on overrun, never overwrites old). */
    dp_tlm_rec_t recs[64];
    uint64_t     seen = 0;
    size_t       n;
    while ((n = dp_tlm_read (t, recs, 64)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          CHECK (recs[i].value == (float)seen + (float)i);
        seen += n;
      }
    CHECK (seen == (uint64_t)cap);
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
      while ((_n = dp_tlm_read (t, recs, 256)) > 0)                           \
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
    CHECK (ordered);
    CHECK (got + dp_tlm_dropped (t) == (uint64_t)pa.n_events);
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
    CHECK (mock_state_bytes (&a) == sizeof (blob_a));
    mock_get_state (&a, blob_a);
    mock_get_state (&detached, blob_b);
    CHECK (memcmp (blob_a, blob_b, sizeof (blob_a)) == 0);

    /* Restore into an attached instance: running state comes from the
     * blob, the receiver's live attachment survives. */
    dp_tlm_t    *t2 = dp_tlm_create (256);
    mock_state_t b  = { 9.9, 1, { t2, 5, 0 } };
    CHECK (mock_set_state (&b, blob_a) == DP_OK);
    CHECK (b.phase == 0.5 && b.count == 7);
    CHECK (b.tlm.ctx == t2 && b.tlm.id_x == 5);

    /* Envelope reject still applies. */
    blob_a[0] ^= 0xff;
    CHECK (mock_set_state (&b, blob_a) == DP_ERR_INVALID);
    dp_tlm_destroy (t2);
    dp_tlm_destroy (t);
  }

  if (_fails)
    {
      fprintf (stderr, "test_telemetry_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_telemetry_core PASSED\n");
  return 0;
}
