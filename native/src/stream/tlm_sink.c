/*
 * tlm_sink.c — NATS PUB sink for telemetry records.
 *
 * Thin glue over doppler's dp_pub_* layer: drains a dp_tlm_t ring in
 * batches and publishes each batch as one TLM16 frame. Lives in the
 * optional libdoppler_stream component (links the vendored nats.c);
 * see telemetry/tlm_sink.h for the contract.
 */
#include "telemetry/tlm_sink.h"

#include <stdlib.h>

#include "stream/stream.h"

/* Records per published frame: 512 x 16 B = 8 KiB payloads — far below any
 * NATS max_payload, big enough that a busy ring drains in a few frames. */
#define TLM_SINK_BATCH 512

struct dp_tlm_sink
{
  dp_pub_t *pub;
  uint64_t  sent;
};

dp_tlm_sink_t *
dp_tlm_sink_open (const char *endpoint)
{
  dp_pub_t *pub = dp_pub_create (endpoint, TLM16);
  if (!pub)
    return NULL;
  dp_tlm_sink_t *s = calloc (1, sizeof (*s));
  if (!s)
    {
      dp_pub_destroy (pub);
      return NULL;
    }
  s->pub = pub;
  return s;
}

int
dp_tlm_sink_pump (dp_tlm_sink_t *sink, dp_tlm_t *tlm)
{
  dp_tlm_rec_t batch[TLM_SINK_BATCH];
  size_t       total = 0;
  for (;;)
    {
      size_t n = dp_tlm_read (tlm, batch, TLM_SINK_BATCH);
      if (n == 0)
        break;
      /* Telemetry carries no sample rate / centre frequency — the records
       * are self-describing (probe id + caller-stamped n). */
      int rc = dp_pub_send_tlm16 (sink->pub, batch, n, 0.0, 0.0);
      if (rc != DP_OK)
        return rc; /* the drained batch is lost — lossy by design */
      sink->sent += n;
      total += n;
    }
  return (int)total;
}

uint64_t
dp_tlm_sink_sent (const dp_tlm_sink_t *sink)
{
  return sink->sent;
}

void
dp_tlm_sink_close (dp_tlm_sink_t *sink)
{
  if (!sink)
    return;
  dp_pub_destroy (sink->pub);
  free (sink);
}
