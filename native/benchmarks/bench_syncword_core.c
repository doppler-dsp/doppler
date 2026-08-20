/* bench_syncword_core.c — the marker search, as a function of marker length.
 *
 * `bench_ccsds_tm_core.c` already times `asm_find` at the one length CCSDS
 * uses, 32 bits, and reports it per received bit — which is the right number
 * for that component, because a demodulator runs it over every bit it
 * recovers. What that cannot show is the shape, and the shape is what a
 * caller choosing a marker needs: the kernel is O(n_bits * n_marker), it
 * examines every offset until it accepts one, and a miss therefore costs the
 * whole window at full price.
 *
 * So the rows sweep the marker LENGTH over a fixed search window with no
 * marker in it — the worst case, and the case a synchroniser hunting for
 * lock is actually in:
 *
 *   find[n=32]  find[n=64]  find[n=128]  find[n=256]
 *
 * A 32-bit row here and the ccsds_tm one measure the same kernel through
 * different faces (this one through the object, that one through CCSDS's
 * configuration of it), so the difference between them IS the per-call
 * binding overhead, which is otherwise invisible.
 *
 * `max_errors_for` gets a row too, for the reason `bench_util_core.c` exists
 * to measure three flops: the question is not whether a design-time helper
 * is fast, it is whether the log-space binomial sum is cheap enough to call
 * per acquisition rather than once at setup. A caller who assumes it is not
 * will cache it, and a cached threshold is one that outlives the marker it
 * was computed for.
 *
 * Timing is MIN over rounds, not mean — benchmark noise is one-sided.
 */
#include "syncword/syncword_core.h"

#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* The window a synchroniser sweeps before it declares no-lock. Large enough
   that the per-offset cost dominates loop setup at every marker length. */
#define SEARCH_BITS 65536
#define ITERATIONS 50

/* Long enough that the arithmetic runs its full t = 0..n sweep. */
#define THRESH_CALLS 2000

/* No length below 32. A random n-bit marker occurs in a 65536-bit stream
   with probability about 2 * 65536 / 2^n, so a 16-bit row does not measure
   the window it reports -- it measures how long the search took to hit by
   accident, which was 20x faster and read as a scaling result. The absence
   is ASSERTED below rather than argued for, since the argument is
   probabilistic and the stream is fixed. */
static const size_t MARKER_LENS[] = { 32u, 64u, 128u, 256u };
#define N_LENS ((int)(sizeof MARKER_LENS / sizeof MARKER_LENS[0]))

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile int    sink   = 0;

  uint8_t *bits = malloc (SEARCH_BITS);
  if (!bits)
    return 1;

  /* A cheap LFSR rather than rand(), so the stream is the same on every
     machine and the row is comparable across snapshots. */
  uint32_t lfsr = 0xACE1u;
  for (size_t i = 0; i < SEARCH_BITS; i++)
    {
      lfsr    = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      bits[i] = (uint8_t)(lfsr & 1u);
    }

  printf ("=== syncword benchmark ===\n");
  printf ("window = %d bits, no marker present (every offset examined)\n\n",
          SEARCH_BITS);

  for (int c = 0; c < N_LENS; c++)
    {
      const size_t nm = MARKER_LENS[c];
      uint8_t     *m  = malloc (nm);
      if (!m)
        return 1;
      /* A marker drawn from a DIFFERENT sequence than the stream, so it is
         not accidentally present in it -- a hit would end the search early
         and the row would be timing a shorter window than it reports. */
      uint32_t s = 0x1234u + (uint32_t)nm;
      for (size_t i = 0; i < nm; i++)
        {
          s    = s * 1664525u + 1013904223u;
          m[i] = (uint8_t)((s >> 16) & 1u);
        }

      syncword_state_t *f = syncword_create (m, nm);
      if (!f)
        return 1;

      /* The row's whole premise: every offset is examined. A marker that is
         accidentally in the stream ends the search early, and the number
         printed would then be a fraction of the window at full confidence. */
      if (syncword_find (f, bits, SEARCH_BITS, 0u).found)
        {
          fprintf (stderr,
                   "bench_syncword: the n=%zu marker is present in the "
                   "stream -- this row would time an early exit\n",
                   nm);
          return 1;
        }

      static double t_find[ITERATIONS];
      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          /* max_errors = 0: nothing in the stream can match, so the whole
             window is walked and the row is the honest worst case. */
          sink += syncword_find (f, bits, SEARCH_BITS, 0u).found;
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_find[r] = elapsed_sec (&t0, &t1);
        }

      char name[32];
      snprintf (name, sizeof name, "find[n=%zu]", nm);
      jm_bench_add (&_bench, name, t_find, ITERATIONS, SEARCH_BITS);

      const double s_min = min_sec (t_find, ITERATIONS);
      printf ("  %-14s %8.2f ns/bit  %8.2f Mbit/s  (%.2f ns/bit/tap)\n", name,
              s_min / (double)SEARCH_BITS * 1e9,
              (double)SEARCH_BITS / s_min / 1e6,
              s_min / (double)SEARCH_BITS / (double)nm * 1e9);

      syncword_destroy (f);
      free (m);
    }

  /* The design-time arithmetic, at the length CCSDS uses. */
  {
    uint8_t m32[32];
    for (int i = 0; i < 32; i++)
      m32[i] = (uint8_t)(i & 1);
    syncword_state_t *f = syncword_create (m32, sizeof m32);
    if (!f)
      return 1;

    static double t_thr[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < THRESH_CALLS; k++)
          sink += syncword_max_errors_for (f, 4096u + (size_t)k, 1e-3);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_thr[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "max_errors_for[n=32]", t_thr, ITERATIONS,
                  THRESH_CALLS);
    printf ("\n  %-14s %8.2f us/call\n", "max_errors_for",
            min_sec (t_thr, ITERATIONS) / (double)THRESH_CALLS * 1e6);
    syncword_destroy (f);
  }

  jm_bench_write_json (&_bench, "syncword");
  free (bits);
  (void)sink;
  return 0;
}
