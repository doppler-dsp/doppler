/*
 * test_stream_nats_core.c — C-level round-trip tests against a real
 * nats-server, exercising stream.h's dp_pub/sub/req/rep_* directly.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222 -- this is an integration
 * test against a live broker, not a unit test of pure logic.
 */
#define DP_TEST_VERBOSE 1
#include "dp_test.h"
#include "stream/stream.h"

#include <arpa/inet.h>
#include <complex.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SKIP_CODE 77
#define SETTLE_US 300000 /* core NATS: sub/rep must exist before pub/req */

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

/* Unique subject per test so runs never collide on a shared broker. */
static const char *
nats_ep (const char *hint)
{
  static char buf[128];
  snprintf (buf, sizeof buf, "nats://127.0.0.1:4222/%s-%d-%ld", hint,
            (int)getpid (), (long)time (NULL));
  return buf;
}

/* ------------------------------------------------------------------
 * test_pub_sub_roundtrip
 * ------------------------------------------------------------------ */
static void
test_pub_sub_roundtrip (void)
{
  printf ("\n-- PUB/SUB round-trip --\n");
  const char *ep = nats_ep ("pubsub");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  usleep (SETTLE_US);

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  usleep (SETTLE_US);

  double _Complex tx[3] = { 1 + 2 * I, 3 + 4 * I, 5 + 6 * I };
  DP_CHECK (dp_pub_send_cf64 (pub, tx, 3, 48000.0, 915e6) == DP_OK);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (sub, 3000);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL);
  if (msg)
    {
      DP_CHECK (dp_msg_num_samples (msg) == 3);
      double _Complex *rx = (double _Complex *)dp_msg_data (msg);
      DP_CHECK (memcmp (rx, tx, sizeof tx) == 0);
      DP_CHECK (hdr.sample_rate == 48000.0);
      DP_CHECK (hdr.center_freq == 915e6);
      dp_msg_free (msg);
    }

  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_req_rep_roundtrip
 * ------------------------------------------------------------------ */
static void
test_req_rep_roundtrip (void)
{
  printf ("\n-- REQ/REP round-trip --\n");
  const char *ep = nats_ep ("ctrl");

  dp_rep_t *rep = dp_rep_create (ep);
  DP_CHECK (rep != NULL);
  usleep (SETTLE_US);

  dp_req_t *req = dp_req_create (ep);
  DP_CHECK (req != NULL);

  const char *ping = "ping";
  DP_CHECK (dp_req_send (req, ping, strlen (ping) + 1) == DP_OK);

  dp_msg_t *rq_msg  = NULL;
  size_t    rq_size = 0;
  dp_rep_set_timeout (rep, 3000);
  DP_CHECK (dp_rep_recv (rep, &rq_msg, &rq_size) == DP_OK);
  if (rq_msg)
    {
      DP_CHECK (rq_size == strlen (ping) + 1);
      DP_CHECK (strcmp ((const char *)dp_msg_data (rq_msg), ping) == 0);
      dp_msg_free (rq_msg);
    }

  const char *pong = "pong";
  DP_CHECK (dp_rep_send (rep, pong, strlen (pong) + 1) == DP_OK);

  dp_msg_t *rp_msg  = NULL;
  size_t    rp_size = 0;
  dp_req_set_timeout (req, 3000);
  DP_CHECK (dp_req_recv (req, &rp_msg, &rp_size) == DP_OK);
  if (rp_msg)
    {
      DP_CHECK (rp_size == strlen (pong) + 1);
      DP_CHECK (strcmp ((const char *)dp_msg_data (rp_msg), pong) == 0);
      dp_msg_free (rp_msg);
    }

  dp_req_destroy (req);
  dp_rep_destroy (rep);
}

/* ------------------------------------------------------------------
 * test_chunked_pub_sub: a >1 MiB frame is split and reassembled
 * byte-identical (PUB/SUB chunking; matches
 * test_stream.py::test_nats_chunked_pub_sub at the C-API level).
 * ------------------------------------------------------------------ */
static void
test_chunked_pub_sub (void)
{
  printf ("\n-- Chunked PUB/SUB (>1 MiB) --\n");
  const char  *ep = nats_ep ("chunk");
  const size_t n  = 100000; /* 1.6 MB of CF64 > 1 MiB max_payload */

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  usleep (SETTLE_US);

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  usleep (SETTLE_US);

  double _Complex *tx = malloc (n * sizeof *tx);
  DP_CHECK (tx != NULL);
  if (tx)
    {
      for (size_t i = 0; i < n; i++)
        tx[i] = (double)i + (double)(i + 1) * I;

      DP_CHECK (dp_pub_send_cf64 (pub, tx, n, 1e6, 2.4e9) == DP_OK);

      dp_msg_t   *msg = NULL;
      dp_header_t hdr;
      dp_sub_set_timeout (sub, 5000);
      DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
      if (msg)
        {
          DP_CHECK (dp_msg_num_samples (msg) == n);
          DP_CHECK (memcmp (dp_msg_data (msg), tx, n * sizeof *tx) == 0);
          dp_msg_free (msg);
        }
      free (tx);
    }

  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
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

  test_pub_sub_roundtrip ();
  test_req_rep_roundtrip ();
  test_chunked_pub_sub ();

  printf ("\n");
  DP_TEST_END ("test_stream_nats_core");
}
