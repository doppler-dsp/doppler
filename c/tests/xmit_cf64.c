/*
 * Cross-language integration helper — CF64 transmitter.
 *
 * Usage: xmit_cf64 <port>
 *
 * Binds a PUB socket on tcp://127.0.0.1:<port>, sends exactly one
 * CF64 frame containing 4 known complex samples, then exits.
 * The Python integration test subscribes and verifies the payload.
 *
 * Known payload:
 *   samples : { 1+2j, 3+4j, 5+6j, 7+8j }  (double complex)
 *   sample_rate  : 2 048 000
 *   center_freq  : 1 420 405 752   (hydrogen line, memorable)
 *   sequence     : 0
 */

#include <doppler.h>
#include <stdio.h>
#include <unistd.h>

#define N_SAMPLES   4
#define SAMPLE_RATE 2048000
#define CENTER_FREQ 1420405752

int
main (int argc, char *argv[])
{
  if (argc != 2)
    {
      fprintf (stderr, "usage: xmit_cf64 <port>\n");
      return 1;
    }

  char endpoint[64];
  snprintf (endpoint, sizeof (endpoint), "tcp://127.0.0.1:%s", argv[1]);

  dp_pub *pub = dp_pub_create (endpoint, DP_CF64);
  if (!pub)
    {
      fprintf (stderr, "dp_pub_create failed\n");
      return 1;
    }

  dp_cf64_t samples[N_SAMPLES] = {
    { 1.0, 2.0 },
    { 3.0, 4.0 },
    { 5.0, 6.0 },
    { 7.0, 8.0 },
  };

  /* ZMQ PUB/SUB slow-joiner: subscription filters propagate
   * asynchronously after the subscriber connects.  Send the frame
   * repeatedly (100 ms apart) for up to 3 s — the first few will
   * be dropped, but once the subscription is live Python catches one
   * and the test exits.  The sequence number is held at 0 for every
   * repeat so the receiver can validate it regardless of which
   * repetition arrives. */
  for (int i = 0; i < 30; i++)
    {
      dp_pub_send_cf64 (pub, samples, N_SAMPLES, SAMPLE_RATE, CENTER_FREQ);
      usleep (100000); /* 100 ms */
    }

  dp_pub_destroy (pub);
  return 0;
}
