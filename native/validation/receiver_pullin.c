/**
 * @file receiver_pullin.c
 * @brief The receiver's pull-in from a seed off in carrier, as a curve:
 *        the cell mode's estimate on its live chain (design section
 *        12.28; measured beside the hand-off flavour's refine chain
 *        before that flavour was retired -- the record keeps the pair).
 *
 * Section 12.27 met one 40 dB-Hz stint where a data-block seed 822 Hz off
 * was pulled in by the hand-off receiver's refine and not by the cell
 * receiver's estimate. One stint is a count. This harness draws the curve:
 * one emitter at the operating geometry (Gold-1023 at 5 Mcps, 2700 sym/s
 * async BPSK with the code-only window, through the channel at its own
 * Doppler, noise after it), and for each seed offset in carrier a fresh
 * receiver of each flavour seeded from the TRUTH at that offset -- a tenth
 * of a chip off in code, so the carrier axis is the one under test -- from
 * a point inside the data (the searcher's data-block copy seeds there,
 * section 12.14), fed for PULLIN_S, and scored: both lock flags up at the
 * end with the status Doppler within a row of the truth. The fraction of
 * TRIALS draws that pull in, per offset, at 45 and 40 dB-Hz.
 *
 * Usage:
 *   validate_receiver_pullin            the curve: OFFSETS at both C/N0s,
 *                                       TRIALS draws each
 *   validate_receiver_pullin --check    45 dB-Hz, three offsets, fewer
 *                                       draws: every draw pulls in to
 *                                       800 Hz
 */
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "awgn/awgn_core.h"
#include "clib_common.h"
#include "doppler_channel/doppler_channel_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CPS (CHIP_RATE / SYM_RATE)
#define W_SYM 450u
#define F_SYM 4950u
#define CARRIER_HZ 2.5e9
#define PFA 1e-3
#define PD 0.9
#define SEGMENTS 4u
#define RX_SPS 8u
#define D_OP 154u /* the pool's depth: the cell's correction interval */
#define SEED_U0                                                               \
  0.1                /* the seed's code offset, chips: the carrier axis is    \
                        the one under test                               */
#define PULLIN_S 1.0 /* fed this long from the seed                 */
#define SEED_AT_FRAMES 2.5 /* seed inside the data of the third frame    */
#define TRIALS 10
#define CHECK_TRIALS 6
#define DISCARD 4096u

static const double OFFSETS[]
    = { 0.0, 100.0, 200.0, 400.0, 600.0, 800.0, 1000.0, 1500.0 };
#define N_OFFSETS (sizeof OFFSETS / sizeof OFFSETS[0])
static const double CHECK_OFFSETS[] = { 0.0, 400.0, 800.0 };
#define N_CHECK_OFFSETS (sizeof CHECK_OFFSETS / sizeof CHECK_OFFSETS[0])

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

/* One draw's stream: the synth from a random frame position through the
   channel at `ppm`, noise after; the truth is the channel's mapping. */
typedef struct
{
  float complex *x;
  size_t         n;
  double         ppm, delay, doppler_hz;
} draw_t;

static int
draw_open (draw_t *d, const uint8_t *code, double cn0_dbhz, uint32_t seed,
           double seconds)
{
  uint32_t rng = seed * 0x9e3779b9u;
  for (int w = 0; w < 4; w++)
    (void)dp_xs32 (&rng);
  d->ppm        = -20.0 + 40.0 * (double)(dp_xs32 (&rng) >> 8) / 16777216.0;
  d->doppler_hz = d->ppm * 1e-6 * CARRIER_HZ;
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                          seed, (int)SPC, 15, 0, 0, 0.0);
  DP_REQUIRE (syn != NULL);
  DP_REQUIRE (
      wfm_synth_set_dsss_cont (syn, code, SF, CPS, WFM_DSSS_DATA_PRBS, NULL, 0)
      == 0);
  DP_REQUIRE (wfm_synth_set_dsss_window (syn, W_SYM, F_SYM) == 0);
  doppler_channel_state_t *ch
      = doppler_channel_create (FS, CARRIER_HZ, d->ppm, 0.0);
  DP_REQUIRE (ch != NULL);
  d->delay        = doppler_channel_get_delay_samples (ch);
  awgn_state_t *g = awgn_create (
      seed * 7919u + 1u,
      awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
  DP_REQUIRE (g != NULL);
  d->n                = (size_t)(seconds * FS);
  d->x                = dp_xmalloc ((d->n + 4 * TE) * sizeof *d->x);
  float complex *sig  = dp_xmalloc (TE * sizeof *sig);
  float complex *fifo = dp_xmalloc (4 * TE * sizeof *fifo);
  float complex *nz   = dp_xmalloc (TE * sizeof *nz);
  size_t         pend = 0, out = 0, dropped = 0;
  while (out < d->n)
    {
      wfm_synth_steps (syn, sig, TE);
      pend += doppler_channel_execute (ch, sig, TE, fifo + pend, 2 * TE);
      size_t take = pend;
      if (dropped < DISCARD)
        {
          size_t skip = take < DISCARD - dropped ? take : DISCARD - dropped;
          memmove (fifo, fifo + skip, (pend - skip) * sizeof *fifo);
          pend -= skip;
          dropped += skip;
          continue;
        }
      if (out + take > d->n)
        take = d->n - out;
      awgn_generate (g, take, nz, TE);
      for (size_t i = 0; i < take; i++)
        d->x[out + i] = fifo[i] + nz[i];
      out += take;
      memmove (fifo, fifo + take, (pend - take) * sizeof *fifo);
      pend -= take;
    }
  free (nz);
  free (fifo);
  free (sig);
  awgn_destroy (g);
  doppler_channel_destroy (ch);
  wfm_synth_destroy (syn);
  return 0;
}

