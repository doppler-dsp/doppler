/*
 * test_stream_nats_core.c — C-level round-trip tests against a real
 * nats-server, exercising stream.h's dp_pub/sub/req/rep_* directly.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222 -- this is an integration
 * test against a live broker, not a unit test of pure logic.
 */
#define DP_TEST_VERBOSE 1
#include "doppler/stream/stream.h"
#include "dp_test.h"

/* Forging a frame the parser must REJECT is the whole point of the
   poison test below, and no doppler API can produce one -- every
   writer emits a well-formed header. So this one test reaches the
   transport directly. stream_nats.c remains the only place the
   LIBRARY includes it. */
#include <nats.h>

#include "doppler/dp_complex.h"
#include "doppler/dp_thread.h"
#include "dp_nats_test.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------
 * test_pub_sub_roundtrip
 * ------------------------------------------------------------------ */
static void
test_pub_sub_roundtrip (void)
{
  printf ("\n-- PUB/SUB round-trip --\n");
  const char *ep = dp_nats_endpoint ("pubsub");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();

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
  const char *ep = dp_nats_endpoint ("eos");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();

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
  const char *ep = dp_nats_endpoint ("eosack");

  dp_pub_t *push = dp_push_create (ep, CF32);
  DP_CHECK (push != NULL);
  dp_sub_t *pull = dp_pull_create (ep);
  DP_CHECK (pull != NULL);
  dp_nats_settle ();

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
  const char *ep = dp_nats_endpoint ("ctrl");

  dp_rep_t *rep = dp_rep_create (ep);
  DP_CHECK (rep != NULL);
  dp_nats_settle ();

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
  const char  *ep = dp_nats_endpoint ("chunk");
  const size_t n  = 100000; /* 1.6 MB of CF64 > 1 MiB max_payload */

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();

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
DP_THREAD_FN (interrupt_after_delay, arg)
{
  (void)arg;
  dp_thread_sleep_us (400000); /* let the receive get properly blocked first */
  dp_stream_interrupt ();
  DP_THREAD_RETURN;
}

static void
test_interrupt_unblocks_recv (void)
{
  printf ("\n-- interrupt unblocks a blocking recv --\n");
  const char *ep = dp_nats_endpoint ("interrupt");

  dp_stream_resume ();
  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  if (!sub)
    return;
  dp_nats_settle ();

  /* No dp_sub_set_timeout: this blocks, and nothing will ever publish. */
  dp_thread_t th;
  DP_CHECK (dp_thread_create (&th, interrupt_after_delay, NULL) == 0);

  uint64_t t0 = dp_mono_ns ();

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  int         rc = dp_sub_recv (sub, &msg, &hdr);

  uint64_t t1 = dp_mono_ns ();
  dp_thread_join (th);

  double elapsed = (double)(t1 - t0) / 1e9;

  DP_CHECK (rc == DP_ERR_INTERRUPTED);
  DP_CHECK (msg == NULL);
  /* Generous against the 100 ms slice, and tiny against the hour a
     blocking NextMsg would otherwise wait. */
  DP_CHECK (elapsed < 3.0);
  printf ("  returned in %.3f s\n", elapsed);

  /* Sticky: a receive STARTED while the flag is set refuses at once, so a
     signal cannot be missed by racing it. */
  t0 = dp_mono_ns ();
  rc = dp_sub_recv (sub, &msg, &hdr);
  t1 = dp_mono_ns ();
  DP_CHECK (rc == DP_ERR_INTERRUPTED);
  DP_CHECK ((double)(t1 - t0) / 1e9 < 0.5);

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
  const char *ep = dp_nats_endpoint ("flush");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();
  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();
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
 * test_send_refuses_a_count_that_wraps (doppler#2016)
 *
 * The send path sized the payload as num_samples * elem and only THEN
 * compared it with the 32-bit wire limit, so a count near SIZE_MAX
 * wrapped under the limit: the call returned DP_OK and put a header on
 * the wire claiming 2^60 + 2 samples over a 32-byte payload.
 * ------------------------------------------------------------------ */
static void
test_send_refuses_a_count_that_wraps (void)
{
  printf ("\n-- a count whose byte size wraps is refused, not sent --\n");
  const char *ep = dp_nats_endpoint ("wrap");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();
  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();
  if (!sub || !pub)
    {
      dp_pub_destroy (pub);
      dp_sub_destroy (sub);
      return;
    }

  double _Complex tx[2] = { 1, 2 };

  /* 16 * (SIZE_MAX / 16 + 3) = 2^64 + 32, which wraps to exactly the
     32 bytes tx holds. */
  size_t forged = SIZE_MAX / 16 + 3;
  DP_CHECK ((size_t)(forged * 16) == sizeof tx);
  DP_CHECK (dp_pub_send_cf64 (pub, tx, forged, 1.0, 0.0) == DP_ERR_TOO_LARGE);

  /* The plain over-limit count, which never wrapped, is refused too. */
  DP_CHECK (dp_pub_send_cf64 (pub, tx, (size_t)UINT32_MAX / 16 + 1, 1.0, 0.0)
            == DP_ERR_TOO_LARGE);

  /* And the honest count still goes: the refusals are the counts'. */
  DP_CHECK (dp_pub_send_cf64 (pub, tx, 2, 1.0, 0.0) == DP_OK);
  DP_CHECK (dp_pub_flush (pub, 2000) == DP_OK);

  /* The honest frame is the first thing on the wire, not a forged one. */
  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (sub, 1000);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  DP_CHECK (msg != NULL && dp_msg_num_samples (msg) == 2);
  if (msg)
    dp_msg_free (msg);

  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * #2016 item 5: a message's ack after its context is gone.
 *
 * nats.c's ack reads the message's subscription, that subscription's
 * JetStream context and its connection, and a natsMsg keeps none of them
 * alive. dp_msg_ack() after dp_pull_destroy() read freed memory; every
 * message now shares a refcounted link with its context, which the
 * destroy marks closed before tearing anything down.
 * ------------------------------------------------------------------ */
static void
test_ack_after_close_is_refused (void)
{
  printf ("\n-- an ack after its Pull is closed is refused --\n");
  const char *ep = dp_nats_endpoint ("ackclose");

  dp_pub_t *push = dp_push_create (ep, CF32);
  dp_sub_t *pull = dp_pull_create (ep);
  DP_CHECK (push != NULL && pull != NULL);
  dp_nats_settle ();
  if (!push || !pull)
    {
      if (push)
        DP_CHECK (dp_ctx_delete_stream (push) == DP_OK);
      dp_pub_destroy (push);
      dp_sub_destroy (pull);
      return;
    }

  float _Complex tx[4] = { 1, 2, 3, 4 };
  DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 0.0) == DP_OK);
  DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 0.0) == DP_OK);

  dp_msg_t   *m1 = NULL, *m2 = NULL;
  dp_header_t hdr;
  dp_sub_set_timeout (pull, 3000);
  DP_CHECK (dp_sub_recv (pull, &m1, &hdr) == DP_OK);
  DP_CHECK (dp_sub_recv (pull, &m2, &hdr) == DP_OK);
  /* The control: while the Pull is open, an ack goes through. */
  DP_CHECK (m1 && dp_msg_ack (m1) == DP_OK);
  dp_msg_free (m1);

  /* m2 outlives its Pull: the ack is refused, and freeing it is fine. */
  dp_sub_destroy (pull);
  DP_CHECK (m2 && dp_msg_ack (m2) == DP_ERR_CLOSED);
  dp_msg_free (m2);

  /* Refused means not acked: the broker hands it to the next consumer. */
  dp_sub_t *next = dp_pull_create (ep);
  DP_CHECK (next != NULL);
  if (next)
    {
      dp_msg_t *again = NULL;
      dp_sub_set_timeout (next, PULL_ACKWAIT_MS + 3000);
      DP_CHECK_MSG (dp_sub_recv (next, &again, &hdr) == DP_OK,
                    "the frame whose ack was refused is redelivered");
      if (again)
        {
          DP_CHECK (dp_msg_ack (again) == DP_OK);
          dp_msg_free (again);
        }
      dp_sub_destroy (next);
    }
  /* The queue was made up here, so ending it is ours (#1136). */
  DP_CHECK (dp_ctx_delete_stream (push) == DP_OK);
  dp_pub_destroy (push);
}

