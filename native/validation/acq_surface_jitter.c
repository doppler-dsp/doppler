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
 *           truth -- the detector's view, for scale.
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
 *                                          DLL running locks on the cell
 */
#include "acq/acq_core.h"
#include "awgn/awgn_core.h"
#include "clib_common.h"
#include "dll/dll_core.h"
#include "doppler_channel/doppler_channel_core.h"
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
#define DLL_U0 0.1   /* the coasting loop\'s seed offset from the cell    */

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
  double locked; /* the lock flag's mean over the block               */
  double e_sd;   /* the discriminator's scatter within the block       */
  double dll_u;  /* the loop's actual offset from the truth at the
                    block's end, chips: the seed plus what it drifted  */
} dwell_t;

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
} sink_ctx_t;

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
           uint32_t seed, size_t win_sym)
{
  memset (s, 0, sizeof *s);
  s->ppm = ppm;
  s->syn = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                             seed, (int)SPC, 15, 0, 0, 0.0);
  if (!s->syn
      || wfm_synth_set_dsss_cont (s->syn, code, SF, CPS, WFM_DSSS_DATA_PRBS,
                                  NULL, 0)
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
  /* The re-correlation: the block as pushed through the coasting DLL,
     epoch by epoch, and the mean of the discriminator outputs it emitted
     -- the symbol-aided window on the searcher's timing. */
  d->dll_e = NAN;
  if (c->dll && D > 1
      && acq_block_raw (c->a, c->raw, D * c->cols) == D * c->cols)
    {
      /* The correction a tracker applies once per block: the loop put at
         the cell's phase -- the truth's here, as every other read -- plus
         the seed offset, at the block's start, before it is fed. What the
         loop reads over the block is then u0 plus the drift within it. */
      if (!c->track)
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
      d->n_e    = ne;
      d->e_sd   = ne ? sqrt (fmax (se2 / (double)ne
                                       - (se / (double)ne) * (se / (double)ne),
                                   0.0))
                     : 0.0;
      d->locked = nl ? (double)nlk / (double)nl : NAN;
      if (ne && c->n >= DLL_SETTLE)
        d->dll_e = se / (double)ne;
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

/* Run one stimulus through one engine for `n` decided dwells. */
static int
run (acq_state_t *a, const uint8_t *code, double ppm, double cn0,
     uint32_t seed, double c0, size_t win_sym, double u0, sink_ctx_t *c,
     size_t n)
{
  stim_t st;
  if (stim_open (&st, code, ppm, cn0, seed, win_sym))
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
  c->dll = NULL;
  c->u0  = u0;
  if (!isnan (u0) && a->coherent_bins > 1)
    {
      const double seed_chip = dp_fmod_pos (
          truth_chips (&st, 0.0) + c0 + c->c_dll + u0, (double)SF);
      c->dll = dll_create (code, SF, SPC, seed_chip, BN, 0.707, 0.5, SEGMENTS);
      DP_REQUIRE_MSG (c->dll != NULL, "the DLL opens");
      DP_REQUIRE (dll_set_symbol_period (c->dll, P_SYM) == DP_OK);
      dll_set_rate_aid (c->dll, c->f_hz / CARRIER_HZ);
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
      c->lo = lo_create (-c->f_hz / FS);
      DP_REQUIRE_MSG (c->lo != NULL, "the wipe's LO opens");
      c->lo_buf
          = dp_xmalloc (a->coherent_bins * a->code_bins * sizeof *c->lo_buf);
      c->prt = dp_xmalloc (dll_steps_max_out (c->dll) * sizeof *c->prt);
    }
  acq_reset (a);
  acq_set_surface_sink (a, on_surface, c, 1u);
  acq_result_t hits[16];
  while (c->n < n)
    (void)acq_push (a, stim_block (&st), TE, hits, 16);
  acq_set_surface_sink (a, NULL, NULL, 1u);
  stim_close (&st);
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
typedef struct
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
  double coh_bias, coh_sig; /* calibrated E/L on the truth row     */
  double par_bias, par_sig; /* Parseval over the tile's rows       */
  double pb_bias, pb_sig;   /* parabola on the truth row           */
  double f_bias, f_sig;     /* parabola over the rows, Hz          */
  double dot_bias, dot_sig; /* coherent E/L on the complex surface */
  double ed_bias, ed_sig;   /* per-epoch coherent E/L over the block */
  double ff_bias, ff_sig;   /* the prompt column's phase-fit Doppler */
  double ph_rms;            /* the fit's residual, rad per epoch     */
  size_t n_blk;             /* dwells with the block taps read       */
  double dll_bias, dll_sig; /* the coasting DLL, its read minus u0   */
  size_t n_dll;             /* dwells with a DLL read                */
  double e_raw, e_raw2;     /* the raw discriminator's sums          */
  double n_e_sum, lock_sum; /* outputs per block; lock flag mean     */
  double e_sd_sum;          /* within-block scatter of e, summed     */
  double snr;               /* mean prompt over the gate's units   */
} stat_t;

static void
stats (const sink_ctx_t *c, const cal_t *cal, int want_window, stat_t *o)
{
  memset (o, 0, sizeof *o);
  double sc = 0, sc2 = 0, sp = 0, sp2 = 0, sb = 0, sb2 = 0, sf = 0, sf2 = 0,
         ss = 0, sd = 0, sd2 = 0, se = 0, se2 = 0, sq = 0, sq2 = 0, sr2 = 0;
  double sl = 0, sl2 = 0;
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
          o->n_e_sum += (double)d->n_e;
          o->e_sd_sum += d->e_sd;
          o->lock_sum += isnan (d->locked) ? 0.0 : d->locked;
          o->n_dll++;
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
  c.d = dp_xmalloc ((n_dw > n_cal ? n_dw : n_cal) * sizeof *c.d);

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
  if (check)
    DP_TEST_END ("validate_acq_surface_jitter");
  return 0;
}
