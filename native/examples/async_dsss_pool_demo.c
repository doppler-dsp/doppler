/**
 * async_dsss_pool_demo.c -- an AsyncDsssPool holding several emitters that
 * come and go, in C.
 *
 * This is the C twin of `src/doppler/examples/async_dsss_pool_demo.py`: the
 * same capture from the same seeds, the same pool, the same checks, and the
 * same printed lines (src/doppler/tests/test_c_example_pairs.py runs both
 * and requires that). Three receivers share one stream. Four emitters on
 * one Gold code reach it at different times, code phases and carriers: A, B
 * and C arrive one after another, B leaves, and D arrives after it.
 *
 * What the binding does for you, and this file therefore shows:
 *
 *   - create() takes every receiver's configuration in one call and builds
 *     the pool's receivers alike; the code array is the caller's to keep
 *     only until create() returns.
 *   - push() takes any block. The pool feeds its search and its receivers
 *     one code period a call, a group of ASYNC_DSSS_POOL_PERIODS_PER_JOIN
 *     periods at a time, runs its controller at the end of each group, and
 *     keeps what is past the last whole group for the next push. So a push
 *     of one code epoch, as here, mostly fills the carry, and every
 *     sixteenth one runs a group: the result is the stream's, not the
 *     blocking's, and the latency is up to one group.
 *   - status() is one receiver's status as of the last whole period fed;
 *     symbols() copies the symbols one receiver emitted during the last
 *     push into a buffer the CALLER owns, sized by symbols_max_out().
 *   - the release counters (released_lost, released_duplicate,
 *     released_ghost) are fields of the pool's state, read directly.
 *   - destroy() frees the pool and every receiver in it; the buffers are
 *     freed here.
 *
 * The waveform is not built here: each emitter is the shipped `wfm_synth`
 * continuous DSSS with its own PRBS seed, placed on the stream at its own
 * sample offset and carrier, and the noise is the shipped awgn at the sigma
 * dp_wfm_awgn_amplitude() gives for the Es/N0 every emitter shares. The
 * stream is summed in the Python twin's order (emitters A to D, then the
 * noise), so the two captures are the same float32 samples.
 *
 * The checks read only what the pool reports, after every push. An emitter
 * is identified by its code phase, which this file knows because it placed
 * each emitter on the stream:
 *
 *   - one receiver per emitter while it is on the air (from 0.5 s after it
 *     arrives), and none on it once it has gone (from lost_confirm_s plus
 *     0.2 s after it leaves);
 *   - a departure freed a receiver (released_lost), and the newcomer D was
 *     taken by the receiver B freed;
 *   - every receiver has symbol lock at the end, and a receiver tracking an
 *     emitter emits that emitter's symbols.
 *
 * The design is docs/design/async-dsss-receiver.md, sections 5 to 9.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_pool_demo
 */
#include "doppler/dp_complex.h"
#include <doppler/async_dsss_pool/async_dsss_pool_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/gold/gold_core.h>
#include <doppler/wfm/wfm_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Setup: the Python twin's numbers ──────────────────────────────────────
 * Gold-1023 at 3.069 Mcps, 2700 sym/s BPSK, two samples a chip, so one code
 * epoch is 2,046 samples: one push. The search covers 5 kHz either side;
 * every emitter is at Es/N0 10 dB. */
#define SF 1023u
#define SPC 2u
#define TE ((size_t)SF * SPC)
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define EPOCH_S ((double)TE / FS)
#define CARRIER_UNCERTAINTY 5000.0
#define ESN0_DB 10.0
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
#define LOST_CONFIRM_S 0.1
#define N_RECEIVERS 3u
#define SEED_NOISE 5u
#define N_EPOCHS 9600u /* 3.2 s */

/* The emitters: name, carrier (Hz), code-phase offset (samples into its
   first epoch, so each sits hundreds of chips from the others), and the
   epochs it is on the air, [first, end). One table, read by the stream,
   the checks and the report. */
