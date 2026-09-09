/**
 * @file acq_surface_jitter.c
 * @brief The searcher's surface as a code tracker: the early/prompt/late
 *        discriminator read off the correlation surface, its jitter and
 *        bias against the generator's clock, beside the DLL's (§12.5).
 *
 * The continuous searcher computes every emitter's code phase and Doppler
 * at full price every dwell, and each hand-off receiver then re-derives
 * the same position with its own DLL. The surface's cells sit on the same
 * half-chip grid the DLL's early and late arms use, so the normalised
 * `(L - E) / (L + E)` at the cell nearest an emitter IS the DLL's
 * discriminator, integrated over a dwell instead of an epoch. If its
 * jitter and bias come in at or under the DLL's, the searcher can own the
 * code phase and the receiver keeps only what lives at symbol rate. If
 * not, the idea dies here, cheaply.
 *
 * What is measured, per decided dwell, at the cell NEAREST THE TRUTH (the
 * cell a locked tracker would be holding, so the number is the
 * discriminator's noise and not a detector's):
 *
 *   code    the calibrated `(L - E) / (L + E)` of magnitudes on the
 *           truth's row; the same on the tile's rows summed as power
 *           (Parseval: the per-epoch non-coherent sum, which is what the
 *           surface still holds while the emitter carries data and its
 *           coherent peak is spread across the rows); a three-point
 *           parabola on the same cells as the calibration-free
 *           alternative; and, from the complex intermediates the engine
 *           exposes (§12.21), the COHERENT discriminator
 *           `Re(conj(P) (L - E)) / |P|^2` on the truth's row of the
 *           complex surface, and the same formed per epoch from the
 *           block's prompt column at the three cells and summed over the
 *           block -- data-invariant, since an epoch's E, P and L carry
 *           the same symbol -- which is the read under data;
 *   doppler the parabola over the rows above and below, in Hz; and the
 *           slope of a linear phase fit along the block's prompt column,
 *           unsquared in the window and on P^2 under data, with the fit's
 *           residual as the carrier phase noise per epoch;
 *   raw     the symbol-aligned re-correlation on `block_raw` (§12.22):
 *           the shipped DLL, its loop held (dll_set_coast), its rate
 *           aided by the held Doppler, its symbol window on, PUT AT THE
 *           CELL'S PHASE at every block's start (dll_set_code_phase --
 *           a coasting loop drifts on its NCO's quantisation of the aid,
 *           measured here in chips per second) and fed the block wiped of
 *           the held Doppler by the shipped LO, as a receiver's Costas
 *           wipes it before its DLL; the mean of its discriminator over
 *           the block, through its own S-curve, is the read -- the DLL's
 *           own window on the searcher's timing;
 *   argmax  whether the surface's own maximum was within a chip of the
 *           truth -- the detector's view, for scale;
 *   held    the same coasting DLL closed on the SEARCHER'S OWN CELL
 *           (§12.23), the truth consulted only to score: the tracker
 *           acquires at the first window dwell from the surface alone
 *           (the argmax cell, the calibrated E/L on its row for the
 *           phase within the cell, the parabola over the rows for the
 *           Doppler, which is also the code rate), dead-reckons the held
 *           phase across each block on that rate, puts the loop there,
 *           and corrects the phase by a GAIN times what the loop read,
 *           once the symbol aid has settled: gain 1 puts the phase at
 *           the read (§12.23); below it a first-order loop keeps
 *           g / (2 - g) of the read's variance and the dead reckoning
 *           carries the rest (§12.24, gains 1, 1/2, 1/4, 1/8 under
 *           dilation). Scored per block as the phase it held at the
 *           block's middle against the truth, and whether it ever left
 *           the cell.
 *
 * Each in two classes: dwells whose block lies inside the emitter's
 * code-only window (the coherent case the searcher detects in) and
 * dwells under data. The discriminator's gain and the truth's constant
 * (the synth's shaping delay and the engine's column convention) are
 * calibrated once on a CLEAN stream under dilation, which sweeps the
 * truth through every sub-cell offset; the gain is printed, and a
 * tracker would carry the same number.
 *
 * Method. The operating point of §12: Gold-1023 (CCSDS #365) at 5 Mcps,
 * two samples per chip, async BPSK at 2700 sym/s with the shipped synth's
 * PRBS data and its 450-of-4950 code-only window; the synth at baseband
 * through the shipped doppler_channel at 0 and 18 ppm of 2.5 GHz (the
 * chips dilated with the carrier, 45 kHz inside the ±50 kHz span);
 * noise from the shipped awgn after the channel, sized by
 * awgn_amplitude_for_snr() from the C/N0. The engine is the pool's:
 * acq_create_continuous() at ±50 kHz with the window's depth (D = 154
 * under SPEC's Doppler rate) and the carrier told, its surface read
 * through acq_set_surface_sink(). The truth is the synth's own clock
 * through the channel's documented mapping, at the dwell's middle
 * (§12.11: the block's peak is its middle). Nothing here builds a chip,
 * a bit or a sigma by hand.
 *
 * Usage:
 *   validate_acq_surface_jitter            the full tables
 *   validate_acq_surface_jitter --check    the spot check CTest runs: the
 *                                          discriminator is linear across
 *                                          the cell on a clean sweep, and
 *                                          at 45 dB-Hz under dilation the
 *                                          window-dwell jitter and bias of
 *                                          the magnitude and the coherent
 *                                          reads, and the coasting DLL's
 *                                          read under data, are within the
 *                                          measured bounds; and the same
 *                                          DLL running locks on the cell;
 *                                          and closed on its own cell the
 *                                          tracker acquires, never leaves
 *                                          it, and holds the phase within
 *                                          the same bound; at gain 1/4 it
 *                                          holds under three quarters of
 *                                          gain 1's jitter
 */
#include "acq/acq_core.h"
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "awgn/awgn_core.h"
#include "clib_common.h"
#include "dll/dll_core.h"
#include "doppler_channel/doppler_channel_core.h"
#include "dp_ber_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "dp_tlm/dp_tlm_core.h"
#include "gold/gold_core.h"
#include "lo/lo_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CPS (CHIP_RATE / SYM_RATE) /* chips per symbol, 1851.85       */
#define W_SYM 450u                 /* code-only symbols per frame     */
#define F_SYM 4950u                /* frame, symbols (§5.4)           */
#define CARRIER_HZ 2.5e9
#define MAX_PPM 20.0
#define DU (MAX_PPM * 1e-6 * CARRIER_HZ) /* the searcher's span, ±50 kHz */
#define PFA 1e-3
#define PD 0.9
#define DOPPLER_RATE 500.0 /* SPEC's Hz/s: bounds D to 154 in the window */
/* Whole code-only epochs the window holds at any chip phase (§2.1). */
#define CODE_ONLY_EPOCHS ((size_t)((double)W_SYM * CPS / (double)SF) - 1u)
#define THREADS 8

#define DISCARD                                                               \
  300u                   /* output samples dropped before the engine's stream \
                            starts: the truth sits off a cell boundary        */
#define MEAS_DWELLS 800  /* ~70 in the window, the rest under data   */
#define CHECK_DWELLS 150 /* ~12 in the window                        */
#define CAL_DWELLS 200   /* code only: every dwell is coherent         */
#define DLL_45 0.0132    /* §12.5: the receiver's DLL, per-epoch, chips     */
#define DLL_40 0.0208
#define SEGMENTS 4u /* the receiver\'s partials per epoch (dll_aid_jitter) */
#define BN 0.002    /* the receiver\'s DLL bandwidth; held, so unused   */
#define P_SYM ((double)SEGMENTS * CHIP_RATE / ((double)SF * SYM_RATE))
#define DLL_SETTLE 6 /* blocks the symbol aid settles over, not scored   */
#define N_BITS (1u << 16) /* the synth's payload, a harness PRBS, cycled  */
#define RX_SPS 8u         /* the receiver's MpskReceiver samples per symbol */
#define DLL_U0 0.1 /* the coasting loop\'s seed offset from the cell    */
/* The held tracker's correction gains: 1 puts the phase at the read; a
   first-order loop below it keeps g / (2 - g) of the read's variance
   and lets the dead reckoning carry the rest (section 12.24). */
static const double HELD_GAINS[] = { 1.0, 0.5, 0.25, 0.125 };
#define N_GAINS (sizeof HELD_GAINS / sizeof HELD_GAINS[0])

/* The stimulus: one emitter through the channel, noise after it. */
typedef struct
{
  wfm_synth_state_t       *syn;
  doppler_channel_state_t *ch;
  awgn_state_t            *g;
  double                   ppm, delay;
  float complex           *sig, *fifo, *blk, *nz;
  size_t                   pend;
} stim_t;

/* One decided dwell, as the sink saw it. */
typedef struct
{
  uint64_t at;     /* samples_consumed at the dwell's end             */
  double   truth;  /* raw truth at the dwell's middle, chips (no c0)  */
  double   sym_lo; /* frame position of the block's start, symbols   */
  double   sym_hi; /* and its end                                     */
  double   x_cell; /* the argmax cell's chip phase minus the raw truth,
                      wrapped: c0 plus the sub-cell offset            */
  int hit;         /* argmax within a chip of the truth               */
  int in_win;      /* the block inside the code-only window           */
  /* At the cell nearest the truth (only meaningful once c0 is known;
     during calibration the argmax cell is used instead): */
  double e, p, l;    /* the truth row's early, prompt, late            */
  double ep, pp, lp; /* the tile's rows summed as power                */
  double u, pc, b;   /* the rows above, at and below, at the truth col */
  double row_hz;     /* the truth row's frequency                      */
  double cell_chip;  /* the cell's chip phase                          */
  /* From the complex intermediates: */
  double dot;      /* Re(conj P (L - E)) / |P|^2 on the truth row's cells */
  double edot;     /* the same per epoch over the block's prompt column,
                      summed: sum Re(conj P_k (L_k - E_k)) / sum |P_k|^2  */
  double f_fit;    /* the prompt column's linear-phase Doppler, Hz       */
  double ph_rms;   /* the fit's residual, rad per epoch                  */
  int    have_blk; /* the block taps read (D > 1 and a whole block)     */
  /* From the shipped DLL coasting on the searcher\'s timing, fed the raw
     block: its symbol-aided discriminator\'s mean over the block. */
  double dll_e;  /* mean of "dll.e" over the block; NAN = not read    */
  size_t n_e;    /* discriminator outputs in the block (symbols)      */
  double dll_es; /* the per-steer mean, dll_take_error (section 12.25) */
  size_t n_es;   /* steers it counted                                  */
  double locked; /* the lock flag's mean over the block               */
  double e_sd;   /* the discriminator's scatter within the block       */
  double dll_u;  /* the loop's actual offset from the truth at the
                    block's end, chips: the seed plus what it drifted  */
  /* The tracker closed on the searcher's OWN cell (§12.23): the phase it
     held through this block, dead-reckoned from its last correction,
     against the truth at the block's middle. */
  int    h_have; /* acquired before this block                         */
  double h_err;  /* held phase at the middle minus the truth, chips    */
  /* The shipped receiver in its cell mode (section 12.26): its status at
     this dwell's start (it is fed the epoch after the engine is). */
  int    rx_have;
  double rx_err; /* its chip phase minus the truth there, chips        */
  int    rx_code, rx_sym;
} dwell_t;

