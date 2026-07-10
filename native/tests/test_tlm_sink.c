/*
 * test_tlm_sink.c — telemetry-ring -> NATS round-trip for the dp_tlm_sink_*
 * helper (telemetry/tlm_sink.h): pump publishes TLM16 frames a dp_sub_*
 * receiver decodes back into the exact emitted records.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222 — this is an integration
 * test against a live broker, like test_stream_nats_core.
 */
#include "stream/stream.h"
#include "telemetry/tlm_sink.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SKIP_CODE 77
#define SETTLE_US 300000 /* core NATS: sub must exist before pub */

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
          _fails++;                                                           \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          printf ("  PASS  %s\n", #cond);                                     \
        }                                                                     \
    }                                                                         \
  while (0)

static int _fails = 0;

static int
broker_reachable (void)
{
  int fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return 0;
  struct sockaddr_in addr;
  memset (&addr, 0, sizeof addr);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons (4222);
  addr.sin_addr.s_addr = inet_addr ("127.0.0.1");
  int ok = (connect (fd, (struct sockaddr *)&addr, sizeof addr) == 0);
  close (fd);
  return ok;
}

int
main (void)
{
  if (!broker_reachable ())
    {
      printf ("test_tlm_sink SKIPPED (no nats-server on 127.0.0.1:4222)\n");
      return SKIP_CODE;
    }

  /* Unique subject so runs never collide on a shared broker. */
  char ep[128];
  snprintf (ep, sizeof ep, "nats://127.0.0.1:4222/tlm-%d-%ld", (int)getpid (),
            (long)time (NULL));

  dp_sub_t *sub = dp_sub_create (ep);
  CHECK (sub != NULL);
  usleep (SETTLE_US);

  dp_tlm_sink_t *sink = dp_tlm_sink_open (ep);
  CHECK (sink != NULL);
  usleep (SETTLE_US);

  /* Fill a telemetry ring with a known series: two probes, one of them
   * decimated, with a caller-stamped sample index. */
  dp_tlm_t *tlm = dp_tlm_create (1024);
  CHECK (tlm != NULL);
  int id_a = dp_tlm_probe (tlm, "loop.e", 1);
  int id_b = dp_tlm_probe (tlm, "loop.rate", 2);
  CHECK (id_a >= 0 && id_b >= 0);
  dp_tlm_set_now (tlm, 4096);
  for (int i = 0; i < 10; i++)
    {
      dp_tlm_emit (tlm, id_a, (double)i);         /* 10 records          */
      dp_tlm_emit (tlm, id_b, 100.0 + (double)i); /* every 2nd -> 5    */
    }

  /* Pump drains everything available in one call. */
  int sent = dp_tlm_sink_pump (sink, tlm);
  CHECK (sent == 15);
  CHECK (dp_tlm_sink_sent (sink) == 15);

  /* The subscriber receives one TLM16 frame with the exact records. */
  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (sub, 3000);
  CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  CHECK (msg != NULL);
  if (msg)
    {
      CHECK (hdr.sample_type == TLM16);
      CHECK (hdr.num_samples == 15);
      CHECK (dp_msg_num_samples (msg) == 15);
      const dp_tlm_rec_t *recs = (const dp_tlm_rec_t *)dp_msg_data (msg);
      /* Ring order is emit order: a,b,a,a,b,a,... — spot-check the first
       * pair and count per probe. */
      CHECK (recs[0].probe == (uint16_t)id_a && recs[0].value == 0.0f);
      CHECK (recs[0].n == 4096);
      size_t na = 0, nb = 0;
      for (size_t i = 0; i < 15; i++)
        {
          if (recs[i].probe == (uint16_t)id_a)
            na++;
          else if (recs[i].probe == (uint16_t)id_b)
            nb++;
        }
      CHECK (na == 10 && nb == 5);
      dp_msg_free (msg);
    }

  /* An empty ring pumps zero records and publishes nothing. */
  CHECK (dp_tlm_sink_pump (sink, tlm) == 0);
  dp_msg_t *none = NULL;
  dp_sub_set_timeout (sub, 300);
  CHECK (dp_sub_recv (sub, &none, &hdr) != DP_OK);

  /* sent() accumulates across pumps. */
  dp_tlm_emit (tlm, id_a, 42.0);
  CHECK (dp_tlm_sink_pump (sink, tlm) == 1);
  CHECK (dp_tlm_sink_sent (sink) == 16);
  dp_sub_set_timeout (sub, 3000);
  CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  if (msg)
    {
      CHECK (hdr.num_samples == 1);
      dp_msg_free (msg);
    }

  dp_tlm_sink_close (sink);
  dp_tlm_sink_close (NULL); /* NULL-safe */
  dp_tlm_destroy (tlm);
  dp_sub_destroy (sub);

  if (_fails)
    {
      fprintf (stderr, "test_tlm_sink FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_tlm_sink PASSED\n");
  return 0;
}
