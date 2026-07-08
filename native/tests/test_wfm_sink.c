/*
 * test_wfm_sink.c — NATS PUB sink smoke test (Phase B, POSIX only).
 *
 * Opens a PUB on a unique subject per wire type and publishes a couple of
 * blocks, asserting the convert+send path succeeds (dp_pub_send returns 0).
 * A PUB with no subscriber still reports success (fire-and-forget), so this
 * is deterministic — the full pub→sub round-trip is exercised by
 * test_stream_nats_core.c and the streaming example apps, not here.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222.
 */
#include "wfm/wfm_sink.h"

#include <arpa/inet.h>
#include <complex.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SKIP_CODE 77

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

static int
broker_reachable (void)
{
  int fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return 0;
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons (4222);
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
      printf ("SKIP: no nats-server on 127.0.0.1:4222 (run `nats-server "
              "-js`)\n");
      return SKIP_CODE;
    }

  float _Complex blk[256];
  for (int i = 0; i < 256; i++)
    blk[i] = 0.5f + 0.5f * I;

  /* one PUB per wire type on a unique subject */
  for (int t = 0; t < 5; t++)
    {
      char ep[96];
      snprintf (ep, sizeof ep, "nats://127.0.0.1:4222/wfm-sink-test-%d-%ld",
                t, (long)time (NULL));
      wfm_stream_sink_t *s = wfm_stream_sink_open (ep, t);
      CHECK (s, "sink open");
      CHECK (wfm_stream_sink_send (s, blk, 256, 1e6, 2.4e9) == 0, "send 1");
      CHECK (wfm_stream_sink_send (s, blk, 256, 1e6, 2.4e9) == 0, "send 2");
      wfm_stream_sink_close (s);
    }

  /* invalid wire type → NULL */
  CHECK (!wfm_stream_sink_open ("nats://127.0.0.1:4222/wfm-sink-bad", 9),
         "bad type rejected");

  printf ("test_wfm_sink: OK (5 wire types published)\n");
  return 0;
}