struct scurve;

typedef struct
{
  acq_state_t  *a;
  const stim_t *st;
  double        c0;   /* NAN during calibration: read at the argmax cell   */
  double        f_hz; /* the truth Doppler                                 */
  double       *hz;   /* per surface row                                   */
  double       *chip; /* per column                                        */
  size_t        rows, cols, tile_rows;
  size_t        dwell_len; /* samples per decided dwell                      */
  size_t        win_sym;   /* the stimulus's code-only symbols per frame     */
  dwell_t      *d;
  size_t        n, cap;
  /* The re-correlator: a persistent DLL, coasting, rate-aided by the
     held Doppler, seeded once at the cell plus DLL_U0; NULL = off. */
  dll_state_t    *dll;
  dp_tlm_t       *tlm;
  int             id_e;
  int             id_locked;
  lo_state_t     *lo;     /* the carrier wipe at the held Doppler          */
  float _Complex *lo_buf; /* D * code_bins phasors                          */
  double          u0;     /* the seed\'s offset from the truth, chips        */
  double c_dll; /* the DLL\'s phase convention against the cell, chips */
  int    track; /* 1: the loop runs (the convention\'s calibration)   */
  double last_phase_err; /* tracking: phase at the block\'s end - truth */
  double last_rate;      /* tracking: code_rate at the block\'s end     */
  float _Complex *raw;   /* D * code_bins, the block as pushed              */
  float _Complex *prt;   /* the DLL\'s output scratch                       */
  /* The tracker on the searcher's own cell: 1 = the correction closes on
     the cell the surface listed, the truth never consulted after the
     calibration constants. Acquired at the first window dwell from the
     surface alone; the DLL and the wipe open there. */
  int                  held;
  int                  acquired;
  const uint8_t       *code;    /* to open the DLL at acquisition        */
  const struct scurve *coh_cal; /* the surface's E/L S-curve, for the seed */
  const struct scurve *dll_cal; /* the coasting DLL's, for the correction  */
  double               h;       /* held phase at the next block's start,
                                   surface frame (truth + c0), unwrapped  */
  double f_h;                   /* held Doppler, Hz, from the surface     */
  double h_rate;                /* chips per sample at the held Doppler   */
  size_t blk_acq;               /* blocks since acquisition               */
  size_t at_acq;                /* the dwell index it acquired on         */
  double h_acq_err, f_acq_err;  /* the seed's error, chips, Hz */
  double h_gain;                /* the correction's gain on the read */
  /* The shipped CellAsyncDsssReceiver (section 12.26), fed the same
     epochs the engine is pushed from the epoch after its seed, seeded
     from the surface at the first window dwell as the held mode is,
     scored on its status(); its symbols kept for the BER. */
  async_dsss_receiver_state_t *rx;
  double rx_gain;    /* NAN = no receiver                    */
  int    rx_handoff; /* the hand-off flavour on the same seed,
                        for parity (no cell gain)            */
  int             rx_seeded;
  uint64_t        rx_seed_at;  /* samples_consumed at the seed         */
  uint64_t        rx_seed_sym; /* the synth's symbol index there       */
  double          rx_seed_err; /* the seed's phase error, chips        */
  size_t          rx_at;       /* the dwell it was seeded on           */
  float _Complex *rx_out;      /* one epoch's symbols, scratch         */
  float _Complex *rx_syms;     /* every symbol it emitted              */
  size_t          rx_nsyms, rx_syms_cap;
  uint8_t        *bits; /* the synth's payload (WFM_DSSS_DATA_BITS)    */
} sink_ctx_t;

static double disc (double e, double l);
static double parabola (double a, double b, double c);
static double sinv (const struct scurve *sc, double y);
static int    dll_open (sink_ctx_t *c, double seed_chip, double f_hz);

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

static double
wrap_chips (double e)
{
  e = fmod (e, (double)SF);
  if (e > 0.5 * SF)
    e -= (double)SF;
  else if (e <= -0.5 * SF)
    e += (double)SF;
  return e;
}

/* The emitter's code phase at engine stream sample `k`, by the synth's
   own clock through the channel's documented mapping: output k carries
   the input at `k (1 + d) - delay`; DISCARD outputs came first. Chips,
   UNWRAPPED (the frame position needs the whole count). */
static double
truth_chips (const stim_t *s, double k)
{
  const double n_in = (k + (double)DISCARD) * (1.0 + s->ppm * 1e-6) - s->delay;
  return n_in / (double)SPC;
}

