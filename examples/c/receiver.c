/*
 * receiver.c — NATS SUB receiver example.
 *
 * Subscribes to a doppler transmitter and displays a live dashboard
 * showing signal power, packet statistics, and first few samples.
 * Requires a running nats-server (e.g. `nats-server -js`).
 *
 * Usage:
 *   receiver [endpoint]
 *   receiver                                   # nats://127.0.0.1:4222/iq
 *   receiver nats://broker.example:4222/iq     # remote broker
 *
 * Press Ctrl+C to stop.
 *
 * Build:
 *   make build
 *   ./build/native/examples/receiver
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

static volatile int keep_running = 1;

static void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

static double
power_cf64 (const double _Complex *s, size_t n)
{
  double p = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double re = creal (s[i]), im = cimag (s[i]);
      p += re * re + im * im;
    }
  return p / (double)n;
}

static double
power_ci32 (const int32_t *s, size_t n)
{
  const double scale = 1.0 / 2147483647.0;
  double       p     = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double re = s[2 * i] * scale, im = s[2 * i + 1] * scale;
      p += re * re + im * im;
    }
  return p / (double)n;
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
print_samples (const void *samples, dp_sample_type_t type, size_t count)
{
  size_t show = count < 5 ? count : 5;
  printf ("  First %zu samples:\n", show);
  for (size_t i = 0; i < show; i++)
    {
      if (type == CI32)
        {
          const int32_t *s = (const int32_t *)samples;
          printf ("    [%zu] I: %d, Q: %d\n", i, s[2 * i], s[2 * i + 1]);
        }
      else if (type == CF64)
        {
          const double _Complex *s = (const double _Complex *)samples;
          printf ("    [%zu] I: %+.6f, Q: %+.6f\n", i, creal (s[i]),
                  cimag (s[i]));
        }
      else if (type == CF128)
        {
          const long double _Complex *s
              = (const long double _Complex *)samples;
          printf ("    [%zu] I: %+.6Lf, Q: %+.6Lf\n", i, creall (s[i]),
                  cimagl (s[i]));
        }
    }
}

int
main (int argc, char *argv[])
{
  const char *endpoint = "nats://127.0.0.1:4222/iq";

  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      printf ("Usage: %s [endpoint]\n\n", argv[0]);
      printf ("  endpoint  NATS SUB endpoint"
              "  (default: nats://127.0.0.1:4222/iq)\n\n");
      printf ("Press Ctrl+C to stop.\n");
      return 0;
    }
  if (argc > 1)
    endpoint = argv[1];

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  dp_sub_t *ctx = dp_sub_create (endpoint);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create subscriber on %s\n", endpoint);
      return 1;
    }

  printf ("doppler Receiver\n  Endpoint: %s\n\nWaiting for packets...\n",
          endpoint);
  fflush (stdout);

  uint64_t total_samples = 0;
  uint64_t packet_count  = 0;
  uint64_t last_seq      = 0;
  uint64_t dropped       = 0;

  while (keep_running)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;

      if (dp_sub_recv (ctx, &msg, &hdr) != DP_OK)
        continue;

      size_t           n    = dp_msg_num_samples (msg);
      dp_sample_type_t type = dp_msg_sample_type (msg);
      void            *data = dp_msg_data (msg);

      packet_count++;
      total_samples += n;

      if (packet_count > 1 && hdr.sequence != last_seq + 1)
        dropped += hdr.sequence - last_seq - 1;
      last_seq = hdr.sequence;

      double pwr = 0.0;
      if (type == CI32)
        pwr = power_ci32 ((const int32_t *)data, n);
      else if (type == CF64)
        pwr = power_cf64 ((const double _Complex *)data, n);
      double pwr_db = 10.0 * log10 (pwr + 1e-12);

      char ts[32];
      format_timestamp (hdr.timestamp_ns, ts, sizeof (ts));

      double mb = (double)(total_samples * dp_sample_size (type))
                  / (1024.0 * 1024.0);

      printf ("\033[2J\033[H");
      printf ("doppler Receiver\n================\n");
      printf ("  Endpoint:     %s\n", endpoint);
      printf ("  Sample Type:  %s\n", dp_sample_type_str (type));
      printf ("  Sample Rate:  %.2f MHz\n", hdr.sample_rate / 1e6);
      printf ("  Center Freq:  %.2f GHz\n", hdr.center_freq / 1e9);
      printf ("\n");
      printf ("  Sequence:     %lu\n", (unsigned long)hdr.sequence);
      printf ("  Timestamp:    %s\n", ts);
      printf ("  Num Samples:  %lu\n", (unsigned long)n);
      printf ("  Power:        %.2f dB\n", pwr_db);
      printf ("\n");
      printf ("  Packets:      %lu\n", (unsigned long)packet_count);
      printf ("  Total:        %lu samples (%.2f MB)\n",
              (unsigned long)total_samples, mb);
      printf ("  Dropped:      %lu\n", (unsigned long)dropped);
      printf ("\n");
      print_samples (data, type, n);
      printf ("\nPress Ctrl+C to stop.\n");
      fflush (stdout);

      dp_msg_free (msg);
    }

  dp_sub_destroy (ctx);
  return 0;
}
