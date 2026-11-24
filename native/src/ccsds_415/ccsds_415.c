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

static int
refuse (const char **why, const char *sentence)
{
  if (why)
    *why = sentence;
  return -1;
}

int
dp_ccsds_415_rx (dp_ccsds_415_rx_t *a, size_t code_len, double carrier_freq_hz,
                 double lost_confirm_s, bool refine, const char **why)
{
  if (code_len != DP_CCSDS_415_SF)
    return refuse (why, "the code must be 1,023 chips: the point's Gold "
                        "code length (the code is the mission's)");
  if (!isfinite (carrier_freq_hz))
    return refuse (why, "carrier_freq_hz must be finite");
  /* The two edges are the receiver's own conditions at the point's rates,
     symbol_rate x code_len <= chip_rate and chip_rate <= sample_rate,
     tested on the chip rate itself so the window is exactly the one
     dp_async_dsss_receiver_create() accepts. */
  const double chip_rate = dp_ccsds_415_chip_rate (carrier_freq_hz);
  if (!(DP_CCSDS_415_SYM * (double)DP_CCSDS_415_SF <= chip_rate))
    return refuse (why,
                   "carrier_freq_hz is below DP_CCSDS_415_RX_FC_MIN (about "
                   "2.1004 GHz): its chip rate is under 1,023 x 3,000, so a "
                   "symbol is shorter than a code period");
  if (!(chip_rate <= DP_CCSDS_415_FS))
    return refuse (why,
                   "carrier_freq_hz is above DP_CCSDS_415_RX_FC_MAX (about "
                   "7.528 GHz): its chip rate passes the 11 MSa/s sample "
                   "rate");
  if (!(lost_confirm_s >= 0.0))
    return refuse (why, "lost_confirm_s must be >= 0");
  *a = (dp_ccsds_415_rx_t){
    .chip_rate                 = chip_rate,
    .symbol_rate               = DP_CCSDS_415_SYM,
    .sample_rate               = DP_CCSDS_415_FS,
    .m                         = 2,
    .cn0_dbhz                  = dp_ccsds_415_cn0 (DP_CCSDS_415_FLOOR_ESN0_DB),
    .pfa                       = 1e-3,
    .pd                        = 0.9,
    .doppler_uncertainty       = DP_CCSDS_415_SPAN_HZ,
    .segments                  = 4,
    .sps                       = 8,
    .differential              = 0,
    .refine_max_error_db       = 0.5,
    .refine_samples_per_symbol = 4,
    .refine_design_margin_db   = 14.0,
    .refine_n_fft              = 64,
    .refine_zero_pad           = 8,
    .refine_sequential         = false,
    .refine_max_n_blocks       = 100000,
    .carrier_freq_hz           = carrier_freq_hz,
    .lost_confirm_s            = lost_confirm_s,
    .reacquire                 = false,
    .code_only_s  = (double)DP_CCSDS_415_CODE_ONLY_SYM / DP_CCSDS_415_SYM,
    .doppler_rate = DP_CCSDS_415_R_ENV_HZ_S,
    .refine       = refine,
  };
  return 0;
}
