/*
 * test_tlm_sink.c — telemetry-ring -> NATS round-trip for the dp_tlm_sink_*
 * helper (stream/tlm_sink.h): pump publishes TLM16 frames a dp_sub_*
 * receiver decodes back into the exact emitted records.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222 — this is an integration
 * test against a live broker, like test_stream_nats_core.
 */
#define DP_TEST_VERBOSE 1
#include "doppler/stream/stream.h"
#include "doppler/stream/tlm_sink.h"
#include "dp_test.h"

#include "dp_nats_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int
main (void)
{
  if (!dp_nats_broker_reachable ())
    {
      printf ("test_tlm_sink SKIPPED (no nats-server on 127.0.0.1:4222)\n");
      return DP_NATS_SKIP;
    }

  /* Unique subject so runs never collide on a shared broker. */
  const char *ep = dp_nats_endpoint ("tlm");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();

  /* A malformed endpoint fails the open cleanly. */
  DP_CHECK (dp_tlm_sink_open ("not-a-nats-url") == NULL);

  dp_tlm_sink_t *sink = dp_tlm_sink_open (ep);
  DP_CHECK (sink != NULL);
  dp_nats_settle ();

  /* Fill a telemetry ring with a known series: two probes, one of them
   * decimated, with a caller-stamped sample index. */
  dp_tlm_t *tlm = dp_tlm_create (1024);
  DP_CHECK (tlm != NULL);
  int id_a = dp_tlm_probe (tlm, "loop.e", 1);
  int id_b = dp_tlm_probe (tlm, "loop.rate", 2);
  DP_CHECK (id_a >= 0 && id_b >= 0);
  dp_tlm_set_now (tlm, 4096);
  for (int i = 0; i < 10; i++)
    {
      dp_tlm_emit (tlm, id_a, (double)i);         /* 10 records          */
      dp_tlm_emit (tlm, id_b, 100.0 + (double)i); /* every 2nd -> 5    */
    }

  /* Pump drains everything available in one call. */
  int sent = dp_tlm_sink_pump (sink, tlm);
  DP_CHECK (sent == 15);
  DP_CHECK (dp_tlm_sink_sent (sink) == 15);

  /* The subscriber receives one TLM16 frame with the exact records. */
  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (sub, 3000);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL);
  if (msg)
    {
      DP_CHECK (hdr.kind == DP_KIND_TLM);
      DP_CHECK (hdr.format == 0); /* no BLUE code for records */
      DP_CHECK (hdr.num_samples == 15);
      DP_CHECK (dp_msg_num_samples (msg) == 15);
      const dp_tlm_rec_t *recs = (const dp_tlm_rec_t *)dp_msg_data (msg);
      /* Ring order is emit order: a,b,a,a,b,a,... — spot-check the first
       * pair and count per probe. */
      DP_CHECK (recs[0].probe == (uint16_t)id_a && recs[0].value == 0.0f);
      DP_CHECK (recs[0].n == 4096);
      size_t na = 0, nb = 0;
      for (size_t i = 0; i < 15; i++)
        {
          if (recs[i].probe == (uint16_t)id_a)
            na++;
          else if (recs[i].probe == (uint16_t)id_b)
            nb++;
        }
      DP_CHECK (na == 10 && nb == 5);
      dp_msg_free (msg);
    }

  /* An empty ring pumps zero records and publishes nothing. */
  DP_CHECK (dp_tlm_sink_pump (sink, tlm) == 0);
  dp_msg_t *none = NULL;
  dp_sub_set_timeout (sub, 300);
  DP_CHECK (dp_sub_recv (sub, &none, &hdr) != DP_OK);

  /* sent() accumulates across pumps. */
  dp_tlm_emit (tlm, id_a, 42.0);
  DP_CHECK (dp_tlm_sink_pump (sink, tlm) == 1);
  DP_CHECK (dp_tlm_sink_sent (sink) == 16);
  dp_sub_set_timeout (sub, 3000);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  if (msg)
    {
      DP_CHECK (hdr.num_samples == 1);
      dp_msg_free (msg);
    }

  dp_tlm_sink_close (sink);
  dp_tlm_sink_close (NULL); /* NULL-safe */
  dp_tlm_destroy (tlm);
  dp_sub_destroy (sub);

  DP_TEST_END ("test_tlm_sink");
}
