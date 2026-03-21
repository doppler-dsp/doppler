// C/examples/pipeline_demo.c
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <doppler.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ipc:// is POSIX-only; Windows ZMQ only supports tcp:// and inproc:// */
#ifdef _WIN32
const char *ENDPOINT = "tcp://127.0.0.1:15100";
#else
const char *ENDPOINT = "ipc:///tmp/dp_pipeline.ipc";
#endif
const int NUM_BATCHES = 100;
const int SAMPLES_PER_BATCH = 1024;

// Producer thread
void *
producer_thread (void *arg)
{
  (void)arg;

  printf ("Producer: Starting...\n");

  // Create PUSH context (binds)
  dp_push *ctx = dp_push_create (ENDPOINT, DP_CF64);
  if (!ctx)
    {
      fprintf (stderr, "Producer: Failed to create PUSH context\n");
      return NULL;
    }

  // Allow time for connection
  dp_usleep (100000);

  // Allocate samples
  dp_cf64_t *samples = malloc (SAMPLES_PER_BATCH * sizeof (dp_cf64_t));
  if (!samples)
    {
      dp_push_destroy (ctx);
      return NULL;
    }

  for (int batch = 0; batch < NUM_BATCHES; batch++)
    {
      // Generate synthetic signal
      for (int i = 0; i < SAMPLES_PER_BATCH; i++)
        {
          double t = 2.0 * M_PI * (double)i / SAMPLES_PER_BATCH;
          double freq_mod
              = 1.0 + 0.5 * sin (2.0 * M_PI * (double)batch / NUM_BATCHES);

          samples[i].i = 0.8 * cos (freq_mod * t);
          samples[i].q = 0.8 * sin (freq_mod * t);
        }

      // Send
      int rc = dp_push_send_cf64 (ctx, samples, SAMPLES_PER_BATCH, 1e6, 2.4e9);
      if (rc != DP_OK)
        {
          fprintf (stderr, "Producer: Send error: %s\n", dp_strerror (rc));
          break;
        }

      if ((batch + 1) % 10 == 0)
        {
          printf ("Producer: Sent batch %d/%d\n", batch + 1, NUM_BATCHES);
        }

      dp_usleep (1000); /* 1 ms throttle */
    }

  printf ("Producer: Finished.\n");

  free (samples);
  dp_push_destroy (ctx);
  return NULL;
}

/* Compute mean signal power for a CF64 buffer (I^2+Q^2 averaged). */
static double
mean_power_cf64 (const dp_cf64_t *s, size_t n)
{
  double acc = 0.0;
  for (size_t i = 0; i < n; i++)
    acc += s[i].i * s[i].i + s[i].q * s[i].q;
  return acc / (double)n;
}

// Consumer thread
void *
consumer_thread (void *arg)
{
  (void)arg;

  /* Wait briefly for producer to bind */
  dp_usleep (50000);

  printf ("Consumer: Starting — connecting to %s\n", ENDPOINT);

  dp_pull *ctx = dp_pull_create (ENDPOINT);
  if (!ctx)
    {
      fprintf (stderr, "Consumer: Failed to create PULL context\n");
      return NULL;
    }

  int batches_received = 0;
  uint64_t total_samples = 0;
  double power_sum = 0.0;

  for (int i = 0; i < NUM_BATCHES; i++)
    {
      dp_msg_t *msg = NULL;
      dp_header_t header;

      int rc = dp_pull_recv (ctx, &msg, &header);
      if (rc != DP_OK)
        {
          fprintf (stderr, "Consumer: Recv error: %s\n", dp_strerror (rc));
          break;
        }

      size_t num_samples = dp_msg_num_samples (msg);
      dp_sample_type_t type = dp_msg_sample_type (msg);

      batches_received++;
      total_samples += num_samples;

      /* Compute and accumulate signal power */
      double pwr = 0.0;
      if (type == DP_CF64)
        pwr = mean_power_cf64 ((const dp_cf64_t *)dp_msg_data (msg),
                               num_samples);
      power_sum += pwr;

      /* Print header info on first batch, then every 10 */
      if (i == 0)
        {
          printf ("Consumer: First batch — type=%s  count=%zu  rate=%.2f MHz  "
                  "seq=%llu\n",
                  dp_sample_type_str (type), num_samples,
                  header.sample_rate / 1e6,
                  (unsigned long long)header.sequence);
        }
      if ((i + 1) % 10 == 0)
        {
          printf ("Consumer: batch %3d/%d  power=%.4f (%.2f dB)\n",
                  batches_received, NUM_BATCHES, pwr,
                  10.0 * log10 (pwr + 1e-12));
        }

      dp_msg_free (msg);
    }

  double mean_pwr
      = (batches_received > 0) ? power_sum / batches_received : 0.0;
  double total_kb = (double)(total_samples * sizeof (dp_cf64_t)) / 1024.0;

  printf ("Consumer: Done — %d/%d batches  %llu samples  %.2f KB  "
          "mean power=%.4f (%.2f dB)\n",
          batches_received, NUM_BATCHES, (unsigned long long)total_samples,
          total_kb, mean_pwr, 10.0 * log10 (mean_pwr + 1e-12));

  dp_pull_destroy (ctx);
  return NULL;
}

int
main (int argc, char *argv[])
{
  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      printf ("Usage: %s\n", argv[0]);
      printf ("\n");
      printf ("Demonstrates ZMQ PUSH/PULL (pipeline) pattern using "
              "doppler.\n");
      printf ("\n");
      printf ("Two threads are spawned in-process:\n");
      printf ("  Producer  PUSH  binds    %s\n", ENDPOINT);
      printf ("  Consumer  PULL  connects %s\n", ENDPOINT);
      printf ("\n");
      printf ("The producer sends %d batches of %d CF64 samples each,\n",
              NUM_BATCHES, SAMPLES_PER_BATCH);
      printf ("using a frequency-modulated complex tone.  The consumer\n");
      printf ("receives and validates every batch and prints per-batch\n");
      printf ("signal power with a final throughput summary.\n");
      return 0;
    }

  printf ("=== doppler Pipeline Demo ===\n");
  printf ("  Transport : %s\n", ENDPOINT);
  printf ("  Batches   : %d x %d CF64 samples\n", NUM_BATCHES,
          SAMPLES_PER_BATCH);
  printf ("  Data      : %.2f KB total\n",
          (double)(NUM_BATCHES * SAMPLES_PER_BATCH * sizeof (dp_cf64_t))
              / 1024.0);
  printf ("\n");

  pthread_t prod_tid, cons_tid;

  pthread_create (&prod_tid, NULL, producer_thread, NULL);
  pthread_create (&cons_tid, NULL, consumer_thread, NULL);

  pthread_join (prod_tid, NULL);
  pthread_join (cons_tid, NULL);

  printf ("\nDemo complete.\n");
  return 0;
}
