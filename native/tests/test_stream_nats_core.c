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
#include <pthread.h>
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
 * test_eos_ends_the_stream
 *
 * The point of the whole contract: a subscriber learns the sender has
 * finished instead of inferring it from silence. Before this, "no frame
 * arrived" meant either "the sender is idle" or "the sender is gone", and
 * nothing could tell them apart.
 * ------------------------------------------------------------------ */
static void
test_eos_ends_the_stream (void)
{
  printf ("\n-- end of stream --\n");
  const char *ep = nats_ep ("eos");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  usleep (SETTLE_US);

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  usleep (SETTLE_US);

  /* Data first, so the marker is proven to arrive AFTER a real frame
     rather than instead of one. */
  double _Complex tx[2] = { 1 + 1 * I, 2 + 2 * I };
  DP_CHECK (dp_pub_send_cf64 (pub, tx, 2, 48000.0, 915e6) == DP_OK);
  DP_CHECK (dp_pub_send_eos (pub) == DP_OK);

  dp_sub_set_timeout (sub, 3000);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL);
  if (msg)
    {
      DP_CHECK (dp_msg_num_samples (msg) == 2);
      dp_msg_free (msg);
    }

  /* The marker: reported as a STATE, not handed back as an empty frame the
     caller would have to recognise. */
  msg = NULL;
  DP_CHECK_MSG (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_EOF,
                "a subscriber must learn the sender finished, rather than "
                "waiting out a timeout that means only 'not yet'");
  DP_CHECK_MSG (msg == NULL,
                "no message is produced for an end-of-stream frame, so "
                "there is nothing for the caller to free");

  /* DP_ERR_EOF is distinct from DP_ERR_TIMEOUT, which is the whole point:
     with nothing further sent, the next receive times out rather than
     repeating the end-of-stream. */
  msg = NULL;
  dp_sub_set_timeout (sub, 300);
  DP_CHECK_MSG (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_TIMEOUT,
                "'finished' and 'nothing yet' must not be the same answer");

  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_eos_is_acked_on_the_work_queue
 *
 * PULL is an explicit-ack consumer on a WorkQueue stream, and an EOS
 * frame is the one message the CALLER can never ack: it is reported as a
 * state and no dp_msg_t is handed back. So if the receive path does not
 * ack it, nothing does -- it redelivers every AckWait forever, is never
 * removed from the stream, and the next run against the subject opens
 * onto an ending that belongs to the previous one.
 *
 * The wait below is the consumer's own AckWait (5 s, set in
 * nats_pull_subscribe) plus a margin. Nothing cheaper observes this:
 * a redelivery is only scheduled when that timer expires, so a shorter
 * wait cannot tell an acked message from an unacked one and would pass
 * either way.
 * ------------------------------------------------------------------ */
#define PULL_ACKWAIT_MS 5000

