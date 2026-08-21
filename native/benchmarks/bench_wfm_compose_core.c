/* bench_wfm_compose_core.c -- what a segment BOUNDARY costs.
 *
 * The composer's own contribution has never been measured directly. Its
 * waiver in `scripts/check_bench_coverage.py` said the segment assembly is
 * "benchmarked indirectly through wfm_synth only" -- which measures the
 * engine underneath and says nothing about the layer on top, because the
 * header is explicit that a one-segment spec is byte-identical to calling
 * `synth` directly. Everything the composer adds happens at a boundary:
 * it owns ONE synth at a time, so crossing from one segment to the next
 * means standing up a new engine mid-stream.
 *
 * So the measurement is a subtraction, and the way to make it is to hold
 * the sample count fixed and vary only how many boundaries produce it.
 * Four segment lengths, one total, one pull size:
 *
 *   1 segment  x 262144 samples    ~one boundary per round
 *   16 x 16384, 64 x 4096, 256 x 1024
 *
 * Same samples, same waveform, same engine, same number of `execute`
 * calls. The difference between the rows is boundaries and nothing else,
 * and dividing it by the boundary count gives what one costs -- reported
 * below both in nanoseconds and in samples-equivalent, which is the unit a
 * caller sizing a burst schedule actually thinks in.
 *
 * The stream is `continuous`, so the composer loops forever and no round
 * pays a create or an exhaustion. `off_samples` is zero throughout: a gap
 * emits noise floor rather than signal and is cheaper per sample, which
 * would confound the very difference being measured. A single tone source
 * is used for the same reason -- the cheapest per-sample work available,
 * so the boundary is as visible as it can be. Against a dearer waveform
 * the absolute boundary cost is the same and its share is smaller.
 */
#include "dp_bench.h"
#include "wfm/wfm_compose.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define TOTAL 262144
#define PULL 4096
#define FS 1.0e6
#define N_CFG 4

/* Segment lengths, each dividing TOTAL so every configuration produces the
   same samples from a whole number of segments. */
static const size_t seg_len[N_CFG] = { 262144, 16384, 4096, 1024 };
#define REF_IDX 0

int
main (void)
{
  jm_bench_t           _bench = { 0 };
  struct timespec      t0, t1;
  static double        t[N_CFG][ITERATIONS];
  wfm_compose_state_t *comp[N_CFG] = { 0 };
  wfm_source_t         src[N_CFG];
  wfm_segment_t        seg[N_CFG];
  float complex       *out = NULL;
  char                 name[72];

  out = malloc (PULL * sizeof *out);
  if (!out)
    return 1;

  for (int c = 0; c < N_CFG; c++)
    {
      /* A plain tone at a high SNR: the cheapest per-sample work the engine
         offers, so the boundary is as large a share as it can be. */
      src[c] = (wfm_source_t){ .type      = 0,
                               .freq      = 1e5,
                               .snr       = 100.0,
                               .snr_mode  = 0,
                               .seed      = 1,
                               .sps       = 8,
                               .pn_length = 7,
                               .pn_poly   = 0 };
      seg[c] = (wfm_segment_t){ .sources     = &src[c],
                                .n_sources   = 1,
                                .fs          = FS,
                                .num_samples = seg_len[c],
                                .off_samples = 0 };
      /* continuous = 1: the sequence never ends, so no round pays a create
         or runs the composer dry. */
      comp[c] = wfm_compose_create (&seg[c], 1, 0, 1);
      if (!comp[c])
        {
          fprintf (stderr, "compose create failed (seg_len %zu)\n",
                   seg_len[c]);
          return 1;
        }
    }

  printf ("=== wfm_compose (multi-segment composer over one synth) ===\n");
  printf ("%d samples per round in %d-sample pulls, %d rounds, min over "
          "rounds\n\n",
          TOTAL, PULL, ITERATIONS);

  DP_BENCH_SETTLE ((void)wfm_compose_execute (comp[REF_IDX], out, PULL));

  /* Rounds outside, segment lengths inside: the entire result is the
     difference between these rows, so a thermal step must not land on one
     segment length alone. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (size_t done = 0; done < TOTAL; done += PULL)
          (void)wfm_compose_execute (comp[c], out, PULL);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "execute[seg=%zu,boundaries=%zu]",
                      seg_len[c], (size_t)TOTAL / seg_len[c]);
      dp_bench_record (&_bench, name, t[c], ITERATIONS, TOTAL, "sample");
    }

  printf ("\n  the boundary, by subtraction from the %zu-sample segment:\n",
          seg_len[REF_IDX]);
  {
    const double base   = dp_bench_min (t[REF_IDX], ITERATIONS);
    const double per_s  = base / (double)TOTAL;
    const size_t base_b = (size_t)TOTAL / seg_len[REF_IDX];

    for (int c = 0; c < N_CFG; c++)
      {
        const size_t nb = (size_t)TOTAL / seg_len[c];
        const double d  = dp_bench_min (t[c], ITERATIONS) - base;
        if (nb <= base_b)
          {
            printf ("    seg=%-7zu %4zu boundaries   (baseline)\n", seg_len[c],
                    nb);
            continue;
          }
        printf ("    seg=%-7zu %4zu boundaries   %7.2f us each  "
                "= %8.0f samples\n",
                seg_len[c], nb, d / (double)(nb - base_b) * 1e6,
                d / (double)(nb - base_b) / per_s);
      }
  }
  printf ("  The samples-equivalent column is the one to design against: a\n"
          "  burst schedule whose segments are shorter than that number is\n"
          "  spending more on starting waveforms than on emitting them.\n");

  for (int c = 0; c < N_CFG; c++)
    wfm_compose_destroy (comp[c]);
  free (out);
  jm_bench_write_json (&_bench, "wfm_compose");
  return 0;
}
