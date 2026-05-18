/*
 * pipeline_demo.c — ZMQ PUSH/PULL pipeline demo.
 *
 * Two threads in-process:
 *   Producer  PUSH  binds   ipc:///tmp/dp_pipeline.ipc  (tcp on Windows)
 *   Consumer  PULL  connects the same endpoint
 *
 * The producer sends 100 batches of 1024 CF64 samples using a
 * frequency-modulated complex tone.  The consumer receives every batch,
 * computes signal power, and prints a summary.
 *
 * Build:
 *   make build
 *   ./build/native/examples/pipeline_demo
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <doppler.h>
#include <stream/stream.h>

#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#  define dp_usleep(us) Sleep ((DWORD)((us) / 1000))
const char *ENDPOINT = "tcp://127.0.0.1:15100";
#else
#  include <unistd.h>
#  define dp_usleep(us) usleep ((useconds_t)(us))
const char *ENDPOINT = "ipc:///tmp/dp_pipeline.ipc";
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_BATCHES      100
#define SAMPLES_PER_BATCH 1024

static void *
producer_thread (void *arg)
{
  (void)arg;
  printf ("Producer: starting...\n");

  dp_push *ctx = dp_push_create (ENDPOINT, DP_CF64);
  if (!ctx) { fputs ("Producer: dp_push_create failed\n", stderr); return NULL; }

  dp_usleep (100000); /* allow consumer to connect */

  double _Complex *samples = malloc (SAMPLES_PER_BATCH * sizeof (double _Complex));
  if (!samples) { dp_push_destroy (ctx); return NULL; }

  for (int batch = 0; batch < NUM_BATCHES; batch++)
    {
      for (int i = 0; i < SAMPLES_PER_BATCH; i++)
        {
          double t = 2.0 * M_PI * (double)i / SAMPLES_PER_BATCH;
          double fm = 1.0 + 0.5 * sin (2.0 * M_PI * (double)batch
                                       / NUM_BATCHES);
          samples[i] = (0.8 * cos (fm * t)) + (0.8 * sin (fm * t)) * _Complex_I;
        }

      int rc = dp_push_send_cf64 (ctx, samples, SAMPLES_PER_BATCH, 1e6, 2.4e9);
      if (rc != DP_OK)
        {
          fprintf (stderr, "Producer: send error: %s\n", dp_strerror (rc));
          break;
        }

      if ((batch + 1) % 10 == 0)
        printf ("Producer: sent batch %d/%d\n", batch + 1, NUM_BATCHES);

      dp_usleep (1000);
    }

  printf ("Producer: done.\n");
  free (samples);
  dp_push_destroy (ctx);
  return NULL;
}

static double
mean_power_cf64 (const double _Complex *s, size_t n)
{
  double acc = 0.0;
  for (size_t i = 0; i < n; i++)
    { double re = creal (s[i]), im = cimag (s[i]); acc += re*re + im*im; }
  return acc / (double)n;
}

static void *
consumer_thread (void *arg)
{
  (void)arg;
  dp_usleep (50000); /* brief pause for producer to bind */

  printf ("Consumer: connecting to %s\n", ENDPOINT);

  dp_pull *ctx = dp_pull_create (ENDPOINT);
  if (!ctx) { fputs ("Consumer: dp_pull_create failed\n", stderr); return NULL; }

  int      batches     = 0;
  uint64_t total_samps = 0;
  double   power_sum   = 0.0;

  for (int i = 0; i < NUM_BATCHES; i++)
    {
      dp_msg_t    *msg = NULL;
      dp_header_t  hdr;

      if (dp_pull_recv (ctx, &msg, &hdr) != DP_OK)
        {
          fprintf (stderr, "Consumer: recv error\n");
          break;
        }

      size_t n    = dp_msg_num_samples (msg);
      dp_sample_type_t type = dp_msg_sample_type (msg);
      batches++;
      total_samps += n;

      double pwr = 0.0;
      if (type == DP_CF64)
        pwr = mean_power_cf64 ((const double _Complex *)dp_msg_data (msg), n);
      power_sum += pwr;

      if (i == 0)
        printf ("Consumer: first batch — type=%s  n=%zu  rate=%.2f MHz"
                "  seq=%llu\n",
                dp_sample_type_str (type), n, hdr.sample_rate / 1e6,
                (unsigned long long)hdr.sequence);
      if ((i + 1) % 10 == 0)
        printf ("Consumer: batch %3d/%d  power=%.4f (%.2f dB)\n",
                batches, NUM_BATCHES, pwr, 10.0 * log10 (pwr + 1e-12));

      dp_msg_free (msg);
    }

  double mean_pwr = batches > 0 ? power_sum / batches : 0.0;
  double kb = (double)(total_samps * sizeof (double _Complex)) / 1024.0;
  printf ("Consumer: done — %d/%d batches  %llu samples  %.2f KB"
          "  mean power=%.4f (%.2f dB)\n",
          batches, NUM_BATCHES, (unsigned long long)total_samps, kb,
          mean_pwr, 10.0 * log10 (mean_pwr + 1e-12));

  dp_pull_destroy (ctx);
  return NULL;
}

int
main (int argc, char *argv[])
{
  if (argc > 1 && (strcmp (argv[1], "--help") == 0
                   || strcmp (argv[1], "-h") == 0))
    {
      printf ("Usage: %s\n\n", argv[0]);
      printf ("PUSH/PULL pipeline demo — two in-process threads,\n");
      printf ("%d batches of %d CF64 samples via %s\n\n",
              NUM_BATCHES, SAMPLES_PER_BATCH, ENDPOINT);
      return 0;
    }

  printf ("=== doppler Pipeline Demo ===\n");
  printf ("  Transport: %s\n", ENDPOINT);
  printf ("  Batches:   %d x %d CF64 samples\n", NUM_BATCHES, SAMPLES_PER_BATCH);
  printf ("  Data:      %.2f KB total\n\n",
          (double)(NUM_BATCHES * SAMPLES_PER_BATCH
                   * sizeof (double _Complex)) / 1024.0);

  pthread_t prod, cons;
  pthread_create (&prod, NULL, producer_thread, NULL);
  pthread_create (&cons, NULL, consumer_thread, NULL);
  pthread_join (prod, NULL);
  pthread_join (cons, NULL);

  printf ("\nDemo complete.\n");
  return 0;
}
