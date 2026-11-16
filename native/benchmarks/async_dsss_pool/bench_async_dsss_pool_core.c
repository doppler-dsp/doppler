/* bench_async_dsss_pool_core.c -- what a pool of searching receivers costs
 * per input sample, at the one operating point we pin.
 *
 * The point: a 1,023-chip Gold code (the CCSDS default, wfm.Gold()) at
 * 5 Mcps, captured at 11 MSa/s. The receivers take whole samples per chip,
 * so the capture goes through one RateConverter to 10 MSa/s (2 samples a
 * chip) first, and that front end is inside every timed push: it is what a
 * caller at 11 MSa/s pays. The search is the design's: block-coherent over
 * the depth the Doppler rate allows: 3 ksym/s with a 500-symbol data-free
 * block (code_only_s) and a 500 Hz/s Doppler rate (doppler_rate), D = 150
 * code periods at 5 Mcps (the rate's 154 rounded to a 7-smooth length,
 * docs/design/async-dsss-receiver.md §9), so a tile resolves D Doppler rows
 * in slow time. Two spans:
 *
 * - native: one slow-time tile, +/- 1 / (2 T_E) = +/- 2,444 Hz;
 * - full:   +/- 50 kHz, the design's whole carrier range.
 *
 * And the full span again with an S-band carrier set (2.2 GHz): every
 * side tile then runs its code-rate ramp, which the one delay-Doppler map
 * changes (#2752, docs/design/acq-surface.md §18.4), so its speed gate
 * measures it against main. One tile has no ramp, so native has no such
 * row.
 *
 * One timed push is one dwell, D code periods at the receivers' rate, so
 * every push decides exactly one dwell and the rows compare like for like.
 *
 * The rows ask two questions. n=1 against n=4 on one thread: does a pool of
 * N idle receivers cost one search or N? The controller owns the one search
 * (docs/design/async-dsss-receiver.md §5) and an idle receiver runs none,
 * so it should be one, whatever N is. And n=4 on 4 threads against one:
 * what do the workers buy back, given push() joins them once every code
 * period? At the full span that includes the search's own tile fan, on the
 * pool's thread count (#2773).
 *
 * Fed noise alone from the shipped awgn, so every receiver stays idle and
 * the search runs all along; a row is recorded only if every receiver was
 * idle for every sample fed and the search ran every period (a false
 * alarm's seed would be timed as a refine). The rows are interleaved round
 * by round, so their ratios are measured on one machine state.
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_LEN 1023
#define LEAD 2
#define ITERATIONS 10
#define CODE_ONLY_S (500.0 / 3000.0) /* 500 data-free symbols */
#define DOPPLER_RATE 500.0           /* Hz/s, the design's bound */
#define N_ROWS 8

typedef struct
{
  const char *name;
  size_t      n_rx;
  double      span_hz; /* the receivers' doppler_uncertainty, +/- */
  int         threads;
  double      carrier_hz; /* RF carrier, Hz; 0 = none (no code-rate ramp) */
} row_t;

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* Every receiver idle for every sample fed and the one search fed every
   period, or a message. The receivers have been fed the whole periods
   pushed: the pool carries the rest to the next push. */
static int
all_searching (dp_async_dsss_pool_state_t *p, size_t n_rx, uint64_t pushed,
               const char *tag)
{
  const uint64_t fed = pushed - p->carry_len;
  for (size_t i = 0; i < n_rx; i++)
    {
      async_dsss_receiver_status_t st = dp_async_dsss_pool_status (p, i);
      if (st.state != ASYNC_DSSS_RX_IDLE || st.state_samples != fed)
        {
          (void)fprintf (stderr,
                         "bench_async_dsss_pool[%s]: receiver %zu state=%d "
                         "state_samples=%llu of %llu -- not idle all "
                         "along\n",
                         tag, i, st.state,
                         (unsigned long long)st.state_samples,
                         (unsigned long long)fed);
          return 0;
        }
    }
  if (p->search_periods * p->period != fed || p->search->refused_hits != 0)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_pool[%s]: the search ran %llu of %llu "
                     "periods -- not a search all along\n",
                     tag, (unsigned long long)p->search_periods,
                     (unsigned long long)(fed / p->period));
      return 0;
    }
  return 1;
}

