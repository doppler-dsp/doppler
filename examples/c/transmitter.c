/*
 * transmitter.c — ZMQ PUB transmitter example.
 *
 * Generates a complex tone and publishes 8192-sample packets via a
 * ZMQ PUB socket.  Supports CF64 and CI32 wire types.
 *
 * Usage:
 *   transmitter [endpoint [sample_type]]
 *   transmitter                          # tcp://*:5555, CF64
 *   transmitter tcp://*:5556 ci32        # port 5556, CI32
 *
 * Press Ctrl+C to stop.
 *
 * Build:
 *   make build
 *   ./build/native/examples/transmitter
 */

#include <doppler.h>
#include <stream/stream.h>

#include <complex.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define dp_usleep(us) Sleep ((DWORD)((us) / 1000))
#else
#include <unistd.h>
#define dp_usleep(us) usleep ((useconds_t)(us))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 1e6   /* 1 MHz          */
#define CENTER_FREQ 2.4e9 /* 2.4 GHz        */
#define BUFFER_SIZE 8192
#define SIGNAL_FREQ 10000.0 /* 10 kHz tone    */

static volatile int keep_running = 1;

static void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

static void
format_timestamp (uint64_t ts_ns, char *buf, size_t buf_size)
{
  time_t       secs = (time_t)(ts_ns / 1000000000ULL);
  unsigned int ms   = (unsigned int)((ts_ns % 1000000000ULL) / 1000000ULL);
  struct tm    tm;
#ifdef _WIN32
  localtime_s (&tm, &secs);
#else
  localtime_r (&secs, &tm);
#endif
  snprintf (buf, buf_size, "%02d:%02d:%02d.%03u", tm.tm_hour, tm.tm_min,
            tm.tm_sec, ms);
}

static void
generate_tone_cf64 (double _Complex *samples, size_t n, double freq,
                    double sample_rate, double phase)
{
  for (size_t i = 0; i < n; i++)
    {
      double angle = 2.0 * M_PI * freq * (double)i / sample_rate + phase;
      samples[i]   = (cos (angle)) + (sin (angle)) * _Complex_I;
    }
}

static void
generate_tone_ci32 (int32_t *samples, size_t n, double freq,
                    double sample_rate, double phase)
{
  const int32_t max_val = 2147483647;
  for (size_t i = 0; i < n; i++)
    {
      double angle       = 2.0 * M_PI * freq * (double)i / sample_rate + phase;
      samples[2 * i]     = (int32_t)(cos (angle) * max_val * 0.9);
      samples[2 * i + 1] = (int32_t)(sin (angle) * max_val * 0.9);
    }
}

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [endpoint [sample_type]]\n", prog);
  printf ("\n");
  printf ("  endpoint     ZMQ PUB bind address  (default: tcp://*:5555)\n");
  printf ("  sample_type  ci32 | cf64           (default: cf64)\n");
  printf ("\n");
  printf ("Publishes a %.0f kHz tone at %.0f MHz sample rate.\n",
          SIGNAL_FREQ / 1e3, SAMPLE_RATE / 1e6);
  printf ("Press Ctrl+C to stop.\n");
}

int
main (int argc, char *argv[])
{
  const char      *endpoint    = "tcp://*:5555";
  dp_sample_type_t sample_type = CF64;

  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      print_usage (argv[0]);
      return 0;
    }
  if (argc > 1)
    endpoint = argv[1];
  if (argc > 2)
    {
      if (strcmp (argv[2], "ci32") == 0)
        sample_type = CI32;
      else if (strcmp (argv[2], "cf64") == 0)
        sample_type = CF64;
      else
        {
          fprintf (stderr, "Unknown type '%s'. Use: ci32, cf64\n", argv[2]);
          return 1;
        }
    }

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  dp_pub_t *ctx = dp_pub_create (endpoint, sample_type);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create publisher on %s\n", endpoint);
      return 1;
    }

  printf ("doppler Transmitter\n");
  printf ("  Endpoint    : %s\n", endpoint);
  printf ("  Sample type : %s\n", dp_sample_type_str (sample_type));
  printf ("  Sample rate : %.2f MHz\n", SAMPLE_RATE / 1e6);
  printf ("  Centre freq : %.4f GHz\n", CENTER_FREQ / 1e9);
  printf ("  Signal freq : %.1f kHz\n", SIGNAL_FREQ / 1e3);
  printf ("  Packet size : %d samples\n", BUFFER_SIZE);
  printf ("\nWaiting 1 s for subscribers to connect...\n");
  fflush (stdout);

  dp_usleep (1000000);

  void *samples = NULL;
  if (sample_type == CI32)
    samples = malloc (BUFFER_SIZE * 2 * sizeof (int32_t));
  else
    samples = malloc (BUFFER_SIZE * sizeof (double _Complex));

  if (!samples)
    {
      fputs ("Failed to allocate sample buffer\n", stderr);
      dp_pub_destroy (ctx);
      return 1;
    }

  uint64_t total_samples = 0;
  uint64_t packet_count  = 0;
  double   phase         = 0.0;

  while (keep_running)
    {
      int rc;
      if (sample_type == CI32)
        {
          generate_tone_ci32 ((int32_t *)samples, BUFFER_SIZE, SIGNAL_FREQ,
                              SAMPLE_RATE, phase);
          rc = dp_pub_send_ci32 (ctx, (int32_t *)samples, BUFFER_SIZE,
                                 SAMPLE_RATE, CENTER_FREQ);
        }
      else
        {
          generate_tone_cf64 ((double _Complex *)samples, BUFFER_SIZE,
                              SIGNAL_FREQ, SAMPLE_RATE, phase);
          rc = dp_pub_send_cf64 (ctx, (double _Complex *)samples, BUFFER_SIZE,
                                 SAMPLE_RATE, CENTER_FREQ);
        }

      if (rc != DP_OK)
        {
          fprintf (stderr, "Send error: %s\n", dp_strerror (rc));
          break;
        }

      phase
          = fmod (phase + 2.0 * M_PI * SIGNAL_FREQ * BUFFER_SIZE / SAMPLE_RATE,
                  2.0 * M_PI);
      total_samples += BUFFER_SIZE;
      packet_count++;

      if (total_samples % (uint64_t)SAMPLE_RATE == 0)
        {
          char ts[32];
          format_timestamp (dp_get_timestamp_ns (), ts, sizeof (ts));
          double mb = (double)(total_samples * dp_sample_size (sample_type))
                      / (1024.0 * 1024.0);
          printf ("\033[2J\033[H");
          printf ("doppler Transmitter\n====================\n");
          printf ("  Timestamp:  %s\n", ts);
          printf ("  Packets:    %lu\n", (unsigned long)packet_count);
          printf ("  Total:      %lu samples (%.2f MB)\n",
                  (unsigned long)total_samples, mb);
          printf ("\nPress Ctrl+C to stop.\n");
          fflush (stdout);
        }

      dp_usleep (8000); /* ~8 ms throttle for 8192 samples @ 1 MHz */
    }

  free (samples);
  dp_pub_destroy (ctx);
  return 0;
}
