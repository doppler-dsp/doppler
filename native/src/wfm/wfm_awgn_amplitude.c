/*
 * wfm_awgn_amplitude.c — wfm module-level function.
 *
 * Public alias exposing the AWGN amplitude solver under the wfm module
 * namespace. The kernel lives once in awgn_core.c (C-first: the algorithm is
 * not duplicated here), because it inverts THAT generator's per-component
 * convention and awgn_create() is what consumes the result. Keeping it there
 * also puts it in doppler_lib_static, so a C test or validation harness can
 * reach it -- it could not before, which is why several harnesses derived
 * their own noise level and one of them derived it 3 dB wrong (gh-713).
 */
#include "awgn/awgn_core.h" /* awgn_amplitude_for_snr */
#include "wfm/wfm_core.h"

float
wfm_awgn_amplitude (float snr_db, float signal_power)
{
  return awgn_amplitude_for_snr (snr_db, signal_power);
}
