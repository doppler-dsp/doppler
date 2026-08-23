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
  /* Unblock a receive that is parked inside the NATS client. Setting the
     flag below is not enough on its own: the loop that reads it is exactly
     what a blocking dp_sub_recv is keeping us out of, so with no traffic
     arriving this handler would run and nothing would happen. Both lines
     are needed -- this one to get back to the loop, the flag to tell the
     loop why. dp_stream_interrupt is async-signal-safe by construction. */
  dp_stream_interrupt ();
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
  uint64_t total_bytes   = 0;
  uint64_t packet_count  = 0;
  uint64_t last_seq      = 0;
  uint64_t dropped       = 0;

  /* Throughput is measured from the FIRST frame, not from start-up: the
     wait for a sender to appear is not part of the rate, and counting it
     makes the number climb for a minute after the stream begins. */
  uint64_t t_first_ns = 0;

  /* End-to-end latency, from the sender's own dp_header_t.timestamp_ns to
     the moment this process holds the samples. Both stamps are
     CLOCK_REALTIME, so the figure is only meaningful when the two ends
     share a clock -- the same host, or hosts disciplined by PTP/NTP.
     Across an undisciplined pair it measures clock offset, not transport,
     which is why it is labelled rather than left to be misread. */
  double lat_sum_ms = 0.0;
  double lat_min_ms = 1e300;
  double lat_max_ms = 0.0;

  /* No timeout: this recv BLOCKS, which is what a dashboard wants -- it
     has nothing to do between frames. That is only safe because the
     handler calls dp_stream_interrupt(), which is what brings us back
     here when a human asks to stop. A caller that would rather not
     depend on the interrupt can bound the wait instead
     (dp_sub_set_timeout) and treat DP_ERR_TIMEOUT as "check my flag";
     both work, and doing NEITHER is the bug this example shipped with. */
  while (keep_running)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;

      int rc = dp_sub_recv (ctx, &msg, &hdr);
      if (rc == DP_ERR_INTERRUPTED)
        break; /* Ctrl+C: keep_running is already 0 */
      if (rc != DP_OK)
        continue;

      uint64_t         now  = dp_get_timestamp_ns ();
      size_t           n    = dp_msg_num_samples (msg);
      dp_sample_type_t type = dp_msg_sample_type (msg);
      void            *data = dp_msg_data (msg);

      packet_count++;
      total_samples += n;
      total_bytes += hdr.payload_bytes;
      if (t_first_ns == 0)
        t_first_ns = now;

      /* Unsigned arithmetic: a sender's clock that is AHEAD of ours would
         wrap this to something enormous, so the sign is checked first and
         a negative reading reported as zero rather than as 1.8e10 ms. */
      double lat_ms = (now > hdr.timestamp_ns)
                          ? (double)(now - hdr.timestamp_ns) / 1e6
                          : 0.0;
      lat_sum_ms += lat_ms;
      if (lat_ms < lat_min_ms)
        lat_min_ms = lat_ms;
      if (lat_ms > lat_max_ms)
        lat_max_ms = lat_ms;

      if (packet_count > 1 && hdr.sequence != last_seq + 1)
        dropped += hdr.sequence - last_seq - 1;
      last_seq = hdr.sequence;

      /* One call, whatever the wire carried: dp_msg_mean_power normalises
         the integer formats by full scale, so this is dBFS either way and
         the example does not branch on the type to say so. */
      double pwr_db = 10.0 * log10 (dp_msg_mean_power (msg) + 1e-12);

      char ts[32];
      format_timestamp (hdr.timestamp_ns, ts, sizeof (ts));

      double mb = (double)total_bytes / (1024.0 * 1024.0);
      double secs
          = (now > t_first_ns) ? (double)(now - t_first_ns) / 1e9 : 0.0;
      double msps = (secs > 0.0) ? (double)total_samples / secs / 1e6 : 0.0;
      double mbps = (secs > 0.0) ? mb / secs : 0.0;

      char fmt[3] = { 0, 0, 0 };
      dp_format_chars (type, fmt);

      printf ("\033[2J\033[H");
      printf ("doppler Receiver\n================\n");
      printf ("  Endpoint:     %s\n", endpoint);
      printf ("  Sample Rate:  %.2f MHz\n", hdr.sample_rate / 1e6);
      printf ("  Center Freq:  %.2f GHz\n", hdr.center_freq / 1e9);
      printf ("\n");
      /* The frame header as it is on the wire -- the fields a receiver
         written against docs/design/streaming.md would be parsing. */
      printf ("  -- frame header (v%u) --\n", (unsigned)hdr.version);
      printf ("  format:       %s (\"%s\", %zu B/sample)\n",
              dp_sample_type_str (type), fmt, dp_sample_size (type));
      printf ("  kind:         %s\n",
              hdr.kind == DP_KIND_TLM ? "TLM (records)" : "IQ (samples)");
      printf ("  flags:        0x%04X%s\n", (unsigned)hdr.flags,
              (hdr.flags & DP_FLAG_CHUNKED) ? "  CHUNKED" : "");
      printf ("  data_rep:     %.4s\n", hdr.data_rep);
      printf ("  sequence:     %lu\n", (unsigned long)hdr.sequence);
      printf ("  timestamp:    %s\n", ts);
      printf ("  num_samples:  %lu\n", (unsigned long)n);
      printf ("  payload:      %lu bytes\n", (unsigned long)hdr.payload_bytes);
      printf ("\n");
      printf ("  Power:        %.2f dBFS\n", pwr_db);
      printf ("  Rate:         %.2f MS/s  (%.1f MB/s)\n", msps, mbps);
      printf ("  Latency:      %.3f ms   (min %.3f, mean %.3f, max %.3f)\n",
              lat_ms, lat_min_ms, lat_sum_ms / (double)packet_count,
              lat_max_ms);
      printf ("                one-way, sender clock -> here; only\n");
      printf ("                meaningful if both share a clock\n");
      printf ("\n");
      printf ("  Packets:      %lu\n", (unsigned long)packet_count);
      printf ("  Total:        %lu samples (%.2f MB in %.1f s)\n",
              (unsigned long)total_samples, mb, secs);
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
