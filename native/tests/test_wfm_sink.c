/*
 * test_wfm_sink.c — ZMQ PUB sink smoke test (Phase B, POSIX only).
 *
 * Opens a PUB on a loopback endpoint and publishes a couple of blocks in each
 * wire type, asserting the convert+send path succeeds (dp_pub_send returns 0).
 * A PUB with no subscriber drops messages but still reports success, so this
 * is deterministic — the full pub→sub round-trip is exercised by the streaming
 * example apps, not here (zmq slow-joiner makes that flaky in CI).
 */
#include "wfmgen/wfm_sink.h"

#include <complex.h>
#include <stdio.h>

#define CHECK(c, m)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", m);                                  \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

int
main (void)
{
  float _Complex blk[256];
  for (int i = 0; i < 256; i++)
    blk[i] = 0.5f + 0.5f * I;

  /* one PUB per wire type (distinct ports avoid TIME_WAIT rebinds) */
  for (int t = 0; t < 5; t++)
    {
      char ep[64];
      snprintf (ep, sizeof ep, "tcp://127.0.0.1:%d", 5610 + t);
      wfm_zmq_sink_t *s = wfm_zmq_sink_open (ep, t);
      CHECK (s, "sink open");
      CHECK (wfm_zmq_sink_send (s, blk, 256, 1e6, 2.4e9) == 0, "send 1");
      CHECK (wfm_zmq_sink_send (s, blk, 256, 1e6, 2.4e9) == 0, "send 2");
      wfm_zmq_sink_close (s);
    }

  /* invalid wire type → NULL */
  CHECK (!wfm_zmq_sink_open ("tcp://127.0.0.1:5699", 9), "bad type rejected");

  printf ("test_wfm_sink: OK (5 wire types published)\n");
  return 0;
}
