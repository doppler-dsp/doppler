/*
 * Unit tests for doppler streaming library
 *
 * Covers: PUB/SUB, PUSH/PULL, REQ/REP (raw + signal-frame),
 *         dp_msg_t accessors, all sample types (CI32/CF64/CF128),
 *         timeout handling, header field validation, error paths.
 */

#include <doppler.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* =========================================================================
 * Test harness
 * ========================================================================= */

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define ASSERT(cond, msg)                                                     \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: %s (line %d)\n", msg, __LINE__);            \
          return 0;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

#define RUN_TEST(fn)                                                          \
  do                                                                          \
    {                                                                         \
      g_tests_run++;                                                          \
      if (fn ())                                                              \
        g_tests_passed++;                                                     \
    }                                                                         \
  while (0)

/* Port counter — each test gets a unique port to avoid collisions */
static int g_port = 15200;

static const char *
next_endpoint (void)
{
  static char buf[64];
  g_port++;
  snprintf (buf, sizeof (buf), "tcp://127.0.0.1:%d", g_port);
  return buf;
}

static const char *
next_bind_endpoint (void)
{
  static char buf[64];
  g_port++;
  snprintf (buf, sizeof (buf), "tcp://*:%d", g_port);
  return buf;
}

static const char *
last_connect_endpoint (void)
{
  static char buf[64];
  snprintf (buf, sizeof (buf), "tcp://127.0.0.1:%d", g_port);
  return buf;
}

/* =========================================================================
 * 1. Utility tests
 * ========================================================================= */

int
test_sample_types (void)
{
  printf ("  sample type sizes and strings... ");

  ASSERT (dp_sample_size (DP_CI32) == sizeof (dp_ci32_t),
          "CI32 size incorrect");
  ASSERT (dp_sample_size (DP_CF64) == sizeof (dp_cf64_t),
          "CF64 size incorrect");
  ASSERT (dp_sample_size (DP_CF128) == sizeof (dp_cf128_t),
          "CF128 size incorrect");

  ASSERT (strcmp (dp_sample_type_str (DP_CI32), "CI32") == 0,
          "CI32 string incorrect");
  ASSERT (strcmp (dp_sample_type_str (DP_CF64), "CF64") == 0,
          "CF64 string incorrect");
  ASSERT (strcmp (dp_sample_type_str (DP_CF128), "CF128") == 0,
          "CF128 string incorrect");

  printf ("PASS\n");
  return 1;
}

int
test_timestamp (void)
{
  printf ("  timestamp monotonicity... ");

  uint64_t t1 = dp_get_timestamp_ns ();
  usleep (10000); /* 10ms */
  uint64_t t2 = dp_get_timestamp_ns ();

  ASSERT (t2 > t1, "Timestamps not monotonic");
  ASSERT ((t2 - t1) >= 9000000, "Timestamp delta too small"); /* ~10ms */

  printf ("PASS\n");
  return 1;
}

