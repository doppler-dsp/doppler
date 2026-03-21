/*
 * Example receiver application
 * Receives and displays complex signal samples
 * Dashboard-style display: clears and reprints on every packet
 */

#include <doppler.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile int keep_running = 1;

void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

double
calculate_power_cf64 (const dp_cf64_t *samples, size_t num_samples)
{
  double power = 0.0;
  for (size_t i = 0; i < num_samples; i++)
    {
      power += samples[i].i * samples[i].i + samples[i].q * samples[i].q;
    }
  return power / num_samples;
}

double
calculate_power_ci32 (const dp_ci32_t *samples, size_t num_samples)
{
  double power = 0.0;
  const double scale = 1.0 / 2147483647.0;
  for (size_t i = 0; i < num_samples; i++)
    {
      double i_val = samples[i].i * scale;
      double q_val = samples[i].q * scale;
      power += i_val * i_val + q_val * q_val;
    }
  return power / num_samples;
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
print_samples (const void *samples, dp_sample_type_t type, size_t count)
{
  size_t show = count < 5 ? count : 5;
  printf ("  First %zu samples:\n", show);
  for (size_t i = 0; i < show; i++)
    {
      if (type == DP_CI32)
        {
          const dp_ci32_t *s = (const dp_ci32_t *)samples;
          printf ("    [%zu] I: %d, Q: %d\n", i, s[i].i, s[i].q);
        }
      else if (type == DP_CF64)
        {
          const dp_cf64_t *s = (const dp_cf64_t *)samples;
          printf ("    [%zu] I: %+.6f, Q: %+.6f\n", i, s[i].i, s[i].q);
        }
      else if (type == DP_CF128)
        {
          const dp_cf128_t *s = (const dp_cf128_t *)samples;
          printf ("    [%zu] I: %+.6Lf, Q: %+.6Lf\n", i, s[i].i, s[i].q);
        }
    }
}

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [endpoint]\n", prog);
  printf ("\n");
  printf ("  endpoint  ZMQ SUB connect address  (default: "
          "tcp://localhost:5555)\n");
  printf ("\n");
  printf ("Subscribes to a doppler transmitter and displays a live "
          "dashboard\n");
  printf ("showing signal power, packet statistics, and sample values.\n");
  printf ("\n");
  printf ("Examples:\n");
  printf ("  %s                            # connect to localhost:5555\n",
          prog);
  printf ("  %s tcp://192.168.1.10:5555   # connect to remote host\n", prog);
  printf ("\n");
  printf ("Press Ctrl+C to stop.\n");
}

int
main (int argc, char *argv[])
{
  const char *endpoint = "tcp://localhost:5555";

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

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  dp_sub *ctx = dp_sub_create (endpoint);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create subscriber on %s\n", endpoint);
      return 1;
    }

  printf ("doppler Receiver\n");
  printf ("  Endpoint : %s\n", endpoint);
  printf ("\nWaiting for packets...\n");
  fflush (stdout);

  uint64_t total_samples = 0;
  uint64_t packet_count = 0;
  uint64_t last_sequence = 0;
  uint64_t dropped_packets = 0;

  while (keep_running)
    {
      dp_msg_t *msg = NULL;
      dp_header_t header;

      int rc = dp_sub_recv (ctx, &msg, &header);

      if (rc != DP_OK)
        {
          fprintf (stderr, "Receive error: %s\n", dp_strerror (rc));
          continue;
        }

      size_t num_samples = dp_msg_num_samples (msg);
      dp_sample_type_t sample_type = dp_msg_sample_type (msg);
      void *samples = dp_msg_data (msg);

      packet_count++;
      total_samples += num_samples;

      /* Track drops */
      if (packet_count > 1 && header.sequence != last_sequence + 1)
        {
          dropped_packets += header.sequence - last_sequence - 1;
        }
      last_sequence = header.sequence;

      /* Calculate signal power */
      double power = 0.0;
      if (sample_type == DP_CI32)
        {
          power
              = calculate_power_ci32 ((const dp_ci32_t *)samples, num_samples);
        }
      else if (sample_type == DP_CF64)
        {
          power
              = calculate_power_cf64 ((const dp_cf64_t *)samples, num_samples);
        }
      double power_db = 10.0 * log10 (power + 1e-12);

      /* Format timestamp */
      char ts_buf[32];
      format_timestamp (header.timestamp_ns, ts_buf, sizeof (ts_buf));

      double throughput_mb
          = (double)(total_samples * dp_sample_size (sample_type))
            / (1024.0 * 1024.0);

      /* Clear screen and print dashboard */
      printf ("\033[2J\033[H");
      printf ("doppler Receiver\n");
      printf ("=====================\n");
      printf ("  Endpoint:     %s\n", endpoint);
      printf ("  Sample Type:  %s\n", dp_sample_type_str (sample_type));
      printf ("  Sample Rate:  %.2f MHz\n", header.sample_rate / 1e6);
      printf ("  Center Freq:  %.2f GHz\n", header.center_freq / 1e9);
      printf ("\n");
      printf ("  Sequence:     %lu\n", (unsigned long)header.sequence);
      printf ("  Timestamp:    %s\n", ts_buf);
      printf ("  Num Samples:  %lu\n", (unsigned long)num_samples);
      printf ("  Power:        %.2f dB\n", power_db);
      printf ("\n");
      printf ("  Packets:      %lu\n", (unsigned long)packet_count);
      printf ("  Total:        %lu samples (%.2f MB)\n",
              (unsigned long)total_samples, throughput_mb);
      printf ("  Dropped:      %lu\n", (unsigned long)dropped_packets);
      printf ("\n");
      print_samples (samples, sample_type, num_samples);
      printf ("\n");
      printf ("Press Ctrl+C to stop.\n");
      fflush (stdout);

      dp_msg_free (msg);
    }

  /* Final stats on exit (after clear screen) */
  printf ("\033[2J\033[H");
  printf ("doppler Receiver — Shutting down\n");
  printf ("=====================================\n");
  printf ("  Total packets:   %lu\n", (unsigned long)packet_count);
  printf ("  Total samples:   %lu\n", (unsigned long)total_samples);
  printf ("  Dropped packets: %lu\n", (unsigned long)dropped_packets);
  if (packet_count > 0)
    {
      printf ("  Drop rate:       %.3f%%\n",
              100.0 * dropped_packets / (packet_count + dropped_packets));
    }

  dp_sub_destroy (ctx);

  return 0;
}