/* ── On the air ────────────────────────────────────────────────────────────
 * The rows above are noise alone, so the search lists one peak a dwell and
 * stops. With an emitter on the air its own column and sidelobes clear the
 * gate and the list's cost is per listed peak (doppler#3060): those rows
 * cannot see it. These rows run the pool at its specification -- ten
 * receivers, 11 MSa/s in, a 2.5 GHz carrier, a 500-symbol data-free block
 * opening every 1,000-symbol frame -- with emitters on the air, and time a
 * push only in steady state -- each expected emitter held by its own
 * receiver (by tracked Doppler), nothing released since the push before,
 * one settled push after lock skipped, and the search fed every period --
 * so a row is the cost of holding those signals while searching, not of
 * acquiring them (acquisitions and refines are excluded).
 *
 * The pool is fed 11 MSa/s directly, so its search runs the 2,048-sample
 * epoch it resamples to (doppler#3062), not the 2,046 the rows above get
 * through their own RateConverter. Each emitter is the library's continuous
 * DSSS synth at 10 MSa/s (it takes whole samples a chip) with PRBS data and
 * the data-free window, taken to 11 MSa/s by a RateConverter, then through
 * a DopplerChannel at 2.5 GHz, which dilates its clocks and shifts its
 * carrier from one ppm, as the pool's carrier aiding assumes. Emitters are
 * spread over +/- 40 kHz, 197 samples apart at 10 MSa/s (98.5 chips),
 * plus the shipped AWGN. "design"
 * puts each at the pool's design C/N0, "+6dB" 6 dB above. A near-far row
 * holds one emitter 20 dB above the design level beside one at it: the deep
 * search takes the weaker only to about +14 dB (async-dsss-receiver.md
 * §7.2, doppler#3067), so one receiver tracks; the row times the search
 * with a strong signal held and a weak one on the air. The stream is made a
 * push at a time, outside the timed call.
 *
 * Two depths: D = 150, the design's (a 500-symbol code-only block), and
 * D = 1 (a code-only block of one code period). Every push is the D = 150
 * dwell's length, so the rows compare per sample.
 *
 * And the same rows at the CCSDS 415.1-B-1 specification point
 * (async-dsss-receiver.md §1), at 2.2 and 2.8 GHz: nine emitters held and
 * the tenth receiver idle, so the search runs every period, on one thread
 * and on 16, every push the pool's own dwell (main() names the point). */
#define AIR_RX 10
#define AIR_ITERS 4
#define AIR_MAX_PUSHES 80
#define AIR_MAX_K 9
#define AIR_CHUNK 65536u
#define AIR_CARRIER 2.5e9
#define AIR_CODE_ONLY_SYM 500u
#define AIR_FRAME_SYM 1000u

/* The operating point an on-air row runs at. The rows above it run the
   5 Mcps point (AIR_5MCPS); the 415.1-B-1 rows run the specification point
   (docs/design/async-dsss-receiver.md §1) at a band end. */
typedef struct
{
  double chip;        /* chips a second */
  double cn0;         /* the pool's design C/N0, dB-Hz */
  double carrier_hz;  /* RF carrier */
  double span_hz;     /* the receivers' doppler_uncertainty, +/- */
  double code_only_s; /* the deep rows' code-only window */
  double rate_hz_s;   /* the search's rate bound */
  double f_lo, f_hi;  /* emitters spread over [f_lo, f_hi] at the start */
  double ramp_hz_s;   /* every emitter's Doppler rate */
  double tol_hz;      /* a tracked Doppler within this of its emitter's */
  size_t code_only_sym, frame_sym; /* the synth's data-free window */
  size_t block_periods; /* code periods a push; 0: the pool's own D */
} air_pt_t;

