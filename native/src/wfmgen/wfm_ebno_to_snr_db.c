/*
 * wfm_ebno_to_snr_db.c — wfmgen module-level function.
 */
#include "wfmgen/wfmgen_core.h"
#include <math.h>

/*
 * Convert Eb/No (dB) to SNR (dB) measured over the full sample rate fs.
 *   Es/No = Eb/No + 10log10(bits_per_symbol)         (energy per symbol)
 *   SNR_fs = Es/No - 10log10(samples_per_symbol)     (spread over fs)
 * i.e. SNR_lin = Eb/No_lin * bits_per_symbol / samples_per_symbol.
 */
float
wfm_ebno_to_snr_db (float ebno_db, int bits_per_symbol,
                    float samples_per_symbol)
{
  return ebno_db + 10.0f * log10f ((float)bits_per_symbol)
         - 10.0f * log10f (samples_per_symbol);
}
