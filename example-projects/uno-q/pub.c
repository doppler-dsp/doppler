/* uno_q_pub — put an RTL-SDR's samples on doppler's NATS wire.
 *
 *     rtl_sdr -f FC -s FS - | uno_q_pub --nats URL --fs FS --fc FC
 *     uno_q_pub --nats URL --synthetic SECONDS        (the self-test scene)
 *
 * Reads cu8 (the dongle's unsigned offset-binary bytes) from stdin, or
 * synthesises the uno-q self-test scene, and publishes it in frames of
 * --frame complex samples. Every frame carries doppler's wire header: a
 * sequence number, the capture time, the sample rate and the centre
 * frequency — what rtl_tcp's stream does not have, and what lets the
 * receiver count lost and repeated frames exactly.
 *
 * ON THE WIRE IT IS ci8. The wire's formats are BLUE codes, and BLUE has no
 * unsigned byte, so each code goes out as (x - 128): exact, and bit-identical
 * downstream — I8ToF32 at scale 128 of (x - 128) is U8ToF32's `shift` of x.
 *
 *     --nats URL        endpoint, e.g. nats://127.0.0.1:4222/rtl (required)
 *     --pattern P       `pub` (default; core pub/sub) or `push` (JetStream
 *                       work queue, server-acked, at-least-once)
 *     --fs HZ           sample rate for the header (default 2.4e6)
 *     --fc HZ           centre frequency for the header (default 0: unknown)
 *     --frame N         complex samples per frame (default 32768)
 *     --synthetic S     publish S seconds of the self-test scene instead of
 *                       stdin, paced at real time
 *     --offset HZ       the scene's tone offset (default 250e3)
 *
 * A send the broker refuses is counted and reported, never retried
 * silently: on `push` that is the broker saying it did not take the frame.
 * The run ends with an end-of-stream frame and a drain, so everything sent
 * is on the server before the program exits.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doppler/stream/stream.h"
#include "scene.h"

static double
now_s (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static void
sleep_until (double t)
{
  double d = t - now_s ();
  if (d <= 0.0)
    return;
  struct timespec ts = { (time_t)d, (long)((d - (double)(time_t)d) * 1e9) };
  nanosleep (&ts, NULL);
}

int
main (int argc, char **argv)
{
  const char *url = NULL, *pattern = "pub";
  double      fs = 2.4e6, fc = 0.0, synthetic = 0.0, offset = 250e3;
  size_t      frame = 32768;

  for (int i = 1; i < argc; i++)
    {
      const char *a       = argv[i];
      int         has_val = i + 1 < argc;
      if (strcmp (a, "--nats") == 0 && has_val)
        url = argv[++i];
      else if (strcmp (a, "--pattern") == 0 && has_val)
        pattern = argv[++i];
      else if (strcmp (a, "--fs") == 0 && has_val)
        fs = strtod (argv[++i], NULL);
      else if (strcmp (a, "--fc") == 0 && has_val)
        fc = strtod (argv[++i], NULL);
      else if (strcmp (a, "--frame") == 0 && has_val)
        frame = (size_t)strtoul (argv[++i], NULL, 10);
      else if (strcmp (a, "--synthetic") == 0 && has_val)
        synthetic = strtod (argv[++i], NULL);
      else if (strcmp (a, "--offset") == 0 && has_val)
        offset = strtod (argv[++i], NULL);
      else
        {
          (void)fprintf (stderr,
                         "usage: %s --nats URL [--pattern pub|push] [--fs HZ] "
                         "[--fc HZ]\n          [--frame N] [--synthetic S] "
                         "[--offset HZ]\n",
                         argv[0]);
          return 2;
        }
    }
  int push = strcmp (pattern, "push") == 0;
  if (!url || frame == 0 || !(fs > 0.0) || (!push && strcmp (pattern, "pub")))
    {
      (void)fprintf (stderr, "need --nats URL, --frame > 0, --fs > 0 and "
                             "--pattern pub|push\n");
      return 2;
    }

  /* PUSH and PUB are the same send-capable context underneath; only the
     constructor differs. */
  dp_pub_t *pub = push ? (dp_pub_t *)dp_push_create (url, CI8)
                       : dp_pub_create (url, CI8);
  if (!pub)
    {
      (void)fprintf (stderr, "cannot connect to %s\n", url);
      return 1;
    }

  uint8_t *scene   = NULL;
  size_t   scene_n = 0;
  if (synthetic > 0.0)
    {
      scene_n = (size_t)(synthetic * fs);
      scene   = scene_cu8 (fs, offset, scene_n);
      if (!scene)
        return 1;
    }

  uint8_t *cu8 = malloc (2 * frame);
  int8_t  *ci8 = malloc (2 * frame);
  if (!cu8 || !ci8)
    return 1;

  size_t sent = 0, refused = 0, samples = 0;
  double t0 = now_s ();
  for (;;)
    {
      size_t pairs;
      if (scene)
        {
          if (samples >= scene_n)
            break;
          pairs = scene_n - samples < frame ? scene_n - samples : frame;
          memcpy (cu8, scene + 2 * samples, 2 * pairs);
          sleep_until (t0 + (double)samples / fs); /* real-time pacing */
        }
      else
        {
          size_t got = fread (cu8, 2, frame, stdin); /* whole I/Q pairs */
          if (got == 0)
            break;
          pairs = got;
        }
      for (size_t i = 0; i < 2 * pairs; i++)
        ci8[i] = (int8_t)((int)cu8[i] - 128);
      int rc = push ? dp_push_send_ci8 ((dp_push_t *)pub, ci8, pairs, fs, fc)
                    : dp_pub_send_ci8 (pub, ci8, pairs, fs, fc);
      if (rc == DP_OK)
        sent++;
      else
        refused++;
      samples += pairs;
    }

  dp_pub_send_eos (pub);
  int drained = dp_stream_drain (pub, 5000);
  if (push)
    dp_push_destroy ((dp_push_t *)pub);
  else
    dp_pub_destroy (pub);

  printf ("uno_q_pub (%s): %zu frames sent, %zu refused, %zu samples in "
          "%.2f s; drain %s\n",
          push ? "push" : "pub", sent, refused, samples, now_s () - t0,
          drained == DP_OK ? "ok" : "timed out");
  free (scene);
  free (cu8);
  free (ci8);
  return refused || drained != DP_OK ? 1 : 0;
}