/* Only a PULL message is a JetStream message, and the header promises a
   no-op ack for every other. nats.c's own ack is wrong for both: a SUB
   message has no reply subject, so it is refused (NATS_ILLEGAL_STATE); a
   REP request has one, so nats.c reads its subscription's NULL JetStream
   context -- undefined, and in a Release build "+ACK" published to the
   requester's inbox, which the requester then reads as its reply. */
static void
test_ack_on_core_nats_is_a_noop (void)
{
  printf ("\n-- an ack on a SUB or REP message is a no-op, before and after "
          "close --\n");
  const char *ep = dp_nats_endpoint ("acknoop");

  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  dp_nats_settle ();
  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  dp_nats_settle ();
  dp_msg_t *msg = NULL;
  if (sub && pub)
    {
      double _Complex tx[2] = { 1, 2 };
      DP_CHECK (dp_pub_send_cf64 (pub, tx, 2, 1.0, 0.0) == DP_OK);
      dp_header_t hdr;
      dp_sub_set_timeout (sub, 3000);
      DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
      DP_CHECK (msg && dp_msg_ack (msg) == DP_OK);
    }
  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
  /* Its context gone, a SUB message still has nothing to acknowledge:
     DP_OK, as a reassembled frame from the same context answers, never the
     Pull's DP_ERR_CLOSED. */
  if (msg)
    {
      DP_CHECK_MSG (dp_msg_ack (msg) == DP_OK,
                    "a SUB ack after its context is destroyed is a no-op");
      dp_msg_free (msg);
    }

  const char *rep_ep = dp_nats_endpoint ("acknooprep");
  dp_rep_t   *rep    = dp_rep_create (rep_ep);
  DP_CHECK (rep != NULL);
  dp_nats_settle ();
  dp_req_t *req = dp_req_create (rep_ep);
  DP_CHECK (req != NULL);
  dp_msg_t *rq = NULL;
  if (rep && req)
    {
      DP_CHECK (dp_req_send (req, "ping", 5) == DP_OK);
      size_t rq_size = 0;
      dp_rep_set_timeout (rep, 3000);
      DP_CHECK (dp_rep_recv (rep, &rq, &rq_size) == DP_OK);
      DP_CHECK (rq && dp_msg_ack (rq) == DP_OK);
      /* The no-op is what the requester sees, too. nats.c's JetStream ack
         on a request reads a NULL JetStream context (undefined: a fault,
         or in an optimised build the dead load dropped) and publishes
         "+ACK" to the request's reply subject, the requester's inbox, so
         its first reply was "+ACK", not the replier's. */
      DP_CHECK (dp_rep_send (rep, "pong", 5) == DP_OK);
      dp_msg_t *rp      = NULL;
      size_t    rp_size = 0;
      dp_req_set_timeout (req, 3000);
      DP_CHECK (dp_req_recv (req, &rp, &rp_size) == DP_OK);
      DP_CHECK_MSG (rp && rp_size == 5
                        && memcmp (dp_msg_data (rp), "pong", 5) == 0,
                    "the requester's first reply is the replier's");
      if (rp)
        dp_msg_free (rp);
    }
  dp_req_destroy (req);
  dp_rep_destroy (rep);
  if (rq)
    {
      DP_CHECK_MSG (dp_msg_ack (rq) == DP_OK,
                    "a REP ack after its context is destroyed is a no-op");
      dp_msg_free (rq);
    }
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
  const char *ep = dp_nats_endpoint ("drain");

  dp_pub_t *pub = dp_pub_create (ep, CF64);
  DP_CHECK (pub != NULL);
  if (!pub)
    return;
  dp_nats_settle ();

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
  (void)snprintf (base, sizeof base, "poison-%llu",
                  (unsigned long long)dp_mono_ns ());
  char ep[160];
  (void)snprintf (ep, sizeof ep, DP_NATS_URL "/%s", base);

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
  const char *ep   = dp_nats_endpoint ("agecap");
  dp_push_t  *push = dp_push_create (ep, CF64);
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
  char pubep[160];
  (void)snprintf (pubep, sizeof (pubep), "%s", dp_nats_endpoint ("nojs"));
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

/* A worker started BEFORE any producer, on a subject no stream has ever
 * existed for. Pull used to only bind to the work-queue stream Push
 * creates, so dp_pull_create returned NULL here -- the first thing a reader
 * following the demos in worker-first order saw (#956). Now either side
 * provisions it. Asserted against the broker, not our struct: the stream
 * must exist after a Pull-only create, and the frame sent afterwards must
 * reach the worker that was waiting for it. */
static void
test_pull_first_provisions_the_work_queue (void)
{
  printf ("\n-- a worker started first provisions the work queue --\n");
  const char *ep = dp_nats_endpoint ("pullfirst");

  dp_sub_t *pull = dp_pull_create (ep);
  DP_CHECK_MSG (pull != NULL,
                "a Pull worker created before any Push must not fail on a "
                "subject whose work-queue stream does not exist yet");
  if (!pull)
    return;

  natsConnection *conn = NULL;
  jsCtx          *js   = NULL;
  DP_CHECK (natsConnection_ConnectTo (&conn, "nats://127.0.0.1:4222")
            == NATS_OK);
  DP_CHECK (natsConnection_JetStream (&js, conn, NULL) == NATS_OK);
  char name[256];
  (void)snprintf (name, sizeof (name), "DP_WORK_%s", strrchr (ep, '/') + 1);
  jsStreamInfo *si = NULL;
  DP_CHECK_MSG (js_GetStreamInfo (&si, js, name, NULL, NULL) == NATS_OK,
                "the Pull side created no stream");
  if (si)
    {
      /* the SAME configuration Push would have created */
      DP_CHECK (si->Config->Retention == js_WorkQueuePolicy);
      DP_CHECK (si->Config->MaxAge == DP_WORK_QUEUE_MAX_AGE_NS);
      jsStreamInfo_Destroy (si);
    }

  /* and the producer that arrives afterwards adopts it, reaching the
     worker that was already waiting */
  dp_pub_t *push = dp_push_create (ep, CF32);
  DP_CHECK (push != NULL);
  if (push)
    {
      float _Complex tx[4] = { 1 + 1 * I, 2 + 2 * I, 3 + 3 * I, 4 + 4 * I };
      DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 915e6) == DP_OK);
      dp_sub_set_timeout (pull, 3000);
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;
      DP_CHECK (dp_sub_recv (pull, &msg, &hdr) == DP_OK);
      DP_CHECK (msg != NULL && hdr.num_samples == 4);
      if (msg)
        {
          DP_CHECK (dp_msg_ack (msg) == DP_OK);
          dp_msg_free (msg);
        }
      DP_CHECK (dp_ctx_delete_stream (push) == DP_OK); /* leave no residue */
      dp_pub_destroy (push);
    }

  jsCtx_Destroy (js);
  natsConnection_Destroy (conn);
  dp_sub_destroy (pull);
}