/* The code phase (chips, unwrapped) the stream carries at output k: the
   channel's mapping, output k carries the input at k (1 + d) - delay. */
static double
truth_chips (const draw_t *d, double k)
{
  return ((k + (double)DISCARD) * (1.0 + d->ppm * 1e-6) - d->delay)
         / (double)SPC;
}

/* One cell receiver seeded `off_hz` off the truth at sample `at`, fed
   PULLIN_S from there: 1 if both flags are up at the end with the status
   Doppler within a row of the truth; `t_lock` the seconds to the first
   push with both flags up (or -1). */
static int
pullin (const draw_t *d, const uint8_t *code, double cn0_dbhz, double off_hz,
        size_t at, double *t_lock)
{
  async_dsss_receiver_state_t *rx = async_dsss_receiver_create_cell (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0_dbhz, PFA, PD, SEGMENTS,
      RX_SPS, 0, CARRIER_HZ, 0.0, D_OP, ASYNC_DSSS_RX_CELL_GAIN,
      ASYNC_DSSS_RX_CELL_PULLIN);
  DP_REQUIRE (rx != NULL);
  const double phase
      = dp_fmod_pos (truth_chips (d, (double)at) + SEED_U0, (double)SF);
  DP_REQUIRE (
      async_dsss_receiver_seed (rx, phase, d->doppler_hz + off_hz, cn0_dbhz)
      == DP_OK);
  const size_t   n_feed = (size_t)(PULLIN_S * FS);
  float complex *out    = dp_xmalloc (TE * sizeof *out);
  *t_lock               = -1.0;
  size_t pos            = at;
  while (pos + TE <= at + n_feed && pos + TE <= d->n)
    {
      (void)async_dsss_receiver_steps (rx, d->x + pos, TE, out, TE);
      pos += TE;
      if (*t_lock < 0.0)
        {
          async_dsss_receiver_status_t st = async_dsss_receiver_status (rx);
          if (st.code_locked && st.locked)
            *t_lock = (double)(pos - at) / FS;
        }
    }
  async_dsss_receiver_status_t st = async_dsss_receiver_status (rx);
  const int ok = st.code_locked && st.locked
                 && fabs (st.doppler_hz - d->doppler_hz) < 31.7;
  free (out);
  async_dsss_receiver_destroy (rx);
  return ok;
}

int
main (int argc, char **argv)
{
  const int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t   code[SF];
  gold_1023 (code);
  const double *offs    = check ? CHECK_OFFSETS : OFFSETS;
  const size_t  n_offs  = check ? N_CHECK_OFFSETS : N_OFFSETS;
  const size_t  trials  = check ? CHECK_TRIALS : TRIALS;
  const double  cn0s[]  = { 45.0, 40.0 };
  const size_t  n_cn0   = check ? 1 : 2;
  const size_t  at      = (size_t)(SEED_AT_FRAMES * F_SYM * FS / SYM_RATE);
  const double  seconds = (double)at / FS + PULLIN_S + 0.1;

  printf ("the receiver's pull-in from a seed off in carrier: Gold-1023 at "
          "5 Mcps, %.0f sym/s async BPSK, window %u of %u; the seed from "
          "the truth %.1f chip off in code at %.1f frames (inside the "
          "data), fed %.1f s; loop 1's bound %.1f Hz, reliable to twice; "
          "%zu draws per point\n\n",
          SYM_RATE, W_SYM, F_SYM, SEED_U0, SEED_AT_FRAMES, PULLIN_S,
          ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (CHIP_RATE, SF), trials);
  for (size_t ci = 0; ci < n_cn0; ci++)
    {
      printf ("  %.0f dB-Hz:\n", cn0s[ci]);
      printf ("    offset Hz   pulled in   t_lock mean\n");
      draw_t *draws = dp_xcalloc (trials, sizeof *draws);
      for (size_t t = 0; t < trials; t++)
        DP_REQUIRE (
            draw_open (&draws[t], code, cn0s[ci], 100u + (uint32_t)t, seconds)
            == 0);
      for (size_t oi = 0; oi < n_offs; oi++)
        {
          size_t ok = 0;
          double tl = 0.0;
          for (size_t t = 0; t < trials; t++)
            {
              double tlock;
              /* Both signs: the seed's error has no preferred side. */
              const double sgn = (t & 1u) ? -1.0 : 1.0;
              int r = pullin (&draws[t], code, cn0s[ci], sgn * offs[oi], at,
                              &tlock);
              ok += (size_t)r;
              if (r)
                tl += tlock;
            }
          printf ("    %7.0f     %2zu / %2zu     %.3f s\n", offs[oi], ok,
                  trials, ok ? tl / (double)ok : 0.0);
          if (check)
            DP_CHECK_MSG (ok == trials, "at 45 dB-Hz the receiver pulls in "
                                        "every draw to 800 Hz off");
        }
      for (size_t t = 0; t < trials; t++)
        free (draws[t].x);
      free (draws);
      printf ("\n");
    }
  DP_TEST_END ("validate_receiver_pullin");
}
