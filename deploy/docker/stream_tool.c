/*
 * stream_tool — a long-running doppler producer/consumer for the Kubernetes
 * pipeline (deploy/pipeline).  It carries *self-verifying* I/Q: each frame's
 * payload is an MLS/PN sequence (doppler pn object) seeded by the frame index,
 * which the producer assigns as the wire header.sequence.  The consumer reads
 * that sequence, regenerates the same PN, and bit-compares — so a dropped,
 * duplicated, or corrupted frame is caught, not just counted.  The point is to
 * exercise the resilient NATS work-queue tier under KEDA autoscaling and prove
 * no bits are lost.
 *
 *   stream_tool produce    # PN -> BPSK CF32 blocks, PUSH at RATE_HZ frames/s
 *   stream_tool consume    # PULL, regenerate PN, verify, ack; WORK_MS/frame
 *
 * Endpoint from $STREAM_ENDPOINT (e.g. nats://nats.doppler.svc:4222/default).
 * BLOCK samples/frame ($BLOCK, default 1024).  The producer outpaces a single
 * consumer (WORK_MS work/frame), so a backlog builds until KEDA scales
 * consumers out; lag then drains and it scales back in.
 */
#include "pn/pn_core.h"
#include "stream/stream.h"
#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A length-7 MLS (period 127); poly 96 is primitive. Each frame seeds the PN
 * from its index so it is independently regenerable on the consumer. */
#define PN_POLY 96u
#define PN_LEN 7u
#define PN_PERIOD 127u

static const char *
endpoint (void)
{
  const char *ep = getenv ("STREAM_ENDPOINT");
  return ep ? ep : "nats://127.0.0.1:4222/default";
}

static long
env_long (const char *name, long dflt)
{
  const char *v = getenv (name);
  return v ? strtol (v, NULL, 10) : dflt;
}

/* Fill `iq` with the BPSK-mapped PN frame for index `idx` (chip -> +/-1). */
static void
pn_frame (uint64_t idx, size_t n, uint8_t *chips, float complex *iq)
{
  pn_state_t *pn = pn_create (PN_POLY, (uint64_t)(idx % PN_PERIOD) + 1, PN_LEN,
                              PN_GALOIS);
  pn_generate (pn, n, chips);
  pn_destroy (pn);
  for (size_t j = 0; j < n; j++)
    iq[j] = (float)(2 * chips[j] - 1) + 0.0f * I;
}

static int
run_produce (void)
{
  long           rate   = env_long ("RATE_HZ", 500);
  long           block  = env_long ("BLOCK", 1024);
  long           gap_us = rate > 0 ? 1000000L / rate : 0;
  uint8_t       *chips  = malloc ((size_t)block);
  float complex *iq     = malloc ((size_t)block * sizeof (float complex));
  dp_push_t     *p      = dp_push_create (endpoint (), CF32);
  if (!chips || !iq || !p)
    {
      (void)fprintf (stderr, "produce: init failed (%s)\n", endpoint ());
      return 1;
    }
  (void)fprintf (stdout, "produce: %s, %ld-sample PN/CF32 blocks at %ld Hz\n",
                 endpoint (), block, rate);
  (void)fflush (stdout);
  for (uint64_t i = 0;; i++)
    {
      pn_frame (i, (size_t)block, chips, iq); /* self-verifying payload */
      if (dp_push_send_cf32 (p, iq, (size_t)block, 1e6, 1e9) != DP_OK)
        usleep (10000); /* broker hiccup / reconnect — retry shortly */
      if (gap_us)
        usleep ((useconds_t)gap_us);
    }
}

static int
run_consume (void)
{
  long       work_ms = env_long ("WORK_MS", 20);
  uint8_t   *chips   = malloc (1 << 20); /* >= any BLOCK */
  dp_pull_t *pl      = dp_pull_create (endpoint ());
  if (!chips || !pl)
    {
      (void)fprintf (stderr, "consume: init failed (%s)\n", endpoint ());
      return 1;
    }
  dp_pull_set_timeout (pl, 5000);
  (void)fprintf (stdout, "consume: %s, %ld ms/frame\n", endpoint (), work_ms);
  (void)fflush (stdout);
  unsigned long done = 0, mismatch = 0;
  for (;;)
    {
      dp_msg_t   *m;
      dp_header_t h;
      if (dp_pull_recv (pl, &m, &h) != DP_OK)
        continue; /* timeout / waiting on redelivery — keep polling */

      /* Regenerate the PN for this frame's wire sequence and bit-compare. */
      const float complex *got = (const float complex *)dp_msg_data (m);
      size_t               n   = dp_msg_num_samples (m);
      pn_state_t          *pn  = pn_create (
          PN_POLY, (uint64_t)(h.sequence % PN_PERIOD) + 1, PN_LEN, PN_GALOIS);
      pn_generate (pn, n, chips);
      pn_destroy (pn);
      int bad = 0;
      for (size_t j = 0; j < n; j++)
        if (crealf (got[j]) != (float)(2 * chips[j] - 1))
          {
            bad = 1;
            break;
          }
      mismatch += (unsigned)bad;

      if (work_ms)
        usleep ((useconds_t)(work_ms * 1000)); /* simulate processing */
      dp_msg_ack (m); /* at-least-once: ack only after verify + work */
      dp_msg_free (m);
      if (++done % 100 == 0)
        {
          (void)fprintf (stdout,
                         "consume: %lu frames acked, %lu PN mismatch\n", done,
                         mismatch);
          (void)fflush (stdout);
        }
    }
}

int
main (int argc, char **argv)
{
  if (argc == 2 && strcmp (argv[1], "produce") == 0)
    return run_produce ();
  if (argc == 2 && strcmp (argv[1], "consume") == 0)
    return run_consume ();
  (void)fprintf (stderr, "usage: %s produce|consume\n", argv[0]);
  return 2;
}