typedef struct
{
  const char *name;
  int         k;       /* emitters on the air */
  int         expect;  /* receivers that should track */
  double      gain_db; /* each emitter over the design C/N0 */
  double      lead_db; /* emitter 0 over the others, near-far rows */
  int         deep;    /* D = 150, else D = 1 */
  int         threads;
} air_row_t;

/* Emitter k's Doppler, Hz, t seconds into the stream: spread over the
   point's [f_lo, f_hi], all on the point's ramp. */
static double
air_doppler (const air_pt_t *pt, int k, int k_on, double t)
{
  return pt->f_lo + (pt->f_hi - pt->f_lo) * ((double)k + 0.37) / (double)k_on
         + pt->ramp_hz_s * t;
}

/* K emitters and the noise as one stream at 11 MSa/s, made a push at a
   time: each emitter's synth runs at 10 MSa/s into its RateConverter and
   DopplerChannel, whose output collects in `buf` until a push's worth is
   there. */
typedef struct
{
  int                         k;
  float                       amp[AIR_MAX_K];
  dp_wfm_synth_state_t       *syn[AIR_MAX_K];
  dp_RateConverter_state_t   *rc[AIR_MAX_K];
  dp_doppler_channel_state_t *ch[AIR_MAX_K];
  float _Complex             *buf[AIR_MAX_K];
  size_t                      len[AIR_MAX_K], lag[AIR_MAX_K], cap;
  dp_awgn_state_t            *noise;
  float _Complex             *e10, *e11;
  size_t                      e11_cap;
} air_src_t;

static void
air_src_free (air_src_t *a)
{
  for (int i = 0; i < AIR_MAX_K; i++)
    {
      dp_wfm_synth_destroy (a->syn[i]);
      dp_RateConverter_destroy (a->rc[i]);
      dp_doppler_channel_destroy (a->ch[i]);
      free (a->buf[i]);
    }
  dp_awgn_destroy (a->noise);
  free (a->e10);
  free (a->e11);
}

static int
air_src_init (air_src_t *a, const uint8_t *code, const air_row_t *w,
              const air_pt_t *pt, double sym, double fs_in, size_t block)
{
  const double chip = pt->chip, cn0 = pt->cn0;
  const double fs10 = 2.0 * chip;
  *a                = (air_src_t){ 0 };
  a->k              = w->k;
  a->cap            = block + 4 * AIR_CHUNK;
  a->e11_cap        = 2 * AIR_CHUNK;
  a->noise          = dp_awgn_create (
      8807u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs_in)), 1.0f));
  a->e10 = malloc (AIR_CHUNK * sizeof *a->e10);
  a->e11 = malloc (a->e11_cap * sizeof *a->e11);
  if (!a->noise || !a->e10 || !a->e11)
    return 0;
  for (int k = 0; k < w->k; k++)
    {
      const double f = air_doppler (pt, k, w->k, 0.0);
      a->amp[k] = (float)pow (10.0, (w->gain_db + (k == 0 ? w->lead_db : 0.0))
                                        / 20.0);
      a->syn[k] = dp_wfm_synth_create (WFM_SYNTH_DSSS, fs10, 0.0,
                                       WFM_SYNTH_SNR_CLEAN, 1,
                                       100u + (uint32_t)k, 2, 15, 0, 0, 0.0);
      a->rc[k]  = dp_RateConverter_create (fs_in / fs10, 0);
      a->ch[k]  = dp_doppler_channel_create (
          fs_in, pt->carrier_hz, f / pt->carrier_hz * 1e6,
          pt->ramp_hz_s / pt->carrier_hz * 1e6);
      a->buf[k] = malloc (a->cap * sizeof *a->buf[k]);
      a->lag[k] = 197u * (size_t)k; /* at 10 MSa/s: 216.7 at 11 */
      if (!a->syn[k] || !a->rc[k] || !a->ch[k] || !a->buf[k]
          || dp_wfm_synth_set_dsss_cont (a->syn[k], code, CODE_LEN, chip / sym,
                                         WFM_DSSS_DATA_PRBS, NULL, 0)
                 != 0
          || dp_wfm_synth_set_dsss_window (a->syn[k], pt->code_only_sym,
                                           pt->frame_sym)
                 != 0)
        return 0;
    }
  return 1;
}