struct emitter
{
  const char *name;
  double      freq;
  size_t      offset;
  size_t      first, end;
};
static const struct emitter EMITTERS[] = {
  { "A", 1500.0, 0, 90, 9600 },
  { "B", -2200.0, 600, 900, 4500 },
  { "C", 700.0, 1300, 1800, 9600 },
  { "D", -1000.0, 1800, 5700, 9600 },
};
#define N_EM (sizeof EMITTERS / sizeof EMITTERS[0])

#define CHECK(cond, ...)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: " __VA_ARGS__);                             \
          fputc ('\n', stderr);                                               \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/** @brief Emitter @p k's code phase, chips, at stream sample @p sample: its
 *  code starts at chip 0 on its first sample. */
static double
emitter_phase (size_t k, size_t sample)
{
  const double s0 = (double)(EMITTERS[k].first * TE + EMITTERS[k].offset);
  return fmod (((double)sample - s0) / (double)SPC, (double)SF);
}

/** @brief One emitter's source: the shipped continuous-DSSS synth at its
 *  carrier, a PRBS payload of its own seed on the Gold code, clean. */
static dp_wfm_synth_state_t *
emitter_synth (const uint8_t *code, size_t k)
{
  dp_wfm_synth_state_t *syn = dp_wfm_synth_create (
      WFM_SYNTH_DSSS, FS, EMITTERS[k].freq, WFM_SYNTH_SNR_CLEAN, 1,
      11u + (uint32_t)k, (int)SPC, 15, 0, 0, 0.0);
  if (syn
      && dp_wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                     WFM_DSSS_DATA_PRBS, NULL, 0)
             != 0)
    {
      dp_wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

/** @brief Which emitter receiver @p i holds after push @p e: the one within
 *  1.5 chips of its code phase, or -1 when it holds nothing (idle, lost).
 *  A receiver still holding an emitter that has left -- until its release
 *  fires -- reads that emitter. */
static int
holder_of (const async_dsss_receiver_status_t *st, size_t e)
{
  if (st->state != ASYNC_DSSS_RX_REFINING
      && st->state != ASYNC_DSSS_RX_TRACKING)
    return -1;
  const size_t end = (e + 1) * TE;
  int          out = -1;
  for (size_t k = 0; k < N_EM; k++)
    {
      const double d = fabs (st->chip_phase - emitter_phase (k, end));
      if (e >= EMITTERS[k].first && fmin (d, SF - d) <= 1.5)
        out = (int)k;
    }
  return out;
}

int
main (void)
{
  uint8_t          code[SF];
  dp_gold_state_t *gd = dp_gold_create (934, 350, 567, 73, 10);
  CHECK (gd != NULL, "the Gold generator opens");
  dp_gold_generate (gd, SF, code, SF);
  dp_gold_destroy (gd);

  /* The sources: one synth an emitter, stepped only over the samples it is
     on the air, so each starts its code and data at its own first sample;
     and the noise, stepped over every sample. */
  dp_wfm_synth_state_t *syn[N_EM];
  for (size_t k = 0; k < N_EM; k++)
    {
      syn[k] = emitter_synth (code, k);
      CHECK (syn[k] != NULL, "emitter %s opens", EMITTERS[k].name);
    }
  dp_awgn_state_t *awg = dp_awgn_create (
      SEED_NOISE, dp_wfm_awgn_amplitude (
                      (float)(ESN0_DB - 10.0 * log10 (FS / SYM_RATE)), 1.0f));
  CHECK (awg != NULL, "the noise opens");

  /* The pool. create() takes its parameters in signature order, each named
     here, so an arity mismatch shows as one line rather than a count of
     commas. Everything not set by the scenario is AsyncDsssPool's default;
     lost_confirm_s arms each receiver's release rule, which is what frees
     a receiver whose emitter left. threads 0 is the online cores: the
     result does not depend on it. */
  const size_t n_receivers             = N_RECEIVERS;
  const double chip_rate               = CHIP_RATE;
  const double symbol_rate             = SYM_RATE;
  const double sample_rate             = FS;
  const int    m                       = 2; /* BPSK */
  const double cn0_dbhz                = CN0_DBHZ;
  const double pfa                     = 1e-3;
  const double pd                      = 0.9;
  const double doppler_uncertainty     = CARRIER_UNCERTAINTY;
  const size_t segments                = 4;
  const size_t sps                     = 8;
  const int    differential            = 0;
  const double refine_max_error_db     = 0.5;
  const size_t refine_samples_per_sym  = 4;
  const double refine_design_margin_db = 14.0;
  const size_t refine_n_fft            = 64;
  const size_t refine_zero_pad         = 8;
  const bool   refine_sequential       = false;
  const size_t refine_max_n_blocks     = 100000;
  const double carrier_freq_hz         = 0.0; /* no carrier-to-code aiding */
  const double lost_confirm_s          = LOST_CONFIRM_S;
  const bool   reacquire               = false;
  const double code_only_s             = 0.0;
  const double doppler_rate            = 0.0;
  const int    threads                 = 0;
  const bool   refine                  = true;

  dp_async_dsss_pool_state_t *pool = dp_async_dsss_pool_create (
      code, SF, n_receivers, chip_rate, symbol_rate, sample_rate, m, cn0_dbhz,
      pfa, pd, doppler_uncertainty, segments, sps, differential,
      refine_max_error_db, refine_samples_per_sym, refine_design_margin_db,
      refine_n_fft, refine_zero_pad, refine_sequential, refine_max_n_blocks,
      carrier_freq_hz, lost_confirm_s, reacquire, code_only_s, doppler_rate,
      threads, refine);
  CHECK (pool != NULL, "the pool opens");

  /* The caller's buffers: one push, one emitter's share of it, one
     receiver's symbols (grown to symbols_max_out() when a push needs
     more), and one row a push of what each receiver holds and its lock. */
  float complex *x       = malloc ((size_t)TE * sizeof *x);
  float complex *sig     = malloc ((size_t)TE * sizeof *sig);
  size_t         sym_cap = 64;
  float complex *sym     = malloc (sym_cap * sizeof *sym);
  int           *held = malloc ((size_t)N_EPOCHS * N_RECEIVERS * sizeof *held);
  int *locked = malloc ((size_t)N_EPOCHS * N_RECEIVERS * sizeof *locked);
  CHECK (x && sig && sym && held && locked, "the buffers allocate");
  size_t own_syms[N_EM] = { 0 }; /* symbols a holder of each emitter gave */

  /* ── stream the capture, one code epoch per push ──────────────────────── */
  for (size_t e = 0; e < N_EPOCHS; e++)
    {
      const size_t a = e * TE, b = a + TE;
      for (size_t i = 0; i < TE; i++)
        x[i] = 0.0f;
      for (size_t k = 0; k < N_EM; k++)
        {
          const size_t s0 = EMITTERS[k].first * TE + EMITTERS[k].offset;
          const size_t s1 = EMITTERS[k].end * TE;
          const size_t lo = a > s0 ? a : s0, hi = b < s1 ? b : s1;
          if (lo >= hi)
            continue;
          dp_wfm_synth_steps (syn[k], sig, hi - lo);
          for (size_t i = 0; i < hi - lo; i++)
            x[lo - a + i] += sig[i];
        }
      dp_awgn_generate (awg, TE, sig, TE);
      for (size_t i = 0; i < TE; i++)
        x[i] += sig[i];

      (void)dp_async_dsss_pool_push (pool, x, TE);

      /* The caller's size for symbols(): the most any receiver emitted. */
      const size_t need = dp_async_dsss_pool_symbols_max_out (pool);
      if (need > sym_cap)
        {
          float complex *grown = realloc (sym, need * sizeof *sym);
          CHECK (grown != NULL, "the symbol buffer grows");
          sym     = grown;
          sym_cap = need;
        }
      for (size_t i = 0; i < N_RECEIVERS; i++)
        {
          const async_dsss_receiver_status_t st
              = dp_async_dsss_pool_status (pool, i);
          const int k                 = holder_of (&st, e);
          held[e * N_RECEIVERS + i]   = k;
          locked[e * N_RECEIVERS + i] = st.locked;
          const size_t n = dp_async_dsss_pool_symbols (pool, i, sym, sym_cap);
          CHECK (n <= need, "receiver %zu gave %zu symbols past the bound %zu",
                 i, n, need);
          if (k >= 0 && st.locked)
            own_syms[k] += n;
        }
    }

  /* ── the checks, on the rows ──────────────────────────────────────────── */
  const size_t settle = (size_t)(0.5 / EPOCH_S);
  const size_t gone   = (size_t)((LOST_CONFIRM_S + 0.2) / EPOCH_S);
  double       arrive_ms[N_EM], release_ms = 0.0;
  for (size_t k = 0; k < N_EM; k++)
    {
      const struct emitter *em = &EMITTERS[k];
      for (size_t e = em->first + settle; e < em->end; e++)
        {
          size_t n = 0;
          for (size_t i = 0; i < N_RECEIVERS; i++)
            n += held[e * N_RECEIVERS + i] == (int)k;
          CHECK (n == 1,
                 "%s: held by %zu receivers while on the air (push "
                 "%zu)",
                 em->name, n, e);
        }
      size_t first = em->end, last = 0;
      for (size_t e = 0; e < N_EPOCHS; e++)
        for (size_t i = 0; i < N_RECEIVERS; i++)
          if (held[e * N_RECEIVERS + i] == (int)k)
            {
              if (e >= em->first && e < first)
                first = e;
              last = e;
            }
      CHECK (first < em->end, "%s was never held", em->name);
      arrive_ms[k] = (double)(first - em->first) * EPOCH_S * 1e3;
      if (em->end < N_EPOCHS)
        {
          /* Left: no receiver on it once the release has had its time. */
          for (size_t e = em->end + gone; e < N_EPOCHS; e++)
            for (size_t i = 0; i < N_RECEIVERS; i++)
              CHECK (held[e * N_RECEIVERS + i] != (int)k,
                     "%s still held after it left (push %zu)", em->name, e);
          release_ms = (double)(last + 1 - em->end) * EPOCH_S * 1e3;
        }
      /* A receiver holding an emitter with symbol lock emits symbols. */
      CHECK (own_syms[k] > 0, "%s's holder emitted no symbols", em->name);
    }

  /* A departure freed a receiver, and the newcomer took it: the first
     receiver ever to hold B is the first ever to hold D. */
  CHECK (pool->released_lost >= 1, "no receiver was released");
  size_t b_rx = N_RECEIVERS, d_rx = N_RECEIVERS;
  for (size_t i = N_RECEIVERS; i-- > 0;)
    for (size_t e = 0; e < N_EPOCHS; e++)
      {
        if (held[e * N_RECEIVERS + i] == 1)
          b_rx = i;
        if (held[e * N_RECEIVERS + i] == 3)
          d_rx = i;
      }
  CHECK (b_rx < N_RECEIVERS && b_rx == d_rx,
         "D was taken by receiver %zu, not the receiver %zu B freed", d_rx,
         b_rx);
  /* Every receiver holding a signal at the end has symbol lock. */
  for (size_t i = 0; i < N_RECEIVERS; i++)
    CHECK (locked[(N_EPOCHS - 1) * N_RECEIVERS + i] == 1,
           "receiver %zu not symbol-locked", i);

  for (size_t k = 0; k < N_EM; k++)
    printf ("%s: held %.0f ms after it arrived\n", EMITTERS[k].name,
            arrive_ms[k]);
  printf ("B left; receiver %zu was released %.0f ms later (%llu release(s), "
          "%llu duplicate(s)) and took D\n",
          b_rx, release_ms, (unsigned long long)pool->released_lost,
          (unsigned long long)pool->released_duplicate);

  /* The pool owns its receivers and its search; destroy() frees them all.
     The buffers and the sources are the caller's. */
  dp_async_dsss_pool_destroy (pool);
  free (locked);
  free (held);
  free (sym);
  free (sig);
  free (x);
  dp_awgn_destroy (awg);
  for (size_t k = 0; k < N_EM; k++)
    dp_wfm_synth_destroy (syn[k]);
  printf ("OK\n");
  return 0;
}