/* ------------------------------------------------------------------
 * Chunked-frame faults a broker cannot produce on cue (#2010): a frame's
 * chunks are published by hand, through the client, laid out exactly as
 * dp__nats_send_signal lays them out, so a test can stop between two.
 * ------------------------------------------------------------------ */
enum
{
  RAW_COUNT  = 2,  /* chunks per frame                  */
  RAW_STRIDE = 64, /* bytes in every chunk but the last */
  RAW_TOTAL  = 96  /* 64 + 32: CF32, 12 samples         */
};

/* The subject a PUB on endpoint `ep` sends CF32 frames to. */
static void
raw_subject (char *subj, size_t n, const char *ep)
{
  (void)snprintf (subj, n, "iq.%s.CF32", strrchr (ep, '/') + 1);
}

static unsigned char
raw_byte (uint64_t seq, size_t i)
{
  return (unsigned char)(seq * 37u + i);
}

/* Publish chunk `idx` of frame `seq`, and flush it to the server, so that
   whatever is published next -- on any connection -- arrives after it. */
static int
raw_chunk (natsConnection *nc, const char *subj, uint64_t seq, uint32_t idx)
{
  dp_header_t h;
  memset (&h, 0, sizeof h);
  h.magic = DP_STREAM_MAGIC;
  memcpy (h.data_rep, dp_host_rep (), 4);
  h.format       = (uint16_t)CF32;
  h.kind         = (uint16_t)DP_KIND_IQ;
  h.version      = DP_WIRE_VERSION;
  h.flags        = DP_FLAG_CHUNKED;
  h.sequence     = seq;
  h.timestamp_ns = 1700000000000000000ull + seq;
  h.sample_rate  = 1e6;
  uint64_t off   = (uint64_t)idx * RAW_STRIDE;
  size_t len = (idx + 1 < RAW_COUNT) ? RAW_STRIDE : (size_t)(RAW_TOTAL - off);
  h.payload_bytes = (uint32_t)len;
  h.num_samples   = len / 8;
  dp_chunk_t ch   = { 0 };
  ch.index        = idx;
  ch.count        = RAW_COUNT;
  ch.total_bytes  = RAW_TOTAL;
  ch.offset       = off;

  unsigned char buf[sizeof h + sizeof ch + RAW_STRIDE];
  memcpy (buf, &h, sizeof h);
  memcpy (buf + sizeof h, &ch, sizeof ch);
  for (size_t i = 0; i < len; i++)
    buf[sizeof h + sizeof ch + i] = raw_byte (seq, (size_t)off + i);
  if (natsConnection_Publish (nc, subj, buf, (int)(sizeof h + sizeof ch + len))
      != NATS_OK)
    return -1;
  return natsConnection_Flush (nc) == NATS_OK ? 0 : -1;
}

