/*
 * wfm_awgn_amplitude.c — wfmgen module-level function.
 */
#include "wfmgen/wfmgen_core.h"
#include <math.h>

/*
 * AWGN amplitude for a target SNR (dB, measured over the full sample rate).
 * awgn_create uses per-component sigma `amplitude`, so total complex noise
 * power = 2*amplitude². For SNR = signal_power / noise_power:
 *   amplitude = sqrt(signal_power / (2 * 10^(snr_db/10)))
 */
float
wfm_awgn_amplitude(float snr_db, float signal_power)
{
    float snr_lin = powf(10.0f, snr_db / 10.0f);
    return sqrtf(signal_power / (2.0f * snr_lin));
}