static int
stim_open (stim_t *s, const uint8_t *code, double ppm, double cn0_dbhz,
           uint32_t seed, size_t win_sym, const uint8_t *bits)
{
  memset (s, 0, sizeof *s);
  s->ppm = ppm;
  s->syn = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                             seed, (int)SPC, 15, 0, 0, 0.0);
  if (!s->syn
      || wfm_synth_set_dsss_cont (s->syn, code, SF, CPS, WFM_DSSS_DATA_BITS,
                                  bits, N_BITS)
             != 0
      || wfm_synth_set_dsss_window (s->syn, win_sym, F_SYM) != 0)
    return 1;
  s->ch = doppler_channel_create (FS, CARRIER_HZ, ppm, 0.0);
  if (!s->ch)
    return 1;
  s->delay = doppler_channel_get_delay_samples (s->ch);
  if (cn0_dbhz < WFM_SYNTH_SNR_CLEAN)
    {
      /* C/N0 to SNR over fs is the one conversion; the amplitude is the
         library's answer to "per rail or total". */
      s->g = awgn_create (seed * 7919u + 1u,
                          awgn_amplitude_for_snr (
                              (float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
      if (!s->g)
        return 1;
    }
  s->sig  = dp_xmalloc (TE * sizeof *s->sig);
  s->fifo = dp_xmalloc ((size_t)4 * TE * sizeof *s->fifo);
  s->blk  = dp_xmalloc (TE * sizeof *s->blk);
  s->nz   = dp_xmalloc (TE * sizeof *s->nz);
  /* The channel's first outputs are dropped so the truth's fraction of a
     cell is not the trivial one. */
  size_t dropped = 0;
  while (dropped < DISCARD)
    {
      wfm_synth_steps (s->syn, s->sig, TE);
      s->pend += doppler_channel_execute (s->ch, s->sig, TE, s->fifo + s->pend,
                                          2 * TE);
      size_t take = s->pend < DISCARD - dropped ? s->pend : DISCARD - dropped;
      memmove (s->fifo, s->fifo + take, (s->pend - take) * sizeof *s->fifo);
      s->pend -= take;
      dropped += take;
    }
  return 0;
}

/* One epoch of the received stream, noise added after the channel. */
static const float complex *
stim_block (stim_t *s)
{
  while (s->pend < TE)
    {
      wfm_synth_steps (s->syn, s->sig, TE);
      s->pend += doppler_channel_execute (s->ch, s->sig, TE, s->fifo + s->pend,
                                          2 * TE);
    }
  memcpy (s->blk, s->fifo, TE * sizeof *s->blk);
  s->pend -= TE;
  memmove (s->fifo, s->fifo + TE, s->pend * sizeof *s->fifo);
  if (s->g)
    {
      awgn_generate (s->g, TE, s->nz, TE);
      for (size_t i = 0; i < TE; i++)
        s->blk[i] += s->nz[i];
    }
  return s->blk;
}

static void
stim_close (stim_t *s)
{
  free (s->nz);
  free (s->blk);
  free (s->fifo);
  free (s->sig);
  awgn_destroy (s->g);
  doppler_channel_destroy (s->ch);
  wfm_synth_destroy (s->syn);
}

/* The surface cell nearest a chip phase (the columns are circular). */
static size_t
nearest_col (const sink_ctx_t *c, double chips)
{
  size_t best = 0;
  double bd   = 1e9;
  for (size_t k = 0; k < c->cols; k++)
    {
      double d = fabs (wrap_chips (c->chip[k] - chips));
      if (d < bd)
        {
          bd   = d;
          best = k;
        }
    }
  return best;
}

static size_t
nearest_row (const sink_ctx_t *c, double hz)
{
  size_t best = 0;
  double bd   = 1e12;
  for (size_t r = 0; r < c->rows; r++)
    if (fabs (c->hz[r] - hz) < bd)
      {
        bd   = fabs (c->hz[r] - hz);
        best = r;
      }
  return best;
}

/* The sink: every decided dwell, the cells around the truth. The pointer
   is the engine's and valid only here, so everything is copied out. */
static void
on_surface (void *ctx, const float *s, size_t rows, size_t cols,
            uint64_t samples_consumed)
{
  sink_ctx_t *c = ctx;
  if (c->n == c->cap)
    return;
  dwell_t *d = &c->d[c->n];
  memset (d, 0, sizeof *d);
  d->at              = samples_consumed;
  const double mid   = (double)samples_consumed - 0.5 * (double)c->dwell_len;
  const double t_mid = truth_chips (c->st, mid);
  d->truth           = dp_fmod_pos (t_mid, (double)SF);
  const double t_lo
      = truth_chips (c->st, (double)samples_consumed - (double)c->dwell_len);
  const double t_hi = truth_chips (c->st, (double)samples_consumed);
  d->sym_lo         = fmod (t_lo / CPS, (double)F_SYM);
  d->sym_hi         = fmod (t_hi / CPS, (double)F_SYM);
  /* The block inside the code-only window, a symbol of margin each end
     and no frame wrap inside it; a code-only stream is always inside. */
  d->in_win = c->win_sym >= F_SYM
              || (d->sym_hi >= d->sym_lo && d->sym_lo >= 1.0
                  && d->sym_hi <= (double)c->win_sym - 1.0);

  /* The surface's own maximum: the detector's view. */
  size_t pk = 0;
  for (size_t k = 1; k < rows * cols; k++)
    if (s[k] > s[pk])
      pk = k;
  const size_t prow = pk / cols, pcol = pk % cols;
  const double truth_ref = isnan (c->c0) ? d->truth : d->truth + c->c0;
  d->x_cell              = wrap_chips (c->chip[pcol] - d->truth);
  d->hit = fabs (wrap_chips (c->chip[pcol] - truth_ref)) <= 1.0;

  /* The cell a tracker holds: the truth's, or the argmax's while
     calibrating (a clean stream, where they coincide). */
  size_t col, row;
  if (isnan (c->c0))
    {
      col = pcol;
      row = prow;
    }
  else
    {
      col = nearest_col (c, truth_ref);
      row = nearest_row (c, c->f_hz);
      /* Within a row of the truth's, the strongest prompt: the row a
         tracker would be holding. */
      size_t best = row;
      for (long dr = -1; dr <= 1; dr++)
        {
          long r = (long)row + dr;
          if (r >= 0 && r < (long)rows
              && s[(size_t)r * cols + col] > s[best * cols + col])
            best = (size_t)r;
        }
      row = best;
    }
  const size_t cm = (col + cols - 1) % cols, cp = (col + 1) % cols;
  d->e         = s[row * cols + cm];
  d->p         = s[row * cols + col];
  d->l         = s[row * cols + cp];
  d->row_hz    = c->hz[row];
  d->cell_chip = c->chip[col];
  /* The tile's rows summed as power: Parseval's per-epoch sum. On the
     coherent path the surface is a magnitude and is squared; the
     non-coherent path already accumulates power. */
  const size_t tile = row / c->tile_rows, r0 = tile * c->tile_rows;
  const int    pw = c->a->n_noncoh > 1;
  for (size_t r = r0; r < r0 + c->tile_rows && r < rows; r++)
    {
      double ve = s[r * cols + cm], vp = s[r * cols + col],
             vl = s[r * cols + cp];
      d->ep += pw ? ve : ve * ve;
      d->pp += pw ? vp : vp * vp;
      d->lp += pw ? vl : vl * vl;
    }
  /* The rows above and below at the column: the Doppler axis. */
  d->pc = d->p;
  d->u  = row + 1 < rows ? s[(row + 1) * cols + col] : 0.0;
  d->b  = row > 0 ? s[(row - 1) * cols + col] : 0.0;

  /* The complex cells underneath, read in place: acq_surface_complex()
     copies the whole surface for a caller who wants it; five cells of it
     are wanted here, and the engine's buffer is the one the sink is being
     handed the magnitude of (the same dwell, decided on it). */
  {
    const float _Complex *z = c->a->out_buf;
    const float _Complex E = z[row * cols + cm], P = z[row * cols + col],
                         L = z[row * cols + cp];
    const double pp        = creal (P) * creal (P) + cimag (P) * cimag (P);
    d->dot                 = pp > 0.0 ? creal (conj (P) * (L - E)) / pp : 0.0;
  }
  /* The block's prompt column at the three cells: the per-epoch coherent
     discriminator summed over the block, and the carrier along P_k. */
  const size_t D = c->a->coherent_bins;
  if (D > 1 && D <= 1024)
    {
      float _Complex pe[1024], pl[1024], pk[1024];
      if (acq_block_prompt (c->a, tile, cm, pe, D) == D
          && acq_block_prompt (c->a, tile, col, pk, D) == D
          && acq_block_prompt (c->a, tile, cp, pl, D) == D)
        {
          d->have_blk = 1;
          double num = 0.0, den = 0.0;
          for (size_t k = 0; k < D; k++)
            {
              num += creal (conj (pk[k]) * (pl[k] - pe[k]));
              den += creal (pk[k]) * creal (pk[k])
                     + cimag (pk[k]) * cimag (pk[k]);
            }
          d->edot = den > 0.0 ? num / den : 0.0;
          /* Linear phase along the column, de-rotated first by the
             Doppler a tracker holds (here the truth's offset from the
             tile's centre, as the cell is the truth's): what is left is
             the residual, small and unambiguous. Unsquared in the window
             (the prompt is code only), on P^2 under data (BPSK removed,
             the slope halved); squaring an un-rotated column at 1 kHz
             off the centre aliases past the epoch rate's half. Unwrapped
             epoch to epoch; least squares. */
          const int    sq    = !d->in_win;
          const double f_ep0 = FS / (double)TE;
          const double w_off
              = 2.0 * M_PI * (c->f_hz - c->hz[tile * c->tile_rows]) / f_ep0;
          double th[1024], prev = 0.0, acc = 0.0;
          for (size_t k = 0; k < D; k++)
            {
              const double ang = -w_off * (double)k;
              float _Complex v
                  = pk[k] * (float _Complex) (cos (ang) + I * sin (ang));
              if (sq)
                v *= v;
              double a = atan2 (cimag (v), creal (v));
              if (k)
                {
                  double dlt = a - prev;
                  while (dlt > M_PI)
                    dlt -= 2.0 * M_PI;
                  while (dlt < -M_PI)
                    dlt += 2.0 * M_PI;
                  acc += dlt;
                }
              prev  = a;
              th[k] = acc;
            }
          double sx = 0, sxx = 0, sy = 0, sxy = 0;
          for (size_t k = 0; k < D; k++)
            {
              sx += (double)k;
              sxx += (double)k * (double)k;
              sy += th[k];
              sxy += (double)k * th[k];
            }
          const double nD    = (double)D;
          const double slope = (sxy - sx * sy / nD) / (sxx - sx * sx / nD);
          const double icpt  = (sy - slope * sx) / nD;
          double       r2    = 0.0;
          for (size_t k = 0; k < D; k++)
            {
              double e = th[k] - (icpt + slope * (double)k);
              r2 += e * e;
            }
          const double f_res = slope / (2.0 * M_PI) * f_ep0 / (sq ? 2.0 : 1.0);
          d->f_fit  = c->f_hz + f_res; /* the held Doppler plus the residual */
          d->ph_rms = sqrt (r2 / nD) / (sq ? 2.0 : 1.0);
        }
    }
  /* The tracker on the searcher's own cell acquires at the first window
     dwell, from the surface alone: the argmax cell, the calibrated E/L on
     its row for the phase within the cell, the parabola over the rows
     for the Doppler. The code dilates with the carrier, so the held
     Doppler is also the held code rate; the DLL and the wipe open here.
     Nothing of the truth is consulted from this point. */
  if ((c->held || c->rx) && !c->acquired && d->in_win && D > 1
      && !isnan (c->c0))
    {
      const double row_sp = c->rows > 1 ? fabs (c->hz[1] - c->hz[0]) : 0.0;
      const double up = prow + 1 < rows ? s[(prow + 1) * cols + pcol] : 0.0;
      const double dn = prow > 0 ? s[(prow - 1) * cols + pcol] : 0.0;
      const size_t am = (pcol + cols - 1) % cols, ap = (pcol + 1) % cols;
      const double e0 = s[prow * cols + am], l0 = s[prow * cols + ap];
      c->f_h             = c->hz[prow] + parabola (dn, s[pk], up) * row_sp;
      c->h_rate          = (1.0 + c->f_h / CARRIER_HZ) / (double)SPC;
      const double h_mid = c->chip[pcol] - sinv (c->coh_cal, disc (e0, l0));
      c->h               = h_mid - c->h_rate * 0.5 * (double)c->dwell_len;
      c->h_acq_err       = wrap_chips (h_mid - (t_mid + c->c0));
      c->f_acq_err       = c->f_h - c->f_hz;
      c->at_acq          = c->n;
      c->blk_acq         = 0;
      if (c->rx)
        {
          /* The seed the pool would hand it: the phase at the next sample
             (this dwell's end) in the Dll's convention, the row's
             Doppler. Fed from the next epoch on. */
          const double ph_end
              = h_mid + c->h_rate * 0.5 * (double)c->dwell_len + c->c_dll;
          DP_CHECK_MSG (async_dsss_receiver_seed (
                            c->rx, dp_fmod_pos (ph_end, (double)SF), c->f_h,
                            c->a->cn0_dbhz)
                            == DP_OK,
                        "the cell receiver takes the surface's seed");
          const double chips = truth_chips (c->st, (double)samples_consumed);
          c->rx_seeded       = 1;
          c->rx_seed_at      = samples_consumed;
          c->rx_seed_sym     = (uint64_t)(chips / CPS);
          c->rx_at           = c->n;
          c->rx_seed_err = wrap_chips (ph_end - c->c_dll - (chips + c->c0));
        }
      if (c->held
          && dll_open (c, dp_fmod_pos (c->h + c->c_dll + c->u0, (double)SF),
                       c->f_h))
        return;
      c->acquired = 1;
    }
  /* The shipped receiver's read: fed through the previous epoch, its phase
     is at this dwell's end less one epoch. */
  if (c->rx && c->rx_seeded && samples_consumed > c->rx_seed_at)
    {
      async_dsss_receiver_status_t st = async_dsss_receiver_status (c->rx);
      d->rx_have                      = 1;
      d->rx_err                       = wrap_chips (
          st.chip_phase
          - (truth_chips (c->st, (double)samples_consumed - (double)TE) + c->c0
             + c->c_dll));
      d->rx_code = st.code_locked;
      d->rx_sym  = st.locked;
    }

  /* The re-correlation: the block as pushed through the coasting DLL,
     epoch by epoch, and the mean of the discriminator outputs it emitted
     -- the symbol-aided window on the searcher's timing. */
  d->dll_e = NAN;
  if (c->dll && D > 1
      && acq_block_raw (c->a, c->raw, D * c->cols) == D * c->cols)
    {
      /* The correction a tracker applies once per block: the loop put at
         the cell's phase -- the truth's, as every other read, or the
         held tracker's own -- plus the seed offset, at the block's start,
         before it is fed. What the loop reads over the block is then u0
         plus the drift within it, plus the held phase's error. */
      if (c->held)
        {
          d->h_have = 1;
          d->h_err  = wrap_chips (c->h + c->h_rate * 0.5 * (double)c->dwell_len
                                  - (t_mid + c->c0));
          dll_set_code_phase (c->dll, c->h + c->c_dll + c->u0);
        }
      else if (!c->track)
        dll_set_code_phase (c->dll,
                            truth_chips (c->st, (double)samples_consumed
                                                    - (double)c->dwell_len)
                                + c->c0 + c->c_dll + c->u0);
      const size_t cap = dll_steps_max_out (c->dll), nb = D * c->cols;
      (void)lo_steps (c->lo, nb, c->lo_buf, nb);
      for (size_t i = 0; i < nb; i++)
        c->raw[i] *= c->lo_buf[i];
      for (size_t k = 0; k < D; k++)
        (void)dll_steps (c->dll, c->raw + k * c->cols, c->cols, c->prt, cap);
      dp_tlm_rec_t rec[256];
      double       se = 0.0, se2 = 0.0;
      size_t       ne = 0, got;
      size_t       nl = 0, nlk = 0;
      while ((got = dp_tlm_read (c->tlm, 256, rec, 256)) > 0)
        for (size_t i = 0; i < got; i++)
          {
            if (rec[i].probe == (uint16_t)c->id_e)
              {
                se += rec[i].value;
                se2 += (double)rec[i].value * rec[i].value;
                ne++;
              }
            else if (rec[i].probe == (uint16_t)c->id_locked)
              {
                nl++;
                nlk += rec[i].value > 0.5f;
              }
          }
      /* The shipped accumulator beside the probe: every steer summed
         (dll_take_error), where the probe is each epoch's last steer. */
      {
        double ssum;
        d->n_es   = dll_take_error (c->dll, &ssum);
        d->dll_es = d->n_es ? ssum / (double)d->n_es : NAN;
      }
      d->n_e    = ne;
      d->e_sd   = ne ? sqrt (fmax (se2 / (double)ne
                                       - (se / (double)ne) * (se / (double)ne),
                                   0.0))
                     : 0.0;
      d->locked = nl ? (double)nlk / (double)nl : NAN;
      if (ne && c->n >= DLL_SETTLE)
        d->dll_e = se / (double)ne;
      /* The held tracker: dead-reckon the phase across the block on the
         held rate, then correct it by what the loop read -- the whole
         read, put back once a block -- once the symbol aid has settled.
         Before that the dead reckoning stands on its own. */
      if (c->held)
        {
          c->h += c->h_rate * (double)c->dwell_len;
          if (ne && c->blk_acq >= DLL_SETTLE)
            c->h -= c->h_gain * (sinv (c->dll_cal, se / (double)ne) - c->u0);
          c->blk_acq++;
        }
      /* Tracking (the convention's calibration): the loop's phase at the
         block's end against the truth there, in the searcher's terms. */
      c->last_phase_err = wrap_chips (
          dll_get_code_phase (c->dll)
          - (truth_chips (c->st, (double)samples_consumed) + c->c0 + c->u0));
      c->last_rate = dll_get_code_rate (c->dll);
      d->dll_u     = wrap_chips (c->last_phase_err + c->u0 - c->c_dll);
    }
  c->n++;
}

/* Normalised early-late, in units the calibration scales to chips. */
static double
disc (double e, double l)
{
  return (e + l) > 0.0 ? (l - e) / (l + e) : 0.0;
}

/* Three-point parabola: the apex's offset from the centre sample, in
   samples of the axis; 0 when the centre is not a maximum. */
static double
parabola (double a, double b, double c)
{
  const double den = a - 2.0 * b + c;
  return den < 0.0 ? 0.5 * (a - c) / den : 0.0;
}

/* The coasting DLL and its wipe, at a seed phase and a held Doppler: the
   loop held from the start (dll_hold_here at zero, then coast), the
   dilation as a rate aid from the Doppler, the symbol window on, its
   discriminator on telemetry. Opened at the truth's cell before a run,
   or at the searcher's own cell when the held tracker acquires. 0 on
   success (DP_REQUIRE's non-zero otherwise). */
static int
dll_open (sink_ctx_t *c, double seed_chip, double f_hz)
{
  const acq_state_t *a = c->a;
  c->dll = dll_create (c->code, SF, SPC, seed_chip, BN, 0.707, 0.5, SEGMENTS);
  DP_REQUIRE_MSG (c->dll != NULL, "the DLL opens");
  DP_REQUIRE (dll_set_symbol_period (c->dll, P_SYM) == DP_OK);
  dll_set_rate_aid (c->dll, f_hz / CARRIER_HZ);
  if (!c->track)
    {
      dll_hold_here (c->dll);
      dll_set_coast (c->dll, 1);
    }
  c->tlm = dp_tlm_create (1u << 14);
  DP_REQUIRE (c->tlm != NULL);
  DP_REQUIRE (dll_set_telemetry (c->dll, c->tlm, "dll", 1) == DP_OK);
  c->id_e      = dp_tlm_probe_id (c->tlm, "dll.e");
  c->id_locked = dp_tlm_probe_id (c->tlm, "dll.locked");
  DP_REQUIRE (c->id_e >= 0);
  c->raw = dp_xmalloc (a->coherent_bins * a->code_bins * sizeof *c->raw);
  /* The carrier wipe a receiver's Costas does before its DLL, at the
     Doppler a tracker holds (the rows read it under a hertz, §12.20):
     the DLL's partials, 256 chips, integrate 2.3 cycles of a 45 kHz
     carrier un-wiped, a 17 dB loss a clean stream survives and a
     noisy one does not. The shipped LO, phase-continuous across
     blocks. */
  c->lo = lo_create (-f_hz / FS);
  DP_REQUIRE_MSG (c->lo != NULL, "the wipe's LO opens");
  c->lo_buf = dp_xmalloc (a->coherent_bins * a->code_bins * sizeof *c->lo_buf);
  c->prt    = dp_xmalloc (dll_steps_max_out (c->dll) * sizeof *c->prt);
  return 0;
}

/* Run one stimulus through one engine for `n` decided dwells. */
static int
run (acq_state_t *a, const uint8_t *code, double ppm, double cn0,
     uint32_t seed, double c0, size_t win_sym, double u0, sink_ctx_t *c,
     size_t n)
{
  stim_t st;
  if (stim_open (&st, code, ppm, cn0, seed, win_sym, c->bits))
    {
      fprintf (stderr, "the stimulus does not open at %.0f ppm, %.0f dB-Hz\n",
               ppm, cn0);
      return 1;
    }
  c->win_sym   = win_sym;
  c->a         = a;
  c->st        = &st;
  c->c0        = c0;
  c->f_hz      = ppm * 1e-6 * CARRIER_HZ;
  c->n         = 0;
  c->cap       = n;
  c->dwell_len = a->n_noncoh * a->coherent_bins * TE;
  /* The coasting DLL: created at the truth plus u0 as the searcher's cell
     would seed it, the loop held from the start (dll_hold_here at zero,
     then coast), the dilation as a rate aid from the held Doppler, the
     symbol window on, its discriminator on telemetry. */
  c->dll      = NULL;
  c->u0       = u0;
  c->code     = code;
  c->acquired = 0;
  if (!isnan (u0) && a->coherent_bins > 1 && !c->held
      && dll_open (c,
                   dp_fmod_pos (truth_chips (&st, 0.0) + c0 + c->c_dll + u0,
                                (double)SF),
                   c->f_hz))
    return 1;
  /* The shipped receiver: the cell mode at this run's C/N0 and the
     engine's block depth as its interval, never released for time. */
  c->rx        = NULL;
  c->rx_seeded = 0;
  c->rx_nsyms  = 0;
  if (!isnan (c->rx_gain) && a->coherent_bins > 1)
    {
      /* The hand-off flavour's refine is the release harness's
         (tracker_through_window.c): the parity reference on the same
         seed and stream. */
      c->rx = c->rx_handoff
                  ? async_dsss_receiver_create_handoff (
                        code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0, PFA, PD,
                        SEGMENTS, RX_SPS, 0, 0.5, 4, 14.0, 64, 8, false,
                        100000, CARRIER_HZ, 0.0)
                  : async_dsss_receiver_create_cell (
                        code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0, PFA, PD,
                        SEGMENTS, RX_SPS, 0, CARRIER_HZ, 0.0, a->coherent_bins,
                        c->rx_gain, ASYNC_DSSS_RX_CELL_PULLIN);
      DP_REQUIRE_MSG (c->rx != NULL, "the receiver opens");
      c->rx_out = dp_xmalloc (TE * sizeof *c->rx_out);
      c->rx_syms_cap
          = (size_t)((double)n * (double)c->dwell_len * SYM_RATE / FS) + 4096;
      c->rx_syms = dp_xmalloc (c->rx_syms_cap * sizeof *c->rx_syms);
    }
  acq_reset (a);
  acq_set_surface_sink (a, on_surface, c, 1u);
  acq_result_t hits[16];
  while (c->n < n)
    {
      const float complex *blk = stim_block (&st);
      /* Seeded at a dwell's end inside this push: fed from the next. */
      const int fed = c->rx && c->rx_seeded;
      (void)acq_push (a, blk, TE, hits, 16);
      if (fed)
        {
          size_t k = async_dsss_receiver_steps (c->rx, blk, TE, c->rx_out, TE);
          if (c->rx_nsyms + k <= c->rx_syms_cap)
            memcpy (c->rx_syms + c->rx_nsyms, c->rx_out,
                    k * sizeof *c->rx_out);
          c->rx_nsyms += k;
        }
    }
  acq_set_surface_sink (a, NULL, NULL, 1u);
  stim_close (&st);
  if (c->rx)
    {
      async_dsss_receiver_destroy (c->rx);
      c->rx = NULL;
      free (c->rx_out);
      c->rx_out = NULL;
    }
  if (c->dll)
    {
      free (c->prt);
      free (c->raw);
      free (c->lo_buf);
      lo_destroy (c->lo);
      c->lo = NULL;
      dll_destroy (c->dll);
      dp_tlm_destroy (c->tlm);
      c->dll = NULL;
      c->tlm = NULL;
    }
  c->st = NULL;
  return 0;
}

#define NB 10 /* S-curve bins across the cell, 0.05 chip each     */

/* A discriminator's measured characteristic S(u) over the sub-cell
   offset u = cell - truth, in NB bins of the clean sweep, and its
   inverse: a normalised early-late is not linear once the chip pulse is
   the channel's resampler's rather than a triangle, and the tile-summed
   power's is less linear still. */
typedef struct scurve
{
  double s[NB]; /* mean discriminator per bin                     */
  double u[NB]; /* the bin's mean offset                           */
  size_t n[NB];
  double gain; /* one slope through the origin, for the record   */
} scurve_t;

typedef struct
{
  scurve_t coh, par;  /* the truth row's cells; the tile's rows summed */
  scurve_t dot, edot; /* the coherent reads: surface row; per epoch   */
  scurve_t dll;       /* the coasting DLL, across seed offsets        */
  double   dll_drift; /* the coasting loop's drift, chips per second    */
  double   c_dll;     /* the DLL\'s phase convention, chips              */
  double   c0;        /* the truth's constant, chips                    */
  double   resid;     /* RMS of the coherent inverse on the sweep, chips*/
  double   drift;     /* x_cell's slope per dwell, chips: a sign or
                         convention mismatch shows here                 */
  size_t n_win;
} cal_t;

static int
sbin (double u)
{
  int b = (int)floor ((u + 0.25) / 0.5 * (double)NB);
  return b < 0 ? 0 : b >= NB ? NB - 1 : b;
}

/* u from a discriminator reading: the bracketing bins interpolated,
   the ends extrapolated on their own slope, clamped to the neighbour
   cell. */
static double
sinv (const scurve_t *sc, double y)
{
  int lo = -1, hi = -1;
  for (int b = 0; b < NB; b++)
    if (sc->n[b])
      {
        if (lo < 0)
          lo = b;
        hi = b;
      }
  if (lo < 0 || hi == lo)
    return 0.0;
  const int dec = sc->s[hi] < sc->s[lo];
  int       a = lo, c = -1;
  for (int b = lo + 1; b <= hi; b++)
    {
      if (!sc->n[b])
        continue;
      double ya = sc->s[a], yb = sc->s[b];
      if ((dec && y <= ya && y >= yb) || (!dec && y >= ya && y <= yb))
        {
          c = b;
          break;
        }
      a = b;
    }
  if (c < 0)
    {
      /* Outside the measured range: the nearer end's slope. */
      int p, q;
      if ((dec && y > sc->s[lo]) || (!dec && y < sc->s[lo]))
        {
          p = lo;
          q = lo + 1;
          while (!sc->n[q])
            q++;
        }
      else
        {
          q = hi;
          p = hi - 1;
          while (!sc->n[p])
            p--;
        }
      a = p;
      c = q;
    }
  const double ya = sc->s[a], yc = sc->s[c];
  double u = yc != ya ? sc->u[a] + (y - ya) * (sc->u[c] - sc->u[a]) / (yc - ya)
                      : 0.5 * (sc->u[a] + sc->u[c]);
  return u > 0.5 ? 0.5 : u < -0.5 ? -0.5 : u;
}

static int
in_window (const dwell_t *d)
{
  return d->in_win;
}

/* Calibrate on a clean, dilated, code-only stream (every dwell coherent):
   the truth sweeps every sub-cell offset, so the argmax cell's phase minus
   the raw truth is c0 plus a zero-mean offset u; the discriminator
   against -u gives the gain. */
static int
calibrate (acq_state_t *a, const uint8_t *code, sink_ctx_t *c, cal_t *out,
           size_t n)
{
  if (run (a, code, 18.0, WFM_SYNTH_SNR_CLEAN, 5u, NAN, F_SYM, NAN, c, n))
    return 1;
  memset (out, 0, sizeof *out);
  double sx = 0.0, sxx = 0.0, sxy = 0.0, sy = 0.0;
  size_t m = 0;
  for (size_t k = 0; k < c->n; k++)
    if (in_window (&c->d[k]))
      {
        double t = (double)k;
        sx += t;
        sxx += t * t;
        sy += c->d[k].x_cell;
        sxy += t * c->d[k].x_cell;
        m++;
      }
  if (m < 20)
    {
      fprintf (stderr, "calibration saw %zu coherent dwells of %zu\n", m,
               c->n);
      return 1;
    }
  out->n_win = m;
  out->drift = (sxy - sx * sy / (double)m) / (sxx - sx * sx / (double)m);
  out->c0    = sy / (double)m;
  /* The S-curves, binned over u = cell - truth; and one slope through the
     origin each (disc = -g u) for the record. */
  double nc = 0.0, dc = 0.0, np = 0.0, dpw = 0.0;
  for (size_t k = 0; k < c->n; k++)
    if (in_window (&c->d[k]))
      {
        const dwell_t *d  = &c->d[k];
        double         u  = wrap_chips (d->x_cell - out->c0);
        double         yc = disc (d->e, d->l), yp = disc (d->ep, d->lp);
        int            b = sbin (u);
        out->coh.s[b] += yc;
        out->coh.u[b] += u;
        out->coh.n[b]++;
        out->par.s[b] += yp;
        out->par.u[b] += u;
        out->par.n[b]++;
        out->dot.s[b] += d->dot;
        out->dot.u[b] += u;
        out->dot.n[b]++;
        if (d->have_blk)
          {
            out->edot.s[b] += d->edot;
            out->edot.u[b] += u;
            out->edot.n[b]++;
          }
        nc += -u * yc;
        dc += yc * yc;
        np += -u * yp;
        dpw += yp * yp;
      }
  for (int b = 0; b < NB; b++)
    {
      if (out->coh.n[b])
        {
          out->coh.s[b] /= (double)out->coh.n[b];
          out->coh.u[b] /= (double)out->coh.n[b];
        }
      if (out->par.n[b])
        {
          out->par.s[b] /= (double)out->par.n[b];
          out->par.u[b] /= (double)out->par.n[b];
        }
      if (out->dot.n[b])
        {
          out->dot.s[b] /= (double)out->dot.n[b];
          out->dot.u[b] /= (double)out->dot.n[b];
        }
      if (out->edot.n[b])
        {
          out->edot.s[b] /= (double)out->edot.n[b];
          out->edot.u[b] /= (double)out->edot.n[b];
        }
    }
  out->coh.gain = dc > 0.0 ? nc / dc : 0.0;
  out->par.gain = dpw > 0.0 ? np / dpw : 0.0;
  double r2     = 0.0;
  for (size_t k = 0; k < c->n; k++)
    if (in_window (&c->d[k]))
      {
        const dwell_t *d = &c->d[k];
        double         u = wrap_chips (d->x_cell - out->c0);
        double e = u - sinv (&out->coh, disc (d->e, d->l)); /* est - truth */
        r2 += e * e;
      }
  out->resid = sqrt (r2 / (double)m);
  return 0;
}

/* The DLL's phase convention against the searcher's cell: one clean,
   code-only, dilated run with the loop RUNNING from the cell's phase at
   the receiver's bn, its converged phase at the last block's end against
   the truth. A coasting loop cannot pull this constant in; the hand-off's
   refine does, from up to half a chip (§12.11). */
static int
calibrate_dll_phase (acq_state_t *a, const uint8_t *code, sink_ctx_t *c,
                     cal_t *cal, size_t n, double *rate)
{
  c->c_dll = 0.0;
  c->track = 1;
  int rc = run (a, code, 18.0, WFM_SYNTH_SNR_CLEAN, 3u, cal->c0, F_SYM, 0.0, c,
                n);
  c->track = 0;
  if (rc)
    return 1;
  cal->c_dll = c->last_phase_err;
  c->c_dll   = cal->c_dll;
  *rate      = c->last_rate;
  return 0;
}

/* The DLL's S-curve over its ACTUAL offset: seeded on a grid across the
   cell, one clean, code-only, dilated run per seed, and binned over the
   offset the loop's own phase reads against the truth at each block's
   end -- a coasting loop drifts on its NCO's quantisation of the rate aid
   (measured below, chips per second), so the seed is where it started
   and not where each block read. Blocks inside the cell only. */
static int
calibrate_dll (acq_state_t *a, const uint8_t *code, sink_ctx_t *c, cal_t *cal,
               size_t n_per)
{
  memset (&cal->dll, 0, sizeof cal->dll);
  cal->dll_drift = 0.0;
  for (int b = 0; b < NB; b++)
    {
      const double u0 = -0.25 + (0.5 / NB) * ((double)b + 0.5);
      /* On the operating stream, data and window, not code only: the
         symbol-aided window's zero sits where the data puts it. */
      if (run (a, code, 18.0, WFM_SYNTH_SNR_CLEAN, 7u + (uint32_t)b, cal->c0,
               W_SYM, u0, c, n_per))
        return 1;
      size_t ne      = 0;
      double u_first = NAN, u_last = NAN;
      for (size_t k = 0; k < c->n; k++)
        if (!isnan (c->d[k].dll_e))
          {
            if (isnan (u_first))
              u_first = c->d[k].dll_u;
            u_last = c->d[k].dll_u;
            {
              /* Corrected at every block's start, the loop's offset over
                 the block is u0 plus the drift within it; binned at u0. */
              cal->dll.s[b] += c->d[k].dll_e;
              cal->dll.u[b] += u0;
              cal->dll.n[b]++;
              ne++;
            }
          }
      if (!ne)
        {
          fprintf (stderr,
                   "the DLL read nothing inside the cell from u0 %+.3f\n", u0);
          return 1;
        }
      /* The drift within one block on the held rate, chips/s: the offset
         the loop reads at the block's end against the u0 it was put at. */
      if (b == 0)
        cal->dll_drift
            = (u_last - u0) / ((double)TE * (double)a->coherent_bins / FS);
      (void)u_first;
    }
  for (int b = 0; b < NB; b++)
    if (cal->dll.n[b])
      {
        cal->dll.s[b] /= (double)cal->dll.n[b];
        cal->dll.u[b] /= (double)cal->dll.n[b];
      }
  return 0;
}

typedef struct
{
  size_t n, hits;
  double coh_bias, coh_sig;    /* calibrated E/L on the truth row     */
  double par_bias, par_sig;    /* Parseval over the tile's rows       */
  double pb_bias, pb_sig;      /* parabola on the truth row           */
  double f_bias, f_sig;        /* parabola over the rows, Hz          */
  double dot_bias, dot_sig;    /* coherent E/L on the complex surface */
  double ed_bias, ed_sig;      /* per-epoch coherent E/L over the block */
  double ff_bias, ff_sig;      /* the prompt column's phase-fit Doppler */
  double ph_rms;               /* the fit's residual, rad per epoch     */
  size_t n_blk;                /* dwells with the block taps read       */
  double dll_bias, dll_sig;    /* the coasting DLL, its read minus u0   */
  size_t n_dll;                /* dwells with a DLL read                */
  double e_raw, e_raw2;        /* the raw discriminator's sums          */
  double es_raw, es_raw2;      /* the per-steer mean\'s sums (12.25)     */
  double n_es_sum;             /* steers per block                      */
  double n_e_sum, lock_sum;    /* outputs per block; lock flag mean     */
  double e_sd_sum;             /* within-block scatter of e, summed     */
  double snr;                  /* mean prompt over the gate's units   */
  size_t n_h, n_held;          /* held-tracker dwells; those within half a
                                  chip of the truth                     */
  double h_bias, h_sig, h_max; /* the held phase against the truth  */
  size_t n_rx, n_rx_held;      /* the shipped receiver's dwells       */
  double rx_bias, rx_sig, rx_max, rx_code, rx_sym;
} stat_t;

static void
stats (const sink_ctx_t *c, const cal_t *cal, int want_window, stat_t *o)
{
  memset (o, 0, sizeof *o);
  double sc = 0, sc2 = 0, sp = 0, sp2 = 0, sb = 0, sb2 = 0, sf = 0, sf2 = 0,
         ss = 0, sd = 0, sd2 = 0, se = 0, se2 = 0, sq = 0, sq2 = 0, sr2 = 0;
  double sl = 0, sl2 = 0, sh = 0, sh2 = 0, srx = 0, srx2 = 0;
  const double row_sp = c->rows > 1 ? fabs (c->hz[1] - c->hz[0]) : 0.0;
  for (size_t k = 0; k < c->n; k++)
    {
      const dwell_t *d = &c->d[k];
      if (in_window (d) != want_window)
        continue;
      const double truth = d->truth + cal->c0;
      const double base = wrap_chips (d->cell_chip - truth); /* cell - truth */
      /* truth_est = cell - u_est; error = base - u_est. */
      const double ec = base - sinv (&cal->coh, disc (d->e, d->l));
      const double ep = base - sinv (&cal->par, disc (d->ep, d->lp));
      const double eb = base + parabola (d->e, d->p, d->l) / (double)SPC;
      const double ef
          = d->row_hz + parabola (d->b, d->pc, d->u) * row_sp - c->f_hz;
      sc += ec;
      sc2 += ec * ec;
      sp += ep;
      sp2 += ep * ep;
      sb += eb;
      sb2 += eb * eb;
      sf += ef;
      sf2 += ef * ef;
      ss += d->p;
      const double ed = base - sinv (&cal->dot, d->dot);
      sd += ed;
      sd2 += ed * ed;
      if (d->have_blk)
        {
          const double ee = base - sinv (&cal->edot, d->edot);
          const double eq = d->f_fit - c->f_hz;
          se += ee;
          se2 += ee * ee;
          sq += eq;
          sq2 += eq * eq;
          sr2 += d->ph_rms * d->ph_rms;
          o->n_blk++;
        }
      if (!isnan (d->dll_e))
        {
          /* Put at u0 every block, the loop's read is u_est. */
          const double el = sinv (&cal->dll, d->dll_e) - c->u0;
          sl += el;
          sl2 += el * el;
          o->e_raw += d->dll_e;
          o->e_raw2 += d->dll_e * d->dll_e;
          if (!isnan (d->dll_es))
            {
              o->es_raw += d->dll_es;
              o->es_raw2 += d->dll_es * d->dll_es;
              o->n_es_sum += (double)d->n_es;
            }
          o->n_e_sum += (double)d->n_e;
          o->e_sd_sum += d->e_sd;
          o->lock_sum += isnan (d->locked) ? 0.0 : d->locked;
          o->n_dll++;
        }
      if (d->h_have)
        {
          sh += d->h_err;
          sh2 += d->h_err * d->h_err;
          if (fabs (d->h_err) > o->h_max)
            o->h_max = fabs (d->h_err);
          o->n_held += fabs (d->h_err) <= 0.5;
          o->n_h++;
        }
      if (d->rx_have)
        {
          srx += d->rx_err;
          srx2 += d->rx_err * d->rx_err;
          if (fabs (d->rx_err) > o->rx_max)
            o->rx_max = fabs (d->rx_err);
          o->n_rx_held += fabs (d->rx_err) <= 0.5;
          o->rx_code += d->rx_code;
          o->rx_sym += d->rx_sym;
          o->n_rx++;
        }
      o->hits += (size_t)d->hit;
      o->n++;
    }
  if (!o->n)
    return;
  const double n = (double)o->n;
  o->coh_bias    = sc / n;
  o->coh_sig     = sqrt (fmax (sc2 / n - o->coh_bias * o->coh_bias, 0.0));
  o->par_bias    = sp / n;
  o->par_sig     = sqrt (fmax (sp2 / n - o->par_bias * o->par_bias, 0.0));
  o->pb_bias     = sb / n;
  o->pb_sig      = sqrt (fmax (sb2 / n - o->pb_bias * o->pb_bias, 0.0));
  o->f_bias      = sf / n;
  o->f_sig       = sqrt (fmax (sf2 / n - o->f_bias * o->f_bias, 0.0));
  o->dot_bias    = sd / n;
  o->dot_sig     = sqrt (fmax (sd2 / n - o->dot_bias * o->dot_bias, 0.0));
  if (o->n_blk)
    {
      const double m = (double)o->n_blk;
      o->ed_bias     = se / m;
      o->ed_sig      = sqrt (fmax (se2 / m - o->ed_bias * o->ed_bias, 0.0));
      o->ff_bias     = sq / m;
      o->ff_sig      = sqrt (fmax (sq2 / m - o->ff_bias * o->ff_bias, 0.0));
      o->ph_rms      = sqrt (sr2 / m);
    }
  if (o->n_dll)
    {
      const double m = (double)o->n_dll;
      o->dll_bias    = sl / m;
      o->dll_sig     = sqrt (fmax (sl2 / m - o->dll_bias * o->dll_bias, 0.0));
    }
  if (o->n_h)
    {
      const double m = (double)o->n_h;
      o->h_bias      = sh / m;
      o->h_sig       = sqrt (fmax (sh2 / m - o->h_bias * o->h_bias, 0.0));
    }
  if (o->n_rx)
    {
      const double m = (double)o->n_rx;
      o->rx_bias     = srx / m;
      o->rx_sig      = sqrt (fmax (srx2 / m - o->rx_bias * o->rx_bias, 0.0));
      o->rx_code /= m;
      o->rx_sym /= m;
    }
  o->snr = ss / n;
}

static acq_state_t *
make_engine (const uint8_t *code, double cn0, sink_ctx_t *c)
{
  acq_state_t *a
      = acq_create_continuous (code, SF, SPC, CHIP_RATE, SYM_RATE, cn0, DU,
                               PFA, PD, 0, CODE_ONLY_EPOCHS, DOPPLER_RATE);
  if (!a)
    return NULL;
  if (acq_set_carrier_freq_hz (a, CARRIER_HZ) != DP_OK)
    {
      acq_destroy (a);
      return NULL;
    }
  (void)acq_set_threads (a, THREADS);
  /* The axes, once: a decided dwell has surface_rows x code_bins cells. */
  c->cols = a->code_bins;
  c->rows = a->window_bins * a->coherent_bins * a->interp;
  c->hz   = dp_xmalloc (c->rows * sizeof *c->hz);
  c->chip = dp_xmalloc (c->cols * sizeof *c->chip);
  /* The axes read back their full length, or the engine is not the one
     this harness understands. */
  if (acq_surface_doppler_hz (a, c->hz, c->rows) != c->rows
      || acq_surface_chip_phase (a, c->chip, c->cols) != c->cols)
    {
      fprintf (stderr, "the surface's axes do not read %zu x %zu\n", c->rows,
               c->cols);
      acq_destroy (a);
      return NULL;
    }
  c->tile_rows = c->rows / a->window_bins;
  return a;
}

static void
print_engine (const acq_state_t *a, const sink_ctx_t *c)
{
  printf ("  engine: %zu tiles x D = %zu, interp %zu (%zu rows of %.1f Hz), "
          "n_noncoh %zu, dwell %.1f ms; sized at %.0f dB-Hz\n",
          a->window_bins, a->coherent_bins, a->interp, c->rows,
          c->rows > 1 ? fabs (c->hz[1] - c->hz[0]) : 0.0, a->n_noncoh,
          (double)(a->n_noncoh * a->coherent_bins * TE) / FS * 1e3,
          a->cn0_dbhz);
}

static void
print_row (const char *label, const stat_t *s)
{
  if (!s->n)
    {
      printf ("    %-7s  (no dwells)\n", label);
      return;
    }
  printf ("    %-7s %4zu  %5.1f%%   %+.4f %.4f   %+.4f %.4f   %+.4f %.4f   "
          "%+7.1f %6.1f   %7.1f\n",
          label, s->n, 100.0 * (double)s->hits / (double)s->n, s->coh_bias,
          s->coh_sig, s->par_bias, s->par_sig, s->pb_bias, s->pb_sig,
          s->f_bias, s->f_sig, s->snr);
  printf ("    %-7s coherent: dot %+.4f %.4f   epoch-dot %+.4f %.4f   "
          "Doppler fit %+7.1f %6.1f   phase %.3f rad/epoch   (%zu with the "
          "block)\n",
          "", s->dot_bias, s->dot_sig, s->ed_bias, s->ed_sig, s->ff_bias,
          s->ff_sig, s->ph_rms, s->n_blk);
  if (s->n_dll)
    printf (
        "    %-7s coasting DLL on the raw block, put at the cell every block: "
        "bias %+.4f sigma %.4f   (%zu dwells; raw e mean %+.4f sigma %.4f "
        "at u0 %+.2f; %.1f reads per block, within-block sd %.3f, "
        "locked %.2f)\n",
        "", s->dll_bias, s->dll_sig, s->n_dll, s->e_raw / (double)s->n_dll,
        sqrt (fmax (s->e_raw2 / (double)s->n_dll
                        - (s->e_raw / (double)s->n_dll)
                              * (s->e_raw / (double)s->n_dll),
                    0.0)),
        DLL_U0, s->n_e_sum / (double)s->n_dll, s->e_sd_sum / (double)s->n_dll,
        s->lock_sum / (double)s->n_dll);
  if (s->n_dll)
    printf ("    %-7s the per-steer sum (dll_take_error): mean %+.4f sigma "
            "%.4f over %.1f steers per block, against the probe's %+.4f\n",
            "", s->es_raw / (double)s->n_dll,
            sqrt (fmax (s->es_raw2 / (double)s->n_dll
                            - (s->es_raw / (double)s->n_dll)
                                  * (s->es_raw / (double)s->n_dll),
                        0.0)),
            s->n_es_sum / (double)s->n_dll, s->e_raw / (double)s->n_dll);
}

/* The held tracker's row: the phase it held through each block against
   the truth, and whether it ever lost the cell. */
static void
print_held (const char *label, const stat_t *s)
{
  if (!s->n_h)
    {
      printf ("    %-7s  (no dwells after acquisition)\n", label);
      return;
    }
  printf ("    %-7s %4zu  argmax %5.1f%%   held phase bias %+.4f sigma %.4f "
          "max |err| %.3f   within half a chip %zu of %zu\n",
          label, s->n_h, 100.0 * (double)s->hits / (double)s->n, s->h_bias,
          s->h_sig, s->h_max, s->n_held, s->n_h);
}

static void
print_rx (const char *label, const stat_t *s)
{
  if (!s->n_rx)
    {
      printf ("    %-7s  (no dwells after the seed)\n", label);
      return;
    }
  printf ("    %-7s %4zu  phase bias %+.4f sigma %.4f max |err| %.3f   "
          "within half a chip %zu of %zu   code flag %.2f sym %.2f\n",
          label, s->n_rx, s->rx_bias, s->rx_sig, s->rx_max, s->n_rx_held,
          s->n_rx, s->rx_code, s->rx_sym);
}

/* The synth's data symbol at absolute symbol index `sym`: 0 (+1) in the
   code-only window, the payload bit otherwise, by the synth's own rule
   (wfm_synth_cont_dsss_chip). */
static uint8_t
truth_bit (const uint8_t *bits, uint64_t sym)
{
  const uint64_t F = F_SYM, W = W_SYM;
  if (sym % F < W)
    return 0u;
  return bits[((sym / F) * (F - W) + (sym % F - W)) % N_BITS] & 1u;
}

/* The BER of the shipped receiver's symbols against the synth's payload:
   the truth from a margin before the seed's symbol on, aligned by
   dp_ber_sync past the receiver's settling, scored past it. One
   alignment for the whole record, so a carrier cycle slip (BPSK: 180
   degrees) inverts every bit after it until the next -- the BER then
   says 0.3 where the decoder between the slips is at theory. `*slips`
   counts them: the scored window in SLIP_CHUNK-symbol chunks, a chunk
   inverted when more than half its bits disagree, a slip at each change
   of state (dp_ber_sync's own `slips` needs a repeating marker; this
   record has one occurrence). A measurement that cannot show the bad
   case would have called 0.3 a decoder. */
#define SLIP_CHUNK 500u
static double
score_ber (const sink_ctx_t *c, double cn0, const char *label, size_t *slips)
{
  *slips = 0;
  if (!c->rx_seeded || c->rx_nsyms < 1000)
    return NAN;
  const size_t margin = 16; /* the receiver's first symbol is a few past
                               the seed's; the sync's lag span is short */
  const size_t n_truth = c->rx_nsyms + 2 * margin;
  uint8_t     *truth   = dp_xmalloc (n_truth);
  for (size_t j = 0; j < n_truth; j++)
    truth[j] = truth_bit (c->bits, c->rx_seed_sym + j - margin);
  dp_ber_t acc;
  dp_ber_init (&acc, 2, 0);
  const double    esn0   = cn0 - 10.0 * log10 (SYM_RATE);
  const size_t    settle = 8 * (size_t)((double)c->dwell_len * SYM_RATE / FS);
  dp_ber_report_t r   = dp_ber_measure (&acc, c->rx_syms, c->rx_nsyms, truth,
                                        n_truth, esn0, settle, 1, NULL);
  const double    ber = r.ber.p_hat;
  /* The same alignment dp_ber_measure used (its blind marker sits a lag
     span past the settling), rescored chunk by chunk. */
  dp_ber_marker_t mk;
  mk.sym    = NULL;
  mk.n      = DP_BER_SYNC_SYMS;
  mk.t0     = settle + (size_t)DP_BER_LAG_SPAN;
  mk.period = 0;
  mk.reps   = 0;
  const dp_ber_sync_t sy
      = dp_ber_sync (c->rx_syms, c->rx_nsyms, truth, n_truth, &mk, 2,
                     DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
  int    inverted = 0;
  size_t chunks   = 0;
  for (size_t lo = r.window_lo; sy.ok && lo < r.window_hi; lo += SLIP_CHUNK)
    {
      const size_t hi
          = lo + SLIP_CHUNK < r.window_hi ? lo + SLIP_CHUNK : r.window_hi;
      dp_ber_t ch;
      dp_ber_init (&ch, 2, 0);
      dp_ber_score (&ch, c->rx_syms, lo, hi, truth, n_truth, &mk, &sy);
      const int inv = ch.bits && 2 * ch.bit_errors > ch.bits;
      dp_ber_free (&ch);
      if (inv != inverted)
        (*slips)++;
      inverted = inv;
      chunks++;
    }
  printf ("    %-7s BER %.2e (%lu errors in %lu bits; theory %.2e at Es/N0 "
          "%.1f dB; EVM %.1f dB; window [%zu,%zu) of %zu symbols; %zu "
          "cycle slips in %zu chunks of %u)\n",
          label, r.ber.p_hat, acc.bit_errors, acc.bits, r.theory_ber, esn0,
          r.evm_db, r.window_lo, r.window_hi, c->rx_nsyms, *slips, chunks,
          SLIP_CHUNK);
  dp_ber_free (&acc);
  free (truth);
  return ber;
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  printf ("the searcher's surface as a code tracker: Gold-1023 at 5 Mcps, "
          "spc %u, %.0f sym/s async BPSK, %u code-only symbols of every %u; "
          "the pool's engine at ±%.0f kHz with the carrier told; the "
          "discriminator read at the truth's cell, calibrated on a clean "
          "sweep at 18 ppm\n\n",
          SPC, SYM_RATE, W_SYM, F_SYM, DU / 1e3);
  printf ("  DLL, per-epoch, from section 12.5: %.4f chips at 45 dB-Hz, "
          "%.4f at 40\n",
          DLL_45, DLL_40);

  const double cn0s[] = { 45.0, 40.0 };
  const double ppms[] = { 0.0, 18.0 };
  const size_t n_cn0  = check ? 1 : 2;
  const size_t n_dw   = check ? CHECK_DWELLS : MEAS_DWELLS;
  const size_t n_cal  = check ? CHECK_DWELLS : CAL_DWELLS;
  sink_ctx_t   c;
  memset (&c, 0, sizeof c);
  c.d       = dp_xmalloc ((n_dw > n_cal ? n_dw : n_cal) * sizeof *c.d);
  c.rx_gain = NAN;
  /* The synth's payload: a harness PRBS the BER is scored against. */
  c.bits = dp_xmalloc (N_BITS);
  {
    uint32_t st = 0xB1750u;
    for (size_t i = 0; i < N_BITS; i++)
      c.bits[i] = (uint8_t)(dp_xs32 (&st) & 1u);
  }

  for (size_t ci = 0; ci < n_cn0; ci++)
    {
      acq_state_t *a = make_engine (code, cn0s[ci], &c);
      DP_REQUIRE_MSG (a != NULL, "the engine opens at the operating point");
      printf ("\n=== sized at %.0f dB-Hz ===\n", cn0s[ci]);
      print_engine (a, &c);

      cal_t cal;
      DP_REQUIRE_MSG (calibrate (a, code, &c, &cal, n_cal) == 0,
                      "the clean sweep calibrates");
      printf ("  calibration, clean code-only at 18 ppm, %zu dwells: c0 %+.3f "
              "chips (drift %+.5f chips/dwell); slope through the origin "
              "coherent %.3f, Parseval %.3f chips/unit; the coherent "
              "S-curve inverted on the sweep: %.4f chips RMS\n",
              cal.n_win, cal.c0, cal.drift, cal.coh.gain, cal.par.gain,
              cal.resid);
      printf ("    S-curve, u = cell - truth (chips) -> magnitude, Parseval, "
              "dot, epoch-dot:");
      for (int b = 0; b < NB; b++)
        if (cal.coh.n[b])
          printf (" %+.3f:%+.3f/%+.3f/%+.3f/%+.3f", cal.coh.u[b], cal.coh.s[b],
                  cal.par.s[b], cal.dot.s[b], cal.edot.s[b]);
      printf ("\n");
      double dll_rate = 0.0;
      DP_REQUIRE_MSG (calibrate_dll_phase (a, code, &c, &cal, 40, &dll_rate)
                          == 0,
                      "the DLL's phase convention calibrates");
      printf ("    the DLL running from the cell, clean at 18 ppm, 40 blocks: "
              "converged %+.3f chips from the cell (its convention; every "
              "seed carries it), code_rate %.6f with the rate aid on\n",
              cal.c_dll, dll_rate);
      DP_REQUIRE_MSG (calibrate_dll (a, code, &c, &cal, check ? 24 : 40) == 0,
                      "the DLL's S-curve calibrates across seeds");
      /* The same loop RUNNING under this C/N0's noise, on the wiped
         block: the receiver's case, so the harness's DLL is known to be
         the receiver's before its coasting read is scored. */
      {
        c.track = 1;
        DP_REQUIRE (
            run (a, code, 18.0, cn0s[ci], 99u, cal.c0, W_SYM, 0.0, &c, 40)
            == 0);
        c.track   = 0;
        double lk = 0.0;
        size_t m  = 0;
        for (size_t k = DLL_SETTLE; k < c.n; k++)
          if (!isnan (c.d[k].dll_e))
            {
              lk += isnan (c.d[k].locked) ? 0.0 : c.d[k].locked;
              m++;
            }
        const double locked = m ? lk / (double)m : 0.0;
        printf ("    the DLL running at %.0f dB-Hz on the wiped block: locked "
                "%.2f of the time, %+.3f chips from the cell at the end, "
                "code_rate %.6f\n",
                cn0s[ci], locked, c.last_phase_err, c.last_rate);
        if (check)
          DP_CHECK_MSG (locked > 0.9 && fabs (c.last_phase_err) < 0.05,
                        "the harness's DLL is the receiver's: running, it "
                        "locks and holds the cell");
      }
      printf ("    coasting DLL: drifts %+.3f chips/s on the held rate (the "
              "NCO's quantisation of the aid; put back every block); "
              "S-curve, u0 -> discriminator:",
              cal.dll_drift);
      for (int b = 0; b < NB; b++)
        printf (" %+.3f:%+.4f", cal.dll.u[b], cal.dll.s[b]);
      printf ("\n");
      printf ("\n");
      if (check)
        {
          DP_CHECK_MSG (fabs (cal.drift) < 0.01,
                        "the surface's code axis and the truth share a "
                        "convention: the cell minus the truth does not drift");
          DP_CHECK_MSG (cal.coh.gain != 0.0 && cal.resid < 0.05,
                        "the discriminator's S-curve inverts across the cell "
                        "on a clean stream");
        }

      printf ("  per-dwell error at the truth's cell (chips; Doppler in "
              "Hz); argmax = the surface's own maximum within a chip\n");
      printf ("    class     n   argmax   E/L bias  sigma   Parseval bias "
              "sigma   parabola bias sigma   Doppler bias sigma   prompt\n");
      for (size_t pi = 0; pi < 2; pi++)
        {
          printf ("  %.0f dB-Hz at %.0f ppm:\n", cn0s[ci], ppms[pi]);
          DP_REQUIRE (run (a, code, ppms[pi], cn0s[ci], 11u + (uint32_t)pi,
                           cal.c0, W_SYM, DLL_U0, &c, n_dw)
                      == 0);
          stat_t w, dt;
          stats (&c, &cal, 1, &w);
          stats (&c, &cal, 0, &dt);
          print_row ("window", &w);
          print_row ("data", &dt);
          /* The same, closed on the searcher's own cell: acquired from the
             surface at the first window dwell, corrected once a block by
             the coasting DLL's read, the truth used only to score. Gain 1
             at both drifts; the filtered gains under dilation (the check
             takes 1 and 0.25). */
          c.held      = 1;
          c.coh_cal   = &cal.coh;
          c.dll_cal   = &cal.dll;
          double sig1 = 0.0;
          for (size_t gi = 0; gi < N_GAINS; gi++)
            {
              if (gi && ppms[pi] != 18.0)
                break;
              if (check && gi && HELD_GAINS[gi] != 0.25)
                continue;
              c.h_gain = HELD_GAINS[gi];
              DP_REQUIRE (run (a, code, ppms[pi], cn0s[ci], 21u + (uint32_t)pi,
                               cal.c0, W_SYM, DLL_U0, &c, n_dw)
                          == 0);
              stat_t hw, hd;
              stats (&c, &cal, 1, &hw);
              stats (&c, &cal, 0, &hd);
              if (!c.acquired)
                {
                  printf ("    tracker on the searcher's own cell: never "
                          "acquired (no window dwell in %zu)\n",
                          c.n);
                  break;
                }
              if (!gi)
                sig1 = hd.h_sig;
              printf ("    tracker on the searcher's own cell, gain %.3f: "
                      "acquired at dwell %zu, %+.3f chips and %+.1f Hz from "
                      "the truth; corrected once a block by that fraction of "
                      "the coasting DLL's read after %d blocks of settling; "
                      "%.1f s; first order predicts sigma %.4f under data\n",
                      c.h_gain, c.at_acq, c.h_acq_err, c.f_acq_err, DLL_SETTLE,
                      (double)(c.n - c.at_acq) * (double)c.dwell_len / FS,
                      sig1 * sqrt (c.h_gain / (2.0 - c.h_gain)));
              print_held ("window", &hw);
              print_held ("data", &hd);
              if (check && ppms[pi] == 18.0 && !gi)
                {
                  DP_CHECK_MSG (c.acquired && fabs (c.h_acq_err) < 0.1
                                    && fabs (c.f_acq_err) < 20.0,
                                "the tracker acquires from the surface "
                                "within a tenth of a chip and a row of the "
                                "truth");
                  DP_CHECK_MSG (hd.n_h >= 100 && hd.n_held == hd.n_h
                                    && hw.n_held == hw.n_h,
                                "the held tracker never leaves the cell");
                  /* Measured in section 12.23; twice the closed loop's
                     jitter is the defect gate, as for the truth-cell
                     read. */
                  DP_CHECK_MSG (hd.h_sig < 2.0 * DLL_45
                                    && fabs (hd.h_bias) < 0.05,
                                "the phase held on the searcher's own cell "
                                "is within twice the loop's closed-loop "
                                "jitter of the truth, without bias");
                }
              else if (check && ppms[pi] == 18.0)
                {
                  /* Section 12.24: at gain 0.25 first order keeps 0.143 of
                     the read's variance, 0.38 of its sigma. Under 0.75 of
                     gain 1's, still never leaving the cell, is the gate:
                     the filter reduces the noise it is there to reduce. */
                  DP_CHECK_MSG (hd.n_held == hd.n_h && hw.n_held == hw.n_h,
                                "the filtered tracker never leaves the "
                                "cell");
                  DP_CHECK_MSG (hd.h_sig < 0.75 * sig1
                                    && fabs (hd.h_bias) < 0.05,
                                "the filtered correction holds the phase "
                                "under three quarters of the unfiltered "
                                "jitter, without bias");
                }
            }
          c.held = 0;
          /* The shipped receiver on the same stream (section 12.26):
             seeded from the surface at the first window dwell, fed the
             epochs from there, its held phase read off its status and
             its symbols scored against the synth's payload. The cell
             mode at the design gain, at gain 1 under dilation, and the
             hand-off flavour on the same seed as the parity reference
             (its phase is undefined until its refine ends, so its
             window row is not a read). The check runs the design gain
             and the reference. */
          if (ppms[pi] == 18.0)
            for (size_t fi = 0; fi < 3; fi++)
              {
                const int    handoff = fi == 2;
                const double g       = fi == 1 ? 1.0 : ASYNC_DSSS_RX_CELL_GAIN;
                if (check && fi == 1)
                  continue;
                c.rx_gain    = g;
                c.rx_handoff = handoff;
                DP_REQUIRE (run (a, code, ppms[pi], cn0s[ci],
                                 31u + (uint32_t)pi, cal.c0, W_SYM, NAN, &c,
                                 n_dw)
                            == 0);
                c.rx_gain        = NAN;
                c.rx_handoff     = 0;
                const char *name = handoff ? "HandoffAsyncDsssReceiver"
                                           : "CellAsyncDsssReceiver";
                stat_t      rw, rd;
                stats (&c, &cal, 1, &rw);
                stats (&c, &cal, 0, &rd);
                if (!c.rx_seeded)
                  {
                    printf ("    %s: never seeded (no window dwell in "
                            "%zu)\n",
                            name, c.n);
                    continue;
                  }
                if (handoff)
                  printf ("    %s: seeded at dwell %zu, %+.3f chips from "
                          "the truth; %zu symbols over %.1f s\n",
                          name, c.rx_at, c.rx_seed_err, c.rx_nsyms,
                          (double)(c.n - c.rx_at) * (double)c.dwell_len / FS);
                else
                  printf ("    %s, gain %.3f: seeded at dwell %zu, %+.3f "
                          "chips from the truth; %zu symbols over %.1f s\n",
                          name, g, c.rx_at, c.rx_seed_err, c.rx_nsyms,
                          (double)(c.n - c.rx_at) * (double)c.dwell_len / FS);
                if (!handoff)
                  print_rx ("window", &rw);
                print_rx ("data", &rd);
                size_t       slips;
                const double ber = score_ber (&c, cn0s[ci], "data", &slips);
                if (check && !handoff)
                  {
                    DP_CHECK_MSG (rd.n_rx >= 100 && rd.n_rx_held == rd.n_rx
                                      && rw.n_rx_held == rw.n_rx,
                                  "the shipped cell receiver never leaves "
                                  "the cell");
                    /* The receiver's steer is the harness's read through
                       its own carrier loop and a rate over the interval;
                       within twice the loop's jitter is the defect gate. */
                    DP_CHECK_MSG (
                        rd.rx_sig < 2.0 * DLL_45 && fabs (rd.rx_bias) < 0.05,
                        "the shipped cell receiver holds the phase "
                        "within twice the loop's closed-loop jitter, "
                        "without bias");
                    DP_CHECK_MSG (rd.rx_code > 0.95 && rd.rx_sym > 0.95,
                                  "the shipped cell receiver holds both lock "
                                  "flags");
                    DP_CHECK_MSG (!isnan (ber) && ber < 1e-2,
                                  "the shipped cell receiver decodes the "
                                  "payload at 45 dB-Hz");
                    DP_CHECK_MSG (slips == 0,
                                  "the shipped cell receiver's carrier "
                                  "never slips a cycle at 45 dB-Hz");
                  }
              }
          if (check && ppms[pi] == 18.0)
            {
              /* The claim under test: at 45 dB-Hz under dilation the
                 coherent discriminator's window-dwell jitter and bias
                 are within the bounds measured on this harness. Bounds
                 are set from the measurement; a defect gate, not a
                 ratchet. */
              DP_CHECK_MSG (w.n >= 8, "window dwells were seen");
              DP_CHECK_MSG (w.hits == w.n,
                            "in the window the surface's maximum is the "
                            "emitter, every dwell");
              /* Measured 0.015 chips at 45 dB-Hz on 59 window dwells
                 (§12.20); the DLL's per-epoch loop reads 0.013. Twice
                 the DLL's is the defect gate, with room for the dozen
                 dwells this check sees. */
              DP_CHECK_MSG (w.coh_sig < 2.0 * DLL_45
                                && fabs (w.coh_bias) < 0.05,
                            "the E/L on the truth's row, inverted through "
                            "its S-curve, reads the code phase within twice "
                            "the DLL's jitter and without bias");
              DP_CHECK_MSG (w.n_blk == w.n,
                            "the block taps read on every window dwell");
              DP_CHECK_MSG (w.dot_sig < 2.0 * DLL_45
                                && fabs (w.dot_bias) < 0.05,
                            "the coherent discriminator on the complex "
                            "surface reads the code phase within twice the "
                            "DLL's jitter and without bias");
              DP_CHECK_MSG (dt.n_dll >= 100,
                            "the coasting DLL read the raw block on the "
                            "data dwells");
              /* Measured 0.0134 chips at 45 dB-Hz under data (section
                 12.22), the loop's own closed-loop 0.013; twice it is the
                 defect gate. */
              DP_CHECK_MSG (dt.dll_sig < 2.0 * DLL_45
                                && fabs (dt.dll_bias) < 0.05,
                            "the coasting DLL's symbol-aided read of the raw "
                            "block under data is within twice the loop's "
                            "closed-loop jitter and without bias");
            }
        }
      acq_destroy (a);
      free (c.hz);
      free (c.chip);
      c.hz = c.chip = NULL;
    }
  free (c.d);
  free (c.bits);
  if (check)
    DP_TEST_END ("validate_acq_surface_jitter");
  return 0;
}
