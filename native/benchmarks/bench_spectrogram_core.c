/* bench_spectrogram_core.c — the streaming spectrogram, per input sample.
 *
 * What a caller has to know before putting it on a live stream: what one
 * input sample costs, at the transform sizes a display uses, at the two hops
 * that bracket the useful range (hop = nfft tiles the stream; hop = nfft/4
 * computes four rows per nfft samples, so it should cost about four times
 * as much per sample), and whether pushing the stream in small chunks costs
 * more than pushing it in one block.
 *
 *   push[nfft=N,hop=H]          one push of the whole block
 *   push[nfft=N,hop=H,chunk=C]  the same block in C-sample pushes, which is
 *                               what a socket or a pull source delivers
 *
 * The chunked rows are the same rows (the object is chunk-invariant, pinned
 * by test_spectrogram_core.c); only the cost can differ, and the design's U1
 * and U2 (docs/design/spectrogram.md) are the questions this starts to
 * answer. It is not those answers: they are measured on purpose, against the
 * hand-written loop, in the design's measurement record.
 *
 * Settled once per process, rounds on the outside and configurations on the
 * inside, MIN over rounds -- dp_bench.h says why each.
 */
#include "doppler/spectrogram/spectrogram_core.h"
#include "dp_bench.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 65536
#define CHUNK 256
#define ROUNDS 30

typedef struct
{
  size_t                  nfft, hop, chunk; /* chunk 0: one push */
  dp_spectrogram_state_t *s;
  float                  *out;
  double                  t[ROUNDS];
} config_t;

/* One timed pass: the whole block, from a fresh stream position. */
static size_t
run (config_t *c, const float _Complex *x)
{
  dp_spectrogram_reset (c->s);
  if (!c->chunk)
    return dp_spectrogram_push (c->s, x, BLOCK, c->out,
                                dp_spectrogram_push_max_out (c->s, BLOCK));
  size_t made = 0;
  for (size_t off = 0; off < BLOCK; off += c->chunk)
    made += dp_spectrogram_push (c->s, x + off, c->chunk, c->out + made,
                                 BLOCK * 4 - made);
  return made;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;
  float _Complex *x      = malloc (BLOCK * sizeof *x);
  if (!x)
    return 1;
  for (int i = 0; i < BLOCK; i++)
    {
      double p = 0.01 * i;
      x[i]     = (float)cos (p) + (float)sin (p * 1.7) * I;
    }

  static const size_t nffts[] = { 256, 1024, 4096 };
  config_t            cfg[12];
  int                 nc = 0;
  for (int k = 0; k < 3; k++)
    for (int h = 0; h < 2; h++)
      for (int ch = 0; ch < 2; ch++)
        {
          config_t *c = &cfg[nc++];
          c->nfft     = nffts[k];
          c->hop      = h ? nffts[k] / 4 : nffts[k];
          c->chunk    = ch ? CHUNK : 0;
          /* Hann, dB, DC-centred: the shape a waterfall asks for */
          c->s = dp_spectrogram_create (c->nfft, c->hop, 0, 0.0f, 0);
          if (!c->s)
            {
              (void)fprintf (stderr, "bench_spectrogram: create NULL\n");
              return 1;
            }
          /* the one-shot's room covers every chunking: same rows */
          c->out = malloc (BLOCK * 4 * sizeof *c->out);
          if (!c->out || dp_spectrogram_push_max_out (c->s, BLOCK) > BLOCK * 4)
            return 1;
        }

  printf ("=== spectrogram benchmark ===\n");
  printf ("block = %d samples, %d rounds, chunk = %d\n\n", BLOCK, ROUNDS,
          CHUNK);

  DP_BENCH_SETTLE (sink += run (&cfg[0], x));
  for (int r = 0; r < ROUNDS; r++)
    for (int i = 0; i < nc; i++)
      {
        uint64_t t0 = jm_bench_now_ns ();
        sink += run (&cfg[i], x);
        uint64_t t1 = jm_bench_now_ns ();
        cfg[i].t[r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int i = 0; i < nc; i++)
    {
      char name[64];
      if (cfg[i].chunk)
        (void)snprintf (name, sizeof name, "push[nfft=%zu,hop=%zu,chunk=%zu]",
                        cfg[i].nfft, cfg[i].hop, cfg[i].chunk);
      else
        (void)snprintf (name, sizeof name, "push[nfft=%zu,hop=%zu]",
                        cfg[i].nfft, cfg[i].hop);
      dp_bench_record (&_bench, name, cfg[i].t, ROUNDS, BLOCK, "sample");
      dp_spectrogram_destroy (cfg[i].s);
      free (cfg[i].out);
    }
  printf ("\n(sink %zu)\n", (size_t)sink);
  jm_bench_write_json (&_bench, "spectrogram");
  free (x);
  return 0;
}
