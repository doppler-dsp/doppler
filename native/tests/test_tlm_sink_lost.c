/**
 * @file test_tlm_sink_lost.c
 * @brief dp_tlm_sink counts the records a refused publish loses (#2011).
 *
 * dp_tlm_sink_pump() drains a batch out of the ring before it publishes
 * it, so a refused publish loses that batch: telemetry is lossy by design.
 * It used to lose it with no count at all; dp_tlm_sink_lost() is the
 * running total. A broker cannot refuse on cue, so this test compiles
 * tlm_sink.c itself beside the stand-in dp_pub_* functions below, which
 * take or refuse each publish as told. Transport-free: no broker, no NATS.
 */

#include "doppler/stream/stream.h"
#include "doppler/stream/tlm_sink.h"
#include "dp_test.h"

#include <stdio.h>

static int refuse; /* the stand-in's one knob */
static int dummy;  /* what the stand-in publisher points at */

dp_pub_t *
dp_pub_create_tlm (const char *endpoint)
{
  (void)endpoint;
  return (dp_pub_t *)&dummy; /* never dereferenced: see below */
}

int
dp_pub_send_tlm16 (dp_pub_t *ctx, const void *records, size_t num_records,
                   double sample_rate, double center_freq)
{
  (void)ctx;
  (void)records;
  (void)num_records;
  (void)sample_rate;
  (void)center_freq;
  return refuse ? DP_ERR_SEND : DP_OK;
}

void
dp_pub_destroy (dp_pub_t *ctx)
{
  (void)ctx;
}

int
main (void)
{
  dp_tlm_sink_t *sink = dp_tlm_sink_open ("nats://127.0.0.1:1/never");
  dp_tlm_t      *tlm  = dp_tlm_create (1024);
  DP_REQUIRE (sink != NULL && tlm != NULL);
  int id = dp_tlm_probe (tlm, "loop.e", 1);
  DP_REQUIRE (id >= 0);

  for (int i = 0; i < 10; i++)
    dp_tlm_emit (tlm, id, (double)i);
  refuse = 0;
  DP_CHECK (dp_tlm_sink_pump (sink, tlm) == 10);
  DP_CHECK (dp_tlm_sink_sent (sink) == 10);
  DP_CHECK (dp_tlm_sink_lost (sink) == 0);

  for (int i = 0; i < 7; i++)
    dp_tlm_emit (tlm, id, (double)i);
  refuse = 1;
  DP_CHECK (dp_tlm_sink_pump (sink, tlm) == DP_ERR_SEND);
  DP_CHECK_MSG (dp_tlm_sink_lost (sink) == 7,
                "the refused batch is counted, not just returned");
  DP_CHECK (dp_tlm_sink_sent (sink) == 10);

  for (int i = 0; i < 3; i++)
    dp_tlm_emit (tlm, id, (double)i);
  refuse = 0;
  DP_CHECK (dp_tlm_sink_pump (sink, tlm) == 3);
  DP_CHECK (dp_tlm_sink_lost (sink) == 7); /* a running total */
  DP_CHECK (dp_tlm_sink_sent (sink) == 13);

  dp_tlm_sink_close (sink);
  dp_tlm_destroy (tlm);
  DP_TEST_END ("test_tlm_sink_lost");
}
