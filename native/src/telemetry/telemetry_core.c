/*
 * telemetry_core.c — dp_tlm context lifecycle, registry and drain.
 *
 * Everything here is setup- or consumer-side; the producer hot path
 * (dp_tlm_emit / dp_tlm_set_now) is inline in telemetry/telemetry.h.
 */
#include "telemetry/telemetry.h"

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
  int id = dp_tlm_lookup (t, name);
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
dp_tlm_lookup (const dp_tlm_t *t, const char *name)
{
  if (!t || !name)
    return DP_ERR_INVALID;
  for (uint32_t i = 0; i < t->n_probes; i++)
    if (strcmp (t->probes[i].name, name) == 0)
      return (int)i;
  return DP_ERR_INVALID;
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

size_t
dp_tlm_read (dp_tlm_t *t, dp_tlm_rec_t *out, size_t max_recs)
{
  /* Consumer side of the SPSC ring, non-blocking: acquire the head once,
   * copy what's there (contiguous thanks to the VM double-mapping), and
   * release the tail.  Deliberately NOT dp_tlmr_wait — that spins. */
  dp_tlmr_t *ring = t->ring;
  size_t     head = DP_LOAD_ACQ (&ring->head);
  size_t     tail = DP_LOAD_RLX (&ring->tail);
  size_t     n    = head - tail;
  if (n > max_recs)
    n = max_recs;
  if (n == 0)
    return 0;
  memcpy (out, &ring->data[(tail & ring->mask) * 2],
          n * sizeof (dp_tlm_rec_t));
  dp_tlmr_consume (ring, n);
  return n;
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
