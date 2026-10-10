/* bench_spectrogram_core.c — the streaming spectrogram, per input sample.
 *
 * What a caller has to know before putting it on a live stream: what one
 * input sample costs, at the transform sizes a display uses, at the hops that
 * bracket the useful range (hop = nfft tiles the stream; hop = nfft/4
 * computes four rows per nfft samples, so it should cost about four times
 * as much per sample), and whether pushing the stream in small chunks costs
 * more than pushing it in one block.
 *
 *   push[nfft=N,hop=H]          one push of the whole block, dB rows
 *   push[nfft=N,hop=H,chunk=C]  the same block in C-sample pushes, which is
 *                               what a socket or a pull source delivers
 *   push[nfft=N,hop=H,mode=power]
 *                               one push, POWER rows: the default since
 *                               #1968, which skips the dB conversion. U4 for
 *                               power (#2094) at nfft 256, 1024 (U4's own
 *                               point, hop 256) and 65536, each at hop
 *                               nfft/4, so the ratio to the dB row beside it
 *                               is the conversion's share at that size
 *   direct[nfft=N,hop=H]        the same rows from a hand-written loop that
 *                               calls dp_psd_frame_db on x + k*hop: no ring,
 *                               no carry, no copy. A MEASURING STICK for the
 *                               design's U1 (the carry's copy against a
 *                               bypass), never a library path; setup refuses
 *                               to run if its rows differ from push's by a
 *                               single bit.
 *
 * The chunked rows are the same rows (the object is chunk-invariant, pinned
 * by test_spectrogram_core.c); only the cost can differ. U1 is push/direct
 * over nfft 256..65536 at hop nfft/4 and nfft; U2 (latency in time) is the
 * chunk rows at nfft 1024, hop 256, chunk 1, hop, nfft and 16 nfft. The
 * answers live in the design's measurement record, not here
 * (docs/design/spectrogram-measurements.md).
 *
 * The block is 65536 samples, or 8 nfft where that is longer, so the largest
 * transform still makes several rows a pass; push and direct always share a
 * block. Settled once per process, rounds on the outside and configurations
 * on the inside, MIN over rounds -- dp_bench.h says why each.
 *
 * 32 rows, the cap: jm_bench.h keeps at most JM_BENCH_MAX_ENTRIES (32) and
 * drops the rest WITHOUT A WORD (just-buildit/just-makeit#2188), so a row
 * added past 32 runs, prints and never reaches the JSON. The power rows took
 * the last three; re-vendor after that ships before adding another.
 */
#include "doppler/psd/psd_core.h"
#include "doppler/spectrogram/spectrogram_core.h"
#include "dp_bench.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 65536
#define CHUNK 256
#define ROUNDS 30
#define NCFG 64

typedef struct
{
  size_t                  nfft, hop, chunk; /* chunk 0: one push */
  int                     direct;           /* the hand-written loop */
  int                     mode;             /* a DP_SPECTROGRAM_* name */
  int                     mate;             /* a push's direct twin, or -1 */
  size_t                  block, cap;
  dp_spectrogram_state_t *s;
  dp_psd_state_t         *p;
  float                  *out;
  double                  t[ROUNDS];
} config_t;

static size_t
block_for (size_t nfft)
{
  return 8 * nfft > BLOCK ? 8 * nfft : BLOCK;
}

/* One timed pass: the whole block, from a fresh stream position. */
static size_t
run (config_t *c, const float _Complex *x)
{
  if (c->direct)
    {
      size_t k = 0;
      for (; k * c->hop + c->nfft <= c->block; k++)
        dp_psd_frame_db (c->p, x + k * c->hop, c->out + k * c->nfft);
      return k * c->nfft;
    }
  dp_spectrogram_reset (c->s);
  if (!c->chunk)
    return dp_spectrogram_push (c->s, x, c->block, c->out, c->cap);
  size_t made = 0;
  for (size_t off = 0; off < c->block; off += c->chunk)
    made += dp_spectrogram_push (c->s, x + off, c->chunk, c->out + made,
                                 c->cap - made);
  return made;
}

