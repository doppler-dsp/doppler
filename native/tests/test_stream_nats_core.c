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

/* Forging a frame the parser must REJECT is the whole point of the
   poison test below, and no doppler API can produce one -- every
   writer emits a well-formed header. So this one test reaches the
   transport directly. stream_nats.c remains the only place the
   LIBRARY includes it. */
#include <nats.h>

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

/* ------------------------------------------------------------------
 * test_unparseable_frame_does_not_wedge_the_queue
 * ------------------------------------------------------------------ */
#define POISON_N 1200 /* > the consumer's MaxAckPending of 1000 */

/* Publish `n` frames that cannot parse, straight onto the work subject. */
static int
publish_poison (const char *base, int n)
{
  natsConnection *nc = NULL;
  jsCtx          *js = NULL;
  if (natsConnection_ConnectTo (&nc, "nats://127.0.0.1:4222") != NATS_OK)
    return -1;
  if (natsConnection_JetStream (&js, nc, NULL) != NATS_OK)
    {
      natsConnection_Destroy (nc);
      return -1;
    }

  char subj[320];
  (void)snprintf (subj, sizeof subj, "work.%s.POISON", base);

  /* Longer than a header so it is refused for its CONTENT, not its size --
     a short frame would exercise the length guard instead. 0xAA never
     matches the magic. */
  unsigned char junk[200];
  memset (junk, 0xAA, sizeof junk);

  int rc = 0;
  for (int i = 0; i < n; i++)
    if (js_Publish (NULL, js, subj, junk, sizeof junk, NULL, NULL) != NATS_OK)
      {
        rc = -1;
        break;
      }

  jsCtx_Destroy (js);
  natsConnection_Destroy (nc);
  return rc;
}

static void
test_unparseable_frame_does_not_wedge_the_queue (void)
{
  printf ("\n-- an unparseable frame is terminated, not left pending --\n");

  char base[96];
  (void)snprintf (base, sizeof base, "poison-%d-%ld", (int)getpid (),
                  (long)time (NULL));
  char ep[160];
  (void)snprintf (ep, sizeof ep, "nats://127.0.0.1:4222/%s", base);

  dp_pub_t *push = dp_push_create (ep, CF32); /* provisions the stream */
  DP_CHECK (push != NULL);

  /* MORE poison than MaxAckPending, because that is the number which
     decides whether the queue stumbles or STOPS: an un-acked frame holds a
     pending slot, and once they are all held the consumer is handed
     nothing ever again. Measured on the unfixed build: exactly 1000 bad
     frames, then DP_ERR_TIMEOUT forever. */
  DP_CHECK (publish_poison (base, POISON_N) == 0);

  /* One good frame BEHIND the poison -- reaching it is the assertion. */
  float _Complex tx[4] = { 1 + 1 * I, 2 + 2 * I, 3 + 3 * I, 4 + 4 * I };
  DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 915e6) == DP_OK);

  dp_sub_t *pull = dp_pull_create (ep);
  DP_CHECK (pull != NULL);
  dp_sub_set_timeout (pull, 3000);

  int bad = 0, reached = 0;
  for (int i = 0; i < POISON_N + 200; i++)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;
      int         rc = dp_sub_recv (pull, &msg, &hdr);
      if (rc == DP_ERR_INVALID)
        {
          bad++;
          continue;
        }
      if (rc == DP_OK)
        {
          reached = 1;
          if (msg)
            {
              (void)dp_msg_ack (msg);
              dp_msg_free (msg);
            }
          break;
        }
      break; /* a timeout here IS the wedge this test exists to catch */
    }

  DP_CHECK_MSG (bad > 1000,
                "the consumer stopped before clearing MaxAckPending worth "
                "of unparseable frames: they were left pending, which is "
                "the wedge -- every slot held by a frame nothing can ack");
  DP_CHECK_MSG (reached,
                "the good frame behind the poison was never delivered: an "
                "unparseable frame must be terminated so the queue moves "
                "past it, or one bad write ends the subject for good");

  dp_sub_destroy (pull);
  dp_pub_destroy (push);
}

/* doppler#1136: the work queue doppler creates itself must carry an age
 * bound, and the units must be the ones NATS means. A work queue drops a
 * frame only when a consumer ACKS it, so without a bound a producer with
 * no consumer is an unbounded FILE-backed disk sink -- 40 GB of it, from
 * repeated test runs. Asserting the VALUE and not merely "nonzero" is the
 * point: MaxAge is nanoseconds, and a seconds-vs-nanoseconds slip would
 * leave a bound a billion times too small, expiring live traffic. */
static void
test_work_queue_is_age_bounded (void)
{
  char ep[128];
  (void)snprintf (ep, sizeof (ep), "nats://127.0.0.1:4222/agecap%d", rand ());
  dp_push_t *push = dp_push_create (ep, CF64);
  DP_CHECK (push != NULL);
  if (!push)
    return;

  /* Read the config back from the broker, not from our own struct. */
  natsConnection *conn = NULL;
  jsCtx          *js   = NULL;
  DP_CHECK (natsConnection_ConnectTo (&conn, "nats://127.0.0.1:4222")
            == NATS_OK);
  DP_CHECK (natsConnection_JetStream (&js, conn, NULL) == NATS_OK);

  char name[256];
  (void)snprintf (name, sizeof (name), "DP_WORK_%s", strrchr (ep, '/') + 1);
  jsStreamInfo *si = NULL;
  natsStatus    s  = js_GetStreamInfo (&si, js, name, NULL, NULL);
  DP_CHECK (s == NATS_OK);
  if (si)
    {
      DP_CHECK (si->Config->MaxAge == DP_WORK_QUEUE_MAX_AGE_NS);
      jsStreamInfo_Destroy (si);
    }

  /* A fan-out publisher has no work queue at all, so asking to delete
     one is a caller error rather than a broker round trip. */
  char pubep[128];
  (void)snprintf (pubep, sizeof (pubep), "nats://127.0.0.1:4222/nojs%d",
                  rand ());
  dp_pub_t *fanout = dp_pub_create (pubep, CF64);
  DP_CHECK (fanout != NULL);
  if (fanout)
    {
      DP_CHECK (dp_ctx_delete_stream (fanout) == DP_ERR_INVALID);
      dp_pub_destroy (fanout);
    }

  /* And the delete entry point actually removes it. */
  DP_CHECK (dp_ctx_delete_stream (push) == DP_OK);
  si = NULL;
  DP_CHECK (js_GetStreamInfo (&si, js, name, NULL, NULL) != NATS_OK);
  if (si)
    jsStreamInfo_Destroy (si);

  jsCtx_Destroy (js);
  natsConnection_Destroy (conn);
  dp_push_destroy (push);
  printf ("  work queue is age-bounded and deletable\n");
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
  test_unparseable_frame_does_not_wedge_the_queue ();
  test_req_rep_roundtrip ();
  test_chunked_pub_sub ();
  test_interrupt_unblocks_recv ();
  test_flush_after_send ();
  test_drain_then_send ();
  test_work_queue_is_age_bounded ();

  printf ("\n");
  DP_TEST_END ("test_stream_nats_core");
}
