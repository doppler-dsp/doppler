/*
 * ccsds_415.c — the CCSDS 415.1-B-1 specification point's arithmetic
 * (doppler/ccsds/ccsds_415.h).
 *
 * Fast math is off for each function alone, as dp_wfm_dsss_cont_edge()
 * does it (wfm_dsp.c): GCC by an optimize pragma around it, Clang and
 * clang-cl by a block-scope float_control. Measured before it was written:
 * under Clang's -ffast-math, start_hz / fc * 1e6 at 2.2 GHz came out one
 * ulp from the value the spec demo and its Python twin typed
 * (-0.0062500000000000003 against -0.0062499999999999995), and a carrier
 * read at run time may make fc * 31 / 21216 into fc * 31 * (1 / 21216),
 * one ulp off at 2.8 GHz. With fast math off every result equals the
 * twins' literals bit for bit under GCC and Clang alike.
 */
#include "doppler/ccsds/ccsds_415.h"

#include <math.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("no-fast-math")
#endif

double
dp_ccsds_415_chip_rate (double fc)
{
#if defined(__clang__)
#pragma float_control(precise, on)
#endif
  return fc * 31.0 / (221.0 * 96.0);
}

double
dp_ccsds_415_cn0 (double esn0_db)
{
#if defined(__clang__)
#pragma float_control(precise, on)
#endif
  return esn0_db + 10.0 * log10 (DP_CCSDS_415_SYM);
}

dp_ccsds_415_scene_t
dp_ccsds_415_scene (double fc, double spc, double start_hz)
{
#if defined(__clang__)
#pragma float_control(precise, on)
#endif
  dp_ccsds_415_scene_t s;
  s.chip_rate  = dp_ccsds_415_chip_rate (fc);
  s.ratio      = DP_CCSDS_415_FS / (spc * s.chip_rate);
  s.start_ppm  = start_hz / fc * 1e6;
  s.rate_ppm_s = DP_CCSDS_415_R_ENV_HZ_S / fc * 1e6;
  return s;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