static config_t *
add (config_t *cfg, int *nc, size_t nfft, size_t hop, size_t chunk, int direct,
     size_t block, int mode)
{
  config_t *c = &cfg[(*nc)++];
  c->mate     = -1;
  c->nfft     = nfft;
  c->hop      = hop;
  c->chunk    = chunk;
  c->direct   = direct;
  c->block    = block;
  c->mode     = mode;
  /* Hann, DC-centred, the mode named by the caller, and the PSD the
     Spectrogram builds for itself. The dB rows are the ones U1-U4
     measured (spectrogram-measurements.md entries 5.6-5.9); the power
     rows are #2094's. */
  c->s = dp_spectrogram_create (nfft, hop, 0, 0.0f, mode);
  c->p = direct ? dp_psd_create (nfft, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0) : NULL;
  if (!c->s || (direct && !c->p))
    return NULL;
  c->cap = dp_spectrogram_push_max_out (c->s, block);
  c->out = malloc (c->cap * sizeof *c->out);
  return c->out ? c : NULL;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;
  const size_t    xlen   = block_for (65536);
  float _Complex *x      = malloc (xlen * sizeof *x);
  if (!x)
    return 1;
  for (size_t i = 0; i < xlen; i++)
    {
      double p = 0.01 * (double)i;
      x[i]     = (float)cos (p) + (float)sin (p * 1.7) * I;
    }

  config_t cfg[NCFG] = { 0 };
  int      nc        = 0;
  /* One table. Every (nfft, hop): a one-push row and, beside it, its direct
     twin (U1), so the two are timed adjacently at every size; and for the
     original sizes 256/1024/4096 the CHUNK-sample row they always had. Hop
     nfft and nfft/4 (the midpoint nfft/2 would pass jm_bench.h's 32-row
     cap; see the top). */
  static const size_t u1[] = { 256, 1024, 4096, 16384, 65536 };
  for (int k = 0; k < 5; k++)
    for (size_t div = 1; div <= 4; div *= 4)
      {
        const size_t n = u1[k], hop = n / div;
        const size_t block = n <= 4096 ? BLOCK : block_for (n);
        config_t *pc = add (cfg, &nc, n, hop, 0, 0, block, DP_SPECTROGRAM_DB);
        if (!pc)
          return 1;
        const int ip = nc - 1;
        config_t *dc = add (cfg, &nc, n, hop, 0, 1, block, DP_SPECTROGRAM_DB);
        if (!dc)
          return 1;
        cfg[ip].mate = nc - 1;
        /* the measuring stick must be the same computation, bit for bit */
        size_t wp = run (&cfg[ip], x), wd = run (dc, x);
        if (wp != wd || memcmp (cfg[ip].out, dc->out, wp * sizeof *dc->out))
          {
            (void)fprintf (stderr,
                           "bench_spectrogram: direct differs from push at "
                           "nfft %zu hop %zu\n",
                           n, hop);
            return 1;
          }
        if (n <= 4096
            && !add (cfg, &nc, n, hop, CHUNK, 0, BLOCK, DP_SPECTROGRAM_DB))
          return 1;
      }
  /* U2: chunk 1, nfft and 16 nfft at nfft 1024, hop 256 (CHUNK = 256 = hop
     is an original row) */
  static const size_t u2[] = { 1, 1024, 16384 };
  for (int i = 0; i < 3; i++)
    if (!add (cfg, &nc, 1024, 256, u2[i], 0, BLOCK, DP_SPECTROGRAM_DB))
      return 1;
  /* U4 for power rows (#2094): one push at hop nfft/4, beside the dB push
     of the same shape, at the two ends of the range and U4's own 1024 */
  static const size_t u4p[] = { 256, 1024, 65536 };
  for (int i = 0; i < 3; i++)
    if (!add (cfg, &nc, u4p[i], u4p[i] / 4, 0, 0,
              u4p[i] <= 4096 ? BLOCK : block_for (u4p[i]),
              DP_SPECTROGRAM_POWER))
      return 1;

  printf ("=== spectrogram benchmark ===\n");
  printf ("block = %d samples (8 nfft where longer), %d rounds\n\n", BLOCK,
          ROUNDS);

  DP_BENCH_SETTLE (sink += run (&cfg[0], x));
  for (int r = 0; r < ROUNDS; r++)
    for (int i = 0; i < nc; i++)
      {
        if (cfg[i].direct)
          continue; /* timed beside its push, below */
        /* a push and its direct twin, adjacent, the pair's order swapped
           every round: U1 is their ratio, and a fixed order would always
           run the same one second, warm */
        int first = i, second = cfg[i].mate;
        if (second >= 0 && r % 2)
          first = cfg[i].mate, second = i;
        for (int pass = 0; pass < 2; pass++)
          {
            const int j = pass ? second : first;
            if (j < 0)
              break;
            uint64_t t0 = jm_bench_now_ns ();
            sink += run (&cfg[j], x);
            uint64_t t1 = jm_bench_now_ns ();
            cfg[j].t[r] = jm_bench_elapsed_sec (t0, t1);
          }
      }

  for (int i = 0; i < nc; i++)
    {
      char name[64];
      if (cfg[i].direct)
        (void)snprintf (name, sizeof name, "direct[nfft=%zu,hop=%zu]",
                        cfg[i].nfft, cfg[i].hop);
      else if (cfg[i].chunk)
        (void)snprintf (name, sizeof name, "push[nfft=%zu,hop=%zu,chunk=%zu]",
                        cfg[i].nfft, cfg[i].hop, cfg[i].chunk);
      else if (cfg[i].mode == DP_SPECTROGRAM_POWER)
        (void)snprintf (name, sizeof name, "push[nfft=%zu,hop=%zu,mode=power]",
                        cfg[i].nfft, cfg[i].hop);
      else
        (void)snprintf (name, sizeof name, "push[nfft=%zu,hop=%zu]",
                        cfg[i].nfft, cfg[i].hop);
      dp_bench_record (&_bench, name, cfg[i].t, ROUNDS, cfg[i].block,
                       "sample");
      dp_spectrogram_destroy (cfg[i].s);
      dp_psd_destroy (cfg[i].p);
      free (cfg[i].out);
    }
  printf ("\n(sink %zu)\n", (size_t)sink);
  /* Every row RECORDS, or nothing is written: jm_bench.h drops entries past
     JM_BENCH_MAX_ENTRIES without a word (just-buildit/just-makeit#2188), so
     the count is checked against nc, which the tables above DERIVE. A short
     set then reaches the publish gate as a missing component (#2062). */
  if (_bench.count != nc)
    {
      (void)fprintf (stderr,
                     "bench_spectrogram: recorded %d rows of %d; writing "
                     "none\n",
                     _bench.count, nc);
      return 1;
    }
  jm_bench_write_json (&_bench, "spectrogram");
  free (x);
  return 0;
}