static dp_reasm_stats_t
reasm_stats (const dp_sub_t *sub)
{
  dp_reasm_stats_t st = { 0 };
  DP_CHECK (dp_sub_reasm_stats (sub, &st) == DP_OK);
  return st;
}

/* ------------------------------------------------------------------
 * test_mid_frame_timeout_resumes_the_frame: a receive that times out
 * between two chunks keeps the frame, counts the timeout, and the next
 * receive completes it.
 * ------------------------------------------------------------------ */
static void
test_mid_frame_timeout_resumes_the_frame (void)
{
  printf ("\n-- a timeout between chunks keeps the frame for the next recv "
          "--\n");
  char ep[160], subj[224];
  (void)snprintf (ep, sizeof ep, "%s", dp_nats_endpoint ("midframe"));
  raw_subject (subj, sizeof subj, ep);
  dp_sub_t       *sub = dp_sub_create (ep);
  natsConnection *nc  = NULL;
  DP_CHECK (sub != NULL);
  DP_CHECK (natsConnection_ConnectTo (&nc, DP_NATS_URL) == NATS_OK);
  if (!sub || !nc)
    goto done;
  dp_nats_settle ();
  dp_sub_set_timeout (sub, 200);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  DP_CHECK (raw_chunk (nc, subj, 5, 0) == 0);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_TIMEOUT);
  DP_CHECK (reasm_stats (sub).mid_frame_timeouts == 1);

  DP_CHECK (raw_chunk (nc, subj, 5, 1) == 0);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  if (msg)
    {
      DP_CHECK (dp_msg_num_samples (msg) == RAW_TOTAL / 8);
      const unsigned char *b     = (const unsigned char *)dp_msg_data (msg);
      int                  exact = 1;
      for (size_t i = 0; i < RAW_TOTAL; i++)
        exact &= b[i] == raw_byte (5, i);
      DP_CHECK (exact);
      dp_msg_free (msg);
    }
  DP_CHECK (reasm_stats (sub).abandoned == 0);
