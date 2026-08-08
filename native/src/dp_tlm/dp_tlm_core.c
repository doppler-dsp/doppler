/*
 * dp_tlm_core.c — dp_tlm context lifecycle, registry and drain.
 *
 * Everything here is setup- or consumer-side; the producer hot path
 * (dp_tlm_emit / dp_tlm_set_now) is inline in dp_tlm/dp_tlm_core.h.
 */
#include "dp_tlm/dp_tlm_core.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

dp_tlm_t *
dp_tlm_create (size_t ring_records)
{
  dp_tlmr_t *ring = dp_tlmr_create (ring_records);
  if (!ring)
    return NULL;
  dp_tlm_t *t = (dp_tlm_t *)calloc (1, sizeof (dp_tlm_t));
  if (!t)
    {
      dp_tlmr_destroy (ring);
      return NULL;
    }
  t->ring = ring;
  return t;
}

void
dp_tlm_destroy (dp_tlm_t *t)
{
  if (!t)
    return;
  dp_tlmr_destroy (t->ring);
  free (t);
}

int
dp_tlm_probe (dp_tlm_t *t, const char *name, uint32_t decim)
{
  if (!t || !name || decim == 0 || strlen (name) >= DP_TLM_NAME_MAX)
    return DP_ERR_INVALID;
  int id = dp_tlm_probe_id (t, name);
  if (id < 0)
    {
      if (t->n_probes >= DP_TLM_MAX_PROBES)
        return DP_ERR_INVALID;
      id = (int)t->n_probes++;
      strcpy (t->probes[id].name, name);
      t->probes[id].emitted = 0;
    }
  /* Prime the phase so the FIRST event after (re-)registration emits. */
  t->probes[id].decim = decim;
  t->probes[id].phase = decim - 1;
  return id;
}

int
dp_tlm_probe_id (const dp_tlm_t *t, const char *name)
{
  if (!t || !name)
    return DP_ERR_INVALID;
  for (uint32_t i = 0; i < t->n_probes; i++)
    if (strcmp (t->probes[i].name, name) == 0)
      return (int)i;
  return DP_ERR_INVALID;
}

int
dp_tlm_emit_checked (dp_tlm_t *t, int32_t id, double v)
{
  /* The registry bound, which the inline emit deliberately does not pay for.
     Unsigned compare, so a negative id fails it too. */
  if (!t || (uint32_t)id >= t->n_probes)
    return DP_ERR_INVALID;
  dp_tlm_emit (t, id, v);
  return DP_OK;
}

int
dp_tlm_set_decim (dp_tlm_t *t, const char *name, uint32_t decim)
{
  /* Retune only. dp_tlm_probe() would register a new probe on a miss, so a
     mistyped name there silently creates a probe no emit site references and
     the caller sees success. Here a miss is an error. */
  int id = dp_tlm_probe_id (t, name);
  if (id < 0 || decim == 0)
    return DP_ERR_INVALID;
  t->probes[id].decim = decim;
  t->probes[id].phase = decim - 1; /* next event emits, as at registration */
  return DP_OK;
}

const char *
dp_tlm_probe_name (const dp_tlm_t *t, int id)
{
  if (!t || id < 0 || (uint32_t)id >= t->n_probes)
    return NULL;
  return t->probes[id].name;
}

size_t
dp_tlm_probe_count (const dp_tlm_t *t)
{
  return t ? t->n_probes : 0;
}

size_t
dp_tlm_capacity (const dp_tlm_t *t)
{
  return t ? t->ring->capacity : 0;
}

int
dp_tlm_probe_id_at (const dp_tlm_t *t, size_t i)
{
  if (!t || i >= t->n_probes)
    return DP_ERR_INVALID;
  return (int)i;
}

size_t
dp_tlm_block_bound (const dp_tlm_t *t, size_t block_samples)
{
  if (!t || block_samples == 0 || t->n_probes == 0)
    return 0;
  size_t probes = t->n_probes;
  /* Saturate rather than wrap: a wrapped bound would size the ring SMALL,
   * which is the one failure mode this function exists to make impossible. */
  if (block_samples > SIZE_MAX / probes)
    return SIZE_MAX;
  return probes * block_samples;
}

size_t
dp_tlm_avail (const dp_tlm_t *t)
{
  if (!t)
    return 0;
  /* Acquire on head pairs with the producer's release; the tail is
   * consumer-owned so a relaxed load is enough.  Racing with the producer
   * can only make the true count larger, never smaller. */
  return DP_LOAD_ACQ (&t->ring->head) - DP_LOAD_RLX (&t->ring->tail);
}

