/*
 * Example transmitter application
 * Generates and transmits complex signal samples
 * Dashboard-style display: clears and reprints periodically
 */

#include <doppler.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 1e6   /* 1 MHz */
#define CENTER_FREQ 2.4e9 /* 2.4 GHz */
#define BUFFER_SIZE 8192
#define SIGNAL_FREQ 10000.0 /* 10 kHz tone */

static volatile int keep_running = 1;

void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

/* Format nanosecond timestamp as HH:MM:SS.mmm */
void
format_timestamp (uint64_t timestamp_ns, char *buf, size_t buf_size)
{
  time_t secs = (time_t)(timestamp_ns / 1000000000ULL);
  unsigned int ms
      = (unsigned int)((timestamp_ns % 1000000000ULL) / 1000000ULL);
  struct tm tm;
#ifdef _WIN32
  localtime_s (&tm, &secs); /* Windows: args are reversed vs localtime_r */
#else
  localtime_r (&secs, &tm);
#endif
  snprintf (buf, buf_size, "%02d:%02d:%02d.%03u", tm.tm_hour, tm.tm_min,
            tm.tm_sec, ms);
}

void
generate_tone_cf64 (dp_cf64_t *samples, size_t num_samples, double freq,
                    double sample_rate, double phase)
{
  for (size_t i = 0; i < num_samples; i++)
    {
      double t = (double)i / sample_rate;
      double angle = 2.0 * M_PI * freq * t + phase;
      samples[i].i = cos (angle);
      samples[i].q = sin (angle);
    }
}

void
generate_tone_ci32 (dp_ci32_t *samples, size_t num_samples, double freq,
                    double sample_rate, double phase)
{
  const int32_t max_val = 2147483647;
  for (size_t i = 0; i < num_samples; i++)
    {
      double t = (double)i / sample_rate;
      double angle = 2.0 * M_PI * freq * t + phase;
      samples[i].i = (int32_t)(cos (angle) * max_val * 0.9);
      samples[i].q = (int32_t)(sin (angle) * max_val * 0.9);
    }
}

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [endpoint [sample_type]]\n", prog);
  printf ("\n");
  printf ("  endpoint     ZMQ PUB bind address  (default: tcp://*:5555)\n");
  printf ("  sample_type  ci32 | cf64 | cf128   (default: cf64)\n");
  printf ("\n");
  printf ("Generates a %.0f kHz complex tone at %.0f MHz centre frequency\n",
          SIGNAL_FREQ / 1e3, CENTER_FREQ / 1e9 * 1000.0);
  printf (
      "and publishes %d-sample packets at %.0f MHz sample rate via ZMQ PUB.\n",
      BUFFER_SIZE, SAMPLE_RATE / 1e6);
  printf ("\n");
  printf ("Examples:\n");
  printf ("  %s                          # bind tcp://*:5555, CF64\n", prog);
  printf ("  %s tcp://*:5556 ci32        # bind port 5556, CI32\n", prog);
  printf ("\n");
  printf ("Press Ctrl+C to stop.\n");
}