done:
  if (nc)
    natsConnection_Destroy (nc);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_a_stale_frame_is_abandoned: with one publisher per subject, an
 * unchunked frame or an end-of-stream after a chunked frame proves that
 * one lost a chunk. It is given up and counted, rather than held for ever
 * with every idle timeout counted as a mid-frame one.
 * ------------------------------------------------------------------ */
static void
test_a_stale_frame_is_abandoned (void)
{
  printf ("\n-- an unchunked frame or EOS abandons a frame that lost a "
          "chunk --\n");
  char ep[160], subj[224];
  (void)snprintf (ep, sizeof ep, "%s", dp_nats_endpoint ("stale"));
  raw_subject (subj, sizeof subj, ep);
  dp_sub_t       *sub = dp_sub_create (ep);
  dp_pub_t       *pub = dp_pub_create (ep, CF32);
  natsConnection *nc  = NULL;
  DP_CHECK (sub != NULL && pub != NULL);
  DP_CHECK (natsConnection_ConnectTo (&nc, DP_NATS_URL) == NATS_OK);
  if (!sub || !pub || !nc)
    goto done;
  dp_nats_settle ();
  dp_sub_set_timeout (sub, 2000);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  float _Complex x[4] = { 1, 2, 3, 4 };
  DP_CHECK (raw_chunk (nc, subj, 50, 0) == 0); /* its chunk 1 never comes */
  DP_CHECK (dp_pub_send_cf32 (pub, x, 4, 1e6, 0.0) == DP_OK);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_OK);
  if (msg)
    {
      DP_CHECK (dp_msg_num_samples (msg) == 4);
      dp_msg_free (msg);
    }
  DP_CHECK (reasm_stats (sub).abandoned == 1);

  dp_sub_set_timeout (sub, 100); /* idle: nothing is held any more */
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_TIMEOUT);
  DP_CHECK (reasm_stats (sub).mid_frame_timeouts == 0);

  dp_sub_set_timeout (sub, 2000);
  DP_CHECK (raw_chunk (nc, subj, 51, 0) == 0);
  DP_CHECK (dp_pub_send_eos (pub) == DP_OK);
  DP_CHECK (dp_sub_recv (sub, &msg, &hdr) == DP_ERR_EOF);
  DP_CHECK (reasm_stats (sub).abandoned == 2);