/* Whether the pool holds what the row says: exactly `expect` receivers
   tracking, each within 1 kHz of a different emitter's Doppler (emitter 0,
   the strongest, when fewer are expected than are on the air), so a
   duplicate, a ghost or a sidelobe held in place of a signal fails it. */
static int
air_holds (dp_async_dsss_pool_state_t *p, const air_row_t *w,
           const air_pt_t *pt, double t)
{
  int held[AIR_MAX_K] = { 0 }, n = 0;
  for (size_t i = 0; i < AIR_RX; i++)
    {
      async_dsss_receiver_status_t st = dp_async_dsss_pool_status (p, i);
      if (st.state != ASYNC_DSSS_RX_TRACKING)
        continue;
      int k = -1;
      for (int j = 0; j < w->k && k < 0; j++)
        if (!held[j]
            && fabs (st.doppler_hz - air_doppler (pt, j, w->k, t))
                   < pt->tol_hz)
          k = j;
      if (k < 0)
        return 0;
      held[k] = 1;
      n++;
    }
  return n == w->expect && (w->expect >= w->k || held[0]);
}

/* The next `n` samples of the stream into `x`. */
static int
air_src_next (air_src_t *a, float _Complex *x, size_t n)
{
  (void)dp_awgn_generate (a->noise, n, x, n);
  for (int k = 0; k < a->k; k++)
    {
      while (a->len[k] < n)
        {
          size_t m = AIR_CHUNK / 2;
          if (a->lag[k])
            { /* the emitter's delay: silence ahead of its first chip */
              m = a->lag[k] < m ? a->lag[k] : m;
              for (size_t i = 0; i < m; i++)
                a->e10[i] = 0;
              a->lag[k] -= m;
            }
          else
            dp_wfm_synth_steps (a->syn[k], a->e10, m);
          const size_t r    = dp_RateConverter_execute (a->rc[k], a->e10, m,
                                                        a->e11, a->e11_cap);
          const size_t room = a->cap - a->len[k];
          if (room < 2 * r + 64)
            return 0;
          a->len[k] += dp_doppler_channel_execute (
              a->ch[k], a->e11, r, a->buf[k] + a->len[k], room);
        }
      for (size_t i = 0; i < n; i++)
        x[i] += a->amp[k] * a->buf[k][i];
      a->len[k] -= n;
      memmove (a->buf[k], a->buf[k] + n, a->len[k] * sizeof *a->buf[k]);
    }
  return 1;
}

static const air_row_t AIR_5MCPS_ROWS[] = {
  { "push[air,K=0,D=150,threads=4]", 0, 0, 0.0, 0.0, 1, 4 },
  { "push[air,K=1,design,D=150,threads=4]", 1, 1, 0.0, 0.0, 1, 4 },
  { "push[air,K=1,+6dB,D=150,threads=4]", 1, 1, 6.0, 0.0, 1, 4 },
  { "push[air,K=5,design,D=150,threads=4]", 5, 5, 0.0, 0.0, 1, 4 },
  { "push[air,K=9,design,D=150,threads=1]", 9, 9, 0.0, 0.0, 1, 1 },
  { "push[air,K=9,design,D=150,threads=4]", 9, 9, 0.0, 0.0, 1, 4 },
  { "push[air,K=9,design,D=150,threads=10]", 9, 9, 0.0, 0.0, 1, 10 },
  { "push[air,K=9,+6dB,D=150,threads=4]", 9, 9, 6.0, 0.0, 1, 4 },
  { "push[air,near-far+20dB,D=150,threads=4]", 2, 1, 0.0, 20.0, 1, 4 },
  { "push[air,K=0,D=1,threads=4]", 0, 0, 0.0, 0.0, 0, 4 },
  { "push[air,K=1,design,D=1,threads=4]", 1, 1, 0.0, 0.0, 0, 4 },
  { "push[air,K=9,design,D=1,threads=1]", 9, 9, 0.0, 0.0, 0, 1 },
  { "push[air,K=9,design,D=1,threads=4]", 9, 9, 0.0, 0.0, 0, 4 },
  { "push[air,K=9,design,D=1,threads=10]", 9, 9, 0.0, 0.0, 0, 10 },
  { "push[air,near-far+20dB,D=1,threads=4]", 2, 1, 0.0, 20.0, 0, 4 },
};