int
main (int argc, char *argv[])
{
  const char *endpoint = "tcp://*:5555";
  dp_sample_type_t sample_type = DP_CF64;

  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      print_usage (argv[0]);
      return 0;
    }
  if (argc > 1)
    {
      endpoint = argv[1];
    }
  if (argc > 2)
    {
      if (strcmp (argv[2], "ci32") == 0)
        {
          sample_type = DP_CI32;
        }
      else if (strcmp (argv[2], "cf64") == 0)
        {
          sample_type = DP_CF64;
        }
      else if (strcmp (argv[2], "cf128") == 0)
        {
          sample_type = DP_CF128;
        }
      else
        {
          fprintf (stderr,
                   "Unknown sample type '%s'. Use: ci32, cf64, cf128\n",
                   argv[2]);
          fprintf (stderr, "Run '%s --help' for usage.\n", argv[0]);
          return 1;
        }
    }

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  dp_pub *ctx = dp_pub_create (endpoint, sample_type);
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

  /* Brief pause to allow subscribers to connect */
  sleep (1);

  void *samples = NULL;
  if (sample_type == DP_CI32)
    {
      samples = malloc (BUFFER_SIZE * sizeof (dp_ci32_t));
    }
  else if (sample_type == DP_CF64)
    {
      samples = malloc (BUFFER_SIZE * sizeof (dp_cf64_t));
    }
  else if (sample_type == DP_CF128)
    {
      samples = malloc (BUFFER_SIZE * sizeof (dp_cf128_t));
    }

  if (!samples)
    {
      fprintf (stderr, "Failed to allocate sample buffer\n");
      dp_pub_destroy (ctx);
      return 1;
    }

  uint64_t total_samples = 0;
  uint64_t packet_count = 0;
  double phase = 0.0;

  while (keep_running)
    {
      /* Generate samples */
      if (sample_type == DP_CI32)
        {
          generate_tone_ci32 ((dp_ci32_t *)samples, BUFFER_SIZE, SIGNAL_FREQ,
                              SAMPLE_RATE, phase);
        }
      else if (sample_type == DP_CF64)
        {
          generate_tone_cf64 ((dp_cf64_t *)samples, BUFFER_SIZE, SIGNAL_FREQ,
                              SAMPLE_RATE, phase);
        }

      phase
          = fmod (phase + 2.0 * M_PI * SIGNAL_FREQ * BUFFER_SIZE / SAMPLE_RATE,
                  2.0 * M_PI);

      int rc;
      if (sample_type == DP_CI32)
        {
          rc = dp_pub_send_ci32 (ctx, (dp_ci32_t *)samples, BUFFER_SIZE,
                                 SAMPLE_RATE, CENTER_FREQ);
        }
      else if (sample_type == DP_CF64)
        {
          rc = dp_pub_send_cf64 (ctx, (dp_cf64_t *)samples, BUFFER_SIZE,
                                 SAMPLE_RATE, CENTER_FREQ);
        }
      else
        {
          rc = DP_ERR_INVALID;
        }

      if (rc != DP_OK)
        {
          fprintf (stderr, "Send error: %s\n", dp_strerror (rc));
          break;
        }

      total_samples += BUFFER_SIZE;
      packet_count++;

      /* Dashboard update every ~1 second worth of samples */
      if (total_samples % (uint64_t)SAMPLE_RATE == 0)
        {
          char ts_buf[32];
          uint64_t now = dp_get_timestamp_ns ();
          format_timestamp (now, ts_buf, sizeof (ts_buf));

          double throughput_mb
              = (double)(total_samples * dp_sample_size (sample_type))
                / (1024.0 * 1024.0);

          printf ("\033[2J\033[H");
          printf ("doppler Transmitter\n");
          printf ("========================\n");
          printf ("  Endpoint:     %s\n", endpoint);
          printf ("  Sample Type:  %s\n", dp_sample_type_str (sample_type));
          printf ("  Sample Rate:  %.2f MHz\n", SAMPLE_RATE / 1e6);
          printf ("  Center Freq:  %.2f GHz\n", CENTER_FREQ / 1e9);
          printf ("  Signal Freq:  %.2f kHz\n", SIGNAL_FREQ / 1e3);
          printf ("\n");
          printf ("  Timestamp:    %s\n", ts_buf);
          printf ("  Packets:      %lu\n", (unsigned long)packet_count);
          printf ("  Total:        %lu samples (%.2f MB)\n",
                  (unsigned long)total_samples, throughput_mb);
          printf ("\n");
          printf ("Press Ctrl+C to stop.\n");
          fflush (stdout);
        }

      usleep (8000); /* ~8ms for 8192 samples at 1 MHz */
    }

  printf ("\033[2J\033[H");
  printf ("doppler Transmitter — Shutting down\n");
  printf ("========================================\n");
  printf ("  Total packets:  %lu\n", (unsigned long)packet_count);
  printf ("  Total samples:  %lu\n", (unsigned long)total_samples);

  free (samples);
  dp_pub_destroy (ctx);

  return 0;
}