done:
  if (nc)
    natsConnection_Destroy (nc);
  dp_pub_destroy (pub);
  dp_sub_destroy (sub);
}

/* ------------------------------------------------------------------
 * test_recv_deadline_holds_while_chunks_keep_coming: a receive has ONE
 * deadline however many chunks it reads. Each chunk below starts a frame
 * that never completes (a lost chunk every time, or two publishers
 * interleaving), and one arrives every 10 ms -- well inside the receive's
 * 100 ms timeout. With a fresh timeout per message, the receive never
 * returned while they kept coming.
 * ------------------------------------------------------------------ */
/* `stop` is atomic, not volatile: the flood thread reads it while the main
   thread writes it, and `make test-tsan` runs this binary against a
   broker with halt_on_error -- a harness that trips the tool cannot be
   evidence about the thing it is watching (test_ccsds_tm_rs_race.c). */
typedef struct
{
  const char *subj;
  atomic_int  stop;
} raw_flood_t;

DP_THREAD_FN (raw_flood, arg)
{
  raw_flood_t    *f  = (raw_flood_t *)arg;
  natsConnection *nc = NULL;
  if (natsConnection_ConnectTo (&nc, DP_NATS_URL) == NATS_OK)
    {
      uint64_t until = dp_mono_ns () + 2000000000ull; /* 2 s at most */
      for (uint64_t seq = 1000;
           !atomic_load (&f->stop) && dp_mono_ns () < until; seq++)
        {
          (void)raw_chunk (nc, f->subj, seq, 0);
          dp_thread_sleep_us (10000);
        }
      natsConnection_Destroy (nc);
    }
  DP_THREAD_RETURN;
}

static void
test_recv_deadline_holds_while_chunks_keep_coming (void)
{
  printf ("\n-- recv(timeout) returns on time while chunks keep coming "
          "--\n");
  char ep[160], subj[224];
  (void)snprintf (ep, sizeof ep, "%s", dp_nats_endpoint ("deadline"));
  raw_subject (subj, sizeof subj, ep);
  dp_sub_t *sub = dp_sub_create (ep);
  DP_CHECK (sub != NULL);
  if (!sub)
    return;
  dp_nats_settle ();
  dp_sub_set_timeout (sub, 100);

  raw_flood_t flood;
  flood.subj = subj;
  atomic_init (&flood.stop, 0);
  dp_thread_t th;
  DP_CHECK (dp_thread_create (&th, raw_flood, &flood) == 0);
  dp_thread_sleep_us (100000); /* the flood is under way */

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  uint64_t    t0 = dp_mono_ns ();
  int         rc = dp_sub_recv (sub, &msg, &hdr);
  double      ms = (double)(dp_mono_ns () - t0) / 1e6;
  printf ("  recv returned %d after %.0f ms\n", rc, ms);
  DP_CHECK (rc == DP_ERR_TIMEOUT);
  DP_CHECK_MSG (ms < 600.0, "one 100 ms deadline, not one per chunk");
  DP_CHECK (reasm_stats (sub).mid_frame_timeouts >= 1);
  /* And the flood was arriving all the while: each chunk starts a frame
     that the next one abandons. */
  DP_CHECK (reasm_stats (sub).abandoned >= 5);

  atomic_store (&flood.stop, 1);
  dp_thread_join (th);
  dp_sub_destroy (sub);
}

int
main (void)
{
  if (!dp_nats_broker_reachable ())
    {
      printf ("SKIP: no nats-server on 127.0.0.1:4222 (run `nats-server "
              "-js`)\n");
      return DP_NATS_SKIP;
    }

  test_pub_sub_roundtrip ();
  test_eos_ends_the_stream ();
  test_eos_is_acked_on_the_work_queue ();
  test_pull_first_provisions_the_work_queue ();
  test_unparseable_frame_does_not_wedge_the_queue ();
  test_req_rep_roundtrip ();
  test_chunked_pub_sub ();
  test_interrupt_unblocks_recv ();
  test_flush_after_send ();
  test_send_refuses_a_count_that_wraps ();
  test_ack_after_close_is_refused ();
  test_ack_on_core_nats_is_a_noop ();
  test_drain_then_send ();
  test_work_queue_is_age_bounded ();
  test_mid_frame_timeout_resumes_the_frame ();
  test_a_stale_frame_is_abandoned ();
  test_recv_deadline_holds_while_chunks_keep_coming ();

  printf ("\n");
  DP_TEST_END ("test_stream_nats_core");
}
