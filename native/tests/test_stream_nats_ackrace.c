/*
 * test_stream_nats_ackrace.c -- an ack racing its Pull's destroy (#2016),
 * against a real nats-server.
 *
 * Its own binary, apart from test_stream_nats_core, because it needs the
 * linker's --wrap, which only GNU ld and lld have. Where the hold cannot
 * be linked in, this skips (exit 77) rather than run a race it could not
 * fail -- and without the hold it could not fail on the property it is
 * for. It also skips without a broker, like test_stream_nats_core.
 */
#define DP_TEST_VERBOSE 1
#include "doppler/stream/stream.h"
#include "dp_test.h"

/* For natsMsg_Ack's signature: the wrapper stands in for it. */
#include <nats.h>

#include "doppler/dp_complex.h"
#include "doppler/dp_thread.h"
#include "dp_nats_test.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * test_ack_racing_close: an ack in flight finishes before the teardown
 * begins, and every ack after it is refused.
 *
 * This test links with --wrap=natsMsg_Ack (see
 * native/src/stream/CMakeLists.txt), so doppler's call into nats.c lands
 * in __wrap_natsMsg_Ack first, which HOLDS one ack there: past doppler's
 * closed check, before nats.c reads the subscription. A destroy that waits
 * for an ack in flight cannot return during the hold. One that does not
 * (closed read under the lock, the lock dropped before the ack) returns
 * inside it, and the held ack sees that. Without the hold, such an ack's
 * window is about a microsecond against a teardown tens of microseconds
 * long, so it nearly always won, and the test could not fail.
 * ------------------------------------------------------------------ */
enum
{
  RACE_N = 8
};

/* Long enough that a destroy which does not wait (a few ms; more under
   TSan) returns inside it. */
#define ACK_HOLD_US 500000u

static atomic_int ack_hold_armed;   /* hold the next ack in natsMsg_Ack */
static atomic_int ack_held;         /* an ack is held there             */
static atomic_int ack_hold_saw_end; /* ... and the destroy returned     */
static atomic_int race_destroyed;   /* dp_sub_destroy has returned      */

#ifdef DP_TEST_HOLD_NATS_ACK
natsStatus __real_natsMsg_Ack (natsMsg *msg, jsOptions *opts);
natsStatus __wrap_natsMsg_Ack (natsMsg *msg, jsOptions *opts);

natsStatus
__wrap_natsMsg_Ack (natsMsg *msg, jsOptions *opts)
{
  if (atomic_exchange (&ack_hold_armed, 0))
    {
      atomic_store (&ack_held, 1);
      dp_thread_sleep_us (ACK_HOLD_US);
      if (atomic_load (&race_destroyed))
        {
          /* The teardown did not wait: what nats.c would read is gone. */
          atomic_store (&ack_hold_saw_end, 1);
          return NATS_ILLEGAL_STATE;
        }
    }
  return __real_natsMsg_Ack (msg, opts);
}
#endif

/* Wait up to 20 s for a flag another thread sets; 0 if it never came. */
static int
race_wait (atomic_int *flag)
{
  for (int i = 0; i < 20000; i++)
    {
      if (atomic_load (flag))
        return 1;
      dp_thread_sleep_us (1000);
    }
  return 0;
}

typedef struct
{
  dp_msg_t  *msg[RACE_N];
  int        rc[RACE_N];
  atomic_int first_ok;
} race_t;

DP_THREAD_FN (race_acker, arg)
{
  race_t *r = (race_t *)arg;
  r->rc[0]  = dp_msg_ack (r->msg[0]); /* the Pull is open: DP_OK */
  atomic_store (&ack_hold_armed, 1);
  atomic_store (&r->first_ok, 1);
  r->rc[1] = dp_msg_ack (r->msg[1]); /* in flight as the destroy begins */
  race_wait (&race_destroyed);
  for (int i = 2; i < RACE_N; i++)
    r->rc[i] = dp_msg_ack (r->msg[i]); /* after it: refused */
  DP_THREAD_RETURN;
}

static void
test_ack_racing_close (void)
{
  printf ("\n-- an ack racing a close: done before it, or refused --\n");
  const char *ep = dp_nats_endpoint ("ackrace");

  dp_pub_t *push = dp_push_create (ep, CF32);
  dp_sub_t *pull = dp_pull_create (ep);
  race_t   *r    = calloc (1, sizeof *r);
  int       got  = 0;
  DP_CHECK (push != NULL && pull != NULL && r != NULL);
  dp_nats_settle ();
  if (push && pull && r)
    {
      atomic_init (&r->first_ok, 0);
      float _Complex tx[4] = { 1, 2, 3, 4 };
      for (int i = 0; i < RACE_N; i++)
        DP_CHECK (dp_pub_send_cf32 (push, tx, 4, 48000.0, 0.0) == DP_OK);
      dp_header_t hdr;
      dp_sub_set_timeout (pull, 3000);
      for (; got < RACE_N; got++)
        if (dp_sub_recv (pull, &r->msg[got], &hdr) != DP_OK)
          break;
      DP_CHECK (got == RACE_N);
    }

  dp_thread_t t;
  int         started = 0;
  if (got == RACE_N)
    {
      started = dp_thread_create (&t, race_acker, r) == 0;
      DP_CHECK (started);
    }
  if (started)
    {
      DP_CHECK (race_wait (&r->first_ok));
      DP_CHECK_MSG (race_wait (&ack_held), "an ack is held in natsMsg_Ack");
      dp_sub_destroy (pull);
      pull = NULL;
      atomic_store (&race_destroyed, 1);
      dp_thread_join (t);

      int ok = 0, refused = 0, valid = 1, monotone = 1;
      for (int i = 0; i < RACE_N; i++)
        {
          if (r->rc[i] == DP_OK)
            {
              ok++;
              if (refused)
                monotone = 0; /* an ack went through after one was refused */
            }
          else if (r->rc[i] == DP_ERR_CLOSED)
            refused++;
          else
            valid = 0;
        }
      DP_CHECK_MSG (valid, "every ack is DP_OK or DP_ERR_CLOSED");
      DP_CHECK (monotone);
      DP_CHECK_MSG (ok >= 1 && refused >= 1,
                    "the race saw both an ack done and one refused");
      DP_CHECK (r->rc[0] == DP_OK);
      DP_CHECK_MSG (!atomic_load (&ack_hold_saw_end),
                    "the destroy waited for the ack in flight");
      DP_CHECK_MSG (r->rc[1] == DP_OK, "the ack in flight completed");
      for (int i = 2; i < RACE_N; i++)
        DP_CHECK (r->rc[i] == DP_ERR_CLOSED);
    }

  for (int i = 0; i < got; i++)
    dp_msg_free (r->msg[i]);
  free (r);
  dp_sub_destroy (pull);
  if (push)
    DP_CHECK (dp_ctx_delete_stream (push) == DP_OK); /* leave no residue */
  dp_pub_destroy (push);
}

int
main (void)
{
#ifndef DP_TEST_HOLD_NATS_ACK
  /* Without the hold this race cannot fail: skip, rather than pass. */
  printf ("SKIP: needs -Wl,--wrap=natsMsg_Ack (GNU ld or lld, Linux) to hold "
          "an ack in flight\n");
  return DP_NATS_SKIP;
#endif
  if (!dp_nats_broker_reachable ())
    {
      printf ("SKIP: no nats-server on 127.0.0.1:4222 (run `nats-server "
              "-js`)\n");
      return DP_NATS_SKIP;
    }
  test_ack_racing_close ();
  printf ("\n");
  DP_TEST_END ("test_stream_nats_ackrace");
}
