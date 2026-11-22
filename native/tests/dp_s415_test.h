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

/** The receiver at the point and carrier @p fc, designed for the floor:
 *  its `cn0_dbhz` is the floor's C/N0 (Es/N0 DP_S415_FLOOR_DB) whatever
 *  level the scene runs at, since a receiver is built once, for its weakest
 *  signal (owner, 2026-11-21). The binding's defaults, the point's search
 *  span, its gap (the code-only symbols, in seconds) and its Doppler rate.
 *  @p refine is the argument the callers vary. */
static inline dp_async_dsss_receiver_state_t *
dp_s415_rx (const uint8_t *code, double fc, bool refine)
{
  return dp_async_dsss_receiver_create (
      code, DP_S415_SF, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      dp_s415_cn0 (DP_S415_FLOOR_DB), 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, false, 100000, fc, 0.0, false,
      (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM, DP_S415_RATE_HZ_S, refine);
}

/** A pool of @p n receivers at the point, as dp_s415_rx() builds each (at
 *  the floor's C/N0), with its release rule (@p lost_s) and @p threads. */
static inline dp_async_dsss_pool_state_t *
dp_s415_pool (const uint8_t *code, double fc, size_t n, double lost_s,
              int threads)
{
  return dp_async_dsss_pool_create (
      code, DP_S415_SF, n, dp_s415_chip_rate (fc), DP_S415_SYM, DP_S415_FS, 2,
      dp_s415_cn0 (DP_S415_FLOOR_DB), 1e-3, 0.9, DP_S415_SPAN_HZ, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, false, 100000, fc, lost_s, false,
      (double)DP_S415_CODE_ONLY_SYM / DP_S415_SYM, DP_S415_RATE_HZ_S, threads,
      true);
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