int
test_error_codes (void)
{
  printf ("  error code strings... ");

  ASSERT (dp_strerror (DP_OK) != NULL, "OK string NULL");
  ASSERT (strlen (dp_strerror (DP_OK)) > 0, "OK string empty");
  ASSERT (dp_strerror (DP_ERR_INIT) != NULL, "INIT string NULL");
  ASSERT (dp_strerror (DP_ERR_SEND) != NULL, "SEND string NULL");
  ASSERT (dp_strerror (DP_ERR_RECV) != NULL, "RECV string NULL");
  ASSERT (dp_strerror (DP_ERR_INVALID) != NULL, "INVALID string NULL");
  ASSERT (dp_strerror (DP_ERR_TIMEOUT) != NULL, "TIMEOUT string NULL");
  ASSERT (dp_strerror (DP_ERR_MEMORY) != NULL, "MEMORY string NULL");

  /* Unknown code should still return a string */
  ASSERT (dp_strerror (-999) != NULL, "Unknown code string NULL");

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 2. dp_msg_t accessor tests
 * ========================================================================= */

int
test_msg_null_safety (void)
{
  printf ("  dp_msg_t NULL safety... ");

  /* All accessors should handle NULL without crashing */
  ASSERT (dp_msg_data (NULL) == NULL, "data(NULL) != NULL");
  ASSERT (dp_msg_size (NULL) == 0, "size(NULL) != 0");
  ASSERT (dp_msg_num_samples (NULL) == 0, "num_samples(NULL) != 0");
  /* dp_msg_free(NULL) should not crash */
  dp_msg_free (NULL);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 3. PUB/SUB tests
 * ========================================================================= */

int
test_pub_create_destroy (void)
{
  printf ("  PUB create/destroy... ");

  const char *ep = next_bind_endpoint ();
  dp_pub *pub = dp_pub_create (ep, DP_CF64);
  ASSERT (pub != NULL, "Publisher creation failed");
  dp_pub_destroy (pub);

  /* Destroy NULL should not crash */
  dp_pub_destroy (NULL);

  printf ("PASS\n");
  return 1;
}

int
test_pub_send_no_subscriber (void)
{
  printf ("  PUB send without subscriber... ");

  const char *ep = next_bind_endpoint ();
  dp_pub *pub = dp_pub_create (ep, DP_CF64);
  ASSERT (pub != NULL, "Publisher creation failed");

  dp_cf64_t samples[10];
  for (int i = 0; i < 10; i++)
    {
      samples[i].i = i * 0.1;
      samples[i].q = i * 0.2;
    }

  int rc = dp_pub_send_cf64 (pub, samples, 10, 1e6, 2.4e9);
  ASSERT (rc == DP_OK, "Send failed");

  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

/* Thread data for pub-sub tests */
typedef struct
{
  const char *bind_ep;
  const char *connect_ep;
  dp_sample_type_t type;
  int num_frames;
  volatile int ready;
} thread_data_t;

static void *
pub_cf64_thread (void *arg)
{
  thread_data_t *td = (thread_data_t *)arg;

  dp_pub *pub = dp_pub_create (td->bind_ep, DP_CF64);
  if (!pub)
    return NULL;

  td->ready = 1;
  usleep (200000); /* 200ms for subscriber to connect */

  dp_cf64_t samples[10];
  for (int i = 0; i < 10; i++)
    {
      samples[i].i = i * 0.1;
      samples[i].q = i * 0.2;
    }

  for (int f = 0; f < td->num_frames; f++)
    {
      dp_pub_send_cf64 (pub, samples, 10, 1e6, 2.4e9);
      usleep (10000);
    }

  dp_pub_destroy (pub);
  return NULL;
}

int
test_pubsub_cf64 (void)
{
  printf ("  PUB/SUB CF64 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  thread_data_t td
      = { .bind_ep = bind_ep, .connect_ep = conn_ep, .num_frames = 5 };

  pthread_t pub_tid;
  pthread_create (&pub_tid, NULL, pub_cf64_thread, &td);

  /* Wait for publisher to bind */
  while (!td.ready)
    usleep (10000);
  usleep (50000);

  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (sub != NULL, "Subscriber creation failed");
  dp_sub_set_timeout (sub, 2000);

  usleep (200000); /* let connection establish */

  int received = 0;
  for (int i = 0; i < 5; i++)
    {
      dp_msg_t *msg = NULL;
      dp_header_t hdr;
      int rc = dp_sub_recv (sub, &msg, &hdr);
      if (rc == DP_OK)
        {
          /* Validate dp_msg_t accessors */
          ASSERT (dp_msg_data (msg) != NULL, "msg data NULL");
          ASSERT (dp_msg_num_samples (msg) == 10, "wrong num_samples");
          ASSERT (dp_msg_sample_type (msg) == DP_CF64, "wrong sample_type");
          ASSERT (dp_msg_size (msg) == 10 * sizeof (dp_cf64_t),
                  "wrong msg size");

          /* Validate header fields */
          ASSERT (hdr.magic == 0x53494753, "wrong magic");
          ASSERT (hdr.protocol == DP_PROTO_SIGS, "wrong protocol");
          ASSERT (hdr.stream_id == 0, "wrong stream_id");
          ASSERT (hdr.flags == 0, "wrong flags");
          ASSERT (hdr.sample_type == DP_CF64, "wrong hdr sample_type");
          ASSERT (hdr.sample_rate == 1e6, "wrong sample_rate");
          ASSERT (hdr.center_freq == 2.4e9, "wrong center_freq");
          ASSERT (hdr.num_samples == 10, "wrong hdr num_samples");
          ASSERT (hdr.timestamp_ns > 0, "timestamp_ns is zero");

          /* Verify sample data */
          dp_cf64_t *cf64 = (dp_cf64_t *)dp_msg_data (msg);
          ASSERT (fabs (cf64[0].i - 0.0) < 1e-12, "cf64[0].i wrong");
          ASSERT (fabs (cf64[1].i - 0.1) < 1e-12, "cf64[1].i wrong");
          ASSERT (fabs (cf64[1].q - 0.2) < 1e-12, "cf64[1].q wrong");

          dp_msg_free (msg);
          received++;
        }
    }

  ASSERT (received >= 1, "No packets received");

  dp_sub_destroy (sub);
  pthread_join (pub_tid, NULL);

  printf ("PASS (%d frames)\n", received);
  return 1;
}

int
test_pubsub_ci32 (void)
{
  printf ("  PUB/SUB CI32 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_pub *pub = dp_pub_create (bind_ep, DP_CI32);
  ASSERT (pub != NULL, "Publisher creation failed");

  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (sub != NULL, "Subscriber creation failed");
  dp_sub_set_timeout (sub, 2000);

  usleep (200000);

  dp_ci32_t samples[4];
  for (int i = 0; i < 4; i++)
    {
      samples[i].i = i * 1000;
      samples[i].q = i * 2000;
    }

  dp_pub_send_ci32 (pub, samples, 4, 2e6, 915e6);

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  int rc = dp_sub_recv (sub, &msg, &hdr);
  ASSERT (rc == DP_OK, "Recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CI32, "wrong type");
  ASSERT (dp_msg_num_samples (msg) == 4, "wrong count");
  ASSERT (hdr.sample_rate == 2e6, "wrong rate");
  ASSERT (hdr.center_freq == 915e6, "wrong freq");

  dp_ci32_t *ci32 = (dp_ci32_t *)dp_msg_data (msg);
  ASSERT (ci32[2].i == 2000, "ci32[2].i wrong");
  ASSERT (ci32[2].q == 4000, "ci32[2].q wrong");

  dp_msg_free (msg);
  dp_sub_destroy (sub);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

int
test_pubsub_cf128 (void)
{
  printf ("  PUB/SUB CF128 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_pub *pub = dp_pub_create (bind_ep, DP_CF128);
  ASSERT (pub != NULL, "Publisher creation failed");

  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (sub != NULL, "Subscriber creation failed");
  dp_sub_set_timeout (sub, 2000);

  usleep (200000);

  dp_cf128_t samples[3];
  for (int i = 0; i < 3; i++)
    {
      samples[i].i = (long double)i * 1.1L;
      samples[i].q = (long double)i * 2.2L;
    }

  dp_pub_send_cf128 (pub, samples, 3, 5e6, 1.42e9);

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  int rc = dp_sub_recv (sub, &msg, &hdr);
  ASSERT (rc == DP_OK, "Recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CF128, "wrong type");
  ASSERT (dp_msg_num_samples (msg) == 3, "wrong count");
  ASSERT (hdr.sample_type == DP_CF128, "wrong hdr type");

  dp_cf128_t *cf128 = (dp_cf128_t *)dp_msg_data (msg);
  ASSERT (fabsl (cf128[1].i - 1.1L) < 1e-12L, "cf128[1].i wrong");
  ASSERT (fabsl (cf128[1].q - 2.2L) < 1e-12L, "cf128[1].q wrong");

  dp_msg_free (msg);
  dp_sub_destroy (sub);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

int
test_pubsub_multiple_subscribers (void)
{
  printf ("  PUB/SUB multiple subscribers... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_pub *pub = dp_pub_create (bind_ep, DP_CF64);
  ASSERT (pub != NULL, "Publisher creation failed");

  dp_sub *sub1 = dp_sub_create (conn_ep);
  dp_sub *sub2 = dp_sub_create (conn_ep);
  ASSERT (sub1 != NULL && sub2 != NULL, "Subscriber creation failed");
  dp_sub_set_timeout (sub1, 2000);
  dp_sub_set_timeout (sub2, 2000);

  usleep (200000);

  dp_cf64_t samples[5];
  for (int i = 0; i < 5; i++)
    {
      samples[i].i = i;
      samples[i].q = i + 10;
    }
  dp_pub_send_cf64 (pub, samples, 5, 1e6, 0);

  dp_msg_t *m1 = NULL, *m2 = NULL;
  dp_header_t h1, h2;
  ASSERT (dp_sub_recv (sub1, &m1, &h1) == DP_OK, "sub1 recv failed");
  ASSERT (dp_sub_recv (sub2, &m2, &h2) == DP_OK, "sub2 recv failed");
  ASSERT (dp_msg_num_samples (m1) == 5, "sub1 count wrong");
  ASSERT (dp_msg_num_samples (m2) == 5, "sub2 count wrong");

  dp_msg_free (m1);
  dp_msg_free (m2);
  dp_sub_destroy (sub1);
  dp_sub_destroy (sub2);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

int
test_pubsub_sequence (void)
{
  printf ("  PUB/SUB sequence numbers... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_pub *pub = dp_pub_create (bind_ep, DP_CF64);
  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (pub && sub, "creation failed");
  dp_sub_set_timeout (sub, 2000);

  usleep (200000);

  dp_cf64_t s[1] = { { 1.0, 2.0 } };

  for (int i = 0; i < 3; i++)
    dp_pub_send_cf64 (pub, s, 1, 1e6, 0);

  for (int i = 0; i < 3; i++)
    {
      dp_msg_t *msg = NULL;
      dp_header_t hdr;
      int rc = dp_sub_recv (sub, &msg, &hdr);
      ASSERT (rc == DP_OK, "recv failed");
      ASSERT ((int)hdr.sequence == i, "wrong sequence");
      dp_msg_free (msg);
    }

  dp_sub_destroy (sub);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 4. SUB timeout test
 * ========================================================================= */

int
test_sub_timeout (void)
{
  printf ("  SUB timeout... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_pub *pub = dp_pub_create (bind_ep, DP_CF64);
  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (pub && sub, "creation failed");

  dp_sub_set_timeout (sub, 100); /* 100ms timeout */

  usleep (200000);

  /* Don't send anything — recv should timeout */
  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  int rc = dp_sub_recv (sub, &msg, &hdr);
  ASSERT (rc == DP_ERR_TIMEOUT, "expected DP_ERR_TIMEOUT");
  ASSERT (msg == NULL, "msg should be NULL on timeout");

  dp_sub_destroy (sub);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 5. PUSH/PULL tests
 * ========================================================================= */

int
test_pushpull_cf64 (void)
{
  printf ("  PUSH/PULL CF64 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CF64);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 2000);

  usleep (100000);

  dp_cf64_t samples[8];
  for (int i = 0; i < 8; i++)
    {
      samples[i].i = i * 0.5;
      samples[i].q = i * -0.5;
    }

  int rc = dp_push_send_cf64 (push, samples, 8, 3e6, 1.575e9);
  ASSERT (rc == DP_OK, "Push send failed");

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_pull_recv (pull, &msg, &hdr);
  ASSERT (rc == DP_OK, "Pull recv failed");

  ASSERT (dp_msg_num_samples (msg) == 8, "wrong count");
  ASSERT (dp_msg_sample_type (msg) == DP_CF64, "wrong type");
  ASSERT (hdr.sample_rate == 3e6, "wrong rate");
  ASSERT (hdr.center_freq == 1.575e9, "wrong freq");
  ASSERT (hdr.magic == 0x53494753, "wrong magic");

  dp_cf64_t *d = (dp_cf64_t *)dp_msg_data (msg);
  ASSERT (fabs (d[3].i - 1.5) < 1e-12, "d[3].i wrong");
  ASSERT (fabs (d[3].q - (-1.5)) < 1e-12, "d[3].q wrong");

  dp_msg_free (msg);
  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

int
test_pushpull_ci32 (void)
{
  printf ("  PUSH/PULL CI32 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CI32);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 2000);

  usleep (100000);

  dp_ci32_t samples[3]
      = { { 100, 200 }, { -300, 400 }, { 2147483647, -2147483647 } };

  int rc = dp_push_send_ci32 (push, samples, 3, 1e6, 0);
  ASSERT (rc == DP_OK, "Push send failed");

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_pull_recv (pull, &msg, &hdr);
  ASSERT (rc == DP_OK, "Pull recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CI32, "wrong type");

  dp_ci32_t *d = (dp_ci32_t *)dp_msg_data (msg);
  ASSERT (d[0].i == 100, "d[0].i wrong");
  ASSERT (d[1].i == -300, "d[1].i wrong");
  ASSERT (d[2].i == 2147483647, "d[2].i wrong (max int32)");

  dp_msg_free (msg);
  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

int
test_pushpull_cf128 (void)
{
  printf ("  PUSH/PULL CF128 roundtrip... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CF128);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 2000);

  usleep (100000);

  dp_cf128_t samples[2]
      = { { 3.14159265358979L, 2.71828182845904L }, { -1.0L, 0.0L } };

  int rc = dp_push_send_cf128 (push, samples, 2, 10e6, 0);
  ASSERT (rc == DP_OK, "Push send failed");

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_pull_recv (pull, &msg, &hdr);
  ASSERT (rc == DP_OK, "Pull recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CF128, "wrong type");
  ASSERT (dp_msg_num_samples (msg) == 2, "wrong count");

  dp_cf128_t *d = (dp_cf128_t *)dp_msg_data (msg);
  ASSERT (fabsl (d[0].i - 3.14159265358979L) < 1e-12L, "d[0].i wrong");

  dp_msg_free (msg);
  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

int
test_pushpull_multiple_frames (void)
{
  printf ("  PUSH/PULL multiple frames in order... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CF64);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 2000);

  usleep (100000);

  /* Send 5 frames, each with a distinguishing value */
  for (int f = 0; f < 5; f++)
    {
      dp_cf64_t s[1] = { { (double)f, (double)(f * 10) } };
      dp_push_send_cf64 (push, s, 1, 1e6, 0);
    }

  /* Receive and verify ordering */
  for (int f = 0; f < 5; f++)
    {
      dp_msg_t *msg = NULL;
      dp_header_t hdr;
      int rc = dp_pull_recv (pull, &msg, &hdr);
      ASSERT (rc == DP_OK, "recv failed");
      ASSERT ((int)hdr.sequence == f, "wrong sequence");

      dp_cf64_t *d = (dp_cf64_t *)dp_msg_data (msg);
      ASSERT (fabs (d[0].i - (double)f) < 1e-12, "wrong I value");
      ASSERT (fabs (d[0].q - (double)(f * 10)) < 1e-12, "wrong Q value");

      dp_msg_free (msg);
    }

  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

int
test_pull_timeout (void)
{
  printf ("  PULL timeout... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CF64);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 100);

  usleep (100000);

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  int rc = dp_pull_recv (pull, &msg, &hdr);
  ASSERT (rc == DP_ERR_TIMEOUT, "expected timeout");
  ASSERT (msg == NULL, "msg should be NULL");

  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 6. REQ/REP — raw bytes
 * ========================================================================= */

int
test_reqrep_raw (void)
{
  printf ("  REQ/REP raw bytes... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_rep *rep = dp_rep_create (bind_ep);
  dp_req *req = dp_req_create (conn_ep);
  ASSERT (rep && req, "creation failed");
  dp_rep_set_timeout (rep, 2000);
  dp_req_set_timeout (req, 2000);

  usleep (100000);

  /* REQ sends raw bytes */
  const char *request = "HELLO";
  int rc = dp_req_send (req, request, strlen (request));
  ASSERT (rc == DP_OK, "req send failed");

  /* REP receives */
  dp_msg_t *msg = NULL;
  size_t size = 0;
  rc = dp_rep_recv (rep, &msg, &size);
  ASSERT (rc == DP_OK, "rep recv failed");
  ASSERT (size == 5, "wrong recv size");
  ASSERT (memcmp (dp_msg_data (msg), "HELLO", 5) == 0, "data mismatch");
  dp_msg_free (msg);

  /* REP replies */
  const char *reply = "WORLD";
  rc = dp_rep_send (rep, reply, strlen (reply));
  ASSERT (rc == DP_OK, "rep send failed");

  /* REQ receives reply */
  msg = NULL;
  size = 0;
  rc = dp_req_recv (req, &msg, &size);
  ASSERT (rc == DP_OK, "req recv failed");
  ASSERT (size == 5, "wrong reply size");
  ASSERT (memcmp (dp_msg_data (msg), "WORLD", 5) == 0, "reply mismatch");
  dp_msg_free (msg);

  dp_req_destroy (req);
  dp_rep_destroy (rep);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 7. REQ/REP — signal frames
 * ========================================================================= */

int
test_reqrep_signal_cf64 (void)
{
  printf ("  REQ/REP signal-frame CF64... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_rep *rep = dp_rep_create (bind_ep);
  dp_req *req = dp_req_create (conn_ep);
  ASSERT (rep && req, "creation failed");
  dp_rep_set_timeout (rep, 2000);
  dp_req_set_timeout (req, 2000);

  usleep (100000);

  /* REQ sends signal frame */
  dp_cf64_t req_samples[4];
  for (int i = 0; i < 4; i++)
    {
      req_samples[i].i = i * 1.0;
      req_samples[i].q = i * -1.0;
    }
  int rc = dp_req_send_cf64 (req, req_samples, 4, 2e6, 1.42e9);
  ASSERT (rc == DP_OK, "req signal send failed");

  /* REP receives signal frame */
  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_rep_recv_signal (rep, &msg, &hdr);
  ASSERT (rc == DP_OK, "rep signal recv failed");
  ASSERT (dp_msg_num_samples (msg) == 4, "wrong count");
  ASSERT (dp_msg_sample_type (msg) == DP_CF64, "wrong type");
  ASSERT (hdr.sample_rate == 2e6, "wrong rate");
  ASSERT (hdr.center_freq == 1.42e9, "wrong freq");

  dp_cf64_t *d = (dp_cf64_t *)dp_msg_data (msg);
  ASSERT (fabs (d[2].i - 2.0) < 1e-12, "d[2].i wrong");
  ASSERT (fabs (d[2].q - (-2.0)) < 1e-12, "d[2].q wrong");
  dp_msg_free (msg);

  /* REP replies with signal frame */
  dp_cf64_t rep_samples[2] = { { 99.0, 88.0 }, { 77.0, 66.0 } };
  rc = dp_rep_send_cf64 (rep, rep_samples, 2, 5e6, 0);
  ASSERT (rc == DP_OK, "rep signal send failed");

  /* REQ receives signal reply */
  msg = NULL;
  rc = dp_req_recv_signal (req, &msg, &hdr);
  ASSERT (rc == DP_OK, "req signal recv failed");
  ASSERT (dp_msg_num_samples (msg) == 2, "wrong reply count");

  d = (dp_cf64_t *)dp_msg_data (msg);
  ASSERT (fabs (d[0].i - 99.0) < 1e-12, "reply d[0].i wrong");
  ASSERT (fabs (d[1].q - 66.0) < 1e-12, "reply d[1].q wrong");
  dp_msg_free (msg);

  dp_req_destroy (req);
  dp_rep_destroy (rep);

  printf ("PASS\n");
  return 1;
}

int
test_reqrep_signal_ci32 (void)
{
  printf ("  REQ/REP signal-frame CI32... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_rep *rep = dp_rep_create (bind_ep);
  dp_req *req = dp_req_create (conn_ep);
  ASSERT (rep && req, "creation failed");
  dp_rep_set_timeout (rep, 2000);
  dp_req_set_timeout (req, 2000);

  usleep (100000);

  dp_ci32_t req_samples[2] = { { 100, 200 }, { -300, 400 } };
  int rc = dp_req_send_ci32 (req, req_samples, 2, 1e6, 0);
  ASSERT (rc == DP_OK, "req send failed");

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_rep_recv_signal (rep, &msg, &hdr);
  ASSERT (rc == DP_OK, "rep recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CI32, "wrong type");
  ASSERT (dp_msg_num_samples (msg) == 2, "wrong count");

  dp_ci32_t *d = (dp_ci32_t *)dp_msg_data (msg);
  ASSERT (d[0].i == 100 && d[0].q == 200, "d[0] wrong");
  ASSERT (d[1].i == -300 && d[1].q == 400, "d[1] wrong");
  dp_msg_free (msg);

  /* Reply */
  dp_ci32_t rep_samples[1] = { { 42, 43 } };
  rc = dp_rep_send_ci32 (rep, rep_samples, 1, 1e6, 0);
  ASSERT (rc == DP_OK, "rep send failed");

  msg = NULL;
  rc = dp_req_recv_signal (req, &msg, &hdr);
  ASSERT (rc == DP_OK, "req recv failed");
  d = (dp_ci32_t *)dp_msg_data (msg);
  ASSERT (d[0].i == 42 && d[0].q == 43, "reply wrong");
  dp_msg_free (msg);

  dp_req_destroy (req);
  dp_rep_destroy (rep);

  printf ("PASS\n");
  return 1;
}

int
test_reqrep_signal_cf128 (void)
{
  printf ("  REQ/REP signal-frame CF128... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_rep *rep = dp_rep_create (bind_ep);
  dp_req *req = dp_req_create (conn_ep);
  ASSERT (rep && req, "creation failed");
  dp_rep_set_timeout (rep, 2000);
  dp_req_set_timeout (req, 2000);

  usleep (100000);

  dp_cf128_t req_samples[1] = { { 1.23456789012345L, 9.87654321098765L } };
  int rc = dp_req_send_cf128 (req, req_samples, 1, 1e6, 0);
  ASSERT (rc == DP_OK, "req send failed");

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  rc = dp_rep_recv_signal (rep, &msg, &hdr);
  ASSERT (rc == DP_OK, "rep recv failed");
  ASSERT (dp_msg_sample_type (msg) == DP_CF128, "wrong type");

  dp_cf128_t *d = (dp_cf128_t *)dp_msg_data (msg);
  ASSERT (fabsl (d[0].i - 1.23456789012345L) < 1e-12L, "I wrong");
  ASSERT (fabsl (d[0].q - 9.87654321098765L) < 1e-12L, "Q wrong");
  dp_msg_free (msg);

  /* Reply */
  dp_cf128_t rep_samples[1] = { { 0.0L, 0.0L } };
  rc = dp_rep_send_cf128 (rep, rep_samples, 1, 1e6, 0);
  ASSERT (rc == DP_OK, "rep send failed");

  msg = NULL;
  rc = dp_req_recv_signal (req, &msg, &hdr);
  ASSERT (rc == DP_OK, "req recv failed");
  dp_msg_free (msg);

  dp_req_destroy (req);
  dp_rep_destroy (rep);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 8. Error / edge cases
 * ========================================================================= */

int
test_create_null_endpoint (void)
{
  printf ("  NULL endpoint handling... ");

  ASSERT (dp_pub_create (NULL, DP_CF64) == NULL, "pub(NULL) not NULL");
  ASSERT (dp_sub_create (NULL) == NULL, "sub(NULL) not NULL");
  ASSERT (dp_push_create (NULL, DP_CF64) == NULL, "push(NULL) not NULL");
  ASSERT (dp_pull_create (NULL) == NULL, "pull(NULL) not NULL");
  ASSERT (dp_req_create (NULL) == NULL, "req(NULL) not NULL");
  ASSERT (dp_rep_create (NULL) == NULL, "rep(NULL) not NULL");

  printf ("PASS\n");
  return 1;
}

int
test_send_invalid_args (void)
{
  printf ("  send with invalid args... ");

  const char *ep = next_bind_endpoint ();
  dp_pub *pub = dp_pub_create (ep, DP_CF64);
  ASSERT (pub != NULL, "pub creation failed");

  /* NULL samples */
  ASSERT (dp_pub_send_cf64 (pub, NULL, 10, 1e6, 0) == DP_ERR_INVALID,
          "NULL samples not rejected");

  /* Zero count */
  dp_cf64_t s[1] = { { 1.0, 2.0 } };
  ASSERT (dp_pub_send_cf64 (pub, s, 0, 1e6, 0) == DP_ERR_INVALID,
          "zero count not rejected");

  /* NULL context */
  ASSERT (dp_pub_send_cf64 (NULL, s, 1, 1e6, 0) == DP_ERR_INVALID,
          "NULL ctx not rejected");

  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

int
test_destroy_null (void)
{
  printf ("  destroy(NULL) safety... ");

  /* None of these should crash */
  dp_pub_destroy (NULL);
  dp_sub_destroy (NULL);
  dp_push_destroy (NULL);
  dp_pull_destroy (NULL);
  dp_req_destroy (NULL);
  dp_rep_destroy (NULL);

  printf ("PASS\n");
  return 1;
}

int
test_recv_invalid_args (void)
{
  printf ("  recv with invalid args... ");

  /* NULL context */
  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  ASSERT (dp_sub_recv (NULL, &msg, &hdr) == DP_ERR_INVALID,
          "NULL ctx not rejected");

  /* NULL msg pointer */
  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();
  dp_pub *pub = dp_pub_create (bind_ep, DP_CF64);
  dp_sub *sub = dp_sub_create (conn_ep);
  ASSERT (dp_sub_recv (sub, NULL, &hdr) == DP_ERR_INVALID,
          "NULL msg not rejected");

  dp_sub_destroy (sub);
  dp_pub_destroy (pub);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * 9. Header field validation
 * ========================================================================= */

int
test_header_fields (void)
{
  printf ("  header field completeness... ");

  const char *bind_ep = next_bind_endpoint ();
  const char *conn_ep = last_connect_endpoint ();

  dp_push *push = dp_push_create (bind_ep, DP_CF64);
  dp_pull *pull = dp_pull_create (conn_ep);
  ASSERT (push && pull, "creation failed");
  dp_pull_set_timeout (pull, 2000);

  usleep (100000);

  dp_cf64_t s[1] = { { 1.0, 2.0 } };
  dp_push_send_cf64 (push, s, 1, 44100.0, 88200.0);

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  memset (&hdr, 0xFF, sizeof (hdr)); /* fill with garbage first */
  int rc = dp_pull_recv (pull, &msg, &hdr);
  ASSERT (rc == DP_OK, "recv failed");

  /* Validate every header field */
  ASSERT (hdr.magic == 0x53494753, "magic");
  ASSERT (hdr.version == 0x00010000, "version");
  ASSERT (hdr.protocol == DP_PROTO_SIGS, "protocol");
  ASSERT (hdr.stream_id == 0, "stream_id");
  ASSERT (hdr.sample_type == DP_CF64, "sample_type");
  ASSERT (hdr.flags == 0, "flags");
  ASSERT (hdr.sequence == 0, "sequence");
  ASSERT (hdr.timestamp_ns > 0, "timestamp_ns");
  ASSERT (hdr.sample_rate == 44100.0, "sample_rate");
  ASSERT (hdr.center_freq == 88200.0, "center_freq");
  ASSERT (hdr.num_samples == 1, "num_samples");
  ASSERT (hdr.reserved[0] == 0, "reserved[0]");
  ASSERT (hdr.reserved[1] == 0, "reserved[1]");
  ASSERT (hdr.reserved[2] == 0, "reserved[2]");
  ASSERT (hdr.reserved[3] == 0, "reserved[3]");

  dp_msg_free (msg);
  dp_pull_destroy (pull);
  dp_push_destroy (push);

  printf ("PASS\n");
  return 1;
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  printf ("\n========================================\n");
  printf ("doppler streaming unit tests\n");
  printf ("========================================\n\n");

  printf ("[Utilities]\n");
  RUN_TEST (test_sample_types);
  RUN_TEST (test_timestamp);
  RUN_TEST (test_error_codes);

  printf ("\n[dp_msg_t accessors]\n");
  RUN_TEST (test_msg_null_safety);

  printf ("\n[PUB/SUB]\n");
  RUN_TEST (test_pub_create_destroy);
  RUN_TEST (test_pub_send_no_subscriber);
  RUN_TEST (test_pubsub_cf64);
  RUN_TEST (test_pubsub_ci32);
  RUN_TEST (test_pubsub_cf128);
  RUN_TEST (test_pubsub_multiple_subscribers);
  RUN_TEST (test_pubsub_sequence);

  printf ("\n[SUB timeout]\n");
  RUN_TEST (test_sub_timeout);

  printf ("\n[PUSH/PULL]\n");
  RUN_TEST (test_pushpull_cf64);
  RUN_TEST (test_pushpull_ci32);
  RUN_TEST (test_pushpull_cf128);
  RUN_TEST (test_pushpull_multiple_frames);
  RUN_TEST (test_pull_timeout);

  printf ("\n[REQ/REP — raw bytes]\n");
  RUN_TEST (test_reqrep_raw);

  printf ("\n[REQ/REP — signal frames]\n");
  RUN_TEST (test_reqrep_signal_cf64);
  RUN_TEST (test_reqrep_signal_ci32);
  RUN_TEST (test_reqrep_signal_cf128);

  printf ("\n[Error handling]\n");
  RUN_TEST (test_create_null_endpoint);
  RUN_TEST (test_send_invalid_args);
  RUN_TEST (test_destroy_null);
  RUN_TEST (test_recv_invalid_args);

  printf ("\n[Header validation]\n");
  RUN_TEST (test_header_fields);

  printf ("\n========================================\n");
  printf ("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
  printf ("========================================\n\n");

  return (g_tests_passed == g_tests_run) ? 0 : 1;
}
