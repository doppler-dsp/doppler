/**
 * @file dp_s415_test.h
 * @brief The receiver's specification point, CCSDS 415.1-B-1, stated once:
 *        its geometry, the receiver and pool built there, and an emitter on
 *        the air there.
 *
 * Every number is a row of docs/design/async-dsss-receiver.md §1's table,
 * which names its source; nothing here is chosen. A test, a validator or a
 * bench that measures the receiver at the point includes this header rather
 * than restating the point, so they cannot measure two points under one
 * name. Before this header the point was written out six times, under five
 * prefixes.
 *
 *   code         Gold, 1,023 chips (dp_dsss_gold_1023(), dp_dsss_test.h)
 *   chip rate    F_fwd x 31 / (221 x 96)                  table 4-1
 *   carrier      F_fwd 2.2 to 2.8 GHz                     owner
 *   symbols      3,000 sym/s, asynchronous to the code    owner
 *   capture      11 MSa/s, complex                        owner
 *   Doppler      within +/-1,500 Hz, ramping 400 Hz/s     owner
 *   floor        Es/N0 4 dB                               owner
 *   frame        1,000 symbols, the first 275 code alone  owner, §1.4
 *
 * The emitter goes through the scene of dp_air_test.h, which owns the
 * front end's model. The receiver and the pool take the binding's
 * defaults for everything the point does not set.
 */
#ifndef DP_S415_TEST_H
#define DP_S415_TEST_H

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dll/dll_core.h"
#include "dp_air_test.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Chips a code period. */
#define DP_S415_SF 1023u
/** The capture rate, complex samples a second. */
#define DP_S415_FS 11e6
/** Data symbols a second. */
#define DP_S415_SYM 3000.0
/** The band's two ends, F_fwd, Hz. */
#define DP_S415_FC_LO 2.2e9
#define DP_S415_FC_HI 2.8e9
/** The Doppler uncertainty about best lock, one side, Hz. */
#define DP_S415_SPAN_HZ 1500.0
/** The Doppler rate, Hz/s. */
#define DP_S415_RATE_HZ_S 400.0
/** The sensitivity floor, Es/N0 dB. */
#define DP_S415_FLOOR_DB 4.0
/** Symbols a frame, and of them the code-only ones that open it. */
#define DP_S415_FRAME_SYM 1000u
#define DP_S415_CODE_ONLY_SYM 275u

/** Chips a second at carrier @p fc: F_fwd x 31 / (221 x 96). */
static inline double
dp_s415_chip_rate (double fc)
{
  return fc * 31.0 / (221.0 * 96.0);
}

/** C/N0, dB-Hz, of an Es/N0 at the point's symbol rate. */
static inline double
dp_s415_cn0 (double esn0_db)
{
  return esn0_db + 10.0 * log10 (DP_S415_SYM);
}

/** The receiver at the point and carrier @p fc, designed for @p cn0:
 *  the binding's defaults, the point's search span, its gap (the code-only
 *  symbols, in seconds) and its Doppler rate. @p refine and @p window are
 *  the two arguments the callers vary. */