static void
test_eos_is_acked_on_the_work_queue (void)
{
  printf ("\n-- end of stream is acked on the work queue --\n");
  const char *ep = nats_ep ("eosack");

  dp_pub_t *push = dp_push_create (ep, CF32);
  DP_CHECK (push != NULL);
  dp_sub_t *pull = dp_pull_create (ep);
  DP_CHECK (pull != NULL);
  usleep (SETTLE_US);

  /* Data first, so the marker is proven to arrive after a real frame. */
  float _Complex tx[4] = { 1 + 1 * I, 2 + 2 * I, 3 + 3 * I, 4 + 4 * I };
  DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 915e6) == DP_OK);
  DP_CHECK (dp_pub_send_eos (push) == DP_OK);

  dp_sub_set_timeout (pull, 3000);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  DP_CHECK (dp_sub_recv (pull, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL);
  if (msg)
    {
      DP_CHECK (dp_msg_ack (msg) == DP_OK);
      dp_msg_free (msg);
    }

  msg = NULL;
  DP_CHECK_MSG (dp_sub_recv (pull, &msg, &hdr) == DP_ERR_EOF,
                "the work-queue tier must report the ending too");
  DP_CHECK (msg == NULL);

  /* The pin: it must not come back. */
  msg = NULL;
  dp_sub_set_timeout (pull, PULL_ACKWAIT_MS + 1500);
  int rc = dp_sub_recv (pull, &msg, &hdr);
  DP_CHECK_MSG (rc != DP_ERR_EOF,
                "the end-of-stream frame was redelivered: nothing acked it, "
                "so it stays in the work queue forever and the next run "
                "against this subject reads a previous run's ending");
  DP_CHECK_MSG (rc == DP_ERR_TIMEOUT,
                "with the ending consumed and nothing further sent, the "
                "next receive means 'nothing yet'");
  if (msg)
    dp_msg_free (msg);

  dp_pub_destroy (push);
  dp_sub_destroy (pull);
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

/* ------------------------------------------------------------------
 * test_interrupt_unblocks_recv: the whole point of the API.
 *
 * A subscriber with NO timeout waits inside the NATS client. A second
 * thread calls dp_stream_interrupt(), exactly as a signal handler would,
 * and the receive must come back promptly with DP_ERR_INTERRUPTED rather
 * than sitting there until a frame arrives -- which, with no sender, is
 * never.
 * ------------------------------------------------------------------ */
static void *
interrupt_after_delay (void *arg)
{
  (void)arg;
  usleep (400000); /* let the receive get properly blocked first */
  dp_stream_interrupt ();
  return NULL;
}

static void
test_interrupt_unblocks_recv (void)
{
  printf ("\n-- interrupt unblocks a blocking recv --\n");
  const char *ep = nats_ep ("interrupt");

  dp_stream_resume ();
  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  if (!sub)
    return;
  usleep (SETTLE_US);

  /* No dp_sub_set_timeout: this blocks, and nothing will ever publish. */
  pthread_t th;
  DP_CHECK (pthread_create (&th, NULL, interrupt_after_delay, NULL) == 0);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  int         rc = dp_sub_recv (sub, &msg, &hdr);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  pthread_join (th, NULL);

  double elapsed = (double)(t1.tv_sec - t0.tv_sec)
                   + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

  DP_CHECK (rc == DP_ERR_INTERRUPTED);
  DP_CHECK (msg == NULL);
  /* Generous against the 100 ms slice, and tiny against the hour a
     blocking NextMsg would otherwise wait. */
  DP_CHECK (elapsed < 3.0);
  printf ("  returned in %.3f s\n", elapsed);

  /* Sticky: a receive STARTED while the flag is set refuses at once, so a
     signal cannot be missed by racing it. */
  clock_gettime (CLOCK_MONOTONIC, &t0);
  rc = dp_sub_recv (sub, &msg, &hdr);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  DP_CHECK (rc == DP_ERR_INTERRUPTED);
  DP_CHECK ((double)(t1.tv_sec - t0.tv_sec)
                + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9
            < 0.5);

  /* And receiving works again once it is cleared. */
  dp_stream_resume ();
  dp_sub_set_timeout (sub, 200);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_TIMEOUT);

  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_flush_after_send: "the send returned" is not "the server has it".
 * The publish is buffered and written in the background; this is the
 * round trip that makes the difference observable.
 * ------------------------------------------------------------------ */
static void
test_flush_after_send (void)
{
  printf ("\n-- flush waits for the server --\n");
  const char *ep = nats_ep ("flush");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  usleep (SETTLE_US);
  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  usleep (SETTLE_US);
  if (!sub || !pub)
    return;

  double _Complex tx[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  DP_CHECK (dp_pub_send_cf64 (pub, tx, 8, 48000.0, 0.0) == DP_OK);
  DP_CHECK (dp_pub_flush (pub, 2000) == DP_OK);

  /* After a successful flush the frame is the server's problem, not the
     client's buffer, so a receive with a short timeout must find it. */
  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (sub, 1000);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL);
  if (msg)
    dp_msg_free (msg);

  /* Flushing with nothing pending is a no-op that still round-trips. */
  DP_CHECK (dp_pub_flush (pub, 2000) == DP_OK);

  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_drain_then_send: drain is irreversible, and says so.
 *
 * A send issued WHILE a drain runs races its phases and may go either
 * way -- which is why dp_stream_drain waits for CLOSED. Once it has,
 * the answer is determinate, and it is a state (DP_ERR_CLOSED) rather
 * than a transport failure, so a caller can tell "I shut this down"
 * from "the network broke".
 * ------------------------------------------------------------------ */
static void
test_drain_then_send (void)
{
  printf ("\n-- drain, then a send is refused as CLOSED --\n");
  const char *ep = nats_ep ("drain");

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  if (!pub)
    return;
  usleep (SETTLE_US);

  double _Complex tx[4] = { 1, 2, 3, 4 };
  DP_CHECK (dp_pub_send_cf64 (pub, tx, 4, 48000.0, 0.0) == DP_OK);

  /* Everything published before this reaches the server, and the context
     is finished when it returns. */
  DP_CHECK (dp_stream_drain (pub, 5000) == DP_OK);

  /* Determinate now, because the drain has completed rather than merely
     started -- and DP_ERR_CLOSED, not DP_ERR_SEND. */
  int rc = dp_pub_send_cf64 (pub, tx, 4, 48000.0, 0.0);
  DP_CHECK (rc == DP_ERR_CLOSED);
  DP_CHECK (strcmp (dp_strerror (rc), "Context is draining or closed") == 0);

  /* And flushing a closed connection is not a lie either. */
  DP_CHECK (dp_pub_flush (pub, 500) != DP_OK);

  dp_pub_destroy (pub); /* still safe: destroy is just the free now */
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
  test_eos_ends_the_stream ();
  test_eos_is_acked_on_the_work_queue ();
  test_req_rep_roundtrip ();
  test_chunked_pub_sub ();
  test_interrupt_unblocks_recv ();
  test_flush_after_send ();
  test_drain_then_send ();

  printf ("\n");
  DP_TEST_END ("test_stream_nats_core");
}