int
dp_tlm_resize (dp_tlm_t *t, size_t records)
{
  if (!t)
    return DP_ERR_INVALID;
  if (records <= t->ring->capacity)
    return DP_OK; /* already big enough — the common boundary re-check */

  /* buffer.h demands a power of two (and rounds sub-page requests up to the
   * page minimum itself, so we need not model the page size here). */
  size_t want = 1;
  while (want < records)
    {
      if (want > SIZE_MAX / 2)
        return DP_ERR_INVALID;
      want *= 2;
    }

  dp_tlmr_t *fresh = dp_tlmr_create (want);
  if (!fresh)
    return DP_ERR_INVALID; /* old ring still intact and still attached */
  dp_tlmr_destroy (t->ring);
  t->ring = fresh;
  return DP_OK;
}

dp_tlm_stats_t
dp_tlm_stats (const dp_tlm_t *t)
{
  dp_tlm_stats_t s = { 0, 0, 0, 0 };
  if (!t)
    return s;
  for (uint32_t i = 0; i < t->n_probes; i++)
    s.emitted += t->probes[i].emitted;
  s.dropped  = dp_tlm_dropped (t);
  s.capacity = t->ring->capacity;
  s.probes   = t->n_probes;
  return s;
}

size_t
dp_tlm_read_max_out (dp_tlm_t *t)
{
  return dp_tlm_avail (t);
}

size_t
dp_tlm_read (dp_tlm_t *t, size_t n, dp_tlm_rec_t *out, size_t max_out)
{
  /* Consumer side of the SPSC ring, non-blocking: acquire the head once,
   * copy what's there (contiguous thanks to the VM double-mapping), and
   * release the tail.  Deliberately NOT dp_tlmr_wait — that spins. */
  if (!t || !out)
    return 0;
  dp_tlmr_t *ring = t->ring;
  size_t     head = DP_LOAD_ACQ (&ring->head);
  size_t     tail = DP_LOAD_RLX (&ring->tail);
  size_t     have = head - tail;
  /* Clamp to BOTH: n == 0 means "everything", so the two cannot collapse. */
  if (n != 0 && have > n)
    have = n;
  if (have > max_out)
    have = max_out;
  if (have == 0)
    return 0;
  memcpy (out, &ring->data[(tail & ring->mask) * 2],
          have * sizeof (dp_tlm_rec_t));
  dp_tlmr_consume (ring, have);
  return have;
}

void
dp_tlm_demux_counts (const dp_tlm_rec_t *recs, size_t n, size_t *counts,
                     size_t ncounts)
{
  if (!counts || ncounts == 0)
    return;
  memset (counts, 0, ncounts * sizeof *counts);
  if (!recs)
    return;
  for (size_t i = 0; i < n; i++)
    {
      /* Ids ARE registry slots, so this indexes rather than searches. An id
       * past the caller's table is another context's probe, not an error. */
      size_t id = recs[i].probe;
      if (id < ncounts)
        counts[id]++;
    }
}

void
dp_tlm_demux (const dp_tlm_rec_t *recs, size_t n, float *const *values,
              uint64_t *const *index, const size_t *caps, size_t nbuf)
{
  size_t written[DP_TLM_MAX_PROBES];

  if (!recs || !caps || nbuf == 0)
    return;
  if (nbuf > DP_TLM_MAX_PROBES)
    nbuf = DP_TLM_MAX_PROBES;
  memset (written, 0, nbuf * sizeof *written);

  /* One pass over the records, not one pass per probe: each record is placed
   * where it belongs as it is read, so the split is O(n). */
  for (size_t i = 0; i < n; i++)
    {
      size_t id = recs[i].probe;
      if (id >= nbuf)
        continue;
      size_t w = written[id];
      /* A buffer sized from a stale count truncates; it never overruns. */
      if (w >= caps[id])
        continue;
      if (values && values[id])
        values[id][w] = recs[i].value;
      if (index && index[id])
        index[id][w] = recs[i].n;
      written[id] = w + 1;
    }
}

uint64_t
dp_tlm_dropped (const dp_tlm_t *t)
{
  return t ? (uint64_t)DP_LOAD_RLX (&t->ring->dropped) : 0;
}

uint64_t
dp_tlm_emitted (const dp_tlm_t *t, int id)
{
  if (!t || id < 0 || (uint32_t)id >= t->n_probes)
    return 0;
  return t->probes[id].emitted;
}