/* The 415.1-B-1 rows: nine emitters held and the tenth receiver idle, so
   the search runs every period, on one thread and on 16 (the dev box's
   cores), at each band end. */
static const air_row_t AIR_SPEC_22_ROWS[] = {
  { "push[415.1,2.2GHz,K=9,threads=1]", 9, 9, 0.0, 0.0, 1, 1 },
  { "push[415.1,2.2GHz,K=9,threads=16]", 9, 9, 0.0, 0.0, 1, 16 },
};
static const air_row_t AIR_SPEC_28_ROWS[] = {
  { "push[415.1,2.8GHz,K=9,threads=1]", 9, 9, 0.0, 0.0, 1, 1 },
  { "push[415.1,2.8GHz,K=9,threads=16]", 9, 9, 0.0, 0.0, 1, 16 },
};

static int
bench_on_air (jm_bench_t *bench, const uint8_t *code, const air_pt_t *pt,
              double sym, const air_row_t *rows, int n_rows)
{
  const double fs_in = 11e6, chip = pt->chip, cn0 = pt->cn0;
  int          ok = 1;
  for (int r = 0; r < n_rows && ok; r++)
    {
      const air_row_t            *w   = &rows[r];
      air_src_t                   src = { 0 };
      dp_async_dsss_pool_state_t *p   = dp_async_dsss_pool_create (
          code, CODE_LEN, AIR_RX, chip, sym, fs_in, 2, cn0, 1e-3, 0.9,
          pt->span_hz, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000,
          pt->carrier_hz, 2.0, false,
          w->deep ? pt->code_only_s : CODE_LEN / chip, pt->rate_hz_s,
          w->threads, true, 0);
      /* One push: block_periods code periods at 11 MSa/s, or the pool's
         own dwell. */
      const size_t periods = pt->block_periods ? pt->block_periods
                             : p ? dp_async_dsss_pool_get_coherent_bins (p)
                                 : 1;
      const size_t block
          = (size_t)llround ((double)periods * CODE_LEN / chip * fs_in);
      float _Complex *x = malloc (block * sizeof *x);
      if (!p || !x || !air_src_init (&src, code, w, pt, sym, fs_in, block))
        {
          (void)fprintf (stderr, "bench_async_dsss_pool[%s]: setup failed\n",
                         w->name);
          dp_async_dsss_pool_destroy (p);
          air_src_free (&src);
          free (x);
          ok = 0;
          break;
        }
      double   times[AIR_ITERS];
      int      got = 0, pushes = 0, settled = 0, stim_ok = 1;
      uint64_t pushed = 0, released = 0;
      while (got < AIR_ITERS && pushes < AIR_MAX_PUSHES)
        {
          if (!air_src_next (&src, x, block))
            {
              stim_ok = 0;
              break;
            }
          const uint64_t fed0 = (pushed - p->carry_len) / p->period;
          const uint64_t s0   = p->search_periods;
          const uint64_t t0   = jm_bench_now_ns ();
          (void)dp_async_dsss_pool_push (p, x, block);
          const uint64_t t1 = jm_bench_now_ns ();
          pushed += block;
          pushes++;
          const uint64_t fed = (pushed - p->carry_len) / p->period - fed0;
          const uint64_t rel
              = p->released_duplicate + p->released_ghost + p->released_lost;
          /* Steady state: the held set right, nothing released since the
             push before, and one settled push skipped after lock. */
          const int holds = air_holds (p, w, pt, (double)pushed / fs_in)
                            && rel == released;
          released        = rel;
          settled         = holds ? settled + 1 : 0;
          if (settled >= 2 && p->search_periods - s0 == fed)
            times[got++] = jm_bench_elapsed_sec (t0, t1);
        }
      if (!stim_ok)
        {
          (void)fprintf (stderr,
                         "bench_async_dsss_pool[%s]: the stimulus ran out of "
                         "buffer\n",
                         w->name);
          ok = 0;
        }
      else if (got < AIR_ITERS)
        {
          (void)fprintf (
              stderr,
              "bench_async_dsss_pool[%s]: %d of %d pushes in steady "
              "state (the expected emitters held, nothing released, "
              "the search on throughout), in %d\n",
              w->name, got, AIR_ITERS, pushes);
          ok = 0;
        }
      else
        {
          jm_bench_add_unit (bench, w->name, times, AIR_ITERS, block,
                             JM_BENCH_SAMPLE);
          const double sec = min_sec (times, AIR_ITERS);
          printf ("  %-42s D=%-3zu %7.2f ns/sample  %6.2fx real time  (%d "
                  "pushes; released dup %llu ghost %llu lost %llu)\n",
                  w->name, dp_async_dsss_pool_get_coherent_bins (p),
                  sec / (double)block * 1e9, (double)block / sec / fs_in,
                  pushes, (unsigned long long)p->released_duplicate,
                  (unsigned long long)p->released_ghost,
                  (unsigned long long)p->released_lost);
        }
      dp_async_dsss_pool_destroy (p);
      air_src_free (&src);
      free (x);
    }
  return ok;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };

  const double chip = 5e6, fs_in = 11e6;
  const size_t spc  = 2;
  const double fs   = chip * (double)spc; /* 10 MSa/s at the receivers */
  const double sym  = 3000.0;             /* the link's symbol rate */
  const double tile = chip / CODE_LEN;    /* 1 / T_E */
  const double cn0  = 10.0 + 10.0 * log10 (sym);

  const row_t rows[N_ROWS] = {
    { .name    = "push[native,n=1,threads=1]",
      .n_rx    = 1,
      .span_hz = tile / 2.0,
      .threads = 1 },
    { .name    = "push[native,n=4,threads=1]",
      .n_rx    = 4,
      .span_hz = tile / 2.0,
      .threads = 1 },
    { .name    = "push[native,n=4,threads=4]",
      .n_rx    = 4,
      .span_hz = tile / 2.0,
      .threads = 4 },
    { .name    = "push[full,n=1,threads=1]",
      .n_rx    = 1,
      .span_hz = 50e3,
      .threads = 1 },
    { .name    = "push[full,n=4,threads=1]",
      .n_rx    = 4,
      .span_hz = 50e3,
      .threads = 1 },
    { .name    = "push[full,n=4,threads=4]",
      .n_rx    = 4,
      .span_hz = 50e3,
      .threads = 4 },
    { .name       = "push[full,n=1,threads=1,carrier]",
      .n_rx       = 1,
      .span_hz    = 50e3,
      .threads    = 1,
      .carrier_hz = 2.2e9 },
    { .name       = "push[full,n=4,threads=4,carrier]",
      .n_rx       = 4,
      .span_hz    = 50e3,
      .threads    = 4,
      .carrier_hz = 2.2e9 },
  };

  /* The CCSDS default Gold code, as wfm.Gold() makes it. */
  uint8_t          code[CODE_LEN];
  dp_gold_state_t *gold = dp_gold_create (934, 350, 567, 73, 10);
  if (!gold || dp_gold_generate (gold, CODE_LEN, code, CODE_LEN) != CODE_LEN)
    {
      (void)fprintf (stderr, "bench_async_dsss_pool: gold failed\n");
      return 1;
    }
  dp_gold_destroy (gold);

  dp_async_dsss_pool_state_t *p[N_ROWS];
  dp_RateConverter_state_t   *rc[N_ROWS];
  uint64_t                    pushed[N_ROWS] = { 0 };
  for (int k = 0; k < N_ROWS; k++)
    {
      p[k] = dp_async_dsss_pool_create (
          code, CODE_LEN, rows[k].n_rx, chip, sym, chip * (double)spc, 2, cn0,
          1e-3, 0.9, rows[k].span_hz, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
          100000, rows[k].carrier_hz, 2.0, false, CODE_ONLY_S, DOPPLER_RATE,
          rows[k].threads, true, 0);
      rc[k] = dp_RateConverter_create (fs / fs_in, 0);
      if (!p[k] || !rc[k])
        {
          (void)fprintf (stderr, "bench_async_dsss_pool: %s setup failed\n",
                         rows[k].name);
          return 1;
        }
    }
  /* One dwell, D code periods at the receivers' rate, as input samples.
     The depth is the receivers' and the same at either span. */
  const size_t depth = dp_async_dsss_pool_get_coherent_bins (p[0]);
  for (int k = 1; k < N_ROWS; k++)
    if (dp_async_dsss_pool_get_coherent_bins (p[k]) != depth)
      {
        (void)fprintf (stderr,
                       "bench_async_dsss_pool: %s depth %zu, not "
                       "%zu\n",
                       rows[k].name,
                       dp_async_dsss_pool_get_coherent_bins (p[k]), depth);
        return 1;
      }
  const size_t block_in
      = (size_t)llround ((double)(depth * CODE_LEN * spc) * fs_in / fs);
  const size_t    y_cap = (size_t)ceil ((double)block_in * fs / fs_in) + 64;
  float _Complex *y[N_ROWS];
  for (int k = 0; k < N_ROWS; k++)
    if (!(y[k] = malloc (y_cap * sizeof *y[k])))
      return 1;

  const size_t     n_in = (LEAD + ITERATIONS) * block_in;
  dp_awgn_state_t *g    = dp_awgn_create (
      7921u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs_in)), 1.0f));
  float _Complex *x = malloc (n_in * sizeof *x);
  if (!g || !x)
    {
      (void)fprintf (stderr, "bench_async_dsss_pool: setup failed\n");
      return 1;
    }
  (void)dp_awgn_generate (g, n_in, x, n_in);

  printf ("=== async_dsss_pool: 1,023-chip Gold at 5 Mcps, 11 MSa/s in "
          "(-> 10 MSa/s), noise, searching ===\n");
  printf ("depth D = %zu code periods; one push = one dwell = %zu input "
          "samples; %d rounds\n\n",
          depth, block_in, ITERATIONS);

  double times[N_ROWS][ITERATIONS];
  size_t pos = 0;
  for (int r = -LEAD; r < ITERATIONS; r++, pos += block_in)
    for (int k = 0; k < N_ROWS; k++)
      {
        uint64_t     t0 = jm_bench_now_ns ();
        const size_t ny
            = dp_RateConverter_execute (rc[k], x + pos, block_in, y[k], y_cap);
        (void)dp_async_dsss_pool_push (p[k], y[k], ny);
        uint64_t t1 = jm_bench_now_ns ();
        pushed[k] += ny;
        if (r >= 0)
          times[k][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int k = 0; k < N_ROWS; k++)
    {
      if (!all_searching (p[k], rows[k].n_rx, pushed[k], rows[k].name))
        return 1;
      jm_bench_add_unit (&_bench, rows[k].name, times[k], ITERATIONS, block_in,
                         JM_BENCH_SAMPLE);
      const double sec = min_sec (times[k], ITERATIONS);
      printf ("  %-34s %8.3f ms/dwell  %7.2f ns/sample  %6.2fx real time\n",
              rows[k].name, sec * 1e3, sec / (double)block_in * 1e9,
              (double)block_in / sec / fs_in);
    }

  /* The 5 Mcps point the rows above pin: +/- 50 kHz, a 2.5 GHz carrier,
     emitters spread over +/- 40 kHz with no ramp, a 500-symbol data-free
     block in every 1,000-symbol frame, every push the D = 150 dwell. */
  const air_pt_t at5 = { .chip          = chip,
                         .cn0           = cn0,
                         .carrier_hz    = AIR_CARRIER,
                         .span_hz       = 50e3,
                         .code_only_s   = CODE_ONLY_S,
                         .rate_hz_s     = DOPPLER_RATE,
                         .f_lo          = -40e3,
                         .f_hi          = 40e3,
                         .ramp_hz_s     = 0.0,
                         .tol_hz        = 1e3,
                         .code_only_sym = AIR_CODE_ONLY_SYM,
                         .frame_sym     = AIR_FRAME_SYM,
                         .block_periods = 150 };
  printf ("\n=== async_dsss_pool on the air: 11 MSa/s in, 2.5 GHz carrier, "
          "data-free blocks, %d receivers ===\n",
          AIR_RX);
  if (!bench_on_air (&_bench, code, &at5, sym, AIR_5MCPS_ROWS,
                     (int)(sizeof AIR_5MCPS_ROWS / sizeof *AIR_5MCPS_ROWS)))
    return 1;

  /* The 415.1-B-1 specification point at each band end: chips at
     fc x 31/21216, 3,000 sym/s, +/-1,500 Hz, a 400 Hz/s rate bound, a
     275-symbol code-only window in every 1,000-symbol frame, 4 dB Es/N0.
     The nine emitters sit 311 Hz apart from -1,335 Hz to +1,154 Hz (a
     tracked Doppler is matched within 150 Hz) and hold their Doppler, as
     the receiver's 415.1-B-1 rows do: on +400 Hz/s ramps the as-built pool
     holds three of nine at a wrong Doppler and the row never reaches a
     steady state (measured when these rows were written), and holding a
     signal costs the same either way. Every push is the pool's own dwell,
     D code periods. */
  const double     spec_cn0     = 4.0 + 10.0 * log10 (sym);
  const double     fcs[2]       = { 2.2e9, 2.8e9 };
  const air_row_t *spec_rows[2] = { AIR_SPEC_22_ROWS, AIR_SPEC_28_ROWS };
  for (int i = 0; i < 2; i++)
    {
      const air_pt_t sp = { .chip          = fcs[i] * 31.0 / 21216.0,
                            .cn0           = spec_cn0,
                            .carrier_hz    = fcs[i],
                            .span_hz       = 1500.0,
                            .code_only_s   = 275.0 / sym,
                            .rate_hz_s     = 400.0,
                            .f_lo          = -1450.0,
                            .f_hi          = 1350.0,
                            .ramp_hz_s     = 0.0,
                            .tol_hz        = 150.0,
                            .code_only_sym = 275,
                            .frame_sym     = AIR_FRAME_SYM,
                            .block_periods = 0 };
      printf ("\n=== async_dsss_pool at 415.1-B-1, %.1f GHz: %.4f Mcps, "
              "11 MSa/s in, 4 dB Es/N0, 9 emitters, %d receivers ===\n",
              fcs[i] / 1e9, sp.chip / 1e6, AIR_RX);
      if (!bench_on_air (&_bench, code, &sp, sym, spec_rows[i], 2))
        return 1;
    }
  jm_bench_write_json (&_bench, "async_dsss_pool");
  for (int k = 0; k < N_ROWS; k++)
    {
      dp_async_dsss_pool_destroy (p[k]);
      dp_RateConverter_destroy (rc[k]);
      free (y[k]);
    }
  dp_awgn_destroy (g);
  free (x);
  return 0;
}