static inline dp_async_dsss_receiver_state_t *
dp_s415_rx (const uint8_t *code, double fc, double cn0, bool refine,
            int window)
{
  return dp_async_dsss_receiver_create (
      code, DP_S415_SF, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      cn0, 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
      100000, fc, 0.0, false, (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM,
      DP_S415_RATE_HZ_S, refine, window);
}

/** The fixed-epoch comparison (#3134, the owner's question whether the
 *  code loop's integration is the code epoch alone): the receiver with its
 *  tracking Dll's symbol-timing aid OFF, so the lock detector looks once a
 *  partial and the loop steers on the per-epoch look-back.
 *
 *  The receiver turns the aid on when it builds its tracking Dll, at each
 *  hand-over, and sizes the lock detector for the aided window there
 *  (native/src/async_dsss_receiver/async_dsss_receiver_core.c:312-336).
 *  Called after every push, this turns the aid off on a Dll it has not
 *  seen and sizes the detector for a one-partial look by that same rule --
 *  det_n_noncoh at Pd 0.99, Pfa 1e-3 over the look's samples at the design
 *  C/N0, the drop count from 0.01 at a 1e-6 budget.
 *
 *  It is a SECOND COPY of that derivation, for this comparison only, and
 *  it does not stay: if the owner rules for the fixed-epoch look, the
 *  receiver sizes it itself and this goes in that commit; if the aid
 *  stays, this and the harnesses' `--unaided` go once the comparison is
 *  recorded. `seen` starts NULL.
 *
 *  Modes: 1 the aid off (one look a partial); 2 the Dll's epoch look; 3
 *  the aid kept, its window one epoch long (below). With mode 2, the Dll's
 *  epoch look instead (dp_dll_set_epoch_look()):
 *  one look an epoch at the look-back's winning one-epoch window, sized by
 *  the union bound its header states -- the best of `segments` windows
 *  false-alarms at most `segments` times one window's rate, so each look
 *  is sized at Pfa / segments over one epoch's samples. */
/** The placement bound of an epoch-long window placed by the symbol aid
 *  (#3134): the amplitude a window of @p K partials keeps on average when
 *  the symbol is @p P partials and the aid rounds each boundary to the
 *  partial grid. With slack `s = P - K` and the window's start `u` partials
 *  after the true boundary, the edge partial carries a wrong-sign fraction
 *  `x = -u` (u < 0), 0 (0 <= u <= s) or `u - s` (u > s); with a transition
 *  the amplitude is `1 - 2 x / K`. A transition is on half the boundaries
 *  (random data; the frame's code-only symbols are not counted on). Over
 *  the symbols `u` sweeps a unit interval centred at `c`, set by the true
 *  boundary's phase; the hypotheses' integer shifts let the choice take
 *  the best of `c - 1`, `c`, `c + 1`; the bound averages that over `c`.
 *  Midpoint quadrature, 400 x 400 points (the integrand is piecewise
 *  quadratic). `s >= 1` leaves a clean placement for every boundary: 1. */
static inline double
dp_s415_placement_amp (double P, size_t K)
{
  const double sl = P - (double)K;
  if (sl >= 1.0)
    return 1.0;
  enum
  {
    NQ = 400
  };
  double sum = 0.0;
  for (int j = 0; j < NQ; j++)
    {
      const double c0   = -0.5 + (j + 0.5) / NQ;
      double       best = 0.0;
      for (int k = -1; k <= 1; k++)
        {
          double pw = 0.0;
          for (int q = 0; q < NQ; q++)
            {
              const double u = c0 + k - 0.5 + (q + 0.5) / NQ;
              double       x = u < 0.0 ? -u : u <= sl ? 0.0 : u - sl;
              x                = x > 1.0 ? 1.0 : x;
              const double am  = 1.0 - 2.0 * x / (double)K;
              pw += am * am;
            }
          pw = 0.5 + 0.5 * pw / NQ;
          best = pw > best ? pw : best;
        }
      sum += best;
    }
  return sqrt (sum / NQ);
}

static inline void
dp_s415_look (dp_async_dsss_receiver_state_t *rx, dp_dll_state_t **seen,
              int mode)
{
  if (!rx->dll || rx->dll == *seen)
    return;
  *seen = rx->dll;
  const double amp
      = dp_det_cn0_to_snr (rx->cn0_dbhz, rx->chip_rate * (double)rx->spc);
  size_t look = rx->tsamps / rx->segments;
  double pfa = 1e-3, a = amp;
  if (mode == 3)
    {
      /* Mode 3, the symbol-aided look one epoch long: the aid's hypotheses
         place the window, the choice is made on earlier windows (the Dll's
         order), and the window is the epoch's K partials. Sized by det's
         single-window model over one epoch's samples, with the replica's
         correlation loss (1 - p/4)^2 / (1 - p/3) taken off the amplitude
         BY HAND for this comparison (the create commit holds it). */
      const double P = rx->dll->sym_period;
      if (!(P > 0.0))
        abort (); /* mode 3 needs the aid the receiver built */
      (void)dp_dll_set_symbol_period (rx->dll, P); /* clears the detector */
      if (2 * (rx->segments + rx->dll->aid_nhyp) + 2 > rx->dll->aid_ring)
        abort (); /* the ring holds a window of K partials */
      rx->dll->aid_len = rx->segments;
      size_t flips     = 0;
      for (size_t i = 0; i < rx->code_len; i++)
        flips += rx->code[i] != rx->code[(i + 1) % rx->code_len];
      const double p = (double)flips / (double)rx->code_len;
      a = amp * (1.0 - p / 4.0) / sqrt (1.0 - p / 3.0)
          * dp_s415_placement_amp (P, rx->segments);
      look           = rx->tsamps;
    }
  else
    {
      (void)dp_dll_set_symbol_period (rx->dll, 0.0);
      if (mode == 2)
        {
          (void)dp_dll_set_epoch_look (rx->dll, 1);
          look = rx->tsamps;
          pfa  = 1e-3 / (double)rx->segments;
        }
    }
  const int nl = dp_det_n_noncoh (a, (int)look, 0.99, pfa, 4000);
  if (nl >= 1)
    {
      (void)dp_dll_configure_lock (rx->dll, pfa, (size_t)nl, 0.0);
      (void)dp_dll_set_lock_verify (
          rx->dll, rx->dll->lock.n_up,
          (uint32_t)dp_det_verify_count (0.01, 1e-6));
    }
}

/** A pool of @p n receivers at the point, as dp_s415_rx() builds each, with
 *  its release rule (@p lost_s) and @p threads. */
static inline dp_async_dsss_pool_state_t *
dp_s415_pool (const uint8_t *code, double fc, size_t n, double cn0,
              double lost_s, int threads, int window)
{
  return dp_async_dsss_pool_create (
      code, DP_S415_SF, n, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      cn0, 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
      100000, fc, lost_s, false, (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM,
      DP_S415_RATE_HZ_S, threads, true, window);
}

/** An emitter on the air at the point: carrier @p fc, offset @p f0 Hz at
 *  receive time 0, ramping @p rate Hz/s, at @p cn0, its frames as the point
 *  sets them, its first chip @p lag_s after time 0, its data PRBS @p seed.
 *  The fields the point does not set (frame_off, bits, spc) stay 0: the
 *  frame starts at its first symbol, the data is the PRBS, and the scene
 *  renders at DP_AIR_SPC. */
static inline dp_air_emitter_t
dp_s415_emitter (const uint8_t *code, double fc, double f0, double rate,
                 double cn0, double lag_s, uint32_t seed)
{
  dp_air_emitter_t e = { 0 };
  e.code             = code;
  e.code_len         = DP_S415_SF;
  e.chip_rate        = dp_s415_chip_rate (fc);
  e.sym_rate         = DP_S415_SYM;
  e.carrier_hz       = fc;
  e.f0_hz            = f0;
  e.rate_hz_s        = rate;
  e.cn0_dbhz         = cn0;
  e.code_only_sym    = DP_S415_CODE_ONLY_SYM;
  e.frame_sym        = DP_S415_FRAME_SYM;
  e.lag_s            = lag_s;
  e.seed             = seed;
  return e;
}

#endif /* DP_S415_TEST_H */
